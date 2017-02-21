#ifndef __DOGEE_ACCUMULATOR_H_ 
#define __DOGEE_ACCUMULATOR_H_

#include "DogeeBase.h"
#include "DogeeMacro.h"

namespace Dogee
{
	template<typename T>
	class DAccumulator : public DObject
	{
		DefBegin(DObject);
	public:
		Def(arr,Array<T>);
		Def(len, uint32_t);
		Def(num_users, uint32_t);
		DefEnd();

		DAccumulator(ObjectKey k) :DObject(k)
		{}

		DAccumulator(ObjectKey k, Array<T> outarr,uint32_t outlen,uint32_t in_num_user) :DObject(k)
		{
			self->arr = outarr;
			self->len = outlen;
			self->num_users = in_num_user;
		}

		/*
		input a local array "buf" of length "len"(defined in member), dispatch it to each node of the
		cluster.
		*/
		void Accumulate(T* buf) ;
		
	};
}

#endif