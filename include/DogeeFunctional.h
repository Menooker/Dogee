#ifndef __DOGEE_FUNCTIONAL_H_ 
#define __DOGEE_FUNCTIONAL_H_

#include "DogeeDThreadPool.h"
#include "DogeeEnv.h"

namespace Dogee
{
	template <typename T>
	class DData
	{

	};

	template <typename T>
	class DData<Array<T>>
	{
	private:
		unsigned len;
		Array<T> arr;
	public:

		DData(Array<T> _arr, unsigned _len) :arr(_arr), len(_len)
		{}

		template<class _Fty,typename T2> 
		DData<Array<T2>> Map(Array<T2> out_arr, unsigned out_len, _Fty _Fnarg)
		{
			static_assert(std::is_same<decltype(_Fnarg(*(T*)nullptr)),T2>::value, "Function should be \"T2 func(T1)\"!");
			unsigned avg = len / (DogeeEnv::num_nodes - 1);
			if (avg*DSMInterface<T>::dsm_size_of >= 32*DSM_CACHE_BLOCK_SIZE)
				avg = 32*DSM_CACHE_BLOCK_SIZE / DSMInterface<T>::dsm_size_of;
			std::vector<DThreadPool::DThreadPoolEvent> events;
			Array<T> the_arr = arr;
			for (unsigned i = 0; i < len; i += avg)
			{
				unsigned step = i + avg < len ? avg : len - i;
				auto lam = [the_arr, step, _Fnarg, out_arr](uint32_t start)
				{
					T buf[32 * DSM_CACHE_BLOCK_SIZE / DSMInterface<T>::dsm_size_of];
					T outbuf[32 * DSM_CACHE_BLOCK_SIZE / DSMInterface<T>::dsm_size_of];
					the_arr->CopyTo(buf, start, step);
					for (unsigned j = 0; j < step; j++)
					{
						outbuf[j]=_Fnarg(buf[j]);
					}
					out_arr->CopyFrom(outbuf, start, step);
				};
				events.push_back(DogeeEnv::ThreadPoolConfig::thread_pool->submit(lam, i));
				if (events.size() == DogeeEnv::ThreadPoolConfig::thread_pool_max_wait - 20)
				{
					for (auto e : events)
						e.Wait(-1);
					events.clear();
				}
			}
			for (auto e : events)
				e.Wait(-1);
			return  DData<Array<T2>>(out_arr, out_len);
		}
	};

	template<typename T> inline
	DData<Array<T>> GetDData(Array<T> arr,unsigned len)
	{
		return DData<Array<T>>(arr, len);
	}
}

#endif