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
DefConst(step_size, float); //0.005f
DefConst(TEST_PART, float); //0.2f


class LocalDataset
{
public:
	float* dataset;
	float* labelset;
	float* testset;
	float* testlabel;
	int local_dataset_size;
	int local_train_size;
	int local_testset_size;
	int m_param_len;

	LocalDataset() :dataset(nullptr), labelset(nullptr),testset(nullptr), testlabel(nullptr), local_dataset_size(0)
	{}

	void GetThreadData(int tid, float* &odataset, float* &olabelset, float* &otestset, float* &otestlabel)
	{
		int toff = tid  * local_train_size / THREAD_NUM;
		int testoff = tid  * local_testset_size / THREAD_NUM;
		odataset = dataset + m_param_len * toff;
		olabelset = labelset + toff;
		otestset = dataset + (local_train_size + testoff)*m_param_len;
		otestlabel = labelset + local_train_size + testoff;
		return ;
	}

	void Load(int param_len, int num_points,int node_id)
	{
		const int local_line = num_points * (node_id-1) / (DogeeEnv::num_nodes-1);
		local_dataset_size =  num_points / (DogeeEnv::num_nodes-1);
		local_testset_size = local_dataset_size * TEST_PART;
		local_train_size = local_dataset_size - local_testset_size;
		labelset = new float[local_dataset_size];
		dataset = new float[param_len*local_dataset_size];
		testlabel = labelset + local_train_size;
		testset = dataset + param_len* local_train_size;
		m_param_len = param_len;
		printf("LL %d LS %d\n", local_line, local_dataset_size);
		int real_cnt = 0;
		int postive = 0;
		int datapoints = 0;
		ParseCSV("d:\\\\LR\\gene2.csv", [&](const char* cell, int line, int index){
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
				{
					labelset[line - local_line] = atof(cell);
					datapoints++;
					if (cell[0] == '1')
						postive++;
				}
				else
					dataset[(line - local_line)*param_len + index] = atof(cell);
				real_cnt++;
			}

			return true;
		}, local_line);
		std::cout << "Loaded cells " << real_cnt << " num_data_points= " << real_cnt / param_len << " " << datapoints << " Positive= " << postive << std::endl;
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



float* local_param;
int param_len;
LBarrier* local_barrier;


void fetch_global_param(bool is_main,float* buffer)
{
	if (is_main)
	{
		g_param->CopyTo(buffer, 0, param_len);
		printf("Accu grad=%f\n", buffer[20000]);
		for (int i = 0; i < param_len; i++)
		{
			local_param[i] += buffer[i] / local_dataset.local_dataset_size/DogeeEnv::num_nodes;
		}

		local_barrier->count_down_and_wait();
	}
	else
	{
		local_barrier->count_down_and_wait();
	}
}


void slave_worker(float* thread_local_data, float* thread_local_label, int thread_point_num, 
	float* thread_test_data, float* thread_test_label, int thread_test_num,
	float* local_grad, float* local_loss)
{
	DogeeEnv::InitCurrentThread();
	for (int itr = 0; itr < ITER_NUM; itr++)
	{
		float thread_loss = 0;
		fetch_global_param(false,nullptr);
		float* curdata = thread_local_data;
		memset(local_grad, 0, sizeof(float)*param_len);
		for (int i = 0; i < thread_point_num; i++)
		{
			double dot = 0;
			for (int j = 0; j < param_len; j++)
				dot += local_param[j] * curdata[j];
			double h = 1 / (1 + exp(-dot));
			double delta = thread_local_label[i] - h;
			thread_loss += delta*delta;
			for (int j = 0; j < param_len; j++)
				local_grad[j] += step_size * delta * curdata[j];
			curdata += param_len;
			//if (i % 50 == 0)
			//	printf("i=%d curdata=%p h=%f dot=%f delta=%f\n", i, curdata, h, dot, delta);
		}
		//printf("TGrad %p %f %f\n", local_grad, local_grad[20000], curdata[20000]);
		*local_loss = thread_loss;
		//slave_main will accumulate the local_grad and fetch the new parameters
		local_barrier->count_down_and_wait();
	}
	fetch_global_param(false, nullptr);
	int positive = 0;
	int real_true = 0;
	int TP = 0;
	float* cur_test_data = thread_test_data;
	for (int i = 0; i < thread_test_num; i++)
	{
		double dot = 0;
		for (int j = 0; j < param_len; j++)
			dot += local_param[j] * cur_test_data[j];
		double h = 1 / (1 + exp(-dot));
		if (abs(thread_test_label[i] - 1)<0.001)
			real_true++;
		if (h >= 0.8f)
		{
			positive++;
			if (abs(thread_test_label[i] - 1)<0.001)
			{
				TP++;
			}
		}
		cur_test_data += param_len;
	}
	local_grad[0] = positive; local_grad[1] = real_true; local_grad[2] = TP;
	local_barrier->count_down_and_wait();
}

void slave_main(uint32_t tid)
{
	param_len = g_param_len;
	local_barrier = new  LBarrier(THREAD_NUM + 1);

	float** local_grad_arr = new float*[THREAD_NUM];
	barrier = g_barrier;
	local_param = new float[param_len];
	memset(local_param, 0, sizeof(float)*param_len);
	float* local_buffer = new float[param_len];
	float* loss = new float[THREAD_NUM];
	local_dataset.Load(param_len, g_num_points, DogeeEnv::self_node_id);
	
	int thread_point_num = local_dataset.local_train_size / THREAD_NUM;
	int thread_test_num = local_dataset.local_testset_size / THREAD_NUM;
	for (int i = 0; i < THREAD_NUM; i++)
	{
		float* thread_local_data;
		float* thread_local_label;
		float* thread_test_data;
		float* thread_test_label;
		local_dataset.GetThreadData(i, thread_local_data, thread_local_label, thread_test_data, thread_test_label);
		local_grad_arr[i]=new float[param_len];
		auto th = std::thread(slave_worker, thread_local_data, thread_local_label, thread_point_num,
			thread_test_data, thread_test_label, thread_test_num,
			local_grad_arr[i], loss + i);
		th.detach();
	}
	for (int itr = 0; itr < ITER_NUM; itr++)
	{
		fetch_global_param(true,local_buffer);
		//wait for the computation completion of slave nodes
		local_barrier->count_down_and_wait();
		for (int i = 1; i < THREAD_NUM; i++)
		{
			loss[0] += loss[i];
			loss[i] = 0;
			for (int j = 0; j < param_len; j++)
				local_grad_arr[0][j] = local_grad_arr[i][j];
		}
		std::cout << "Loss " << loss[0] << std::endl;
		loss[0] = 0;
		g_accu->AccumulateAndWait(local_grad_arr[0], param_len, 0.0001);
		barrier->Enter();		
	}

	fetch_global_param(true, local_buffer);
	//wait for the computation completion of slave nodes
	local_barrier->count_down_and_wait();
	for (int i = 1; i < THREAD_NUM; i++)
	{
		local_grad_arr[0][0] = local_grad_arr[i][0];
		local_grad_arr[0][1] = local_grad_arr[i][1];
		local_grad_arr[0][2] = local_grad_arr[i][2];
	}
	g_accu->AccumulateAndWait(local_grad_arr[0], param_len);
	barrier->Enter();

	local_dataset.Free();
	for (int i = 0; i < THREAD_NUM; i++)
		delete []local_grad_arr[i];
	delete[]local_grad_arr;
	delete[]local_param;
	delete[]local_buffer;
	delete local_barrier;
}

RegFunc(slave_main);


int main(int argc, char* argv[])
{
	HelperInitCluster(argc, argv);
	int param_len = HelperGetParamInt("num_param");
	g_num_points = HelperGetParamInt("num_points");

	///*
	ITER_NUM = HelperGetParamInt("iter_num"); 
	THREAD_NUM = HelperGetParamInt("thread_num");
	step_size = HelperGetParamDouble( "step_size"); 
	TEST_PART = HelperGetParamDouble("test_partition"); 
	//*/

	std::cout << "Parameters:\n" << "num_param :" << param_len << "\nnum_points : " << g_num_points
		<< "\niter_num : " << ITER_NUM << "\nthread_num : " << THREAD_NUM
		<< "\nstep_size : " << step_size << "\ntest_partition : " << TEST_PART;

	g_param = NewArray<float>(param_len);
	g_accu = NewObj<DAddAccumulator<float>>(g_param,
		param_len,
		DogeeEnv::num_nodes-1);
	g_barrier= barrier = NewObj<DBarrier>(DogeeEnv::num_nodes);
	g_param_len = param_len;
	g_param->Fill([](uint32_t i){return (float) (rand()%100) / 100000; },
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
	std::cout << "Learning done. Waiting for testing..." << std::endl;
	barrier->Enter();
	float positive = g_param[0], real_true = g_param[1], TP = g_param[2];
	std::cout << "positive = " << positive << " real_true = " << real_true << " TP = " << TP << std::endl;
	std::cout << "precision = " << TP / (positive + 0.01) << " recall = " << TP / (real_true+0.01) << std::endl;
	std::string str;
	std::cin >> str;
	CloseCluster();
	return 0;
}

