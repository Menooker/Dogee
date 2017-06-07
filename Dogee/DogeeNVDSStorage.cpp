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
		std::string _k((char*)&myk,sizeof(myk));
		std::string _v((char*)&v, sizeof(v));
		return nvds_client->Put(_k, _v)?SoOK:SoFail;
	}
	uint32_t SoStorageNVDS::get(ObjectKey key, FieldKey fldid)
	{
		LongKey myk = MAKE64(key, fldid);
		std::string _k((char*)&myk, sizeof(myk));
		std::string _v = nvds_client->Get(_k);
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
		std::string _k((char*)&myk, sizeof(myk));
		ObjectInfoNVDS info = { flag, size };
		std::string _v((char*)&info, sizeof(info));
		//assert(0 && "Not implemented!");
		if (nvds_client->Put(_k,_v)) //fix-me : Uncomment until "add" is added into NVDS
		{
			return SoOK;
		}
		return SoFail;
	}
	SoStatus SoStorageNVDS::getinfo(ObjectKey key, uint32_t& flag, uint32_t& size)
	{
		uint64_t myk = MAKE64(key, 0xFFFFFFFF);
		std::string _k((char*)&myk, sizeof(myk));
		std::string _v = nvds_client->Get(_k);
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
			std::string _k((char*)&k, sizeof(k));
			nvds_client->Del(_k);
		}
		k = (MAKE64(key, 0xFFFFFFFF));
		std::string _k((char*)&k, sizeof(k));
		nvds_client->Del(_k);
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
