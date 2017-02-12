#include "DogeeMemcachedStorage.h"

namespace Dogee
{
	THREAD_LOCAL memcached_st *memc = nullptr;
	memcached_st * SoStorageMemcached::main_memc = nullptr;
	inline memcached_return memcached_put(memcached_st* memca, uint64_t k, void* v, size_t len)
	{
		//char ch[17];
		//sprintf(ch,"%016llx",k);
		memcached_return rc = memcached_set(memca, (char*)&k, 8, (char*)v, len, (time_t)0, (uint32_t)0);
		return rc;
	}

	SoStatus SoStorageMemcached::put(ObjectKey key, FieldKey fldid, uint64_t v)
	{
		memcached_return rc = memcached_put(memc, MAKE64(key, fldid), &v, sizeof(v));
		if (rc == MEMCACHED_SUCCESS)
			return SoOK;
		return SoFail;
	}
	uint64_t SoStorageMemcached::get(ObjectKey key, FieldKey fldid)
	{
		uint64_t ret;
		uint64_t k = MAKE64(key, fldid);
		char* mret;

		size_t len;
#ifdef DOGEE_ON_X64
		uint32_t flg;
#else
		size_t flg;
#endif
		memcached_return rc;
		//	char ch[17];
		//	sprintf(ch,"%016llx",k);
		mret = memcached_get(memc, (char*)&k, 8, &len, &flg, &rc);
		if (rc == MEMCACHED_SUCCESS) {
			ret = *(uint64_t*)mret;
			memcached_free2(mret);
			return ret;
		}
		else
		{
			//printf("Memc Error:%d",memc->cached_errno);
			throw 1;
		}


		return ret;
	}
	SoStatus SoStorageMemcached::newobj(ObjectKey key, uint32_t flag)
	{
		uint64_t k = MAKE64(key, 0xFFFFFFFF);
		if (memcached_add(memc, (char*)&k, 8, (char*)&flag, sizeof(flag), (time_t)0, 0) == MEMCACHED_SUCCESS)
		{
			return SoOK;
		}
		return SoFail;
	}
	SoStatus SoStorageMemcached::getinfo(ObjectKey key, uint32_t& flag)
	{
		uint64_t k = MAKE64(key, 0xFFFFFFFF);
		char* mret;

		size_t len;
#ifdef DOGEE_ON_X64
		uint32_t flg;
#else
		size_t flg;
#endif
		memcached_return rc;
		mret = memcached_get(memc, (char*)&k, 8, &len, &flg, &rc);
		if (rc == MEMCACHED_SUCCESS) {
			flag = *(uint32_t*)mret;
			memcached_free2(mret);
			return SoOK;
		}
		else
		{
			return SoFail;
		}

	}
	void* init_memcached_this_thread()
	{
		if (memc)
			return memc;
		//_memc=(memcached_st*)memcached_create(NULL);
		if (SoStorageMemcached::main_memc)
			memc = memcached_clone(memc, SoStorageMemcached::main_memc);
		else
			return NULL;
		return memc;
	}
}