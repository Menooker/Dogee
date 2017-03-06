#ifndef __DOGEE_BASE_H_ 
#define __DOGEE_BASE_H_ 


#include <assert.h>
//#include "hash_compatible.h"
#include <unordered_map>
#include <functional>
#include "Dogee.h"
#include "DogeeEnv.h"
#include "DogeeStorage.h"
#include "DogeeUtil.h"

namespace Dogee
{
	int SetSlaveInitProc(void(*)(uint32_t));
	template <class T> struct AutoRegisterObject;
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
		template <typename T2,int size>
		struct Helper
		{
			static const int dsm_size_of = 1;
			static T2 get(ObjectKey obj_id, FieldKey field_id)
			{
				static_assert(size==4 || size==8, "the size of the field/array element is not 4 or 8");
				return *(T*)nullptr;
			}
			static void set(ObjectKey obj_id, FieldKey field_id, T2 val)
			{
				static_assert(size == 4 || size == 8, "the size of the field/array element is not 4 or 8");
			}
		};
		template <typename T2>
		struct Helper<T2,4>
		{
			static const int dsm_size_of = 1;
			static T2 get(ObjectKey obj_id, FieldKey field_id)
			{
				return trunc_cast<T2>(DogeeEnv::cache->get(obj_id, field_id));
			}
			static void set(ObjectKey obj_id, FieldKey field_id, T2 val)
			{
				DogeeEnv::cache->put(obj_id, field_id, trunc_cast<uint32_t>(val));
			}
		};
		template <typename T2>
		struct Helper<T2, 8>
		{
			static const int dsm_size_of = 2;
			static T2 get(ObjectKey obj_id, FieldKey field_id)
			{
				T2 ret;
				DogeeEnv::cache->getchunk(obj_id, field_id, 1, (uint64_t*)&ret);
				return ret;
			}
			static void set(ObjectKey obj_id, FieldKey field_id, T2 val)
			{
				DogeeEnv::cache->put(obj_id, field_id, trunc_cast<uint64_t>(val));
			}
		};
	public:
		static const int dsm_size_of = Helper<T, sizeof(T)>::dsm_size_of;
		static T get_value(ObjectKey obj_id, FieldKey field_id)
		{
			return Helper<T,sizeof(T)>::get(obj_id,field_id);
		}


		static void set_value(ObjectKey obj_id, FieldKey field_id, T val)
		{
			//fix-me : check the return value
			Helper<T, sizeof(T)>::set(obj_id, field_id, val);
		}

	};


	template <typename T, bool isVirtual>
	class DSMInterface < Ref<T, isVirtual> >
	{
	public:
		static const int dsm_size_of = 1;
		static Ref<T, isVirtual> get_value(ObjectKey obj_id, FieldKey field_id)
		{
			ObjectKey key = DogeeEnv::cache->get(obj_id, field_id);
			Ref<T, isVirtual> ret(key);
			return ret;
		}

		static void set_value(ObjectKey obj_id, FieldKey field_id, Ref<T, isVirtual> val)
		{
			DogeeEnv::cache->put(obj_id, field_id, val.GetObjectId());
		}

	};

	template <typename T>
	class DSMInterface < Array<T> >
	{
	public:
		static const int dsm_size_of = 1;
		static Array<T> get_value(ObjectKey obj_id, FieldKey field_id)
		{
			ObjectKey key = (ObjectKey)DogeeEnv::cache->get(obj_id, field_id);
			Array<T> ret(key);
			return ret;
		}

		static void set_value(ObjectKey obj_id, FieldKey field_id, Array<T> val)
		{
			DogeeEnv::cache->put(obj_id, field_id,val.GetObjectId());
		}

	};
	inline uint32_t GetClassId(ObjectKey obj_id)
	{
		uint32_t ret;
		DogeeEnv::backend->getinfo(obj_id, ret);
		return ret;
	}
/*	inline void SetClassId(ObjectKey obj_id, int cls_id)
	{
		//test code
		DogeeEnv::cache->put(obj_id, 97, cls_id);
	}*/
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

		LongKey get_address()
		{
			return (((LongKey)ok) << 32 | fk);
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

	/* A dirty implementation for operator [] of array.
	 As you can see, most of the members of Value<Array<T>,FieldId> are
	 the same as Value<Array<T>,FieldId> 's. We just add the method
	 ArrayElement<T> operator[](int k).
	 Why not use 
	 decltype(getarray(T(0), 0)) operator[](int k)
	 declared in ArrayElement?
	 It is because when we define a class having a member referencing it self (like a node of
	 linked list), the compiler won't pass, throwing "the class has as incomplete type". 
	 (Note that "decltype(getarray(T(0), 0))" references the class T, which is incomplete 
	 when we are still declaring the member variables of the class T.)
	*/
	template<typename T, FieldKey FieldId> class Value<Array<T>,FieldId>
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
		ArrayElement<T> operator[](int k)
		{
			return get().ArrayAccess(k);
		}
		Array<T> operator->()
		{
			return get();
		}

	};

	template<typename T> class Array
	{
	private:
		// this is a template to check the type of the array - whether it
		// supports array copy operation
		template<typename T2, int n> struct Copyer
		{
			static uint32_t* CopyType(T2* ptr){ abort(); return nullptr; }//unsupported type of array is found
		};

		template<typename T2> struct Copyer<T2, 4>
		{
			static uint32_t* CopyType(T2* ptr){ return (uint32_t*)ptr; }
		};
		template<typename T2> struct Copyer<T2, 8>
		{
			static uint64_t* CopyType(T2* ptr){ return (uint64_t*)ptr; }
		};

		// We should be careful here. If Dogee::Ref<T,false> happens to have only one
		// member, which is of size 4 bytes, then sizeof(Dogee::Ref<T,false>) happens to be 4.
		// So we can use a uint32_t array to represent a Dogee::Ref<T,false> array.
		// However, when using Array<Dogee::Ref<T,true>>, we have to mannually construct
		// each reference for the virtual table. We don't support Ref<T,true> here.
		template<typename T2> struct Copyer<Ref<T2, false>, 4>
		{
			static uint32_t* CopyType(Ref<T2, false>* ptr){ return (uint32_t*)ptr; }
		};

		// We should be careful here. Dogee::Array happens to have only one
		// member, which is of size 4 bytes. sizeof(Dogee::Array) happens to be 4.
		// So we can use a uint32_t array to represent a Dogee::Array array.
		template<typename T2> struct Copyer<Array<T2>, 4>
		{
			static uint32_t* CopyType(Array<T2>* ptr){ return (uint32_t*)ptr; }
		};
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
		Array<T>* operator->()
		{
			return this;
		}

		ArrayElement<T> ArrayAccess(int k)
		{
			FieldKey key = k * DSMInterface<T>::dsm_size_of;
			return ArrayElement<T>(object_id, key);
		}

		ArrayElement<T> operator[](int k)
		{
			return ArrayAccess(k);
		}

		void Fill(std::function<T(uint32_t)> func, uint32_t start_index, uint32_t len)
		{
			const unsigned bsize = DSM_CACHE_BLOCK_SIZE * 8;
			T blk[bsize];
			for (unsigned i = 0; i < len / bsize; i++)
			{
				for (unsigned j = 0; j < bsize; j++)
				{
					blk[j] = func(i*bsize + j);
				}
				CopyFrom(blk, start_index + i*bsize, bsize);
			}
			unsigned remain_start= len / bsize *  bsize;
			for (unsigned j = remain_start; j < len; j++)
			{
				blk[j - remain_start] = func(j);
			}
			CopyFrom(blk, start_index + remain_start, len - remain_start);

		}

		void CopyTo(T* localarr, uint32_t start_index,uint32_t copy_len)
		{
			DogeeEnv::cache->getchunk(object_id, start_index, copy_len, Copyer<T, sizeof(T)>::CopyType(localarr));
		}

		void CopyFrom(T* localarr, uint32_t start_index, uint32_t copy_len)
		{
			DogeeEnv::cache->putchunk(object_id, start_index, copy_len, Copyer<T, sizeof(T)>::CopyType(localarr));
		}
	};



	template<class T> class Ref<T, false>
	{
		static_assert(!std::is_abstract<T>::value, "T should be non-abstract when using non-virtual Reference.");
	private:
		T obj;
	public:

		T* get()
		{
			lastobject = (DObject*)&obj;
			return &obj;
		}
		
		T* operator->()
		{
			return get();
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
		template <class T2,bool isV>
		Ref<T, false>& operator=(ArrayElement<Ref<T2, isV>>& x)
		{
			static_assert(std::is_base_of<T, T2>::value, "T2 should be subclass of T.");
			obj.SetObjectId(x.get().GetObjectId());
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
		T* get()
		{
			if (!pobj) //fix-me : possible memory leak in multithreaded environment
				RefObj(okey);
			lastobject = (DObject*)pobj;
			return pobj;
		}

		T* operator->()
		{
			return get();
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
			delete pobj;
			pobj = nullptr;
			return *this;
		}
		template <class T2, bool isV>
		Ref<T, false>& operator=(ArrayElement<Ref<T2, isV>>& x)
		{
			static_assert(std::is_base_of<T, T2>::value, "T2 should be subclass of T.");
			okey = x.get().GetObjectId();
			delete pobj;
			pobj = nullptr;
			return *this;
		}

		template <class T2, bool isVirtual>
		Ref(Ref<T2, isVirtual> x)
		{
			static_assert(std::is_base_of<T, T2>::value, "T2 should be subclass of T.");
			delete pobj;
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


	inline ObjectKey AllocObjectId(uint32_t cls_id)
	{
		ObjectKey key = 0;
		bool found = false;
		for (int i = 0; i<DOGEE_MAX_SHARED_KEY_TRIES; i++)
		{
			while (key==0)
				key = rand();
			if (DogeeEnv::backend->newobj(key, cls_id) == SoOK)
			{
				found = true;
				break;
			}
		}
		if (!found)
		{
			abort();
			//fix-me : raise some hard error here			
		}
		return key;
	}
	template<typename T>
	inline  Array<T>  NewArray()
	{
		return Array<T>(AllocObjectId(1));
	}

	template <class T> struct AutoRegisterObject;
	template<class T,
	class... _Types> inline
		Ref<T> NewObj(_Types&&... _Args)
	{	// copy from std::shared_ptr

		ObjectKey ok = AllocObjectId(AutoRegisterObject<T>::id);
		//SetClassId(ok, T::CLASS_ID);
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
		FieldKey ret = fid;
		fid+= 2;
		return fid;
	}

	inline int ClassIdInc()
	{
		static int id = 100;
		return id++;
	}

	template <class T> struct AutoRegisterObject
	{
		static int id;
		static DObject* createInstance(ObjectKey key)
		{
			static_assert(!std::is_abstract<T>::value, "No need to register abstract class.");
			static_assert(std::is_base_of<DObject, T>::value, "T should be subclass of DObject.");
			return dynamic_cast<DObject*> (new T(key));
		}
	public:

		static int Init()
		{
			int mid=ClassIdInc();
			RegisterClass(mid, createInstance);
			return mid;
		}

		AutoRegisterObject()
		{
			static_assert(!std::is_abstract<T>::value, "No need to register abstract class.");
			static_assert(std::is_base_of<DObject, T>::value, "T should be subclass of DObject.");
			RegisterClass(id, createInstance);
		}
	};

	template<class TDest, class TSrc>
	inline TDest force_cast(TSrc src)
	{
		return TDest(src.GetObjectId());
	}

	template <class T> int AutoRegisterObject<T>::id = AutoRegisterObject<T>::Init();

}
#endif