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
		template<typename T2, bool hasIndex>
		struct IndexingCaller
		{
			static void call(T2& obj, int k);
		};

		template<typename T2>
		struct IndexingCaller<T2, true>
		{
			static auto call(T2& obj, int k)->decltype(obj.operator[](0))
			{
				return obj.operator[](k);
			}
		};
		auto operator[](int k)->decltype(IndexingCaller<T, DogeeTrait<T>::has_index_operator>::call(val, k))
		{
			return IndexingCaller<T, DogeeTrait<T>::has_index_operator>::call(val, k);
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