#include "DogeeUtil.h"
#include "DogeeHelper.h"
#include <stdlib.h>
#include <thread>
#include <time.h>

#ifdef _MSC_VER
#ifdef _WIN64
Dogee::ptDbgBreakPoint Dogee::_DbgBreakPoint = (Dogee::ptDbgBreakPoint)GetProcAddress(GetModuleHandleW(L"NTDLL.DLL"), "DbgBreakPoint");
#endif
#endif

namespace Dogee
{

	extern void RcInitThreadSystem();
	extern void RcFinalizeThreadSystem();
	void DogeeEnv::InitStorage(BackendType backty, CacheType cachety, std::vector<std::string>& hosts, std::vector<int>& ports,
		std::vector<std::string>& mem_hosts, std::vector<int>& mem_ports, int node_id)
	{
		SoStorageFactory factory(backty, cachety);
		backend = factory.make(mem_hosts, mem_ports);
		cache = factory.makecache(backend, hosts, ports, node_id);
		std::hash<std::thread::id> h;
		srand((int)time(NULL) ^ (int)h(std::this_thread::get_id()));
		RcInitThreadSystem();
	}
	void DogeeEnv::CloseStorage()
	{
		RcFinalizeThreadSystem();
		delete cache;
		delete backend;
	}

}