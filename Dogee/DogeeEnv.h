#ifndef __DOGEE_ENV_H_ 
#define __DOGEE_ENV_H_ 

#include "DogeeStorage.h"
#ifdef DOGEE_USE_MEMCACHED
#include "DogeeMemcachedStorage.h"
#endif
namespace Dogee
{

	enum BackendType
	{
		SoBackendTest,
		SoBackendChunkMemcached,
		SoBackendMemcached,
	};
	enum CacheType
	{
		SoNoCache,
		SoWriteThroughCache,
	};

	class SoStorageFactory
	{
	private:
		BackendType type;
		CacheType cachetype;
	public:
		SoStorageFactory(BackendType ty, CacheType cty)
		{
			setType(ty, cty);
		}
		SoStorageFactory* setType(BackendType ty, CacheType cty)
		{
			type = ty;
			cachetype = cty;
			return this;
		}
		SoStorage* make(std::vector<std::string>& arr_mem_hosts, std::vector<int>& arr_mem_ports)
		{
			switch (type)
			{
			case SoBackendTest:
				return new LocalStorage();
			//case SoBackendChunkMemcached:
			//	return new SoStorageChunkMemcached(arr_mem_hosts, arr_mem_ports);
#ifdef DOGEE_USE_MEMCACHED
			case SoBackendMemcached:
				return new SoStorageMemcached(arr_mem_hosts, arr_mem_ports);
#endif
			default:
				assert(0);
			}
			return NULL;
		}

		DSMCache* makecache(SoStorage* storage, std::vector<std::string>& arr_hosts, std::vector<int>& arr_ports)
		{
			switch (cachetype)
			{
			case SoNoCache:
				return new DSMNoCache(storage);
				break;
			//case SoWriteThroughCache:
				//return new DSMDirectoryCache(storage, arr_hosts, arr_ports, mcache_id, mcontrollisten, mdatalisten);
			default:
				assert(0);
			}
			return NULL;
		}

	};
	


	class DogeeEnv
	{
	private:
		static bool _isMaster;
	public:
		static SoStorage* backend;
		static DSMCache* cache;
		
		static bool isMaster()
		{
			return _isMaster;
		}
		static void SetIsMaster(bool is_master)
		{
			_isMaster=is_master;
		}
		static void InitStorage(BackendType backty,CacheType cachety,std::vector<std::string>& arr_hosts, std::vector<int>& arr_ports)
		{
			SoStorageFactory factory(backty, cachety);
			backend = factory.make(arr_hosts,arr_ports);
			cache = factory.makecache(backend,arr_hosts, arr_ports);
		}
		static void CloseStorage()
		{
			delete cache;
			delete backend;
		}
	};
}

#endif 