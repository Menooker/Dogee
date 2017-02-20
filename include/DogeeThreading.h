#ifndef __DOGEE_THREADING_H_ 
#define __DOGEE_THREADING_H_ 

#include "Dogee.h"
#include "DogeeBase.h"
#include "DogeeMacro.h"
#include "DogeeRemote.h"
#include <vector>
#include <unordered_map>

namespace Dogee
{
	extern THREAD_LOCAL int thread_id;
	typedef void(*thread_proc)(uint32_t);
	void ThRegisterThreadFunction(thread_proc func,int id);
	std::vector<thread_proc>& GetIDProcMap();
	std::unordered_map<thread_proc, int>& GetProcIDMap();

	class AutoRegisterThreadProcClass
	{
		static int id;
	public:
		AutoRegisterThreadProcClass(thread_proc func)
		{
			ThRegisterThreadFunction(func,id);
			id++;
		}
	};

	class DThread : public DObject
	{
		DefBegin(DObject);
	public:
		enum ThreadState
		{
			ThreadCreating = 1,
			ThreadRunning,
			ThreadStoped

		};
		Def(state, ThreadState);
		Def(node_id, int);
		DefEnd();
		DThread(ObjectKey obj_id) : DObject(obj_id)
		{
		}
		DThread(ObjectKey obj_id, thread_proc func, int nd_id, uint32_t param) : DObject(obj_id)
		{
			self->state = ThreadCreating;
			self->node_id = nd_id;
			RcCreateThread(node_id, GetProcIDMap()[func], param, self->GetObjectId());
		}

	};

	extern bool RcEnterBarrier(ObjectKey okey,int timeout=-1);
	class DBarrier : public DObject
	{
		DefBegin(DObject);
	public:
		Def(count, int);
		DefEnd();
		DBarrier(ObjectKey obj_id) : DObject(obj_id)
		{
		}
		DBarrier(ObjectKey obj_id, int count) : DObject(obj_id)
		{
			self->count = count;
		}
		
		bool Enter(int timeout=-1)
		{
			return RcEnterBarrier(self->GetObjectId(), timeout);
		}
	};

	extern bool RcEnterSemaphore(ObjectKey okey, int timeout=-1);
	extern void RcLeaveSemaphore(ObjectKey okey);
	class DSemaphore : public DObject
	{
		DefBegin(DObject);
	public:
		Def(count, int);
		DefEnd();
		DSemaphore(ObjectKey obj_id) : DObject(obj_id)
		{
		}
		DSemaphore(ObjectKey obj_id, int count) : DObject(obj_id)
		{
			self->count = count;
		}

		bool Acquire(int timeout=-1)
		{
			return RcEnterSemaphore(self->GetObjectId(), timeout);
		}
		void Release()
		{
			RcLeaveSemaphore(self->GetObjectId());
		}
	};

}

#endif