#ifndef __DOGEE_REMOTE_H_ 
#define __DOGEE_REMOTE_H_ 

#include "Dogee.h"
#include <vector>
#include <string>
#include "DogeeEnv.h"

namespace Dogee
{
	struct RcCommandPack
	{
		uint32_t cmd;
		int32_t param;
		int32_t param2;
		union
		{
			struct
			{
				int32_t param3;
				int32_t param4;
			};
			uint64_t param34;
		};
	};
	struct SyncThreadNode
	{
		int machine;
		int thread_id;
	};

	/*
	Connect to the Slaves and initialize the cluster. Set isMaster=true. 
	hosts[0] is ignored, ports[0] is the master nodes's listening port
	*/
	int RcMaster(std::vector<std::string>& hosts, std::vector<int>& ports,
		std::vector<std::string>& memhosts, std::vector<int>& memports,
		BackendType backty, CacheType cachety);

	/*
	Called by slave nodes, will not return until the master close the cluster. Set isMaster=false.
	*/
	void RcSlave(int SlavePort);

	/*
	Can be called by the master node. Create a thread on "node_id", with function index "idx"
	*/
	int RcCreateThread(int node_id, uint32_t idx, uint32_t param, ObjectKey okey);

	/*
	Can be called by the master node. Close the whole cluster.
	*/
	void CloseCluster();
}

#endif