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

	extern void* init_memcached_this_thread();
	extern bool isMaster();
	class SoStorageMemcached : public SoStorage
	{

	public:
		static memcached_st *main_memc;
		virtual SoStatus put(ObjectKey key, FieldKey fldid, uint64_t v);
		virtual uint64_t get(ObjectKey key, FieldKey fldid);
		~SoStorageMemcached();



		std::vector<std::string> mem_hosts;
		std::vector<int> mem_ports;

		SoStorageMemcached(std::vector<std::string>& arr_mem_hosts, std::vector<int>& arr_mem_ports)
		{
			mem_hosts = arr_mem_hosts;
			mem_ports = arr_mem_ports;
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
			for (unsigned i = 0; i < mem_hosts.size(); i++)
			{
				servers = memcached_server_list_append(servers, mem_hosts[i].c_str(), mem_ports[i], &rc);
			}

			rc = memcached_server_push(main_memc, servers);
			memcached_server_free(servers);
			if (isMaster())
				memcached_flush(main_memc, 0);
		}

	};
}
#endif