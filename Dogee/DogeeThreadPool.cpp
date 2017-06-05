#include "DogeeDThreadPool.h"


#if !defined (_WIN32) && !defined (_WIN64)
#define LINUX
#include <unistd.h>
#else
#define WINDOWS
#include <windows.h>
#endif


namespace Dogee
{
	int DThreadPool::instance_count = 0;
	unsigned core_count()
	{
		unsigned count = 1; // 至少一个
#if defined (LINUX)
		count = sysconf(_SC_NPROCESSORS_CONF);
#elif defined (WINDOWS)
		SYSTEM_INFO si;
		GetSystemInfo(&si);
		count = si.dwNumberOfProcessors;
#endif
		// syc 
		return count;
	}

	Ref<DEvent>& DThreadPool::FindEvent()
	{
		std::reference_wrapper<Ref<DEvent>> eve = *(Ref<DEvent>*)nullptr;

		{
			std::unique_lock<std::mutex> lock(event_lock);
			if (event_queue.size() == 0)
			{
				if (events.size() < max_wait_events)
				{
					events.push_back(NewObj<DEvent>(true, false));
					eve = events.back();
					return eve;
				}
			}
		}
		sem_lock.wait();
		std::unique_lock<std::mutex> lock(event_lock);
		eve = event_queue.front();
		event_queue.pop();
		return eve;
	}

	LThreadPool* local_thread_pool = nullptr;

	void InitLocalThreadPool()
	{
		if (DogeeEnv::isMaster())
			return;
		if (DogeeEnv::ThreadPoolConfig::thread_pool_count > 0)
			local_thread_pool = new LThreadPool(DogeeEnv::ThreadPoolConfig::thread_pool_count);
		else if (DogeeEnv::ThreadPoolConfig::thread_pool_count < 0)
			local_thread_pool = new LThreadPool(core_count()*2+1);
		else
			local_thread_pool = nullptr;
	}
	void DeleteLocalThreadPool()
	{
		if (local_thread_pool)
			delete local_thread_pool;
	}

	void InitDThreadPool()
	{
		DThreadPoolScheduler* sche;
		if (DogeeEnv::ThreadPoolConfig::scheduler)
		{
			sche = DogeeEnv::ThreadPoolConfig::scheduler;
			DogeeEnv::ThreadPoolConfig::scheduler = nullptr;
		}
		else
			sche = new DDefaultTheadPoolScheduler();
		DogeeEnv::ThreadPoolConfig::thread_pool = new DThreadPool(sche, DogeeEnv::ThreadPoolConfig::thread_pool_max_wait);
	}

	void DeleteDThreadPool()
	{
		if(DogeeEnv::ThreadPoolConfig::thread_pool)
			delete DogeeEnv::ThreadPoolConfig::thread_pool;
	}


	void ExecuteClosureInLocalThreadPool(char* obj, uint32_t param, int id, uint32_t devent,bool need_free)
	{
		typedef void(*pCall)(char* ptr, uint32_t param);
		pCall funcall = (pCall)GetIDProcMap()[id];
		auto lam = [funcall, obj, param, devent, need_free](){
			funcall(obj, param);
			if (devent)
			{
				Ref<DEvent> ev(devent);
				ev->Set();
			}
			if (need_free)
				delete[]obj;
		};
		local_thread_pool->submit2(lam);
	}

	extern void AcExecuteClosureInThreadPool(int nodeid, uint32_t param, uint32_t event, int id, char* obj, size_t sz);
	void SendClosureToThreadPool(int nodeid,uint32_t param,uint32_t event,int id,char* obj,size_t sz)
	{
		if (nodeid == DogeeEnv::self_node_id)
			ExecuteClosureInLocalThreadPool(obj, param, id, event , false);
		else
			AcExecuteClosureInThreadPool(nodeid, param, event, id, obj, sz);
	}

}