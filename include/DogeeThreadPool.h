#ifndef __DOGEE_THREAD_POOL_H_ 
#define __DOGEE_THREAD_POOL_H_ 
#include <queue>
#include <functional>
#include "DogeeLocalSync.h"
#include "DogeeAPIWrapping.h"
#include <thread>
#include <future>
namespace Dogee
{
	class LThreadPool
	{
	private:
		BD_LOCK lock;
		std::queue<std::function<void(void)>> q;
		LSemaphore sem;
		std::vector<std::thread> threads;
		int numthreads;
		bool stop = false;
		void enqueue(std::function<void(void)>&& func)
		{
			UaEnterLock(&lock);
			q.push(std::move(func));
			UaLeaveLock(&lock);
			sem.notify();
		}
		void dequeue()
		{
			for (;;)
			{
				sem.wait();
				if (stop)
					return;
				UaEnterLock(&lock);
				auto ret = std::move(q.front());
				q.pop();
				UaLeaveLock(&lock);
				ret();
			}
			
		}
	public:
		LThreadPool(int num_threads) :numthreads(num_threads), sem(0)
		{
			UaInitLock(&lock);
			auto bd = std::bind(&LThreadPool::dequeue, this);
			for (int i = 0; i < num_threads; i++)
				threads.push_back(std::move(std::thread(bd)));
		}

		template<class _Fty,
		class... _ArgTypes> inline
			std::future<typename std::result_of<_Fty(_ArgTypes...)>::type>
			submit(_Fty&& _Fnarg, _ArgTypes&&... _Args)
		{
			typedef typename std::result_of<_Fty(_ArgTypes...)>::type _Ret;
			using ptr = std::shared_ptr<std::promise<_Ret>>;
			ptr p(new std::promise<_Ret>);
			auto lam = [p]( _Fty&& _Fnarg, _ArgTypes... _Args){
				p->set_value(_Fnarg(std::forward<_ArgTypes>(_Args)...));
			};
			
			auto func = std::bind(lam, std::move(_Fnarg),
				std::forward<_ArgTypes>(_Args)...);
			enqueue(func);
			return  p->get_future();
		}
		template<class _Fty,
		class... _ArgTypes> inline
			void
			submit2(_Fty&& _Fnarg, _ArgTypes&&... _Args)
		{
			auto lam = [](_Fty&& _Fnarg, _ArgTypes... _Args){
				_Fnarg(std::forward<_ArgTypes>(_Args)...);
			};
			auto func = std::bind(lam, std::move(_Fnarg),
				std::forward<_ArgTypes>(_Args)...);
			enqueue(func);
		}
		~LThreadPool()
		{
			stop = true;
			for (int i = 0; i < numthreads; i++)
				sem.notify();
			for (auto& th : threads)
				th.join();
			UaKillLock(&lock);
		}
	};
}

#endif