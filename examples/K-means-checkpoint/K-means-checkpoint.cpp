#ifdef _WIN32
#include "stdafx.h"
#endif

#include "DogeeBase.h"
#include "DogeeRemote.h"
#include <iostream>
#include "DogeeHelper.h"
#include "DogeeAccumulator.h"
#include "DogeeThreading.h"
#include "DogeeLocalSync.h"
#include "DogeeSharedConst.h"
#include "DogeeMacro.h"
#include "DogeeString.h"
#include "DogeeFileTools.h"
#include <cmath>
#include <stdlib.h>
#include <fstream>

using namespace Dogee;
using std::cin;
using std::cout;
using std::endl;

#include <vector>


DefConst(ITER_NUM, int); //300
DefConst(THREAD_NUM, int); //2
DefGlobal(PATH, Ref<DString>)
DefConst(g_num_points, int);


DefConst(g_param, Array<float>);
DefGlobal(g_accu, Ref<DAddAccumulator<float>>);
DefGlobal(g_features, int);

DefGlobal(g_barrier, Ref<DCheckpointBarrier>);
DefGlobal(g_K, int);

static int features;
LBarrier* local_barrier;
static int matK;
float* means;
Ref<DCheckpointBarrier> barrier(1);
static int n_iter = -1;

class LocalDataset
{
public:
	float* dataset;
	int local_dataset_size;
	int m_param_len;

	LocalDataset() :dataset(nullptr), local_dataset_size(0)
	{}

	void GetThreadData(int tid, float* &odataset)
	{
		int toff = tid  * local_dataset_size / THREAD_NUM;
		odataset = dataset + m_param_len * toff;
		return;
	}

	void Load(int param_len, int num_points, int node_id)
	{
		const int local_line = num_points * (node_id - 1) / (DogeeEnv::num_nodes - 1);
		local_dataset_size = num_points / (DogeeEnv::num_nodes - 1);
		dataset = new float[param_len*(local_dataset_size + 1)];
		m_param_len = param_len;
		printf("LL %d LS %d\n", local_line, local_dataset_size);
		std::string path = PATH->getstr();
		if (hasEnding(path, ".csv"))
		{
			ParseCSV(path.c_str(), [&](const char* cell, int line, int index){
				dataset[(line - local_line)*param_len + index] = atof(cell);
				return (line - local_line)<local_dataset_size;
			}, local_line);
		}
		else
		{
			std::ifstream f(PATH->getstr());
			SkipFileToLine(f, PATH->getstr(), local_line);
			for (int i = 0; i < local_dataset_size*param_len; i++)
			{
				float data;
				f >> data;
				dataset[i] = data;
			}
		}
	}
	void Free()
	{
		delete[]dataset;
	}
}local_dataset;


void fetch_global_param(bool is_main)
{
	if (is_main)
	{
		g_param->CopyTo(means, 0, features*matK + matK);
		for (int i = 0; i < matK; i++)
		{
			float tmp = means[features * matK + i];
			if (tmp < 0.1) //if the cluster i is a empty cluster
			{
				int dummy = rand() % local_dataset.local_dataset_size;
				for (int j = 0; j < features; j++)
				{
					means[j + i*features] = local_dataset.dataset[j + dummy*features];
				}
			}
			else
			{
				for (int j = i*features; j < (i + 1)*features; j++)
					means[j] = means[j] / tmp;
			}
		}
	}
	local_barrier->count_down_and_wait();
}


void slave_worker(float* thread_local_data, int thread_point_num,
	float* center, float* local_loss, int *cluster_num,int iter_start)
{
	DogeeEnv::InitCurrentThread();
	for (int itr = iter_start; itr < ITER_NUM; itr++)
	{
		fetch_global_param(false);
		memset(center, 0, sizeof(float)* features*matK);
		memset(cluster_num, 0, sizeof(int)*matK);
		float thread_loss = 0;
		int pindex = 0;
		int base = 0;
		float loss = 0;
		for (int i = 0; i < thread_point_num; i++)
		{
			float sum = 1e30f;
			int selected_k=0;
			int means_base = 0;
			//calculate the distance from the data point to the centers
			for (int k = 0; k < matK; k++)
			{
				float tmp = 0;
				for (int j = 0; j < features; j++)
				{
					float tmp2 = thread_local_data[base + j] - means[means_base + j];
					tmp += tmp2*tmp2;
				}
				if (tmp <= sum)
				{
					sum = tmp;
					selected_k = k;
				}
				means_base += features;
			}
			loss += sum;
			cluster_num[selected_k] += 1;
			for (int j = 0; j < features; j++)
				center[selected_k*features + j] += thread_local_data[base + j];
			base += features;
		}
		*local_loss = loss;
		local_barrier->count_down_and_wait();
	}

}

void slave_main(uint32_t tid)
{
	//init global variables
	features = g_features;
	matK = g_K;
	local_barrier = new  LBarrier(THREAD_NUM + 1);
	float** centers = new float*[THREAD_NUM];
	barrier = g_barrier;

	std::cout << "Parameters :\n" << "num_features :" << features << "\nnum_points : " << g_num_points
		<< "\niter_num : " << ITER_NUM << "\nthread_num : " << THREAD_NUM
		<< "\nK : " << matK
		<< "\npath : " << PATH->getstr() << std::endl;

	//alloc the buffer
	means = new float[features*matK + matK];
	g_param->CopyTo(means, 0, features*matK);

	float* loss = new float[THREAD_NUM];
	int* num_point_in_cluster = new int[THREAD_NUM*matK];
	local_dataset.Load(features, g_num_points, DogeeEnv::self_node_id);
	int thread_point_num = local_dataset.local_dataset_size / THREAD_NUM;
	barrier->Enter();
	for (int i = 0; i < THREAD_NUM; i++)
	{
		float* thread_local_data;
		local_dataset.GetThreadData(i, thread_local_data);
		centers[i] = new float[features*matK];
		auto th = std::thread(slave_worker, thread_local_data, thread_point_num,
			centers[i], loss + i, num_point_in_cluster + i*matK, n_iter + 1);
		th.detach();
	}
	for (int itr = n_iter+1; itr < ITER_NUM; itr++)
	{
		fetch_global_param(true);
		//wait for the computation completion of slave nodes
		local_barrier->count_down_and_wait();
		memset(means, 0, sizeof(float)* (features*matK + matK));
		float total_loss = 0;
		for (int i = 0; i < THREAD_NUM; i++)
		{
			total_loss += loss[i];
			loss[i] = 0;
			for (int j = 0; j < features*matK; j++)
				means[j] += centers[i][j];
			for (int k = 0; k < matK; k++)
				means[features*matK + k] += num_point_in_cluster[i*matK + k];
		}
		std::cout << "Loss " << total_loss << std::endl;
		g_accu->AccumulateAndWait(means, features*matK + matK);
		n_iter = itr;
		barrier->Enter();
	}

	local_dataset.Free();
	for (int i = 0; i < THREAD_NUM; i++)
		delete[]centers[i];
	delete[]centers;
	delete[]means;
	delete local_barrier;
}


int main_process(bool normal);
class MasterCheckPoint :public CheckPoint
{
public:
	SerialDef(_iter, std::reference_wrapper<int>);
	MasterCheckPoint() :_iter(n_iter)
	{}
	void DoRestart()
	{
		main_process(false);
	}
};
SerialDecl(MasterCheckPoint, _iter, std::reference_wrapper<int>);

class SlaveCheckPoint :public CheckPoint
{
public:
	SerialDef(_iter, std::reference_wrapper<int>);
	SlaveCheckPoint() :_iter(n_iter)
	{}
};
SerialDecl(SlaveCheckPoint, _iter, std::reference_wrapper<int>);


void init_param()
{
	features = HelperGetParamInt("num_features");
	g_features = features;
	g_num_points = HelperGetParamInt("num_points");
	matK = HelperGetParamInt("K");
	g_K = matK;
	ITER_NUM = HelperGetParamInt("iter_num");
	THREAD_NUM = HelperGetParamInt("thread_num");
	PATH = NewObj<DString>(HelperGetParam("path"));
}

int main(int argc, char* argv[])
{
	HelperInitClusterCheckPoint<MasterCheckPoint, SlaveCheckPoint>(argc, argv, "kmeans");
	init_param();

	g_param = NewArray<float>(features*matK + matK);
	g_param->Fill([](uint32_t i){return (float)(rand() % 1000) / 100; }, 0, features*matK);
	g_param->Fill([](uint32_t i){return 1.0f; }, features*matK, matK);

	main_process(true);
}

int main_process(bool normal)
{
	init_param();
	g_accu = NewObj<DAddAccumulator<float>>(g_param,
		features*matK + matK,
		DogeeEnv::num_nodes - 1);
	g_barrier = barrier = NewObj<DCheckpointBarrier>(DogeeEnv::num_nodes);

	std::cout << "Parameters :\n" << "num_features :" << features << "\nnum_points : " << g_num_points
		<< "\niter_num : " << ITER_NUM << "\nthread_num : " << THREAD_NUM
		<< "\npath : " << PATH->getstr() << std::endl;


	for (int i = 0; i < DogeeEnv::num_nodes; i++)
	{
		NewObj<DThread>(THREAD_PROC(slave_main), i, i);
	}
	barrier->Enter();
	auto t = std::chrono::system_clock::now();
	for (int itr = n_iter+1; itr < ITER_NUM; itr++)
	{
		auto t2 = std::chrono::system_clock::now();
		n_iter = itr;
		barrier->Enter();
		std::cout << "Iter" << itr << " took " <<
			std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - t2).count()
			<< " milliseconds\nCount in clusters: ";
		if (normal && itr == 5)
		{
			//exit(255);
		}
	}
	std::cout << "Total time" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - t).count()
		<< std::endl;
	//std::string str;
	//std::cin >> str;
	for (int i = 0; i < 10; i++)
		cout << (float)g_param[features*matK+i] << " ";
	cout << endl;
	CloseCluster();
	return 0;
}

