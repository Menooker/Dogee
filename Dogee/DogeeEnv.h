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

		class RemoteNodes
		{
		private:
			std::vector<uint64_t> connections;
		public:
			void PushConnection(uint64_t s)
			{
				connections.push_back(s);
			}
			uint64_t GetConnection(int node_id)
			{
				return connections[node_id];
			}
		};
		static RemoteNodes remote_nodes;
		static bool isMaster()
		{
			return _isMaster;
		}
		static void SetIsMaster(bool is_master)
		{
			_isMaster=is_master;
		}
		static void InitStorage(BackendType backty, CacheType cachety, std::vector<std::string>& arr_hosts, std::vector<int>& arr_ports);
		static void CloseStorage();
	};
}

#endif 