#ifdef _WIN32
#include "stdafx.h"
#endif

#include "DogeeBase.h"
#include "DogeeMacro.h"
#include "DogeeRemote.h"
#include <iostream>
#include "DogeeHelper.h"
#include "DogeeAccumulator.h"
#include "DogeeThreading.h"
#include "DogeeLocalSync.h"
#include <stdlib.h>
#include <fstream>

using namespace Dogee;
using std::cin;
using std::cout;
using std::endl;
 
#include <vector>

class LocalDataset
{
public:
	float* dataset;
	float* labelset;
	int local_dataset_size;

	LocalDataset() :dataset(nullptr), labelset(nullptr), local_dataset_size(0)
	{}

	void GetThreadData(int tid, float* &odataset, float* &olabelset)
	{
		odataset = dataset + tid;
		olabelset = labelset + tid;
		return ;
	}

	void Load(int param_len, int num_points,int node_id)
	{
		const int local_line = num_points * (node_id-1) / (DogeeEnv::num_nodes-1);
		local_dataset_size =  num_points / (DogeeEnv::num_nodes-1);

		labelset = new float[local_dataset_size];
		dataset = new float[param_len*local_dataset_size];
		printf("LL %d LS %d\n", local_line, local_dataset_size);
		int real_cnt = 0;
		ParseCSV("d:\\\\LR\\gene.csv", [&](const char* cell, int line, int index){
			if (index > param_len)
			{
				std::cout << "CSV out of index, line="<< line<<" index="<<index<<"\n";
				return false;
			}
			if (line >= local_line + local_dataset_size)
				return false;
			if (line >= local_line )
			{
				if (index == param_len)
					labelset[line - local_line] = atof(cell);
				else
					dataset[(line - local_line)*num_points + index] = atof(cell);
				real_cnt++;
			}

			return true;
		});
		std::cout << "Loaded cells " << real_cnt << " num_data_points= " << real_cnt/param_len << std::endl;
	}
	void Free()
	{
		delete[]dataset;
		delete[]labelset;
	}
}local_dataset;

DefGlobal(g_param, Array<float>);
DefGlobal(g_accu, Ref<DAddAccumulator<float>>);
DefGlobal(g_param_len, int);
DefGlobal(g_num_points, int);
DefGlobal(g_barrier, Ref<DBarrier>);

Ref<DBarrier> barrier(0);

#define ITER_NUM 3
#define THREAD_NUM 4
#define step_size 0.01f

float* local_param;
int param_len;
LBarrier local_barrier(THREAD_NUM + 1);


void fetch_global_param(bool is_main)
{
	if (is_main)
	{
		g_param->CopyTo(local_param, 0, param_len);
		local_barrier.count_down_and_wait();
	}
	else
	{
		local_barrier.count_down_and_wait();
	}
}


void slave_worker(float* thread_local_data, float* thread_local_label, int thread_point_num, float* local_grad)
{
	DogeeEnv::InitCurrentThread();
	memset(local_grad, 0, sizeof(local_grad));
	for (int itr = 0; itr < ITER_NUM; itr++)
	{
		fetch_global_param(false);
		for (int i = 0; i < thread_point_num; i++)
		{
			float dot = 0;
			for (int j = 0; j < param_len; j++)
				dot += local_param[j] * thread_local_data[j];
			float h = 1 / (1 + exp(-dot));
			float delta = thread_local_label[i] - h;
			for (int j = 0; j < param_len; j++)
			{
				local_grad[j] += step_size * delta * thread_local_data[j];
			}
		}
		//slave_main will accumulate the local_grad and fetch the new parameters
		local_barrier.count_down_and_wait();
	}
}

void slave_main(uint32_t tid)
{
	param_len = g_param_len;
	float** local_grad_arr = new float*[THREAD_NUM];
	barrier = g_barrier;
	local_param = new float[param_len];
	local_dataset.Load(param_len, g_num_points, DogeeEnv::self_node_id);
	
	int thread_point_num = local_dataset.local_dataset_size / THREAD_NUM;
	for (int i = 0; i < THREAD_NUM; i++)
	{
		float* thread_local_data;
		float* thread_local_label;
		local_dataset.GetThreadData(tid, thread_local_data, thread_local_label);
		local_grad_arr[i]=new float[param_len];
		auto th = std::thread(slave_worker, thread_local_data, thread_local_label, thread_point_num, local_grad_arr[i]);
		th.detach();
	}
	for (int itr = 0; itr < ITER_NUM; itr++)
	{
		fetch_global_param(true);
		//wait for the computation completion of slave nodes
		local_barrier.count_down_and_wait();
		for (int i = 1; i < THREAD_NUM; i++)
		{
			for (int j = 0; j < param_len; j++)
				local_grad_arr[0][j] = local_grad_arr[i][j];
		}
		g_accu->AccumulateAndWait(local_grad_arr[0], param_len, 0.01);
		barrier->Enter();
	}
	local_dataset.Free();
	for (int i = 0; i < THREAD_NUM; i++)
		delete []local_grad_arr[i];
	delete[]local_grad_arr;
	delete[]local_param;
}

RegFunc(slave_main);


int main(int argc, char* argv[])
{
	HelperInitCluster(argc, argv);
	int param_len = HelperGetParamInt("num_param");
	g_num_points = HelperGetParamInt("num_points");

	
	g_param = NewArray<float>();
	g_accu = NewObj<DAddAccumulator<float>>(g_param,
		param_len,
		DogeeEnv::num_nodes-1);
	g_barrier= barrier = NewObj<DBarrier>(DogeeEnv::num_nodes);
	g_param_len = param_len;
	g_param->Fill([](uint32_t i){return Dogee::bit_cast<float>(rand()); },
		0, param_len);
	for (int i = 0; i < DogeeEnv::num_nodes; i++)
	{
		NewObj<DThread>(slave_main, i, i);
	}
	for (int itr = 0; itr < ITER_NUM; itr++)
	{
		auto t=std::chrono::system_clock::now();
		barrier->Enter();
		std::cout << "Iter"<<itr<<" took "<<
			std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - t).count()
			<<" milliseconds\n";
	}
	std::string str;
	std::cin >> str;
	CloseCluster();
	return 0;
}

