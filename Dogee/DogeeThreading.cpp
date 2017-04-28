#include "DogeeThreading.h"
#include <atomic>

namespace Dogee
{
	int AutoRegisterThreadProcClass::id = 0;
	THREAD_LOCAL int current_thread_id=0;
	std::atomic<int> tid_count = {1};

	int AllocThreadId()
	{
		return tid_count++;
	}

	std::vector<thread_proc>& GetIDProcMap()
	{
		static std::vector<thread_proc> map;
		return map;
	}
	std::unordered_map<thread_proc, int>& GetProcIDMap()
	{
		static std::unordered_map<thread_proc, int> map;
		return map;
	}

	void ThRegisterThreadFunction(thread_proc func,int id)
	{
		GetIDProcMap().push_back(func);
		GetProcIDMap()[func] = id;
	}


	void ThThreadEntry(int thread_id, int index, uint32_t param, ObjectKey okey)
	{
		DogeeEnv::InitCurrentThread();
		Ref<DThread> ref(okey);
		ref->state = DThread::ThreadRunning;
		GetIDProcMap()[index](param);
		ref->state = DThread::ThreadStoped;
		DogeeEnv::DestroyCurrentThread();
	}

}