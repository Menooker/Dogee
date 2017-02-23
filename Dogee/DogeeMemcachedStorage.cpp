#include "DogeeMemcachedStorage.h"
#include "DogeeUtil.h"
namespace Dogee
{
	THREAD_LOCAL memcached_st *memc = nullptr;
	memcached_st *main_memc = nullptr;


	void InitMemcachedStorage(std::vector<std::string>& arr_mem_hosts, std::vector<int>& arr_mem_ports)
	{
			memcached_return rc;
			memcached_server_st *servers;
			assert(!main_memc); //main_memc should be null, or memcached has been initialized
			main_memc = (memcached_st*)memcached_create(NULL);
			memc = main_memc;
			memcached_behavior_set(main_memc, MEMCACHED_BEHAVIOR_BINARY_PROTOCOL, 1);

#ifndef WIN32
			memcached_behavior_set(main_memc, MEMCACHED_BEHAVIOR_NO_BLOCK, 1);
			memcached_behavior_set(main_memc, MEMCACHED_BEHAVIOR_TCP_NODELAY, 1);
			memcached_behavior_set(main_memc, MEMCACHED_BEHAVIOR_NOREPLY, 1);
			//memcached_behavior_set((memcached_st*)_memc, MEMCACHED_BEHAVIOR_TCP_KEEPALIVE, 1);
#endif
			servers = NULL;
			for (unsigned i = 0; i < arr_mem_hosts.size(); i++)
			{
				servers = memcached_server_list_append(servers, arr_mem_hosts[i].c_str(), arr_mem_ports[i], &rc);
			}

			rc = memcached_server_push(main_memc, servers);
			memcached_server_free(servers);
			if (isMaster())
				memcached_flush(main_memc, 0);
		
	}

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
	SoStatus MemcachedGetChunk(LongKey k, uint32_t len, T* buf)
	{
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

	uint64_t SoStorageMemcached::getcounter(ObjectKey key, FieldKey fldid)
	{
		//char ch[17];
		//sprintf(ch,"%016llx",MAKE64(key,fldid));
		uint64_t k = MAKE64(key, fldid);
		size_t return_key_length = 8;
		size_t return_value_length;
		uint32_t flags;
		memcached_return rc;
		char* return_value = memcached_get(memc, (char*)&k, return_key_length, &return_value_length, &flags, &rc);
		if (rc != MEMCACHED_SUCCESS)
			throw 1;
		uint64_t ret = atoll(return_value);
		memcached_free2(return_value);
		return ret;
	}
	SoStatus SoStorageMemcached::setcounter(ObjectKey key, FieldKey fldid, uint64_t n)
	{
		//char ch[17];
		//sprintf(ch,"%016llx",MAKE64(key,fldid));
		uint64_t k = MAKE64(key, fldid);
		size_t return_key_length = 8;
		memcached_return rc;

		char ch2[65];
		sprintf(ch2, "%lu", n);
		rc = memcached_set(memc, (char*)&k, return_key_length, ch2, strlen(ch2), 0, 0);
		if (rc != MEMCACHED_SUCCESS)
			return SoFail;
		return SoOK;
	}

	uint64_t SoStorageMemcached::inc(ObjectKey key, FieldKey fldid, uint64_t inc)
	{
		//char ch[17];
		//sprintf(ch,"%016llx",MAKE64(key,fldid));
		uint64_t k = MAKE64(key, fldid);
		uint64_t ret;
		memcached_return rc;
		rc = memcached_increment(memc, (char*)&k, 8, inc, &ret);
		if (rc != MEMCACHED_SUCCESS)
			throw 1;
		return ret;

	}

	uint64_t SoStorageMemcached::dec(ObjectKey key, FieldKey fldid, uint64_t dec)
	{
		//char ch[17];
		//sprintf(ch,"%016llx",MAKE64(key,fldid));
		uint64_t k = MAKE64(key, fldid);
		uint64_t ret;
		memcached_return rc;
		rc = memcached_increment(memc, (char*)&k, 8, dec, &ret);
		if (rc != MEMCACHED_SUCCESS)
			throw 1;
		return ret;

	}

	SoStatus SoStorageMemcached::getchunk(ObjectKey key, FieldKey fldid, uint32_t len, uint32_t* buf)
	{
		return MemcachedGetChunk(MAKE64(key, fldid), len, buf);
	}
	SoStatus SoStorageMemcached::getchunk(ObjectKey key, FieldKey fldid, uint32_t len, uint64_t* buf)
	{
		return getchunk(key, fldid, len * 2, (uint32_t*)buf);
	}
	SoStatus SoStorageMemcached::getblock(LongKey id, uint32_t* buf)
	{
		return MemcachedGetChunk(id, DSM_CACHE_BLOCK_SIZE, buf);
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
			printf("Memc Error");
			ret = 0;
			//throw 1;
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
		if (main_memc)
			memc = memcached_clone(memc, main_memc);
		else
			return NULL;
		return memc;
	}

	/////////////////////////////////////////////////////////////////
	//SoStorageChunkMemcached
	/////////////////////////////////////////////////////////////////
	SoStatus SoStorageChunkMemcached::put(ObjectKey key, FieldKey fldid, uint32_t v)
	{
		return putchunk(key, fldid, 1, &v);
	}

	SoStatus SoStorageChunkMemcached::put(ObjectKey key, FieldKey fldid, uint64_t v)
	{
		return putchunk(key, fldid, 2, (uint32_t*)&v);
	}

	uint32_t SoStorageChunkMemcached::get(ObjectKey key, FieldKey fldid)
	{
		LongKey k = MAKE64(key, fldid);
		uint32_t buf[DSM_CACHE_BLOCK_SIZE];
		getblock(k, buf);
		return buf[k & DSM_CACHE_LOW_MASK_64];
	}



	static SoStatus fetchchunk2(LongKey k, uint32_t len)
	{
		uint32_t true_len = len / DSM_CACHE_BLOCK_SIZE + ((len%DSM_CACHE_BLOCK_SIZE) ? 1 : 0);
		typedef char* _PCHAR;
		char** keys = new _PCHAR[true_len];
		size_t* key_length = new size_t[true_len];
		LongKey* maddr = new LongKey[true_len];

		for (uint32_t i = 0; i<true_len; i++)
		{
			maddr[i] = k + i;
			keys[i] = (char*)&maddr[i];
			key_length[i] = 8;
		}

		memcached_return rc = memcached_mget(memc, (char**)keys, key_length, true_len);
		delete[]keys;
		delete[]key_length;
		delete[]maddr;
		return (rc == MEMCACHED_SUCCESS) ? SoOK : SoFail;
	}


	SoStatus SoStorageChunkMemcached::putchunk(ObjectKey key, FieldKey fldid, uint32_t len, uint64_t* buf)
	{
		return putchunk(key, fldid, len * 2, (uint32_t*)buf);
	}

	SoStatus SoStorageChunkMemcached::putchunk(ObjectKey key, FieldKey fldid, uint32_t len, uint32_t* buf)
	{
		uint32_t k = fldid;
		uint32_t k_start, k_end, k_tail;

		k_tail = k + len;
		if (k & DSM_CACHE_LOW_MASK)
			k_start = (k & DSM_CACHE_HIGH_MASK) + DSM_CACHE_BLOCK_SIZE;
		else
			k_start = k;

		k_end = k_tail & DSM_CACHE_HIGH_MASK;
		SoStatus ret = SoOK;
		uint32_t var[DSM_CACHE_BLOCK_SIZE];
		uint32_t idx = 0;
		uint32_t i = k;

		if (k<k_start)
		{
			getblock(MAKE64(key, k), var);
			for (i = k; i<k_start && i<k_tail; i++)
			{
				var[i & DSM_CACHE_LOW_MASK] = buf[idx];
				idx++;
			}
			memcached_return rc = memcached_put(memc, MAKE64(key, k / DSM_CACHE_BLOCK_SIZE), var, sizeof(uint32_t)*DSM_CACHE_BLOCK_SIZE);
			if (rc != MEMCACHED_SUCCESS)
				ret = SoFail;
		}


		for (i = k_start; i<k_end; i += DSM_CACHE_BLOCK_SIZE)
		{
			memcached_return rc = memcached_put(memc, MAKE64(key, i / DSM_CACHE_BLOCK_SIZE), buf + idx, sizeof(uint32_t)*DSM_CACHE_BLOCK_SIZE);
			if (rc != MEMCACHED_SUCCESS)
				ret = SoFail;
			idx += DSM_CACHE_BLOCK_SIZE;
		}
		if (idx<len && k_end != k_tail)
		{
			int kkk = k_end / DSM_CACHE_BLOCK_SIZE;
			getblock(MAKE64(key, k_end), var);
			for (; i<k_tail; i++)
			{
				var[i & DSM_CACHE_LOW_MASK] = buf[idx];
				idx++;
			}
			memcached_return rc = memcached_put(memc, MAKE64(key, k_end / DSM_CACHE_BLOCK_SIZE), var, sizeof(uint32_t)*DSM_CACHE_BLOCK_SIZE);
			if (rc != MEMCACHED_SUCCESS)
				ret = SoFail;
		}
		return ret;
	}



	SoStatus do_getchunk(ObjectKey key, FieldKey fldid, uint32_t len, uint32_t* buf)
	{
		LongKey k = MAKE64(key, fldid / DSM_CACHE_BLOCK_SIZE);
		memcached_result_st results_obj;
		memcached_result_st* results;
		results = memcached_result_create(memc, &results_obj);
		memset(buf, 0, sizeof(uint32_t)*len);

		uint32_t offset = fldid % DSM_CACHE_BLOCK_SIZE;
		memcached_return rc;
		SoStatus ret = fetchchunk2(k, len + offset);
		while ((results = memcached_fetch_result(memc, &results_obj, &rc)))
		{
			if (rc == MEMCACHED_SUCCESS)
			{
				LongKey* pkey;
				uint32_t* pvalue;
				pkey = (LongKey*)memcached_result_key_value(results);
				pvalue = (uint32_t*)memcached_result_value(results);
				uint32_t idx = (*pkey) - k;
				uint32_t i = (idx == 0) ? offset : 0;
				uint32_t j = idx*DSM_CACHE_BLOCK_SIZE + i - offset;
				for (; i<DSM_CACHE_BLOCK_SIZE, j<len; i++, j++)
				{
					buf[j] = pvalue[i];
				}
			}
		}
		memcached_result_free(&results_obj);
		return ret;
	}
	

	SoStatus splited_getchunk(ObjectKey key, FieldKey fldid, uint32_t len, uint32_t* buf)
	{
		uint32_t l = fldid;
		if (len >= 15040)
		{
			uint32_t limit = fldid + len - 15040;
			for (; l<limit; l += 15040)
			{
				if (do_getchunk(key, l, 15040, buf + (l - fldid)) != SoOK)
					return SoFail;
			}
		}
		uint32_t remain = fldid + len - l;
		if (remain)
			return do_getchunk(key, l, remain, buf + (l - fldid));
		else
			return SoOK;
	}

	SoStatus SoStorageChunkMemcached::getchunk(ObjectKey key, FieldKey fldid, uint32_t len, uint32_t* buf)
	{
		return splited_getchunk(key, fldid, len, buf);
	}
	SoStatus SoStorageChunkMemcached::getchunk(ObjectKey key, FieldKey fldid, uint32_t len, uint64_t* buf)
	{
		return splited_getchunk(key, fldid, len*2, (uint32_t*)buf);
	}



	SoStatus SoStorageChunkMemcached::getblock(LongKey addr, uint32_t* buf)
	{
		LongKey k = (addr & 0xffffffff00000000) | ((addr & 0xffffffff) / DSM_CACHE_BLOCK_SIZE);
		memcached_result_st results_obj;
		memcached_result_st* results;
		results = memcached_result_create(memc, &results_obj);
		memset(buf, 0, sizeof(uint32_t)*DSM_CACHE_BLOCK_SIZE);

		memcached_return rc;
		SoStatus ret = fetchchunk2(k, DSM_CACHE_BLOCK_SIZE);
		while ((results = memcached_fetch_result(memc, &results_obj, &rc)))
		{
			if (rc == MEMCACHED_SUCCESS)
			{
				LongKey* pkey;
				uint32_t* pvalue;
				pkey = (LongKey*)memcached_result_key_value(results);
				pvalue = (uint32_t*)memcached_result_value(results);
				for (uint32_t i = 0; i<DSM_CACHE_BLOCK_SIZE; i++)
				{
					buf[i] = pvalue[i];
				}
			}
		}
		memcached_result_free(&results_obj);
		return ret;
	}



}