#ifndef __DOGEE_MEMC_H_ 
#define __DOGEE_MEMC_H_ 

#include "DogeeStorage.h"
#include <libmemcached/memcached.h>
#include <vector>
#include <string>
#include <assert.h>
//#include "DogeeEnv.h"
#ifdef __GNUC__
#define memcached_free2(a) free(a)
#endif
namespace Dogee
{
	extern THREAD_LOCAL memcached_st *memc;
	extern memcached_st *main_memc;
	extern void* init_memcached_this_thread();
	extern bool isMaster();
	extern void InitMemcachedStorage(std::vector<std::string>& arr_mem_hosts, std::vector<int>& arr_mem_ports);
	class SoStorageMemcached : public SoStorage
	{

	public:
		static void InitInCurrentThread()
		{
			init_memcached_this_thread();
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

		~SoStorageMemcached(){}



		std::vector<std::string> mem_hosts;
		std::vector<int> mem_ports;

		SoStorageMemcached(std::vector<std::string>& arr_mem_hosts, std::vector<int>& arr_mem_ports)
		{
			mem_hosts = arr_mem_hosts;
			mem_ports = arr_mem_ports;
			InitMemcachedStorage(arr_mem_hosts, arr_mem_ports);
		}

	};

	class SoStorageChunkMemcached : public SoStorageMemcached
	{

	public:
		static void InitInCurrentThread()
		{
			init_memcached_this_thread();
		}
		virtual SoStatus put(ObjectKey key, FieldKey fldid, uint64_t v);
		virtual SoStatus put(ObjectKey key, FieldKey fldid, uint32_t v);
		virtual uint32_t get(ObjectKey key, FieldKey fldid);

		SoStatus del(ObjectKey key);
		SoStatus getchunk(ObjectKey key, FieldKey fldid, uint32_t len, uint32_t* buf);
		SoStatus getchunk(ObjectKey key, FieldKey fldid, uint32_t len, uint64_t* buf);
		SoStatus putchunk(ObjectKey key, FieldKey fldid, uint32_t len, uint64_t* buf);
		SoStatus putchunk(ObjectKey key, FieldKey fldid, uint32_t len, uint32_t* buf);
		SoStatus getblock(LongKey id, uint32_t* buf);
		~SoStorageChunkMemcached();


		SoStorageChunkMemcached(std::vector<std::string>& arr_mem_hosts, std::vector<int>& arr_mem_ports) :SoStorageMemcached(arr_mem_hosts,arr_mem_ports)
		{
		}

	};

}
#endif