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
DefGlobal(PATH,Ref<DString>)
DefConst(g_num_points, int);


DefGlobal(stepp, float); //0.005f
DefGlobal(stepq, float); //0.005f
DefGlobal(g_param, Array<float>);
DefGlobal(g_accu, Ref<DAddAccumulator<float>>);
DefGlobal(g_features, int);

DefGlobal(g_barrier, Ref<DBarrier>);
DefGlobal(g_K, int);

static int features;
LBarrier* local_barrier;
static int matK;
int shared_dsm_size;
static float* __RESTRICT matQ;
Ref<DBarrier> barrier(1);
static float alpha;
static float beta;

class LocalDataset
{
public:
	float* dataset;
	int local_dataset_size;
	int m_param_len;

	LocalDataset() :dataset(nullptr),local_dataset_size(0)
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
		dataset = new float[param_len*local_dataset_size];
		m_param_len = param_len;
		printf("LL %d LS %d\n", local_line, local_dataset_size);
		std::ifstream f(PATH->getstr());
		SkipFileToLine(f, PATH->getstr(), local_line);
		
		for (int i = 0; i < local_dataset_size*param_len; i++)
		{
			float data;
			f >> data;
			dataset[i] = data;
		}
	}
	void Free()
	{
		delete[]dataset;
	}
}local_dataset;



inline int upper_div(int a, int b)
{
	if (a%b == 0)
		return a / b;
	else
		return a / b + 1;
}

inline int align_to_block_size(int a)
{
	return DSM_CACHE_BLOCK_SIZE*upper_div(a , DSM_CACHE_BLOCK_SIZE);
}


void fetch_global_param(bool is_main, float* buffer)
{
	if (is_main)
	{
		g_param->CopyTo(buffer, 0, shared_dsm_size);
		printf("Accu grad=%f\n", buffer[20]);
		int sz = local_dataset.local_dataset_size * DogeeEnv::num_nodes;
		for (int i = 0; i < shared_dsm_size; i++)
		{
			matQ[i] += buffer[i] / sz;
			if (matQ[i] < 0)
				matQ[i] = 0;
		}

		local_barrier->count_down_and_wait();
	}
	else
	{
		local_barrier->count_down_and_wait();
	}
}


void slave_worker(float* thread_local_data,  int thread_point_num,
	float* __RESTRICT local_grad, float* local_loss, float* __RESTRICT myP)
{
	DogeeEnv::InitCurrentThread();
	for (int itr = 0; itr < ITER_NUM; itr++)
	{
		fetch_global_param(false, nullptr);
		memset(local_grad,0,sizeof(float)* features*matK);
		
		float thread_loss = 0;
		int pindex = 0;
		int base = 0;
		for (int i = 0; i < thread_point_num; i++)
		{
			int qindex = 0;
			float objv = 0.0f;
			float* __RESTRICT  mp = myP + pindex;
			for (int j = 0; j < features; j++)
			{
				float* __RESTRICT mq = matQ + qindex;
				float* __RESTRICT mg = local_grad + qindex;
				//Q[k][j]
				float dot = 0;
				for (int k = 0; k < matK; k++)
				{
					dot += mp[k] * mq[k];
				}
				dot = thread_local_data[base + j] - dot;
				objv += dot*dot;
				float ad = alpha*dot;
				float bd = beta*dot;
				for (int k = 0; k < matK; k++)
				{
					mp[k] += ad* mq[k];
					mg[k] += bd * mp[k];
				}
				qindex += matK;
			}

			thread_loss += objv;
			for (int k = 0; k < matK; k++)
			{
				if (myP[pindex + k] < 0)
				{
					myP[pindex + k] = 0;
				}
			}
			pindex += matK;
			base += features;
		}
		*local_loss = thread_loss;
		local_barrier->count_down_and_wait();
	}

}

void slave_main(uint32_t tid)
{
	//init global variables
	features = g_features;
	matK = g_K;
	local_barrier = new  LBarrier(THREAD_NUM + 1);
	float** local_grad_arr = new float*[THREAD_NUM];
	barrier = g_barrier;
	alpha = stepp;
	beta = stepq;

	std::cout << "Parameters :\n" << "num_features :" << features << "\nnum_points : " << g_num_points
		<< "\niter_num : " << ITER_NUM << "\nthread_num : " << THREAD_NUM
		<< "\nalpha : " << stepp << "\nbeta : " << stepq 
		<< "\npath : " << PATH->getstr() << std::endl;
	
	//alloc the buffer
	shared_dsm_size = align_to_block_size(features*matK);
	matQ = new float[shared_dsm_size];
	memset(matQ, 0, sizeof(float)* shared_dsm_size);
	
	float* local_buffer = new float[shared_dsm_size];
	float* loss = new float[THREAD_NUM];

	local_dataset.Load(features, g_num_points, DogeeEnv::self_node_id);

	float* matP = new float[local_dataset.local_dataset_size * matK];
	for(int i=0;i<local_dataset.local_dataset_size * matK;i++)
		matP[i]=(float)(rand() % 100) / 10000;
	int thread_point_num = local_dataset.local_dataset_size / THREAD_NUM;
	barrier->Enter();
	for (int i = 0; i < THREAD_NUM; i++)
	{
		float* thread_local_data;
		local_dataset.GetThreadData(i, thread_local_data);
		local_grad_arr[i] = new float[features*matK];
		auto th = std::thread(slave_worker, thread_local_data, thread_point_num,
			local_grad_arr[i], loss + i, matP + thread_point_num*i *matK);
		th.detach();
	}
	for (int itr = 0; itr < ITER_NUM; itr++)
	{
		fetch_global_param(true, local_buffer);
		//wait for the computation completion of slave nodes
		local_barrier->count_down_and_wait();
		for (int i = 1; i < THREAD_NUM; i++)
		{
			loss[0] += loss[i];
			loss[i] = 0;
			for (int j = 0; j < features*matK; j++)
				local_grad_arr[0][j] += local_grad_arr[i][j];
			//printf("grad %f\n",local_grad_arr[i][100]);
		}
		std::cout << "Loss " << loss[0] << std::endl;
		loss[0] = 0;
		g_accu->AccumulateAndWait(local_grad_arr[0], shared_dsm_size, 0.00001);
		barrier->Enter();
	}

	local_dataset.Free();
	for (int i = 0; i < THREAD_NUM; i++)
		delete[]local_grad_arr[i];
	delete[]local_grad_arr;
	delete[]matQ;
	delete[]matP;
	delete[]local_buffer;
	delete local_barrier;
}


int main(int argc, char* argv[])
{
	HelperInitCluster(argc, argv);
	features = HelperGetParamInt("num_features");
	g_features = features;
	g_num_points = HelperGetParamInt("num_points");
	matK = HelperGetParamInt("K");
	g_K = matK;

	///*
	ITER_NUM = HelperGetParamInt("iter_num");
	THREAD_NUM = HelperGetParamInt("thread_num");
	stepp = HelperGetParamDouble("alpha");
	stepq = HelperGetParamDouble("beta");
	PATH = NewObj<DString>(HelperGetParam("path"));
	//*/

	std::cout << "Parameters :\n" << "num_features :" << features << "\nnum_points : " << g_num_points
		<< "\niter_num : " << ITER_NUM << "\nthread_num : " << THREAD_NUM
		<< "\nalpha : " << stepp << "\nbeta : " << stepq 
		<< "\npath : " << PATH->getstr() << std::endl;

	shared_dsm_size = align_to_block_size(features*matK);
	g_param = NewArray<float>(shared_dsm_size);
	g_accu = NewObj<DAddAccumulator<float>>(g_param,
		shared_dsm_size,
		DogeeEnv::num_nodes - 1);
	g_barrier = barrier = NewObj<DBarrier>(DogeeEnv::num_nodes);
	
	g_param->Fill([](uint32_t i){return (float)(rand() % 100) / 10000; },
		0, features*matK);

	for (int i = 0; i < DogeeEnv::num_nodes; i++)
	{
		NewObj<DThread>(THREAD_PROC(slave_main), i, i);
	}
	barrier->Enter();
	auto t = std::chrono::system_clock::now();
	for (int itr = 0; itr < ITER_NUM; itr++)
	{
		auto t2 = std::chrono::system_clock::now();
		barrier->Enter();
		std::cout << "Iter"<<itr<<" took "<<
			std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - t2).count()
			<<" milliseconds\n";
	}
	std::cout << "Total time" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - t).count()
		<< std::endl;
	//std::string str;
	//std::cin >> str;
	CloseCluster();
	return 0;
}

