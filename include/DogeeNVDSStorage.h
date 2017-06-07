#ifndef __DOGEE_NVDS_H_ 
#define __DOGEE_NVDS_H_ 

#include "DogeeStorage.h"
#include "client.h"
#include <string>
namespace Dogee
{
	extern THREAD_LOCAL nvds::Client *nvds_client;
	class SoStorageNVDS : public SoStorage
	{
		static std::string addr;
	public:
		static void InitInCurrentThread()
		{
			nvds_client = new nvds::Client(addr);
		}
		static void DestroyInCurrentThread()
		{
			if (nvds_client) delete nvds_client;
		}
		SoStorageNVDS(const std::string& _addr)
		{
			addr = _addr;
		}
		virtual SoStatus put(ObjectKey key, FieldKey fldid, uint64_t v);
		virtual SoStatus put(ObjectKey key, FieldKey fldid, uint32_t v);
		virtual uint32_t get(ObjectKey key, FieldKey fldid);
		virtual SoStatus newobj(ObjectKey key, uint32_t flag, uint32_t size);
		virtual SoStatus getinfo(ObjectKey key, uint32_t& flag, uint32_t& size);

		SoStatus del(ObjectKey key);
		SoStatus getchunk(ObjectKey key, FieldKey fldid, uint32_t len, uint32_t* buf);
		SoStatus getchunk(ObjectKey key, FieldKey fldid, uint32_t len, uint64_t* buf);
		SoStatus putchunk(ObjectKey key, FieldKey fldid, uint32_t len, uint64_t* buf);
		SoStatus putchunk(ObjectKey key, FieldKey fldid, uint32_t len, uint32_t* buf);
		SoStatus getblock(LongKey id, uint32_t* buf);

		virtual uint64_t inc(ObjectKey key, FieldKey fldid, uint64_t inc);
		virtual uint64_t dec(ObjectKey key, FieldKey fldid, uint64_t dec);
		virtual uint64_t getcounter(ObjectKey key, FieldKey fldid);
		virtual SoStatus setcounter(ObjectKey key, FieldKey fldid, uint64_t n);

		~SoStorageNVDS(){}
	};
}

#endif