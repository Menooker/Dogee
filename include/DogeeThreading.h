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


	extern void RcSetEvent(ObjectKey okey);
	extern void RcResetEvent(ObjectKey okey);
	extern bool RcWaitForEvent(ObjectKey okey, int timeout);
	class DEvent : public DObject
	{
		DefBegin(DObject);
	public:
		Def(auto_reset, int);
		Def(is_signal, int);
		DefEnd();
		DEvent(ObjectKey obj_id) : DObject(obj_id)
		{
		}
		DEvent(ObjectKey obj_id, bool auto_reset, bool is_signal) : DObject(obj_id)
		{
			self->auto_reset = auto_reset;
			self->is_signal = is_signal;
		}

		/*
		Signal the event. If auto_reset is set, and multiple threads are waiting for 
		the event, only one of the thread in the queue will resume. If there is only
		one thread waiting, the thread will resume running and the event will be 
		automatically reset. If auto_reset is not set, all threads waiting for the 
		event will continue running and the event will stay signaled.
		*/
		void Set()
		{
			RcSetEvent(self->GetObjectId());
		}

		/*
		Reset the event. You only need to reset the event when auto_reset is set.
		*/
		void Reset()
		{
			RcResetEvent(self->GetObjectId());
		}
		
		/*
		Wait for the event.
		Param: timeout - the timeout for the waiting. -1 means infinity
		Returns: True if time is not out. False if timeout happens.
		*/
		bool Wait(int timeout = -1)
		{
			return RcWaitForEvent(self->GetObjectId(), timeout);
		}


	};


	class AutoRegisterThreadProcClass
	{
		static int id;
	public:
		static int Register(thread_proc func)
		{
			ThRegisterThreadFunction(func, id);
			return id++;
		}
		AutoRegisterThreadProcClass(thread_proc func)
		{
			Register(func);
		}
	};

	template<typename T>
	class AutoRegisterLambda
	{
	public:
		static const bool is_simple = std::is_convertible<T, thread_proc>::value;
		static const int id;
	private:
		template<typename T2,bool _is_simple>
		struct RegisterImp
		{//default case: if type T is a complex function object
			static void _Call(T2* ptr, uint32_t param)
			{
				ptr->operator()(param);
			}
			static int Reg()
			{
				return AutoRegisterThreadProcClass::Register((thread_proc)_Call);
			}
		};
		template<typename T2>
		struct RegisterImp<T2,true>
		{//if type T is a simple function object
			static int Reg()
			{
				T* a = nullptr;
				thread_proc ptr = *a;
				return AutoRegisterThreadProcClass::Register(ptr);
			}
		};
		
		static int Register()
		{
			static_assert(std::is_object<T>::value, "Must be a lambda");
			return RegisterImp < T, is_simple >::Reg();
		}

	};
	template <typename T>
	const int AutoRegisterLambda<T>::id = AutoRegisterLambda<T>::Register();

	template<thread_proc func>
	class ThreadProcFunctionWrapper
	{
	public:
		operator thread_proc()
		{
			return func;
		}
	};


	class DThread : public DEvent
	{
		DefBegin(DEvent);
	public:
		enum ThreadState
		{
			ThreadCreating = 1,
			ThreadRunning,
			ThreadStoped

		};
	private:
		Def(state, ThreadState);
	public:
		Def(node_id, int);
		DefEnd();
		DThread(ObjectKey obj_id) : DEvent(obj_id, false, false)
		{
		}
		friend void ThThreadEntry(int thread_id, int index, uint32_t param, ObjectKey okey);
		friend void ThThreadEntryObject(int thread_id, int index, uint32_t param, ObjectKey okey, char* data);
		ThreadState GetState()
		{
			return self->state;
		}

		/*
		Wait for the completion of the thread.
		Params : timeout  - the timeout for the waiting. -1 means infinity
		Returns: True if time is not out. False if timeout happens.
		*/
		bool Join(int timeout = -1)
		{
			return self->Wait(timeout);
		}
		/*only if you have registered the function with RegFunc can you use this
		constructor to create a thread. Because you must make sure the function
		has been registered*/
		DThread(ObjectKey obj_id, thread_proc func, int nd_id, uint32_t param) : DEvent(obj_id, false, false)
		{
			self->state = ThreadCreating;
			self->node_id = nd_id;
			RcCreateThread(nd_id, GetProcIDMap()[func], param, self->GetObjectId());
		}

		/*this constructor accepts lambda and other callable objects.
		Callable objects that can be converted to thread_proc is called in a simple way.
		Make sure the class T is unique for different functions (one class for each function).
		To pass a function name, you should wrap it with a THREAD_PROC wrapper.
		params: 
		- func: the function/function object to be executed
		- nd_id: the id of a slave node to create a thread on.
		- param: the parameter to pass to the function
		*/
		template<typename T>
		DThread(ObjectKey obj_id, T func, int nd_id, uint32_t param) : DEvent(obj_id, false, false)
		{
			self->state = ThreadCreating;
			self->node_id = nd_id;
			if (AutoRegisterLambda<T>::is_simple) //just too lazy to use a template to do this
				RcCreateThread(node_id, AutoRegisterLambda<T>::id, param, self->GetObjectId());
			else
				RcCreateThread(node_id, AutoRegisterLambda<T>::id, param, self->GetObjectId(), &func, sizeof(func));
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
#define THREAD_PROC(...) ThreadProcFunctionWrapper<__VA_ARGS__>()
#endif