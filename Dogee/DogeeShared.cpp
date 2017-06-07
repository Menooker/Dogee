#include "DogeeBase.h"
#include "DogeeEnv.h"
#include <iostream>
#include <fstream>
#include <string>
#include "DogeeRemote.h"
#include "DogeeThreading.h"
#include <sstream>
#include <limits>
#include <mutex>
#include <set>

#define DOGEE_CONFIG_VER 1

namespace Dogee
{
	ObjectKey objid=1;
	FieldKey gloabl_fid = 0;
	THREAD_LOCAL DObject* lastobject = nullptr;
	SoStorage* DogeeEnv::backend=nullptr;
	DSMCache* DogeeEnv::cache=nullptr;
	bool DogeeEnv::_isMaster = false;
	int DogeeEnv::self_node_id=-1;
	int DogeeEnv::num_nodes=0;
	int DogeeEnv::ThreadPoolConfig::thread_pool_count = 0;
	DThreadPool* DogeeEnv::ThreadPoolConfig::thread_pool = nullptr;
	DThreadPoolScheduler* DogeeEnv::ThreadPoolConfig::scheduler = nullptr;
	int DogeeEnv::ThreadPoolConfig::thread_pool_max_wait = 256;

	DogeeEnv::InitStorageCurrentThreadProc DogeeEnv::InitStorageCurrentThread = nullptr;
	DogeeEnv::InitStorageCurrentThreadProc DogeeEnv::DestroyStorageCurrentThread = nullptr;
	DogeeEnv::InitStorageCurrentThreadProc DogeeEnv::DeleteCheckpoint = nullptr;
	DogeeEnv::InitStorageCurrentThreadProc DogeeEnv::InitCheckpoint = nullptr;
	void* DogeeEnv::checkboject = nullptr;
	std::string DogeeEnv::application_name;
	std::mutex object_list_lock;
	std::set<ObjectKey> object_list;

	int GetObjectNumber()
	{
		std::lock_guard<std::mutex> lock(object_list_lock);
		return object_list.size();
	}
	void DeleteObject(ObjectKey key)
	{
		std::unique_lock<std::mutex> lock(object_list_lock);
		object_list.erase(key);
	}

	void PushObject(ObjectKey key)
	{
		std::unique_lock<std::mutex> lock(object_list_lock);
		object_list.insert(key);
	}

	void ForEachObject(std::function<void(ObjectKey key)> func)
	{
		std::unique_lock<std::mutex> lock(object_list_lock);
		for(auto key:object_list)
			func(key);
	}

	ObjectKey AllocObjectId(uint32_t cls_id, uint32_t size)
	{
		ObjectKey key = 0;
		bool found = false;
		for (int i = 0; i<DOGEE_MAX_SHARED_KEY_TRIES; i++)
		{
			while (key == 0)
				key = rand();
			if (DogeeEnv::backend->newobj(key, cls_id, size) == SoOK)
			{
				PushObject(key);
				found = true;
				break;
			}
		}
		if (!found)
		{
			abort();
			//fix-me : raise some hard error here			
		}
		return key;
	}

	void DogeeEnv::InitCurrentThread()
	{
		if (current_thread_id == 0)
		{
			current_thread_id = AllocThreadId();
			RcPrepareNewThread();
			InitStorageCurrentThread();
		}

	}

	void  DogeeEnv::DestroyCurrentThread()
	{
		if (current_thread_id != 0)
		{
			RcDeleteThread();
			DestroyStorageCurrentThread();
			current_thread_id = 0;
		}

	}

	bool isMaster()
	{
		return DogeeEnv::isMaster();
	}

	static bool str_starts_with(std::string& str, const std::string& substr)
	{
		return !strncmp(str.c_str(), substr.c_str(), substr.length());
	}

	static std::vector<std::function<void()>>* map = nullptr;

	void RegisterConstInit(std::function<void()> func)
	{
		if (map == nullptr)
			map = new std::vector<std::function<void()>>;
		map->push_back(func);
	}

	void DeleteSharedConstInitializer()
	{
		if (map)
		{
			delete map;
			map = nullptr;
		}

	}

	void InitSharedConst()
	{
		if (map)
		{
			for (auto itr : *map)
			{
				itr();
			}
			delete map;
			map = nullptr;
		}

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
	std::unordered_map<std::string, std::string> param;
	void HelperInitCluster(int argc, char* argv[],const char* appname)
	{
		if (appname)
			DogeeEnv::application_name = appname;
		else
			DogeeEnv::application_name = "Unnamed";
		if (argc >= 3 && std::string(argv[1]) == "-s") //if slave
		{
			RcSlave(atoi(argv[2]));
			exit(0);
			return;
		}
		for (int i = 1; i + 1 < argc; i+=2)
		{
			param[argv[i]] = argv[i + 1];
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
		int back = FindIndex({ "BackendTest","ChunkMemcached","Memcached","NVDS" }, str);
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



	

	thread_proc slave_init_proc = nullptr;
	int SetSlaveInitProc(thread_proc p)
	{
		slave_init_proc = p;
		return 0;
	}

	std::string& HelperGetParam(const std::string&  str)
	{
		return param[str];
	}

	int HelperGetParamInt(const std::string& str)
	{
		return atoi(HelperGetParam(str).c_str());
	}

	double HelperGetParamDouble(const std::string& str)
	{
		return atof(HelperGetParam(str).c_str());
	}

	//http://stackoverflow.com/questions/1120140/how-can-i-read-and-parse-csv-files-in-c
	//Modified from Loki Astari 's answer
	//It is somehow too slow
	/*void ParseCSV(std::istream& str,std::function<bool(const std::string& cell,int line,int index)> func)
	{
		int line_n=0;
		while (str)
		{
			int cell_n=0;
			std::vector<std::string>   result;
			std::string                line;
			std::getline(str, line);

			std::stringstream          lineStream(line);
			std::string                cell;

			while (std::getline(lineStream, cell, ','))
			{
				if (!func(cell, line_n, cell_n))
					return;
				cell_n++;
			}
			// This checks for a trailing comma with no data after it.
			if (!lineStream && cell.empty())
			{
				// If there was a trailing comma then add an empty element.
				if(!func("", line_n, cell_n))
					return;
				cell_n++;
			}
			line_n++;
		}
	}*/

	void WriteFilePointerCache(std::istream::pos_type pos, const std::string path, int mark)
	{
		std::stringstream pathbuf;
		pathbuf << path << "." << mark << ".cache";
		std::fstream outfile(pathbuf.str(), std::ios::out);
		outfile << pos;
		outfile.close();
	}

	void WriteFilePointerCache(std::istream& fs, const std::string path,int mark)
	{
		std::stringstream pathbuf;
		pathbuf << path << "." << mark << ".cache";
		std::fstream outfile(pathbuf.str(), std::ios::out);
		outfile << fs.tellg();
		outfile.close();
	}
	bool RestoreFilePointerCache(std::istream& fs, const std::string path, int mark)
	{
		std::stringstream pathbuf;
		pathbuf << path << "." << mark << ".cache";
		std::fstream outfile(pathbuf.str(), std::ios::in);
		if (!outfile)
			return false;
		unsigned long f;
		outfile >> f;
		fs.seekg(f, std::istream::beg);
		outfile.close();
		return true;
	}


	void SkipFileToLine(std::istream& file, const std::string path, unsigned int num)
	{
		file.seekg(std::ios::beg);
		if (num==0)
			return;
		if (!RestoreFilePointerCache(file, path, num))
		{	
			std::cout << "Cache file: " << path << " for line " << num << "not found" << std::endl;
			for (unsigned i = 0; i < num; ++i){
				file.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
			}
			WriteFilePointerCache(file, path, num);
		}

	}

	void ParseCSV(const char* path, std::function<bool(const char* cell, int line, int index)> func, int skip_to_line, bool use_cache)
	{
		char buf[2048];
		int cnt = 0;
		int line = 0;
		buf[0] = 0;
		char* st;
		uint32_t lagacy = 0;
		char* last;

		FILE* f = (FILE*)fopen(path,"r");
		if (use_cache)
		{
			std::stringstream pathbuf;
			pathbuf << path <<"."<< skip_to_line << ".cache";
			std::fstream outfile(pathbuf.str(), std::ios::in);
			if (!outfile)
			{
				std::cout << "No line cache file\n";
			}
			else
			{
				long fp;
				outfile >> fp;
				std::cout << "Read cache file " << pathbuf.str() << "\nFile pointer is " << fp << std::endl;
				fseek(f, fp, SEEK_SET);
				line = skip_to_line;
			}
		}
		for (;;)
		{
			st = buf + lagacy;
			fgets(st, 2048 - lagacy, f);//str.getline(st, 512 - lagacy,0);//fgets(st, 2048 - lagacy, f);
			char *p = st;
			last = buf;
			while (*p)
			{
				if (*p == '\n')
				{
					if (use_cache && line + 1 == skip_to_line)
					{
						std::stringstream pathbuf;
						pathbuf << path <<"."<< skip_to_line << ".cache";
						std::fstream outfile(pathbuf.str(), std::ios::out);
						long fp = ftell(f) - (strlen(p) - 1);
						outfile << fp;
						std::cout << "Output cache file " << pathbuf.str() <<"\nFile pointer is "<< fp<< std::endl;
					}
					*p = 0;
					if (line >= skip_to_line && !func(last, line, cnt))
						return;
					line++;
					cnt = 0;
					last = p + 1;
					if (line % 100 == 0)
						printf("line %d\n", line);
				}
				else if (*p == ',')
				{
					*p = 0;
					if (line >= skip_to_line && !func(last, line, cnt))
						return;
					cnt++;
					last = p + 1;
				}
				p++;
			}
			if (p <= buf  )
			{
				break;
			}
			if (feof(f))
			{
				break;
			}
			lagacy = p  - last;
			memmove(buf, last, lagacy);
		}

		if (line >= skip_to_line)
			func(last, line, cnt);
		fclose(f);
	}

}
