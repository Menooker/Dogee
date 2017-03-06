#ifndef __DOGEE_HELPER_H_ 
#define __DOGEE_HELPER_H_ 

#include "DogeeStorage.h"
#ifdef DOGEE_USE_MEMCACHED
#include "DogeeMemcachedStorage.h"
#endif
#include "DogeeEnv.h"
#include "DogeeDirectoryCache.h"
#include "DogeeSocket.h"
#include <istream>
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
			/*case SoBackendTest:
				DogeeEnv::InitStorageCurrentThread = LocalStorage::InitInCurrentThread;
				return new LocalStorage();*/
#ifdef DOGEE_USE_MEMCACHED
			case SoBackendChunkMemcached:
				DogeeEnv::InitStorageCurrentThread = SoStorageChunkMemcached::InitInCurrentThread;
				return new SoStorageChunkMemcached(arr_mem_hosts, arr_mem_ports);

			case SoBackendMemcached:
				DogeeEnv::InitStorageCurrentThread = SoStorageMemcached::InitInCurrentThread;
				return new SoStorageMemcached(arr_mem_hosts, arr_mem_ports);
#endif
			default:
				assert(0);
			}
			return NULL;
		}

		DSMCache* makecache(SoStorage* storage, std::vector<std::string>& arr_hosts, std::vector<int>& arr_ports,int node_id)
		{
			switch (cachetype)
			{
			case SoNoCache:
				return new DSMNoCache(storage);
				break;
			case SoWriteThroughCache:
				SOCKET controllisten, datalisten;
				controllisten = Socket::RcCreateListen(arr_ports[node_id] + 1);
				if (!controllisten)
					abort();
				datalisten = Socket::RcCreateListen(arr_ports[node_id] + 2);
				if (!datalisten)
					abort();
				return new DSMDirectoryCache(storage, arr_hosts, arr_ports, node_id, controllisten, datalisten);
			default:
				assert(0);
			}
			return NULL;
		}

	};

	extern void HelperInitCluster(int argc, char* argv[]);
	extern std::string& HelperGetParam(const std::string& str);
	extern int HelperGetParamInt(const std::string& str);
	extern double HelperGetParamDouble(const std::string& str);
	extern void ParseCSV(const char* path, std::function<bool(const char* cell, int line, int index)> func, int skip_to_line=0, bool use_cache=true);

}

#endif