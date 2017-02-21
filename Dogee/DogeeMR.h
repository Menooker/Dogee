#ifndef __DOGEE_MR_H_
#define __DOGEE_MR_H_

#include "DogeeBase.h"
#include "DogeeMacro.h"
#include <unordered_map>
#include <vector>
#include "DogeeThreading.h"


namespace Dogee
{

	template<class IN_VALUE, class OUT_VALUE>
	class Mapper : DObject
	{
		DefBegin(DObject);
		Def(out, Array<Array<OUT_VALUE>>);
		Def(counters, Array<int>);
		Def(sem, Ref<DSemaphore>);
	public:
		DefEnd();

		Mapper(ObjectKey key) :DObject(key)
		{}

		Mapper(ObjectKey key,int dummy) :DObject(key)
		{
			out = NewArray<Array<OUT_VALUE>>();
			counters = NewArray<int>();
			sem = NewObj(DSemaphore)(1);
		}

		void Write(uint32_t key, OUT_VALUE* v,uint32_t len)
		{
			sem->Acquire();
			Array<OUT_VALUE> arr = out[key];
			if (arr.GetObjectId() == 0)
			{
				
				arr = NewArray<OUT_VALUE>();
				out[key] = arr;
				counters[key] = 0;
			}
			int cnt = counters[key];
			counters[key] = cnt + len;
			arr.CopyFrom(v,cnt,len);
			sem->Release();
		}

		virtual void Map(uint32_t in_key, IN_VALUE in_value) = 0;

	};

	template<class IN_VALUE, class OUT_VALUE>
	class Reducer : DObject
	{
		DefBegin(DObject);
		Def(out, Array<Array<OUT_VALUE>>);
		Def(counters, Array<int>);
		Def(sem, Ref<DSemaphore>);
	public:
		DefEnd();

		Mapper(ObjectKey key) :DObject(key)
		{}

		Mapper(ObjectKey key, int dummy) :DObject(key)
		{
			out = NewArray<Array<OUT_VALUE>>();
			counters = NewArray<int>();
			sem = NewObj(DSemaphore)(1);
		}

		void Write(uint32_t key, OUT_VALUE* v, uint32_t len)
		{
			sem->Acquire();
			Array<OUT_VALUE> arr = out[key];
			if (arr.GetObjectId() == 0)
			{

				arr = NewArray<OUT_VALUE>();
				out[key] = arr;
				counters[key] = 0;
			}
			int cnt = counters[key];
			counters[key] = cnt + len;
			arr.CopyFrom(v, cnt, len);
			sem->Release();
		}

		virtual void Map(uint32_t in_key, IN_VALUE in_value) = 0;

	};

}

#endif