#ifndef __DOGEE_HELPER_H_ 
#define __DOGEE_HELPER_H_ 

#include "DogeeStorage.h"
#ifdef DOGEE_USE_MEMCACHED
#include "DogeeMemcachedStorage.h"
#endif
#include "DogeeEnv.h"

namespace Dogee
{


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
/*			case SoBackendTest:
				DogeeEnv::InitStorageCurrentThread = LocalStorage::InitInCurrentThread;
				return new LocalStorage();*/
				//case SoBackendChunkMemcached:
				//	return new SoStorageChunkMemcached(arr_mem_hosts, arr_mem_ports);
#ifdef DOGEE_USE_MEMCACHED
			case SoBackendMemcached:
				DogeeEnv::InitStorageCurrentThread = SoStorageMemcached::InitInCurrentThread;
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
}

#endif