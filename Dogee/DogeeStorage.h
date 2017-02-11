#ifndef __DOGEE_STORAGE_H_ 
#define __DOGEE_STORAGE_H_ 


#include "Dogee.h"

namespace Dogee
{

	enum SoStatus
	{
		SoOK,
		SoUnknown,
		SoKeyNotFound,
		SoFail,
	};

	class SoStorage
	{
	public:
		static void InitInCurrentThread()
		{};
		virtual SoStatus newobj(ObjectKey key, uint32_t flag) = 0;
		virtual SoStatus getinfo(ObjectKey key, uint32_t& flag) = 0;
		virtual SoStatus put(ObjectKey key, FieldKey fldid, uint64_t v) = 0;
		virtual uint64_t get(ObjectKey key, FieldKey fldid) = 0;
	};


	class LocalStorage :public SoStorage
	{
		uint64_t data[4096 * 32];
	public:
		SoStatus newobj(ObjectKey key, uint32_t flag)
		{
			data[key * 100 + 97] = flag;
			return SoOK;
		}

		SoStatus getinfo(ObjectKey key, uint32_t& flag)
		{
			flag = (uint32_t)data[key * 100 + 97];
			return SoOK;
		}


		SoStatus put(ObjectKey key, FieldKey fldid, uint64_t v)
		{
			data[key * 100 + fldid] = v;
			return SoOK;
		}

		uint64_t get(ObjectKey key, FieldKey fldid)
		{
			return data[key * 100 + fldid];
		}
	};



	class DSMCache
	{
	protected:
#ifdef BD_DSM_STAT
		unsigned long writes, whit, reads, rhit;
#endif
	public:

		DSMCache(){}
		virtual SoStatus put(ObjectKey key, FieldKey fldid, uint64_t v) = 0;
		virtual uint64_t get(ObjectKey key, FieldKey fldid) = 0;
#ifdef BD_DSM_STAT
		virtual void get_stat(long& mwrites, long& mwhit, long& mreads, long& mrhit)
		{
			mwrites = writes;
			mwhit = whit;
			mreads = reads;
			mrhit = rhit;
		}
#endif


	};


	class DSMNoCache : public DSMCache
	{
	private:
		SoStorage* backend;
	public:
		DSMNoCache(SoStorage* back) :backend(back)
		{}
		SoStatus put(ObjectKey key, FieldKey fldid, uint64_t v)
		{
			return backend->put(key, fldid, v);

		}

		uint64_t get(ObjectKey key, FieldKey fldid)
		{
			return backend->get(key, fldid);
		}

	};


}

#endif