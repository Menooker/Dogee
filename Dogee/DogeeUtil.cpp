#include "DogeeUtil.h"
#include <stdlib.h>
#include <thread>
#include <time.h>

namespace Dogee
{

	void DogeeEnv::InitStorage(BackendType backty, CacheType cachety, std::vector<std::string>& arr_hosts, std::vector<int>& arr_ports)
	{
		SoStorageFactory factory(backty, cachety);
		backend = factory.make(arr_hosts, arr_ports);
		cache = factory.makecache(backend, arr_hosts, arr_ports);
		srand((int)time(NULL) ^ (int)std::this_thread::get_id().hash());
	}
	void DogeeEnv::CloseStorage()
	{
		delete cache;
		delete backend;
	}

}