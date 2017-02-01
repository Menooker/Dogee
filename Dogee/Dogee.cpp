// Dogee.cpp : 定义控制台应用程序的入口点。
//
#ifdef _WIN32
#include "stdafx.h"
#endif

#include <stdio.h>
#include <iostream>

#include <stdint.h>
namespace Dogee
{
	//test code
	
	uint64_t data[4096*32];

	template <class Dest, class Source>
	inline Dest bit_cast(const Source& source) {
		static_assert(sizeof(Dest) <= sizeof(Source),"Error: sizeof(Dest) > sizeof(Source)");
		Dest dest;
		memcpy(&dest, &source, sizeof(dest));
		return dest;
	}

	typedef uint32_t ShortKey;
	typedef uint64_t LongKey;
	class DObject
	{
	private:
		LongKey last_field_id;	
	public:
		//test code
		LongKey RegisterField()
		{
			last_field_id+=8;
			return last_field_id;
		}
		DObject(ShortKey obj_id)
		{
			//last_field_id = ((LongKey)obj_id)<<32;
			last_field_id = (LongKey)(data + obj_id * 10);
		}
	};
	DObject* lastobject;

	template<typename T,int FieldId> class Value
	{
	private:
		LongKey addr;
	public:

		T get()
		{
			return *(T*)addr;
		}

		void set(T val)
		{
			*(T*)addr = val;
		}

		//read
		operator T() 
		{
			return get();
		}

		//copy
		Value<T>& operator=(Value<T>& x)
		{
			this->set(x.get());
			return *this;
		}

		//write
		Value<T>& operator=(T x)
		{
			set(x);
			return *this;
		}
		//operator const   T() { return this->b_; }
		//operator T&() { return this->b_; }
/*		Value(T x)
		{
			this->operator=(x);
		}*/

		Value(DObject* owner)
		{
			addr=owner->RegisterField();
		}

	};
	typedef Value<int> DInt;
	typedef Value<float> DFloat;
	typedef Value<double> DDouble;

	template<class T> class Reference
	{
	private:
		T obj;
	public:
		T* operator->()
		{
			lastobject = &obj;
			return &obj;
		}

		Reference(T& k) :T(k){}
	};
}
using namespace Dogee;
class clsa : DObject
{
public:
	DInt i;
	DFloat j;
	DDouble k;
	clsa(ShortKey obj_id) : DObject(obj_id), i(this), j(this), k(this)
	{}
};

template <int I=0>
class AAA
{
	int jjj;
public:
	void print()
	{
		std::cout << I << std::endl;
	}
};

int main(int argc, char* argv[])
{
/*	Dogee::DInt i = 10;
	int b = 0;
	i = i + 1;
	Dogee::DInt j = 220;
	i = j;
	Reference<clsa> aaa(123);
	std::cout<<*aaa;
	clsa obj(0);
	obj.i = 1;
	obj.j = 2.3;
	obj.k = 4.5;
	std::cout << sizeof(clsa) << std::endl << sizeof(DObject) << std::endl << sizeof(Value<int>) << std::endl;*/
	AAA<32> ooo;
	std::cout << sizeof(ooo);
	return 0;
}

