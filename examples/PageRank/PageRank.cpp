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
#undef max
#include <vector>

#define const_d 0.85f

DefConst(ITER_NUM, int); //300
DefConst(THREAD_NUM, int); 
DefGlobal(PATH, Ref<DString>)
DefConst(g_num_points, int);


DefConst(g_param, Array<float>);
DefGlobal(g_accu, Ref<DAddAccumulator<float>>);

DefGlobal(g_barrier, Ref<DBarrier>);


int features;
LBarrier* local_barrier;
int matK;
float* page_rank;
Ref<DBarrier> barrier(0);
static int base_index;
static int num_points_per_node;

class LocalDataset
{
public:
	struct DataPoint
	{
		uint32_t out_degree;
		int out_edges[0];
	};
	DataPoint** nodes_for_thread;
	char* dataset;

	int local_dataset_size;

	LocalDataset() :dataset(nullptr), local_dataset_size(0)
	{}
	static inline DataPoint* GetNextNode(DataPoint* curr)
	{
		return (DataPoint*)&curr->out_edges[curr->out_degree];
	}

	void GetThreadData(int tid, DataPoint* &odataset)
	{
		odataset = nodes_for_thread[tid];
		return;
	}

	void Load(const std::string path, int num_points, int node_id,int num_nodes)
	{
		const unsigned int local_line = num_points * (node_id - 1) / (num_nodes - 1);
		local_dataset_size = num_points / (num_nodes - 1);
		base_index = local_line;
		num_points_per_node = local_dataset_size;
		const int thread_data_size = local_dataset_size / THREAD_NUM;
		dataset = new char[local_dataset_size*(sizeof(uint32_t) + 100 * sizeof(DataPoint*))];
		nodes_for_thread = new DataPoint*[THREAD_NUM];
		printf("LL %d LS %d\n", local_line, local_dataset_size);
		std::ifstream f(path, std::ifstream::binary); //LiveJournal dataset may cause a problem in f.tellg() using text mode
		unsigned int id;
		bool hascache =  RestoreFilePointerCache(f, path, local_line);
		if (!hascache)
		{
			std::string str;
			while (f >> str && str[0] == '#') //ignore the comments in the head
				f.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
			id = atoi(str.c_str());
			std::streamoff f_pos;
			int data;
			while (id < local_line)//skip to the line we need
			{
				f >> data;
				f_pos = f.tellg();
				f >> id;
			}
			std::cout << "Write cache pos " <<f_pos<< endl;
			WriteFilePointerCache(f_pos, path, local_line);
		}
		else
		{
			std::cout << "Read cache pos " << f.tellg() << std::endl;
			f >> id;
		}
		DataPoint* cur = (DataPoint*)dataset;
		for (int i = 0; i < local_dataset_size; i++)
		{
			if (i%thread_data_size == 0)
				nodes_for_thread[i / thread_data_size] = cur;
			if (id>i + local_line)
			{
				cur->out_degree = 0;
				cur++;
				continue;
			}
			while (id < i + local_line)
			{
				int data;
				f >> data;
				f >> id;
			}
			for (cur->out_degree = 0; id == i + local_line; cur->out_degree++)
			{
				f >> cur->out_edges[cur->out_degree];
				if (cur->out_edges[cur->out_degree] >= num_points)
				{
					cur->out_degree--;
				}
				f >> id;
			}
			int curlen = cur->out_degree;
			cur = (DataPoint*)&cur->out_edges[cur->out_degree];
		}
	}
	void Free()
	{
		delete[]nodes_for_thread;
		delete[]dataset;
	}
}local_dataset;


void fetch_global_param(bool is_main)
{
	if (is_main)
	{
		g_param->CopyTo(page_rank, base_index, num_points_per_node);
	}
	local_barrier->count_down_and_wait();
}


void slave_worker(LocalDataset::DataPoint* thread_local_data, int thread_point_num,	float* local_grad)
{
	DogeeEnv::InitCurrentThread();
	for (int itr = 0; itr < ITER_NUM; itr++)
	{
		LocalDataset::DataPoint* cur = thread_local_data;
		fetch_global_param(false);
		memset(local_grad, 0, sizeof(float)* thread_point_num);
		for (int i = base_index; i < thread_point_num + base_index; i++)
		{
			uint32_t outdegree = cur->out_degree;
			if (outdegree != 0)
			{
				local_grad[i] += (1 - const_d) / outdegree;
				float pr = const_d * page_rank[i - base_index] / outdegree;
				for (unsigned int j = 0; j < outdegree; j++)
				{
					local_grad[cur->out_edges[j]] += pr;
				}
			}
			cur = LocalDataset::GetNextNode(cur);
		}
		local_barrier->count_down_and_wait();
	}

}

void slave_main(uint32_t tid)
{
	//init global variables
	local_barrier = new  LBarrier(THREAD_NUM + 1);
	float** local_grad_arr = new float*[THREAD_NUM];
	barrier = g_barrier;

	std::cout << "Parameters :\n"  << "\nnum_points : " << g_num_points
		<< "\niter_num : " << ITER_NUM << "\nthread_num : " << THREAD_NUM
		<< "\npath : " << PATH->getstr() << std::endl;

	//alloc the buffer

	local_dataset.Load(PATH->getstr(), g_num_points, DogeeEnv::self_node_id,DogeeEnv::num_nodes);
	page_rank = new float[num_points_per_node];
	for (int i = 0; i < num_points_per_node;i++)
		page_rank[i]= 0.001f;
	int thread_point_num = num_points_per_node / THREAD_NUM;
	barrier->Enter();
	for (int i = 0; i < THREAD_NUM; i++)
	{
		LocalDataset::DataPoint* thread_local_data;
		local_dataset.GetThreadData(i, thread_local_data);
		local_grad_arr[i] = new float[g_num_points];
		auto th = std::thread(slave_worker, thread_local_data, thread_point_num,
			local_grad_arr[i]);
		th.detach();
	}
	for (int itr = 0; itr < ITER_NUM; itr++)
	{
		fetch_global_param(true);
		//wait for the computation completion of slave nodes
		local_barrier->count_down_and_wait();
		for (int i = 1; i < THREAD_NUM; i++)
		{
			for (int j = 0; j < g_num_points; j++)
				local_grad_arr[0][j] += local_grad_arr[i][j];
		}
		g_accu->AccumulateAndWait(local_grad_arr[0], g_num_points, 0.0001f);
		barrier->Enter();
	}

	local_dataset.Free();
	for (int i = 0; i < THREAD_NUM; i++)
		delete[]local_grad_arr[i];
	delete[]local_grad_arr;
	delete local_barrier;
}


int main(int argc, char* argv[])
{
	HelperInitCluster(argc, argv);
	g_num_points = HelperGetParamInt("num_points");

	ITER_NUM = HelperGetParamInt("iter_num");
	THREAD_NUM = HelperGetParamInt("thread_num");
	PATH = NewObj<DString>(HelperGetParam("path"));

	std::cout << "Parameters :\n" <<  "\nnum_points : " << g_num_points
		<< "\niter_num : " << ITER_NUM << "\nthread_num : " << THREAD_NUM
		<< "\npath : " << PATH->getstr() << std::endl;

	g_param = NewArray<float>(g_num_points);
	g_accu = NewObj<DAddAccumulator<float>>(g_param,
		g_num_points,
		DogeeEnv::num_nodes - 1);
	g_barrier = barrier = NewObj<DBarrier>(DogeeEnv::num_nodes);

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
	for (int i = 0; i < 20; i++)
		cout << (float)g_param[i]<< endl;
	std::string str;
	std::cin >> str;
	CloseCluster();
	return 0;
}

