#ifndef __DOGEE_HELPER_H_ 
#define __DOGEE_HELPER_H_ 

#include "DogeeStorage.h"
#ifdef DOGEE_USE_MEMCACHED
#include "DogeeMemcachedStorage.h"
#endif
#include "DogeeEnv.h"
#include "DogeeDirectoryCache.h"
#include "DogeeSocket.h"
#include "DogeeCheckpoint.h"
#include "DogeeNVDSStorage.h"
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
				DogeeEnv::DestroyStorageCurrentThread = SoStorageChunkMemcached::DestroyInCurrentThread;
				return new SoStorageChunkMemcached(arr_mem_hosts, arr_mem_ports);

			case SoBackendMemcached:
				DogeeEnv::InitStorageCurrentThread = SoStorageMemcached::InitInCurrentThread;
				DogeeEnv::DestroyStorageCurrentThread = SoStorageMemcached::DestroyInCurrentThread;
				return new SoStorageMemcached(arr_mem_hosts, arr_mem_ports);
#endif
			case SoBackendNVDS:
				DogeeEnv::InitStorageCurrentThread = SoStorageNVDS::InitInCurrentThread;
				DogeeEnv::DestroyStorageCurrentThread = SoStorageNVDS::DestroyInCurrentThread;
				return new SoStorageNVDS(arr_mem_hosts[0]);
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
	/*
	This function reads "DogeeConfig.txt" in the current directory and get the configurations. It then 
	initializes the cluster, connecting the slaves and the memory servers.
	param:
	- argc and argv : the argc and argv from the main() function. If the first parameter of the 
		startup arguments is "-s", HelperInitCluster will run in slave mode and will never return.
	- appname : the application name. Can be null.
	*/
	extern void HelperInitCluster(int argc, char* argv[], const char* appname = nullptr);

	/*
	This function acts as HelperInitCluster, but it also starts Checkpointing. You should specify the
	the checkpoint class in the template parameters. 
	*/
	template<typename MasterCheckPointType, typename SlaveCheckPointType>
	void HelperInitClusterCheckPoint(int argc, char* argv[], const char* appname = nullptr)
	{
		DogeeEnv::InitCheckpoint=InitCheckPoint<MasterCheckPointType, SlaveCheckPointType>;
		HelperInitCluster(argc, argv, appname);
	}

	/*
	Get string parameter in the startup arguments. We treat the startup arguments as Key-Value pairs.
	For example, for startup arguments "-a 123 -b 321", we have Key-Value pairs ("-a","123") and ("-b","321")
	Given the key, this function returns the value of it in the startup arguments.
	Param : str the "key"
	Returns : the "Value" in string
	*/
	extern std::string& HelperGetParam(const std::string& str);
	extern int HelperGetParamInt(const std::string& str);
	extern double HelperGetParamDouble(const std::string& str);

	/*
	Parse a CSV file, use the function "func" to look through the cells of the CSV file. It can skip to a specific line.
	It also can cache the file position so that we can skip to the line faster next time.
	Param : 
	- path : the path to the CSV file
	- func : the function that look through the CSV file. "cell" is the cell string. "line" is the line number(starts from 0),
		"index" is the colum number(starts from 0). 
	- skip_to_line : skip to a specific line, starts from 0
	- use_cache : cache to file position when searching a line
	*/
	extern void ParseCSV(const char* path, std::function<bool(const char* cell, int line, int index)> func, int skip_to_line=0, bool use_cache=true);

}

#endif