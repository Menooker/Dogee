#include "DogeeAccumulator.h"
#include "DogeeThreading.h"
#include <iostream>

using std::cout;
using std::endl;

using namespace Dogee;

#define ACCU_TYPE long
#define ACCU_SIZE 10000
//#define ACCU_SPARSE 

void adder(ACCU_TYPE in, uint32_t index, ACCU_TYPE& out)
{
	out += in;
}

DefGlobal(accu, Ref<DFunctionalAccumulator<ACCU_TYPE, adder>>)

template<typename T>
T get_next_rand(bool reset, T seed)
{
	static T state;
	if (reset)
		state = seed;
	state = state * 3401 + 9;
#ifdef ACCU_SPARSE
	if (state % 2 == 0 || state % 3 ==0)
		return 0;
#endif
	return state;
}

template<typename T>
void PrepareLocalBuf(T seed,int size,T* out)
{
	get_next_rand(true, seed);
	for (int i = 0; i < size; i++)
	{
		out[i] = get_next_rand(false, seed);
	}
}

template<typename T>
void AddLocalBuf(T* src, int size, T* dest)
{
	for (int i = 0; i < size; i++)
	{
		dest[i] += src[i];
	}
}


template<typename T>
void threadproc(uint32_t param)
{
	//generate local buffer and accumulate
	T buf[ACCU_SIZE];
	PrepareLocalBuf((T)DogeeEnv::self_node_id, ACCU_SIZE,buf);
	accu->AccumulateAndWait(buf, ACCU_SIZE);

	//download the buffer
	T buf_remote[ACCU_SIZE];
	Array<T> arr = Array<T>(accu->arr);
	std::cout << "Array 0 = " << arr[0] << std::endl;
	arr->CopyTo(buf_remote, 0, ACCU_SIZE);
	
	//generate the local result in "buf"
	T buf2[ACCU_SIZE];
	for (int i = 0; i < DogeeEnv::num_nodes; i++)
	{
		if (i == DogeeEnv::self_node_id)
			continue;
		PrepareLocalBuf((T)i, ACCU_SIZE, buf2);
		AddLocalBuf(buf2, ACCU_SIZE, buf);
	}

	//compare the local result and the remote result
	for (int i = 0; i < ACCU_SIZE; i++)
	{
		if (buf[i] != buf_remote[i])
		{
			cout << "error idx=" << i << " " << buf[i] << " != " << buf_remote[i] << endl;
			break;
		}

	}
	cout << "Accu test ok" << endl;


}
RegFunc(threadproc<ACCU_TYPE>);

//template<typename T>
void accutest()
{
	get_next_rand(true, DogeeEnv::self_node_id);
	auto arr = NewArray<ACCU_TYPE>(ACCU_SIZE);
	accu = NewObj<DFunctionalAccumulator<ACCU_TYPE, adder>>(arr, ACCU_SIZE, 3);

	for (int i = 1; i < DogeeEnv::num_nodes;i++)
		NewObj<DThread>(threadproc<ACCU_TYPE>, i, 0);
	threadproc<ACCU_TYPE>(0);
}

