#include "DogeeHelper.h"
#include <stdlib.h>
#include <thread>
#include <time.h>

#ifdef _MSC_VER
#ifdef _WIN64
ptDbgBreakPoint _DbgBreakPoint = (ptDbgBreakPoint)GetProcAddress(GetModuleHandleW(L"NTDLL.DLL"), "DbgBreakPoint");;
#endif
#endif

namespace Dogee
{

	void DogeeEnv::InitStorage(BackendType backty, CacheType cachety, std::vector<std::string>& arr_hosts, std::vector<int>& arr_ports)
	{
		SoStorageFactory factory(backty, cachety);
		backend = factory.make(arr_hosts, arr_ports);
		cache = factory.makecache(backend, arr_hosts, arr_ports);
		std::hash<std::thread::id> h;
		srand((int)time(NULL) ^ (int)h(std::this_thread::get_id()));
	}
	void DogeeEnv::CloseStorage()
	{
		delete cache;
		delete backend;
	}

}