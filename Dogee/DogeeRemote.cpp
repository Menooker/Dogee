#include "DogeeRemote.h"
#include "DogeeEnv.h"
#include <thread>
#include <chrono>


#ifdef _WIN32
#include <winsock.h>
int RcWinsockStartup()
{
	WORD sockVersion = MAKEWORD(2, 2);
	WSADATA wsaData;
	if (WSAStartup(sockVersion, &wsaData) != 0)
	{
		return 1;
	}
	return 0;
}
//called when initialized
int RcStartipRet = RcWinsockStartup();
#pragma comment(lib, "WS2_32")
#define RcSocketLastError() WSAGetLastError()
#else
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>
#include <sys/unistd.h>
#define SOCKET int
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define closesocket close
typedef struct sockaddr* LPSOCKADDR;
#define RcSocketLastError() errno
#endif

#define RC_MAGIC_MASTER 0x12345edf
#define RC_MAGIC_SLAVE 0x33450f0e


enum RcCommand
{
	RcCmdClose = 1,
	RcCmdCreateThread,
	RcCmdSuspendThread,
	RcCmdStopThread,
	RcCmdTriggerGC,
	RcCmdDoGC,
	RcCmdDoneGC,
	RcCmdWakeSync,
	RcCmdEnterBarrier,
	RcCmdEnterSemaphore,
	RcCmdLeaveSemaphore,
};

enum RcDataCommand
{
	RcDataHello = 1,
	RcDataAccumulate,
	RcDataAccumulatePartialDone,
};


#pragma pack(push)
#pragma pack(4)

struct MasterInfo
{
	int32_t magic;
	uint32_t num_mem_server;
	uint32_t num_nodes;
	uint32_t node_id;
	uint32_t localport;
	Dogee::BackendType backty;
	Dogee::CacheType cachety;
};

struct SlaveInfo
{
	int32_t magic;
};

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
struct RcDataPack
{
	uint32_t cmd;
	uint32_t size;
	uint64_t id;
	uint32_t datatype;
	int32_t param0;
	union
	{
		struct
		{
			int32_t param1;
			int32_t param2;
		};
		uint64_t param12;
	};
	char buf[0];
};

#pragma pack(pop)

namespace Dogee
{
	namespace Socket
	{
		SOCKET RcCreateListen(int port)
		{
			SOCKET slisten = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			int reuse = 1;
			if (setsockopt(slisten, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) < 0)
				perror("setsockopt(SO_REUSEADDR) failed");

#ifdef SO_REUSEPORT
			reuse = 1;
			if (setsockopt(slisten, SOL_SOCKET, SO_REUSEPORT, (const char*)&reuse, sizeof(reuse)) < 0)
				perror("setsockopt(SO_REUSEPORT) failed");
#endif
			if (slisten == INVALID_SOCKET)
			{
				printf("socket error ! \n");
				return 0;
			}

			//绑定IP和端口
			sockaddr_in sin;
			memset(&sin, 0, sizeof(sin));
			sin.sin_family = AF_INET;
			sin.sin_port = htons(port);
#ifdef _WIN32
			sin.sin_addr.S_un.S_addr = INADDR_ANY;
#else
			sin.sin_addr.s_addr = INADDR_ANY;
#endif
			if (bind(slisten, (LPSOCKADDR)&sin, sizeof(sin)) == SOCKET_ERROR)
			{
				printf("bind error !");
				return 0;
			}

			//开始监听
			if (listen(slisten, 5) == SOCKET_ERROR)
			{
				printf("listen error !");
				return 0;
			}
			return (SOCKET)slisten;
		}
		void RcSetTCPNoDelay(SOCKET fd)
		{
			int enable = 1;
			setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (const char*)&enable, sizeof(enable));
		}

		SOCKET RcConnect(char* ip, int port)
		{
			SOCKET sclient = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			if (sclient == INVALID_SOCKET)
			{
				return NULL;
			}

			sockaddr_in serAddr;
			memset(&serAddr, 0, sizeof(serAddr));
			serAddr.sin_family = AF_INET;
			serAddr.sin_port = htons(port);
#ifdef _WIN32
			serAddr.sin_addr.S_un.S_addr = inet_addr(ip);
#else
			serAddr.sin_addr.s_addr = inet_addr(ip);
#endif
			if (connect(sclient, (sockaddr *)&serAddr, sizeof(serAddr)) == SOCKET_ERROR)
			{
				closesocket(sclient);
				return NULL;
			}
			return (SOCKET)sclient;
		}

		SOCKET RcCreateListen(int port)
		{
			SOCKET slisten = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			int reuse = 1;
			if (setsockopt(slisten, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) < 0)
				perror("setsockopt(SO_REUSEADDR) failed");

#ifdef SO_REUSEPORT
			reuse = 1;
			if (setsockopt(slisten, SOL_SOCKET, SO_REUSEPORT, (const char*)&reuse, sizeof(reuse)) < 0)
				perror("setsockopt(SO_REUSEPORT) failed");
#endif
			if (slisten == INVALID_SOCKET)
			{
				printf("socket error ! \n");
				return 0;
			}

			//绑定IP和端口
			sockaddr_in sin;
			memset(&sin, 0, sizeof(sin));
			sin.sin_family = AF_INET;
			sin.sin_port = htons(port);
#ifdef _WIN32
			sin.sin_addr.S_un.S_addr = INADDR_ANY;
#else
			sin.sin_addr.s_addr = INADDR_ANY;
#endif
			if (bind(slisten, (LPSOCKADDR)&sin, sizeof(sin)) == SOCKET_ERROR)
			{
				printf("bind error !");
				return 0;
			}

			//开始监听
			if (listen(slisten, 5) == SOCKET_ERROR)
			{
				printf("listen error !");
				return 0;
			}
			return (SOCKET)slisten;
		}

		SOCKET RcListen(int port)
		{
			SOCKET slisten = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			int reuse = 1;
			if (setsockopt(slisten, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) < 0)
				perror("setsockopt(SO_REUSEADDR) failed");

#ifdef SO_REUSEPORT
			reuse = 1;
			if (setsockopt(slisten, SOL_SOCKET, SO_REUSEPORT, (const char*)&reuse, sizeof(reuse)) < 0)
				perror("setsockopt(SO_REUSEPORT) failed");
#endif
			if (slisten == INVALID_SOCKET)
			{
				printf("socket error ! \n");
				return 0;
			}

			//绑定IP和端口
			sockaddr_in sin;
			memset(&sin, 0, sizeof(sin));
			sin.sin_family = AF_INET;
			sin.sin_port = htons(port);
#ifdef _WIN32
			sin.sin_addr.S_un.S_addr = INADDR_ANY;
#else
			sin.sin_addr.s_addr = INADDR_ANY;
#endif
			if (bind(slisten, (LPSOCKADDR)&sin, sizeof(sin)) == SOCKET_ERROR)
			{
				printf("bind error !");
				return 0;
			}

			//开始监听
			if (listen(slisten, 5) == SOCKET_ERROR)
			{
				printf("listen error !");
				return 0;
			}

			//循环接收数据
			sockaddr_in remoteAddr;
#ifdef _WIN32
			int nAddrlen = sizeof(remoteAddr);
#else
			unsigned int nAddrlen = sizeof(remoteAddr);
#endif
			printf("port %d waiting for connections...\n", port);
			SOCKET sClient = accept(slisten, (LPSOCKADDR)&remoteAddr, &nAddrlen);
			if (sClient == INVALID_SOCKET)
			{
				printf("accept error !");
				return 0;
			}
			printf("port %d accepted ：%s \n", port, inet_ntoa(remoteAddr.sin_addr));
			closesocket(slisten);
			return (SOCKET)sClient;
		}

		SOCKET RcAccept(SOCKET slisten)
		{
			sockaddr_in remoteAddr;
#ifdef _WIN32
			int nAddrlen = sizeof(remoteAddr);
#else
			unsigned int nAddrlen = sizeof(remoteAddr);
#endif
			SOCKET sClient = accept(slisten, (LPSOCKADDR)&remoteAddr, &nAddrlen);
			if (sClient == INVALID_SOCKET)
			{
				printf("accept error !");
				return 0;
			}
			return (SOCKET)sClient;
		}

		inline int RcSend(SOCKET s, void* data, size_t len)
		{
			return send((SOCKET)s, (char*)data, len, 0);
		}

		inline int RcRecv(SOCKET s, void* data, size_t len)
		{
			return recv((SOCKET)s, (char*)data, len, 0);
		}

		inline int RcCloseSocket(SOCKET s)
		{
			return closesocket((SOCKET)s);
		}
		void get_peer_ip_port(SOCKET fd, std::string& ip, int& port)
		{

			// discovery client information
			struct sockaddr_in addr;
#ifdef BD_ON_WINDOWS
			int addrlen = sizeof(addr);
#else
			size_t addrlen = sizeof(addr);
#endif
			if (getpeername((SOCKET)fd, (struct sockaddr*)&addr, &addrlen) == -1){
				fprintf(stderr, "discovery client information failed, fd=%d, errno=%d(%#x).\n", fd, errno, errno);
				return;
			}
			port = ntohs(addr.sin_port);
			ip = inet_ntoa(addr.sin_addr);

			return;
		}
		SOCKET RcTryConnect(char* ip, int port, int node_id)
		{
			SOCKET ret;
			for (int i = 0; i<3; i++)
			{
				ret = RcConnect(ip, port);
				if (ret)
					break;
				std::this_thread::sleep_for(std::chrono::milliseconds(500));
			}
			if (!ret)
				return 0;
			RcCommandPack cmd;
			cmd.cmd = RcDataHello;
			cmd.param = node_id;
			RcSend(ret, &cmd, sizeof(cmd));
			return ret;
		}

	}



	using namespace Socket;


	void RcSlaveMainLoop(char* path, SOCKET s, std::vector<std::string>& hosts, std::vector<int>& ports,
		std::vector<std::string>& mem_hosts, std::vector<int>& mem_ports, int node_id,
		BackendType backty, CacheType cachety)
	{
		DogeeEnv::InitStorage(backty, cachety, mem_hosts, mem_ports);
		for (;;)
		{
			RcCommandPack cmd;
			int ret = RcRecv(s, &cmd, sizeof(cmd));
			if (ret != sizeof(cmd))
			{
				printf("Socket error!\n");
				break;
			}
			switch (cmd.cmd)
			{
			case RcCmdClose:
				printf("Closing!\n");
				goto CLOSE;
				break;
			case RcCmdCreateThread:

				break;
			case RcCmdWakeSync:
				//RcDoWakeThread(cmd.param34);
				break;
			default:
				printf("Unknown command %d\n", cmd.cmd);
			}
		}
		CLOSE:
		DogeeEnv::CloseStorage(); //*/
		return;
	}


	void RcSlave(int port)
	{
		printf("port %d waiting for connections...\n", port);
		SOCKET slisten = RcCreateListen(port);
		SOCKET s = RcAccept(slisten);
		printf("Waiting for hand shaking...\n");
		MasterInfo mi;
		SlaveInfo si;
		si.magic = RC_MAGIC_SLAVE;
		RcSend(s, &si, sizeof(si));
		int cnt = RcRecv(s, &mi, sizeof(mi));
		int err = 0, err2 = 0;
		if (cnt == sizeof(mi) && mi.magic == RC_MAGIC_MASTER)
		{
			char path[255];
			char mainmod[255];

			DogeeEnv::self_node_id = mi.node_id;
			DogeeEnv::num_nodes = mi.num_nodes;
			//fix-me : create listening thread
			/*
			MasterDataParam* para = new MasterDataParam;
			para->port = slisten;
			UaInitLock(&master_data_lock);
			MasterDataThread = UaCreateThreadEx(RcMasterData, para); //fix-me : Remember to close the thread
			*/
			char buf[255];

			//printf("Host list :\n");
			std::vector<std::string> hosts;
			std::vector<int> ports;

			std::string master;
			int masterport;
			get_peer_ip_port(s, master, masterport);

			hosts.push_back(master);
			ports.push_back(mi.localport);
			//printf("Master = %s:%d\n",master.c_str(),mi.localport);

			for (int i = 1; i<mi.num_nodes; i++)
			{
				uint32_t len, port;
				err = 5;
				if (RcRecv(s, &len, sizeof(len)) != sizeof(len))
					goto ERR;
				if (len>255)
				{
					err = 6;
					goto ERR;
				}
				if (RcRecv(s, buf, len) != len)
				{
					err = 7;
					goto ERR;
				}
				if (RcRecv(s, &port, sizeof(port)) != sizeof(port))
				{
					err = 8;
					goto ERR;
				}
				hosts.push_back(std::string(buf));
				ports.push_back(port);
				//printf("%s:%d\n",buf,port);
			}

			//printf("Memory server list :\n");
			std::vector<std::string> memhosts;
			std::vector<int> memports;
			for (int i = 0; i<mi.num_mem_server; i++)
			{
				uint32_t len, port;
				err = 5;
				if (RcRecv(s, &len, sizeof(len)) != sizeof(len))
					goto ERR;
				if (len>255)
				{
					err = 6;
					goto ERR;
				}
				if (RcRecv(s, buf, len) != len)
				{
					err = 7;
					goto ERR;
				}
				if (RcRecv(s, &port, sizeof(port)) != sizeof(port))
				{
					err = 8;
					goto ERR;
				}
				memhosts.push_back(std::string(buf));
				memports.push_back(port);
				//printf("%s:%d\n",buf,port);
			}

			/*
			direct_sockets[mi.node_id - 1] = RcTryConnect((char*)hosts[0].c_str(), ports[0], mi.node_id);
			if (!direct_sockets[mi.node_id - 1])
			{
				printf("Failed to directly connect to master node\n");
				goto ERR;
			}
			for (int i = 1; i<mi.node_id; i++)
			{
				direct_sockets[i - 1] = RcTryConnect((char*)hosts[i].c_str(), ports[i], mi.node_id);
				if (!direct_sockets[i - 1])
				{
					printf("Failed to directly connect to node %d\n", i);
					goto ERR;
				}
			}
			masterdatanode = direct_sockets[mi.node_id - 1];*/
			DogeeEnv::SetIsMaster(false);
			RcSlaveMainLoop(mainmod, s, hosts, ports, memhosts, memports, mi.node_id,mi.backty,mi.cachety);
		}
		else
		{
		ERR:
			printf("Hand shaking error! %d %d\n", err, err2);

		}
		RcCloseSocket(s);
		/*RcCloseDirectLinks();
		UaStopThread(MasterDataThread);
		UaCloseThread(MasterDataThread);
		UaKillLock(&master_data_lock);
		MasterDataThread = NULL;*/
	}



	int RcMasterHello(SOCKET s, std::vector<std::string>& hosts, std::vector<int>& ports,
		std::vector<std::string>& memhosts, std::vector<int>& memports, int node_id,
		BackendType backty, CacheType cachety)
	{
		SlaveInfo si;
		int mem_cnt;
		mem_cnt = memhosts.size();
		int host_cnt = hosts.size();
		if (RcRecv(s, &si, sizeof(si)) != sizeof(si))
		{
			return 1;
		}
		if (si.magic != RC_MAGIC_SLAVE)
		{
			return 2;
		}
		MasterInfo mi = { RC_MAGIC_MASTER, mem_cnt, host_cnt, node_id, ports[0] ,backty,  cachety };
		RcSend(s, &mi, sizeof(mi));

		for (int i = 1; i<host_cnt; i++)
		{

			uint32_t sendl = hosts[i].size() + 1;
			if (sendl>255)
			{
				sendl = 255;
				hosts[i][254] = 0;
			}
			RcSend(s, &sendl, sizeof(sendl));
			RcSend(s, (char*)hosts[i].c_str(), sendl);
			sendl = ports[i];
			RcSend(s, &sendl, sizeof(sendl));
		}

		for (int i = 0; i<mem_cnt; i++)
		{

			uint32_t sendl = memhosts[i].size() + 1;
			if (sendl>255)
			{
				sendl = 255;
				memhosts[i][254] = 0;
			}
			RcSend(s, &sendl, sizeof(sendl));
			RcSend(s, (char*)memhosts[i].c_str(), sendl);
			sendl = memports[i];
			RcSend(s, &sendl, sizeof(sendl));
		}



		return 0;
	}
}