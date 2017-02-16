#include "DogeeMemcachedStorage.h"
#include "DogeeUtil.h"
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

	static SoStatus fetchchunk(uint64_t k, uint32_t len)
	{
		typedef char* _PCHAR;
		char** keys = new _PCHAR[len];
		size_t* key_length = new size_t[len];
		uint64_t* maddr = new uint64_t[len];

		for (uint32_t i = 0; i<len; i++)
		{
			maddr[i] = k + i;
			keys[i] = (char*)&maddr[i];
			key_length[i] = 8;
		}

		memcached_return rc = memcached_mget(memc, (char**)keys, key_length, len);
		delete[]keys;
		delete[]key_length;
		delete[]maddr;
		return (rc == MEMCACHED_SUCCESS) ? SoOK : SoFail;
	}
	template <typename T>
	SoStatus MemcachedGetChunk(ObjectKey key, FieldKey fldid, uint32_t len, T* buf)
	{
		uint64_t k = MAKE64(key, fldid);
		uint32_t offset;
		SoStatus ret;
		for (offset = 0; offset<len; offset += 15040)
		{
			memcached_result_st results_obj;
			memcached_result_st* results;
			results = memcached_result_create(memc, &results_obj);
			uint32_t mylen = offset + 15040<len ? 15040 : len - offset;
			memset(buf + offset, 0, sizeof(T)*mylen);

			memcached_return rc;
			ret = fetchchunk(k + offset, mylen);
			while ((results = memcached_fetch_result(memc, &results_obj, &rc)))
			{
				if (rc == MEMCACHED_SUCCESS)
				{
					uint64_t* pkey;
					T* pvalue;
					pkey = (uint64_t*)memcached_result_key_value(results);
					pvalue = (T*)memcached_result_value(results);
					//printf("read [%llx]=%d\n",*pkey,pvalue->vi);
					buf[(*pkey) - k] = *pvalue;
				}
			}
			memcached_result_free(&results_obj);
		}
		return ret;
	}

	SoStatus SoStorageMemcached::getchunk(ObjectKey key, FieldKey fldid, uint32_t len, uint32_t* buf)
	{
		return MemcachedGetChunk(key, fldid, len, buf);
	}
	SoStatus SoStorageMemcached::getchunk(ObjectKey key, FieldKey fldid, uint32_t len, uint64_t* buf)
	{
		return getchunk(key, fldid, len * 2, (uint32_t*)buf);
	}

	template <typename T>
	SoStatus MemcachedPutChunk(ObjectKey key, FieldKey fldid, uint32_t len, T* buf)
	{
		uint32_t k = fldid;
		SoStatus ret = SoOK;
		for (uint32_t i = 0; i<len; i++, k++)
		{
			uint32_t v = bit_cast<uint32_t>(buf[i]);
			if (memcached_put(memc, MAKE64(key, k), &v, sizeof(v)) != MEMCACHED_SUCCESS)
				ret = SoFail;
		}
		return ret;
	}

	SoStatus SoStorageMemcached::putchunk(ObjectKey key, FieldKey fldid, uint32_t len, uint32_t* buf)
	{
		return MemcachedPutChunk(key, fldid, len, buf);
	}
	SoStatus SoStorageMemcached::putchunk(ObjectKey key, FieldKey fldid, uint32_t len, uint64_t* buf)
	{
		return putchunk(key, fldid, len * 2, (uint32_t*)buf);
	}

	SoStatus SoStorageMemcached::put(ObjectKey key, FieldKey fldid, uint32_t v)
	{
		memcached_return rc = memcached_put(memc, MAKE64(key, fldid), &v, sizeof(v));
		if (rc == MEMCACHED_SUCCESS)
			return SoOK;
		return SoFail;
	}

	SoStatus SoStorageMemcached::put(ObjectKey key, FieldKey fldid, uint64_t v)
	{
		return putchunk(key, fldid, 2,(uint32_t*)&v);
	}
	uint32_t SoStorageMemcached::get(ObjectKey key, FieldKey fldid)
	{
		uint32_t ret;
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
			ret = *(uint32_t*)mret;
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