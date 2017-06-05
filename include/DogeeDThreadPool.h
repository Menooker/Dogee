#ifndef __DOGEE_DTRHEAD_POOL_H_ 
#define __DOGEE_DTRHEAD_POOL_H_
#include "DogeeThreading.h"
#include "DogeeThreadPool.h"
#include "DogeeLocalSync.h"
#include <vector>
#include <mutex>
#include <queue>
namespace Dogee
{
	class DThreadPoolScheduler
	{
	public:
		virtual int schedule()=0;
		virtual void donejob()=0;
	};

	class DDefaultTheadPoolScheduler :public DThreadPoolScheduler
	{
		std::atomic<unsigned long long> cnt = { 0 };
	public:
		int schedule()
		{
			int node = (++cnt) % (DogeeEnv::num_nodes-1); //master node has no thread pool.
			return node+1;
		}

		void donejob()
		{}
	};
	
	extern void SendClosureToThreadPool(int nodeid, uint32_t param, uint32_t event, int id, char* obj, size_t sz);
	extern void InitLocalThreadPool();
	extern void DeleteLocalThreadPool();

	class DThreadPool
	{
		DThreadPoolScheduler* scheduler;
		unsigned max_wait_events;
		std::vector<Ref<DEvent>> events;
		std::queue<Ref<DEvent>> event_queue;
		std::mutex event_lock;
		LSemaphore sem_lock;
		Ref<DEvent>& FindEvent();
		static int instance_count;
	public:
		DThreadPool(DThreadPoolScheduler* _scheduler, int _max_wait_events = 5) :max_wait_events(_max_wait_events), sem_lock(0), scheduler(_scheduler)
		{
			if (instance_count == 0)
				InitLocalThreadPool();
			instance_count++;
		}
		~DThreadPool()
		{
			for (auto e : events)
			{
				DelObj(e);
			}
			delete scheduler;
			instance_count--;
			if (instance_count == 0)
				DeleteLocalThreadPool();
		}
		class DThreadPoolEvent
		{
		private:
			DThreadPool* ths;
			Ref<DEvent> _event;
		public:
			DThreadPoolEvent(Ref<DEvent>& ev, DThreadPool* that) :_event(ev), ths(that)
			{}
			bool Wait(int timeout = -1)
			{
				bool ret=_event->Wait(timeout);
				ths->event_lock.lock();
				ths->event_queue.push(_event);
				ths->event_lock.unlock();
				ths->sem_lock.notify();
				ths->scheduler->donejob();
				return ret;
			}
		};

		

		template<class _Fty> inline
		DThreadPoolEvent	submit(_Fty _Fnarg,uint32_t param)
		{
			int node=scheduler->schedule();
			Ref<DEvent>& eve = FindEvent();
			SendClosureToThreadPool(node, param, eve->GetObjectId(), AutoRegisterLambda<_Fty>::id,(char*) &_Fnarg, sizeof(_Fty));
			return DThreadPoolEvent(eve, this);
		}
		template<class _Fty> inline
			void submit2(_Fty _Fnarg, uint32_t param)
		{
			int node = scheduler->schedule();
			SendClosureToThreadPool(node, param,0, AutoRegisterLambda<_Fty>::id, (char*)&_Fnarg, sizeof(_Fty));
			return ;
		}
	};


}


#endif