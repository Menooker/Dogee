// Dogee.cpp : 定义控制台应用程序的入口点。
//
#ifdef _WIN32
#include "stdafx.h"
#endif

#include <stdio.h>
#include <iostream>

#include "DogeeMacro.h"
#include <stdint.h>
#include <assert.h>
#include "Dogee.h"

namespace Dogee
{
	//test code

	uint64_t data[4096 * 32];

	template <class Dest, class Source>
	inline Dest bit_cast(const Source& source) {
		static_assert(sizeof(Dest) <= sizeof(Source), "Error: sizeof(Dest) > sizeof(Source)");
		Dest dest;
		memcpy(&dest, &source, sizeof(dest));
		return dest;
	}



	typedef uint32_t ObjectKey;
	typedef uint32_t FieldKey;
	typedef uint64_t LongKey;
	class DObject
	{
	protected:
		static const int _LAST_ = 0;
	private:
		ObjectKey object_id;
	public:
		//test code
		ObjectKey GetObjectId()
		{
			return object_id;
		}

		DObject(ObjectKey obj_id)
		{
			object_id = obj_id;
			//last_field_id = ((LongKey)obj_id)<<32;
			//last_field_id = (LongKey)(data + obj_id * 10);
		}

		/*		DObject(DObject& x)
				{
				object_id=x.object_id;
				}*/

	};
	thread_local DObject* lastobject = nullptr;


	template<class T> class Reference;
	template<typename T> class Array;

	template <typename T>
	class DSMInterface
	{
	public:
		static T get_value(ObjectKey obj_id, FieldKey field_id)
		{
			T ret = *(T*)(data + obj_id * 10 + field_id);
			return ret;
		}


		static void set_value(ObjectKey obj_id, FieldKey field_id, T val)
		{
			*(T*)(data + obj_id * 10 + field_id) = val;
		}

	};

	template <typename T>
	class DSMInterface < Reference<T> >
	{
	public:
		static Reference<T> get_value(ObjectKey obj_id, FieldKey field_id)
		{
			ObjectKey key = *(ObjectKey*)(data + obj_id * 10 + field_id);
			Reference<T> ret(key);
			return ret;
		}

		static void set_value(ObjectKey obj_id, FieldKey field_id, Reference<T> val)
		{
			*(ObjectKey*)(data + obj_id * 10 + field_id) = val->GetObjectId();
		}

	};

	template <typename T>
	class DSMInterface < Array<T> >
	{
	public:
		static Array<T> get_value(ObjectKey obj_id, FieldKey field_id)
		{
			ObjectKey key = *(ObjectKey*)(data + obj_id * 10 + field_id);
			Array<T> ret(key);
			return ret;
		}

		static void set_value(ObjectKey obj_id, FieldKey field_id, Array<T> val)
		{
			*(ObjectKey*)(data + obj_id * 10 + field_id) = val.GetObjectId();
		}

	};


	template<typename T> class ArrayElement
	{
		ObjectKey ok;
		FieldKey fk;
	public:

		//copy 
		ArrayElement<T>& operator=(ArrayElement<T>& x)
		{
			set(x.get());
			return *this;
		}

		T get()
		{
			return DSMInterface<T>::get_value(ok, fk);
		}

		void set(T& x)
		{
			DSMInterface<T>::set_value(ok, fk, x);
		}
		//read
		operator T()
		{
			return get();
		}

		T operator->()
		{
			return get();
		}

		//write
		T operator=(T x)
		{
			set(x);
			return x;
		}


		ArrayElement(ObjectKey o_key, FieldKey f_key) : ok(o_key), fk(f_key)
		{
		}

	};


	//template<typename T> T dummy(T);
	//template<typename T> ArrayElement<T> dummy(Array<T>);
	template<typename T, FieldKey FieldId> class Value
	{
	private:

		//copy functions are forbidden, you should copy the value like "a->val = b->val +0"
		template<typename T2, FieldKey FieldId2>Value<T, FieldId>& operator=(Value<T2, FieldId2>& x);
		Value<T, FieldId>& operator=(Value<T, FieldId>& x);
	public:
		int GetFieldId()
		{
			return FieldId;
		}


		T get()
		{
			assert(lastobject != nullptr);// "You should use a Reference<T> to access the member"
			T ret = DSMInterface<T>::get_value(lastobject->GetObjectId(), FieldId);
#ifdef DOGEE_DBG
			lastobject = nullptr;
#endif
			return ret;
		}
		//read
		operator T()
		{
			return get();
		}

		T operator->()
		{
			return get();
		}


		//write
		Value<T, FieldId>& operator=(T x)
		{
			assert(lastobject != nullptr);// "You should use a Reference<T> to access the member"
			DSMInterface<T>::set_value(lastobject->GetObjectId(), FieldId, x);
#ifdef DOGEE_DBG
			lastobject = nullptr;
#endif
			return *this;
		}
		Value()
		{
		}

	};

	//a dirty bypass for Value<Array<T>, FieldId>, just to add "operator[]" 
	template<typename T, FieldKey FieldId> class Value<Array<T>, FieldId>
	{
	private:

		//copy functions are forbidden, you should copy the value like "a->val = b->val +0"
		template<typename T2, FieldKey FieldId2>Value<Array<T>, FieldId>& operator=(Value<T2, FieldId2>& x);
		Value<Array<T>, FieldId>& operator=(Value<Array<T>, FieldId>& x);
	public:
		int GetFieldId()
		{
			return FieldId;
		}


		Array<T> get()
		{
			assert(lastobject != nullptr);// "You should use a Reference<T> to access the member"
			Array<T> ret = DSMInterface<Array<T>>::get_value(lastobject->GetObjectId(), FieldId);
#ifdef DOGEE_DBG
			lastobject = nullptr;
#endif
			return ret;
		}
		//read
		operator Array<T>()
		{
			return get();
		}
		ArrayElement<T> operator[](int k)
		{
			Array<T> ret = get();
			return ret.ArrayAccess(k);
		}
		//write
		Value<Array<T>, FieldId>& operator=(Array<T> x)
		{
			assert(lastobject != nullptr);// "You should use a Reference<T> to access the member"
			DSMInterface<Array<T>>::set_value(lastobject->GetObjectId(), FieldId, x);
#ifdef DOGEE_DBG
			lastobject = nullptr;
#endif
			return *this;
		}
		Value()
		{
		}

	};



	template<typename T> class Array
	{
	private:
		ObjectKey object_id;
	public:
		//test code
		ObjectKey GetObjectId()
		{
			return object_id;
		}
		Array(ObjectKey obj_id)
		{
			object_id = obj_id;
		}

		ArrayElement<T> ArrayAccess(int k)
		{
			FieldKey key = k;
			return ArrayElement<T>(object_id, key);
		}

		ArrayElement<T> operator[](int k)
		{
			return ArrayAccess(k);
		}
	};


	template<class T> class Reference
	{
	private:
		T obj;
	public:
		T* operator->()
		{
			lastobject = (DObject*)&obj;
			return &obj;
		}

		Reference(ObjectKey key) :obj(key){}

		Reference(T& k) :obj(k){}
	};

	//test code
	ObjectKey objid;
	template<class T> class Alloc
	{
	private:
		//test code

		static ObjectKey AllocObjectId()
		{
			return objid++;
		}
	public:
		static Array<T>  newarray()
		{
			return Array<T>(AllocObjectId());
		}

		static Reference<T>  tnew()
		{
			T ret(AllocObjectId());
			return Reference<T>(ret);
		}

		template<typename P1>
		static Reference<T>  tnew(P1 p1)
		{
			T ret(AllocObjectId(), p1);
			return Reference<T>(ret);
		}

		template<typename P1, typename P2>
		static Reference<T>  tnew(P1 p1, P2 p2)
		{
			T ret(AllocObjectId(), p1, p2);
			return Reference<T>(ret);
		}
		/*
		template<typename P1, typename P2, typename P3>
		T*  tnew(P1 p1, P2 p2, P3 p3)
		{
		return new (alloc()) T(p1, p2, p3);
		}

		template<typename P1, typename P2, typename P3, typename P4>
		T*  tnew(P1 p1, P2 p2, P3 p3, P4 p4)
		{
		return new (alloc()) T(p1, p2, p3, p4);
		}

		template<typename P1, typename P2, typename P3, typename P4, typename P5>
		T*  tnew(P1 p1, P2 p2, P3 p3, P4 p4, P5 p5)
		{
		return new (alloc()) T(p1, p2, p3, p4, p5);
		}*/
	};

	template <class T> inline T* ReferenceObject(T* obj)
	{
		lastobject = (DObject*)obj;
		return obj;
	}
}
using namespace Dogee;

class clsa : public DObject
{
	DefBegin(DObject);
public:
	Def(int, i);
	Def(Array<float>, arr);
	Def(Array<Reference<clsa>>, next);
	DefEnd();
	clsa(ObjectKey obj_id) : DObject(obj_id)
	{}
	

};




template<typename T> void aaa(T* dummy)
{
	std::cout << "Normal" << std::endl;
}
template<> void aaa(int * dummy)
{
	std::cout << "Int" << std::endl;
}
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
	auto ptr = Dogee::Alloc<clsa>::tnew();
	ptr->arr = Dogee::Alloc<float>::newarray();
	ptr->next = Dogee::Alloc<Reference<clsa>>::newarray();
	ptr->next[0] = ptr;
	ptr->next[0]->i = 123;
	ptr->arr[0] = 133;
	ptr->arr[0]=ptr->arr[0] + 1;
	std::cout << ptr->arr[0] << std::endl << ptr->i;
	//std::cout << ptr->i << std::endl << ptr->j << std::endl << ptr->k << std::endl;
	//ptr->next = ptr;
	//ptr->next->next->i = 1;
	//Reference<clsa> ptr2=ptr->next;
	return 0;
}

