#include "DogeeBase.h"
#include "DogeeEnv.h"
#include <iostream>
#include <fstream>
#include <string>
#include "DogeeRemote.h"

#define DOGEE_CONFIG_VER 1

namespace Dogee
{
	ObjectKey objid=1;
	THREAD_LOCAL DObject* lastobject = nullptr;
	SoStorage* DogeeEnv::backend=nullptr;
	DSMCache* DogeeEnv::cache=nullptr;
	bool DogeeEnv::_isMaster = false;
	int DogeeEnv::self_node_id=-1;
	int DogeeEnv::num_nodes=0;
	DogeeEnv::InitStorageCurrentThreadProc DogeeEnv::InitStorageCurrentThread = nullptr;

	bool isMaster()
	{
		return DogeeEnv::isMaster();
	}

	static bool str_starts_with(std::string& str, const std::string& substr)
	{
		return !strncmp(str.c_str(), substr.c_str(), substr.length());
	}

	static int FindIndex(const std::vector<std::string>& arr, std::string& f)
	{
		int i = -1;
		for (auto itr : arr)
		{
			i++;
			if (f == itr)
			{
				return i;
			}
		}
		return -1;
	}

	static void MyAbort(std::string str)
	{
		std::cout << str;
		exit(3);
	}

#define MyAssert(a,str) do{if(!a){MyAbort(str);}}while(0)

	void HelperInitCluster(int argc, char* argv[])
	{
		if (argc == 3 && std::string(argv[1]) == "-s") //if slave
		{
			RcSlave(atoi(argv[2]));
			exit(0);
			return;
		}
		std::ifstream file("DogeeConfig.txt");
		std::string str;
		std::string err_msg;
		file >> str;
		MyAssert(str_starts_with(str, "DogeeConfigVer="), "No DogeeConfigVer\n");
		int ver;
		file >> ver;
		MyAssert((ver == DOGEE_CONFIG_VER), "Bad config version\n");

		file >> str;
		MyAssert(str_starts_with(str, "MasterPort="), "No MasterPort\n");
		int mport;
		file >> mport;

		file >> str;
		MyAssert(str_starts_with(str, "NumSlaves="), "No NumSlaves\n");
		int num_slaves;
		file >> num_slaves;

		file >> str;
		MyAssert(str_starts_with(str, "NumMemServers="), "No NumMemServers\n");
		int num_mem;
		file >> num_mem;

		file >> str;
		MyAssert(str_starts_with(str, "DSMBackend="), "No DSMBackend\n");
		file >> str;
		int back = FindIndex({ "BackendTest","ChunkMemcached","Memcached" }, str);
		MyAssert((back >= 0), "Bad DSMBackend Name:" + str + "\n");

		file >> str;
		MyAssert(str_starts_with(str, "DSMCache="), "No DSMCache\n");
		file >> str;
		int cache = FindIndex({ "NoCache","WriteThroughCache" }, str);
		MyAssert( (cache >= 0), "Bad DSMCache Name:" + str + "\n");

		file >> str;
		MyAssert(str_starts_with(str, "Slaves="), "No Slaves\n");
		std::vector<std::string> slaves;
		std::vector<int> ports;
		slaves.push_back("");
		ports.push_back(mport);
		int port;
		for (int i = 0; i < num_slaves; i++)
		{
			file >> str;
			file >> port;
			slaves.push_back(str);
			ports.push_back(port);
		}

		file >> str;
		MyAssert(str_starts_with(str, "MemServers="), "No MemServers\n");
		std::vector<std::string> mem;
		std::vector<int> mem_ports;
		for (int i = 0; i < num_mem; i++)
		{
			file >> str;
			file >> port;
			mem.push_back(str);
			mem_ports.push_back(port);
		}
		MyAssert((!RcMaster(slaves, ports, mem, mem_ports, (BackendType)back, (CacheType)cache)),"Master Init return non-zero\n");
		return;
	}

}
