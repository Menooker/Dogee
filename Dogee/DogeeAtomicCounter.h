#ifndef __DOGEE_ATOMIC_COUNTER_H_
#define __DOGEE_ATOMIC_COUNTER_H_
#include "DogeeBase.h"
#include "DogeeEnv.h"

namespace Dogee
{
	class DAtomicCounter : public DObject
	{
		uint64_t Get()
		{
			DogeeEnv::backend->getcounter(this->GetObjectId(), 0);
		}

		void Set(uint64_t n)
		{
			DogeeEnv::backend->setcounter(this->GetObjectId(), 0,n);
		}

		uint64_t Inc(uint64_t n)
		{
			DogeeEnv::backend->inc(this->GetObjectId(), 0, n);
		}

		uint64_t Dec(uint64_t n)
		{
			DogeeEnv::backend->dec(this->GetObjectId(), 0, n);
		}

		DAtomicCounter(ObjectKey key) :DObject(key)
		{
		}
		DAtomicCounter(ObjectKey key, uint64_t n) :DObject(key)
		{
			DogeeEnv::backend->setcounter(this->GetObjectId(), 0, n);
		}

	};
}


#endif