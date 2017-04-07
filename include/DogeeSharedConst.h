#ifndef __DOGEE_CONST_H_ 
#define __DOGEE_CONST_H_ 

#include "DogeeBase.h"
#include "DogeeEnv.h"
namespace Dogee
{
	extern void RegisterConstInit(std::function<void()> func);
	template<typename T> class SharedConst
	{
	private:
		FieldKey fk;
		T val;
	public:

		LongKey get_address()
		{
			return (fk);
		}
		
		T get()
		{
			return val;
		}
		void set(T& x)
		{
			val = x;
			DSMInterface<T>::set_value(0, fk, x);
		}
		//read
		operator T() const
		{
			return val;
		}

		T operator->()
		{
			return val;
		}

		//write
		T operator=(T x)
		{
			set(x);
			return x;
		}

		void load()
		{
			val = DSMInterface<T>::get_value(0, fk);
		}

		SharedConst() :val(0)
		{
			fk = Dogee::RegisterGlobalVariable();
			RegisterConstInit(std::bind(&SharedConst::load, this));
		}
	};
}

#endif