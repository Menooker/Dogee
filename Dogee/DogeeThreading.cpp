#include "DogeeThreading.h"


namespace Dogee
{
	int AutoRegisterThreadProcClass::id = 0;
	THREAD_LOCAL int thread_id;

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

	extern void RcDeleteThreadEvent(int id);
	void ThThreadEntry(int thread_id, int index, uint32_t param, ObjectKey okey)
	{
		DogeeEnv::InitStorageCurrentThread();
		Ref<DThread> ref(okey);
		ref->state = DThread::ThreadRunning;
		GetIDProcMap()[index](param);
		ref->state = DThread::ThreadStoped;
		RcDeleteThreadEvent(thread_id);
	}

}