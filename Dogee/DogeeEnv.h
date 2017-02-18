#ifndef __DOGEE_ENV_H_ 
#define __DOGEE_ENV_H_ 

#include "Dogee.h"
#include <vector>
#include <string>

namespace Dogee
{
	class SoStorage;
	class DSMCache;
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
	class DogeeEnv
	{
	private:
		static bool _isMaster;
	public:
		static SoStorage* backend;
		static DSMCache* cache;
		static int self_node_id;
		static int num_nodes;
		typedef void(*InitStorageCurrentThreadProc)();
		static InitStorageCurrentThreadProc InitStorageCurrentThread;

		static bool isMaster()
		{
			return _isMaster;
		}
		static void SetIsMaster(bool is_master)
		{
			_isMaster=is_master;
		}
		static void InitStorage(BackendType backty, CacheType cachety, std::vector<std::string>& arr_hosts, std::vector<int>& arr_ports,int node_id);
		static void CloseStorage();
	};
}

#endif 