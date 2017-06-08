#include "DogeeNVDSStorage.h"
#include <assert.h>
namespace Dogee
{
	std::string SoStorageNVDS::addr;
	THREAD_LOCAL nvds::Client *nvds_client;
	SoStatus SoStorageNVDS::put(ObjectKey key, FieldKey fldid, uint64_t v)
	{
		return putchunk(key, fldid, 1, &v);
	}
	SoStatus SoStorageNVDS::put(ObjectKey key, FieldKey fldid, uint32_t v)
	{
		LongKey myk = MAKE64(key, fldid);
		return nvds_client->Put((const char*)&myk, sizeof(myk), (const char*)&v,sizeof(v)) ? SoOK : SoFail;
	}
	uint32_t SoStorageNVDS::get(ObjectKey key, FieldKey fldid)
	{
		LongKey myk = MAKE64(key, fldid);
		std::string _v = nvds_client->Get((const char*)&myk, sizeof(myk));
		assert(_v.size() == sizeof(uint32_t));
		return *(uint32_t*)_v.c_str();
	}

#pragma pack(push)
#pragma pack(4)
	struct ObjectInfoNVDS
	{
		uint32_t id;
		uint32_t size;
	};
#pragma pack(pop)

	SoStatus SoStorageNVDS::newobj(ObjectKey key, uint32_t flag, uint32_t size)
	{
		uint64_t myk = MAKE64(key, 0xFFFFFFFF);
		ObjectInfoNVDS info = { flag, size };
		if (nvds_client->Add((const char*)&myk, sizeof(myk), (const char*)&info, sizeof(info)))
		{
			return SoOK;
		}
		return SoFail;
	}
	SoStatus SoStorageNVDS::getinfo(ObjectKey key, uint32_t& flag, uint32_t& size)
	{
		uint64_t myk = MAKE64(key, 0xFFFFFFFF);
		std::string _v = nvds_client->Get((const char*)&myk, sizeof(myk));
		assert(_v.size() == sizeof(ObjectInfoNVDS));
		ObjectInfoNVDS* pinfo = (ObjectInfoNVDS*)_v.c_str();
		flag = pinfo->id;
		size = pinfo->size;
		return SoOK;
	}
	SoStatus SoStorageNVDS::del(ObjectKey key)
	{
		uint32_t flg, size;
		getinfo(key, flg, size);
		LongKey k;
		for (uint32_t i = 0; i<size; i++)
		{
			k = MAKE64(key, i);
			nvds_client->Del((const char*)&k, sizeof(k));
		}
		k = (MAKE64(key, 0xFFFFFFFF));
		nvds_client->Del((const char*)&k, sizeof(k));
		return SoOK;
	}
	SoStatus SoStorageNVDS::getchunk(ObjectKey key, FieldKey fldid, uint32_t len, uint32_t* buf)
	{
		for (uint32_t i = 0; i<len; i++)
		{
			buf[i] = get(key, fldid + i);
		}
		return SoOK;
	}
	SoStatus SoStorageNVDS::getchunk(ObjectKey key, FieldKey fldid, uint32_t len, uint64_t* buf)
	{
		return getchunk(key, fldid, len * 2, (uint32_t*)buf);
	}
	SoStatus SoStorageNVDS::putchunk(ObjectKey key, FieldKey fldid, uint32_t len, uint64_t* buf)
	{
		return putchunk(key, fldid, len * 2, (uint32_t*)buf);
	}
	SoStatus SoStorageNVDS::putchunk(ObjectKey key, FieldKey fldid, uint32_t len, uint32_t* buf)
	{
		SoStatus stat = SoOK;
		for (uint32_t i = 0; i<len; i++)
		{
			if (put(key, fldid + i, buf[i]) != SoOK)
				stat= SoFail;
		}
		return stat;
	}
	SoStatus SoStorageNVDS::getblock(LongKey id, uint32_t* buf)
	{
		return putchunk(id >> 32, 0, DSM_CACHE_BLOCK_SIZE, buf);
	}

	uint64_t SoStorageNVDS::inc(ObjectKey key, FieldKey fldid, uint64_t inc)
	{
		assert(0 && "Not implemented");
		return 0;
	}
	uint64_t SoStorageNVDS::dec(ObjectKey key, FieldKey fldid, uint64_t dec)
	{
		assert(0 && "Not implemented");
		return 0;
	}
	uint64_t SoStorageNVDS::getcounter(ObjectKey key, FieldKey fldid)
	{
		assert(0 && "Not implemented");
		return 0;
	}
	SoStatus SoStorageNVDS::setcounter(ObjectKey key, FieldKey fldid, uint64_t n)
	{
		assert(0 && "Not implemented");
		return SoOK;
	}
}
