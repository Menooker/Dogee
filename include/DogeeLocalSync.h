#ifndef __DOGEE_LOCAL_SYNC_H_
#define __DOGEE_LOCAL_SYNC_H_

#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>

namespace Dogee
{


	class Event
	{
		std::mutex mtx;
		std::condition_variable cv;
		bool ready = false;
	public:
		Event(bool state) {
			ready = state;
		}

		void SetEvent() {
			std::unique_lock <std::mutex> lck(mtx);
			ready = true;
			cv.notify_all();
		}

		void ResetEvent() {
			std::unique_lock <std::mutex> lck(mtx);
			ready = false;
		}

		void WaitForEvent() {
			std::unique_lock <std::mutex> lck(mtx);
			while (!ready)
				cv.wait(lck);
		}

		bool WaitForEvent(int timeout) {
			if (timeout<0)
			{
				WaitForEvent();
				return true;
			}
			std::unique_lock <std::mutex> lck(mtx);
			while (!ready)
			{
				if (cv.wait_for(lck, std::chrono::milliseconds(timeout)) == std::cv_status::timeout)
					return false;
			}
			return true;
		}
	};
	
	//https://www.daniweb.com/programming/software-development/threads/498822/c-11-thread-equivalent-of-pthread-barrier
	//The following two impl for barriers are modified from  mike_2000_17 's answer
	class LBarrier
	{
	public:
		LBarrier(const LBarrier&) = delete;
		LBarrier& operator=(const LBarrier&) = delete;
		explicit LBarrier(unsigned int count) :
			m_count(count), m_generation(0),
			m_count_reset_value(count)
		{
		}
		void count_down_and_wait()
		{
			std::unique_lock< std::mutex > lock(m_mutex);
			unsigned int gen = m_generation;
			if (--m_count == 0)
			{
				m_generation++;
				m_count = m_count_reset_value;
				m_cond.notify_all();
				return;
			}
			while (gen == m_generation)
				m_cond.wait(lock);
		}
	private:
		std::mutex m_mutex;
		std::condition_variable m_cond;
		unsigned int m_count;
		unsigned int m_generation;
		unsigned int m_count_reset_value;
	};

	class LSpinBarrier
	{
	public:
		LSpinBarrier(const LSpinBarrier&) = delete;
		LSpinBarrier& operator=(const LSpinBarrier&) = delete;
		explicit LSpinBarrier(unsigned int count) :
			m_count(count), m_generation(0),
			m_count_reset_value(count)
		{
		}
		void count_down_and_wait()
		{
			unsigned int gen = m_generation.load();
			if (--m_count == 0)
			{
				if (m_generation.compare_exchange_weak(gen, gen + 1))
				{
					m_count = m_count_reset_value;
				}
				return;
			}
			while ((gen == m_generation) && (m_count != 0))
				std::this_thread::yield();
		}
	private:
		std::atomic<unsigned int> m_count;
		std::atomic<unsigned int> m_generation;
		unsigned int m_count_reset_value;
	};

	//http://stackoverflow.com/a/19659736/4790873
	//Thanks for Stackoverflow user "Tsuneo Yoshioka"
	class LSemaphore {
	public:
		LSemaphore(int count_ = 0)
			: count(count_) {}

		inline void notify()
		{
			std::unique_lock<std::mutex> lock(mtx);
			count++;
			cv.notify_one();
		}

		inline void wait()
		{
			std::unique_lock<std::mutex> lock(mtx);

			while (count == 0){
				cv.wait(lock);
			}
			count--;
		}

	private:
		std::mutex mtx;
		std::condition_variable cv;
		int count;
	};
}

#endif