#ifndef __DOGEE_BASE_H_ 
#define __DOGEE_BASE_H_ 


#include <assert.h>
//#include "hash_compatible.h"
#include <unordered_map>
#include <functional>
#include "Dogee.h"
#include "DogeeEnv.h"

namespace Dogee
{
	//test code


	/*template <class Dest, class Source>
	inline Dest bit_cast(const Source& source) {
		static_assert(sizeof(Dest) <= sizeof(Source), "Error: sizeof(Dest) > sizeof(Source)");
		Dest dest;
		memcpy(&dest, &source, sizeof(dest));
		return dest;
	}*/

	template <class Dest, class Source>
	inline Dest bit_cast(const Source source) {
		//static_assert(sizeof(Dest) <= sizeof(Source), "Error: sizeof(Dest) > sizeof(Source)");
		//Dest dest;
		//memcpy(&dest, &source, sizeof(dest));
		return * reinterpret_cast<const Dest*>(&source);
	}

	template <class T> struct AutoRegisterObject;
	class DObject
	{
	protected:
		static const int _LAST_ = 0;
		static const int CLASS_ID = 0;
	private:
		ObjectKey object_id;
	public:
		//test code
		ObjectKey GetObjectId()
		{
			return object_id;
		}
		void SetObjectId(ObjectKey ok)
		{
			object_id = ok;
		}
		DObject(ObjectKey obj_id)
		{
			object_id = obj_id;
		}


	};
	extern THREAD_LOCAL DObject* lastobject;

	typedef  std::function<DObject*(ObjectKey)> VirtualReferenceCreator;

	inline std::unordered_map<unsigned, VirtualReferenceCreator>& GetClassMap()
	{
		static std::unordered_map<unsigned, VirtualReferenceCreator> VirtualReferenceCreators;
		return VirtualReferenceCreators;
	}

	inline void RegisterClass(unsigned cls_id, VirtualReferenceCreator  func)
	{
		GetClassMap()[cls_id] = func;
	}

	inline VirtualReferenceCreator& GetClassById(unsigned cls_id)
	{
		return GetClassMap()[cls_id];
	}

	template<class T> T currentClassDummy();

	template<class T, bool isVirtual = false> class Ref;
	template<typename T> class Array;

	template <typename T>
	class DSMInterface
	{
	public:
		static T get_value(ObjectKey obj_id, FieldKey field_id)
		{
			return bit_cast<T>(DogeeEnv::cache->get(obj_id, field_id));
		}


		static void set_value(ObjectKey obj_id, FieldKey field_id, T val)
		{
			//fix-me : check the return value
			DogeeEnv::cache->put(obj_id, field_id, bit_cast<uint64_t>(val));
		}

	};


	template <typename T, bool isVirtual>
	class DSMInterface < Ref<T, isVirtual> >
	{
	public:
		static Ref<T, isVirtual> get_value(ObjectKey obj_id, FieldKey field_id)
		{
			ObjectKey key = bit_cast<ObjectKey>(DogeeEnv::cache->get(obj_id, field_id));
			Ref<T, isVirtual> ret(key);
			return ret;
		}

		static void set_value(ObjectKey obj_id, FieldKey field_id, Ref<T, isVirtual> val)
		{
			DogeeEnv::cache->put(obj_id, field_id, bit_cast<uint64_t>(val.GetObjectId()));
		}

	};

	template <typename T>
	class DSMInterface < Array<T> >
	{
	public:
		static Array<T> get_value(ObjectKey obj_id, FieldKey field_id)
		{
			ObjectKey key = bit_cast<ObjectKey>(DogeeEnv::cache->get(obj_id, field_id));
			Array<T> ret(key);
			return ret;
		}

		static void set_value(ObjectKey obj_id, FieldKey field_id, Array<T> val)
		{
			DogeeEnv::cache->put(obj_id, field_id, bit_cast<uint64_t>(val.GetObjectId()));
		}

	};
	inline int GetClassId(ObjectKey obj_id)
	{
		//test code
		return (int)DogeeEnv::cache->get(obj_id, 97);
	}
	inline void SetClassId(ObjectKey obj_id, int cls_id)
	{
		//test code
		DogeeEnv::cache->put(obj_id, 97, cls_id);
	}
	template<typename T> class ArrayElement
	{
	private:
		ObjectKey ok;
		FieldKey fk;
		template<typename Ty> static Ty getarray(Ty, int);
		template<typename Ty>
		ArrayElement<Ty> static getarray(Array<Ty> arr, int k)
		{
			return arr.ArrayAccess(k);
		}
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
		decltype(getarray(T(0), 0)) operator[](int k)
		{
			return getarray(get(), k);
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


	template<typename T, FieldKey FieldId> class Value
	{
	private:
		template<typename Ty> static Ty getarray(Ty, int);
		template<typename Ty>
		ArrayElement<Ty> static getarray(Array<Ty> arr, int k)
		{
			return arr.ArrayAccess(k);
		}
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
			assert(lastobject != nullptr);// "You should use a Ref<T> to access the member"
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


		decltype(getarray(T(0),0)) operator[](int k)
		{
			return getarray(get(),k);
		}
		//write
		Value<T, FieldId>& operator=(T x)
		{
			assert(lastobject != nullptr);// "You should use a Ref<T> to access the member"
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
/*	template<typename T, FieldKey FieldId> class Value<Array<T>, FieldId>
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
			assert(lastobject != nullptr);// "You should use a Ref<T> to access the member"
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
			assert(lastobject != nullptr);// "You should use a Ref<T> to access the member"
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
	*/


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



	template<class T> class Ref<T, false>
	{
		static_assert(!std::is_abstract<T>::value, "T should be non-abstract when using non-virtual Reference.");
	private:
		T obj;
	public:
		T* operator->()
		{
			lastobject = (DObject*)&obj;
			return &obj;
		}
		ObjectKey GetObjectId()
		{
			return obj.GetObjectId();
		}

		template <class T2>
		Ref<T, false>& operator=(Ref<T2, false> x)
		{
			static_assert(std::is_base_of<T, T2>::value, "T2 should be subclass of T.");
			obj.SetObjectId(x->GetObjectId());
			return *this;
		}

		template <class T2>
		Ref<T, false>& operator=(Ref<T2, true> x)
		{
			static_assert(std::is_base_of<T, T2>::value, "T2 should be subclass of T.");
			obj.SetObjectId(x->GetObjectId());
			return *this;
		}

		template <class T2, bool isVirtual>
		Ref(Ref<T2, isVirtual> x) :obj(x.GetObjectId())
		{
			static_assert(std::is_base_of<T, T2>::value, "T2 should be subclass of T.");
		}

		Ref(ObjectKey key) :obj(key)
		{
			static_assert(std::is_base_of<DObject, T>::value, "T should be subclass of DObject.");
		}

	};

	template<class T> class Ref < T, true >
	{
	private:
		T* pobj;
		ObjectKey okey;
		void RefObj(ObjectKey okey)
		{
			if (okey == 0)
			{
				pobj = nullptr;
				return;
			}
			std::function<DObject*(ObjectKey)> func = GetClassById(GetClassId(okey));
			pobj = (T*)(func(okey)); //dynamic_cast<T*> (func(okey));
			assert(pobj);
		}
	public:
		T* operator->()
		{
			if (!pobj) //possible memory leak in multithreaded environment
				RefObj(okey);
			lastobject = (DObject*)pobj;
			return pobj;
		}

		ObjectKey GetObjectId()
		{
			return okey;
		}

		//copy or upcast
		template <class T2, bool isVirtual>
		Ref<T, true>& operator=(Ref<T2, isVirtual> x)
		{
			static_assert(std::is_base_of<T, T2>::value, "T2 should be subclass of T.");
			okey = x.GetObjectId();
			return *this;
		}

		template <class T2, bool isVirtual>
		Ref(Ref<T2, isVirtual> x)
		{
			static_assert(std::is_base_of<T, T2>::value, "T2 should be subclass of T.");
			pobj = nullptr;
			okey = x.GetObjectId();
		}


		Ref(ObjectKey key)
		{
			static_assert(std::is_base_of<DObject, T>::value, "T should be subclass of DObject.");
			pobj = nullptr;
			okey = key;
		}

		~Ref()
		{
			delete pobj;
		}
	};
	//test code
	extern ObjectKey objid;

	inline ObjectKey AllocObjectId()
	{
		return objid++;
	}
	template<typename T>
	inline  Array<T>  NewArray()
	{
		return Array<T>(AllocObjectId());
	}


	template<class T,
	class... _Types> inline
		Ref<T> NewObj(_Types&&... _Args)
	{	// copy from std::shared_ptr

		ObjectKey ok = AllocObjectId();
		SetClassId(ok, T::CLASS_ID);
		T ret(ok, std::forward<_Types>(_Args)...);
		return Ref<T>(ok);
	}

	template <class T> inline T* ReferenceObject(T* obj)
	{
		lastobject = (DObject*)obj;
		return obj;
	}

	inline FieldKey RegisterGlobalVariable()
	{
		static FieldKey fid = 0;
		return fid++;
	}

	template <class T> struct AutoRegisterObject
	{
		static DObject* createInstance(ObjectKey key)
		{
			return dynamic_cast<DObject*> (new T(key));
		}
	public:
		AutoRegisterObject()
		{
			static_assert(!std::is_abstract<T>::value, "No need to register abstract class.");
			static_assert(std::is_base_of<DObject, T>::value, "T should be subclass of DObject.");
			RegisterClass(T::CLASS_ID, createInstance);
		}
	};

	template<class TDest, class TSrc>
	inline TDest force_cast(TSrc src)
	{
		return TDest(src.GetObjectId());
	}
}
#endif