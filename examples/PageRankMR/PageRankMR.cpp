#ifdef _WIN32
#include "stdafx.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <iostream>

#include "DogeeBase.h"
#include "DogeeMacro.h"
#include "DogeeRemote.h"
#include "DogeeThreading.h"
#include "DogeeHelper.h"
#include "DogeeMR.h"
#include <memory>
using namespace Dogee;


std::unordered_map<int,std::vector<int>> data= {
	{ 0, {0} },
	{ 1, { 1, 2, 3 } },
	{ 2, { 1, 4, 5 } },
	{ 3, { 1 } },
	{ 4, { 2, 5 } },
	{ 5, { 2 } }
};

#define NUM_NODES (sizeof(data)/sizeof(std::vector<int>))
#define MAX_ITER 100

DefGlobal(pagerank, Array<float>);
DefGlobal(accu,Ref<DAddAccumulator<float>>);
DefGlobal(barr, Ref<DBarrier>);
float local_contri[NUM_NODES];

void Map(int& key, std::vector<int>& value, std::unordered_map<int, std::vector<float>>& outmap)
{
	float contribution = pagerank[key]/value.size();
	for (auto out_node : value)
	{
		outmap[out_node].push_back(contribution);
	}
}

void Reduce(const int& key, const std::vector<float>& value)
{
	float sum = 0;
	for (auto v : value)
	{
		sum += v;
	}
	local_contri[key] = sum;
}

void GlobalReduce()
{
	accu->AccumulateAndWait(local_contri, NUM_NODES);
	barr->Enter();
}

int main(int argc, char* argv[])
{
	HelperInitCluster(argc, argv);
	accu = NewObj<DAddAccumulator<float>>(pagerank, NUM_NODES,1);
	barr = NewObj<DBarrier>(DogeeEnv::num_nodes);
	typedef DFunctionMapReduce<int, std::vector<int>, int, float, Map, Reduce,GlobalReduce> PageRankSolver;
	Ref<PageRankSolver> solver = NewObj<PageRankSolver>(1);
	for (int i = 0; i < MAX_ITER; i++)
	{
		solver->Map(data);
		solver->Reduce();
		barr->Enter();
	}
	CloseCluster();
	return 0;
}