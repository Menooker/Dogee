#include "DogeeAccumulator.h"
#include "DogeeEnv.h"
#include "DogeeSocket.h"
#include <vector>
#include <thread>
#include "DogeeRemote.h"
#include "DogeeAPIWrapping.h"
#include <unordered_map>
#include <assert.h>
#include <iostream>


namespace Dogee
{

	AccumulatorMode accu_mode = AutoMode;

	extern bool RcWaitForRemoteEvent(int timeout);
	extern void RcSetRemoteEvent(int local_thread_id);
	extern void RcResetRemoteEvent();
	extern void RcWakeRemoteThread(int dest, int thread_id);

	enum AcDataCommand
	{
		AcDataHello = 1,
		AcDataAccumulate,
		AcDataAccumulatePartialDone,
	};




#pragma pack(push)
#pragma pack(4)
	struct RcDataPack
	{
		uint32_t cmd;
		uint32_t size;
		uint32_t id;
		uint32_t datatype;
		uint32_t param0;
		union
		{
			struct
			{
				uint32_t param1;
				uint32_t param2;
			};
			uint64_t param12;
		};
		char buf[0];
	};
#pragma pack(pop)

	namespace Socket
	{
		SOCKET RcTryConnect(char* ip, int port, int node_id)
		{
			SOCKET ret;
			for (int i = 0; i < 3; i++)
			{
				ret = RcConnect(ip, port);
				if (ret)
					break;
				std::this_thread::sleep_for(std::chrono::milliseconds(500));
			}
			if (!ret)
				return 0;
			RcCommandPack cmd;
			cmd.cmd = AcDataHello;
			cmd.param = node_id;
			RcSend(ret, &cmd, sizeof(cmd));
			return ret;
		}
	}

	class DataSyncNode;
	void init_data_node(DataSyncNode* node);
	class DataSyncNode
	{
	public:
		/*the field "done_node_cnt" is only used in master node,
		representing the count of nodes that have completed the accumulation*/
		int done_node_cnt;

		int val;

		/*the field "count" represents the count of nodes that will send
		the vector to accumulate*/
		int count;

		std::vector<SyncThreadNode>* waitlist;
		Ref<DBaseAccumulator, true> accu;

		// the size of output shared array, in words
		uint32_t total_size;

		// the offset of the part of the vector that this node is responsible
		uint32_t base;

		uint32_t* buf;

		// the size of buffer, in words
		uint32_t size;

		ObjectKey outarray;
		DataSyncNode(ObjectKey aid) :accu(aid)
		{
			init_data_node(this);
		}
		~DataSyncNode()
		{
			if (waitlist)
				delete waitlist;
			delete[]buf;
		}
	};


	void init_data_node(DataSyncNode* node)
	{
		node->size = 0;
		node->count = node->accu->num_users;
		node->val = 0;
		node->done_node_cnt = 0;
		node->waitlist = new std::vector<SyncThreadNode>;
		uint32_t sz = node->accu->len;
		node->total_size = sz;
		node->outarray = node->accu->arr;
		if (node->total_size <= 1024 * 1024 * 128)
		{
			uint32_t blocks = (sz % DSM_CACHE_BLOCK_SIZE == 0) ? sz / DSM_CACHE_BLOCK_SIZE : sz / DSM_CACHE_BLOCK_SIZE + 1;
			blocks = (blocks % DogeeEnv::num_nodes == 0) ? blocks / DogeeEnv::num_nodes : blocks / DogeeEnv::num_nodes + 1;
			node->size = blocks*DSM_CACHE_BLOCK_SIZE;
			if (blocks*DSM_CACHE_BLOCK_SIZE*(DogeeEnv::self_node_id + 1) > sz)
			{
				node->size = sz - blocks * DogeeEnv::self_node_id * DSM_CACHE_BLOCK_SIZE;
				if (node->size < 0)
					node->size = 0;
			}
			node->buf = new uint32_t[node->size];
			node->base = blocks * DogeeEnv::self_node_id * DSM_CACHE_BLOCK_SIZE;
			//printf("===========\nnode->size =%d\nnode->base=%d\n==============\n",size,base);
			memset(node->buf, 0, node->size * sizeof(uint32_t));
		}
		else
		{
			printf("Bad data buffer size\n");
		}
	}

	class AcConnectionManager
	{
	public:
		BD_LOCK lock;
		SOCKET* dataconnections;
		std::unordered_map<ObjectKey, DataSyncNode*> map;
		bool connection_ok;

		DataSyncNode* FindOrCreateDataSyncNode(ObjectKey aid)
		{
			auto itr = map.find(aid);
			DataSyncNode* node;
			if (itr == map.end())
			{
				node = new DataSyncNode(aid);
				map[aid] = node;
			}
			else
			{
				node = itr->second;
			}
			return node;
		}

		AcConnectionManager()
		{
			UaInitLock(&lock);
			connection_ok = false;
			dataconnections = new SOCKET[DogeeEnv::num_nodes - 1];
			memset(dataconnections, 0, sizeof(SOCKET)*(DogeeEnv::num_nodes - 1));
		}
		~AcConnectionManager()
		{
			for (auto itr = map.begin(); itr != map.end(); itr++)
			{
				delete itr->second;
			}

			for (int i = 0; i < DogeeEnv::num_nodes - 1; i++)
			{
				Socket::RcCloseSocket(dataconnections[i]);
			}
			delete[]dataconnections;
			UaKillLock(&lock);
		}

		inline int nodeid2idx(int node_id)
		{
			int ret = (node_id == 0) ? DogeeEnv::self_node_id - 1 : node_id - 1;
			assert(ret >= 0 && ret < DogeeEnv::num_nodes - 1);
			return ret;
		}

		inline int idx2nodeid(int idx)
		{
			int ret = ((idx == DogeeEnv::self_node_id - 1) ? 0 : idx + 1);
			assert(ret >= 0 && ret < DogeeEnv::num_nodes);
			return ret;
		}

		inline void SetConnection(int node_id, SOCKET s)
		{
			dataconnections[nodeid2idx(node_id)] = s;
		}
		inline SOCKET GetConnection(int node_id)
		{
			return dataconnections[nodeid2idx(node_id)];
		}

	};


	AcConnectionManager* accu_manager = nullptr;
	std::thread data_thread;

	bool AcSlaveInitDataConnections(std::vector<std::string>& hosts, std::vector<int>& ports, int node_id)
	{
		SOCKET master_socket = Socket::RcTryConnect((char*)hosts[0].c_str(), ports[0], node_id);
		if (!master_socket)
		{
			printf("Failed to directly connect to master node\n");
			return false;
		}
		accu_manager->SetConnection(0, master_socket);
		for (int i = 1; i < node_id; i++)
		{
			accu_manager->SetConnection(i, Socket::RcTryConnect((char*)hosts[i].c_str(), ports[i], node_id));
			if (!accu_manager->GetConnection(i))
			{
				printf("Failed to directly connect to node %d\n", i);
				return false;
			}
		}
		return true;
	}

	void AcAccumulatePartialDoneMsg(int src, ObjectKey aid, bool locked)
	{
		if (!locked)
			UaEnterLock(&accu_manager->lock);
		DataSyncNode* node = accu_manager->FindOrCreateDataSyncNode(aid);

		node->done_node_cnt++;
		if (node->done_node_cnt >= DogeeEnv::num_nodes)
		{
			for (uint32_t i = 0; i < node->waitlist->size(); i++)
			{
				SyncThreadNode& th = (*node->waitlist)[i];
				RcWakeRemoteThread(th.machine, th.thread_id);
			}
			node->done_node_cnt = 0;
			node->waitlist->clear();
		}
		if (!locked)
			UaLeaveLock(&accu_manager->lock);
	}

	void AcAccumulateMsg(int src, ObjectKey aid, int thread_id, uint32_t offset, uint32_t size_bytes, char* data, uint32_t type)
	{
		UaEnterLock(&accu_manager->lock);
		DataSyncNode* node = accu_manager->FindOrCreateDataSyncNode(aid);

		switch (type)
		{
		case TypeDense:
			node->accu->BaseDoAccumulateDense(data, size_bytes, offset, node->buf, node->base, node->size);
			break;
		case TypeSparse:
			node->accu->BaseDoAccumulateSparse(data, size_bytes, offset, node->buf, node->base, node->size);
			break;
		default:
			assert(0);
		}

		if (thread_id)
		{
			node->val++;
			if (DogeeEnv::self_node_id == 0)
			{
				SyncThreadNode th = { src, thread_id };
				node->waitlist->push_back(th);
			}
			if (node->val >= node->count)
			{
				if (node->size)
					DogeeEnv::cache->putchunk(node->outarray, node->base, node->size, node->buf);
				node->val = 0;
				memset(node->buf, 0, node->size * sizeof(uint32_t));
				if (DogeeEnv::self_node_id == 0)
				{
					AcAccumulatePartialDoneMsg(0, aid, true);
				}
				else
				{
					RcDataPack cmd;
					cmd.cmd = AcDataAccumulatePartialDone;
					cmd.id = aid;
					cmd.size = 0;
					Socket::RcSend(accu_manager->GetConnection(0), &cmd, sizeof(cmd));
				}
			}
		}
		UaLeaveLock(&accu_manager->lock);
	}


	static void AcListenData(SOCKET slisten)
	{
		int n = DogeeEnv::num_nodes - 1; //n is the number of sockets
		int maxfd = 0;
		fd_set readfds;
		RcDataPack cmd;
		char buf[BD_DATA_PROCESS_SIZE];
		for (int i = DogeeEnv::self_node_id; i < n; i++)
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
				printf("Data socket accept error ! %d", RcSocketLastError());
				return;
			};
			RcCommandPack pack;
			if (Socket::RcRecv(sClient, &pack, sizeof(RcCommandPack)) != sizeof(RcCommandPack) || pack.cmd != AcDataHello)
			{
				printf("Data socket handshake error !");
				return;
			}
			accu_manager->SetConnection(pack.param, sClient);
		}
		int connect_done = 0;
		while (connect_done == 0)
		{
			connect_done = 1;
			for (int i = 0; i < DogeeEnv::self_node_id; i++)
			{
				if (!accu_manager->GetConnection(i))
				{
					connect_done = 0;
					std::this_thread::yield();
					break;
				}
			}
		}
		accu_manager->connection_ok = true;
		printf("Data connection ok\n");
		if (n > 0)
			maxfd = accu_manager->dataconnections[0];
#ifdef BD_ON_LINUX
		for (int i = 1; i < n; i++)
		{
			if ( accu_manager->dataconnections[i]>maxfd)
				maxfd =  accu_manager->dataconnections[i];
		}
		maxfd++;
#endif

		void* memc = NULL;
		for (;;)
		{
			FD_ZERO(&readfds);
			for (int i = 0; i < n; i++)
			{
				FD_SET(accu_manager->dataconnections[i], &readfds);
			}
			if (SOCKET_ERROR == select(maxfd, &readfds, NULL, NULL, NULL))
			{
				int err = RcSocketLastError();
				printf("Data Select Error!%d\n", RcSocketLastError());
				break;
			}
			DogeeEnv::InitCurrentThread();
			for (int i = 0; i < n; i++)
			{
				if (FD_ISSET(accu_manager->dataconnections[i], &readfds))
				{
					int cmdrec = 0;
					int cmdtorec = sizeof(cmd);
					while (cmdtorec > 0)
					{
						int inc = Socket::RcRecv(accu_manager->dataconnections[i], (char*)&cmd + cmdrec, cmdtorec);
						if (inc <= 0)
						{
							printf("Data socket closed %d\n", RcSocketLastError());
							return;
						}
						cmdtorec -= inc;
						cmdrec += inc;
					}
					if (cmd.size > BD_DATA_PROCESS_SIZE)
					{
						goto ERR;
					}
					int rec = 0;
					int torec = cmd.size;
					while (torec > 0)
					{
						int inc = Socket::RcRecv(accu_manager->dataconnections[i], (char*)buf + rec, torec);
						if (inc <= 0)
						{
							printf("Data socket closed %d\n", RcSocketLastError());
							return;
						}
						torec -= inc;
						rec += inc;
					}
					switch (cmd.cmd)
					{
					case AcDataAccumulate:
						AcAccumulateMsg(accu_manager->idx2nodeid(i), cmd.id, cmd.param12, cmd.param0, cmd.size, buf, cmd.datatype);
						break;
					case AcDataAccumulatePartialDone:
						if (DogeeEnv::self_node_id == 0)
							AcAccumulatePartialDoneMsg(i, cmd.id, false);
						else
							printf("Error! Slave node received a control message!\n");
						break;
					default:
						printf("Bad command %u!\n", cmd.cmd);
					}
				}
				int dummy;
			}

		}
		return;
	ERR:
		printf("Data connection internal error\n");
		return;
	}


	void AcInit(SOCKET sock)
	{
		assert(accu_manager == nullptr);
		accu_manager = new AcConnectionManager();
		data_thread = std::move(std::thread(AcListenData, sock));
		data_thread.detach();
	}

	bool AcWaitForReady()
	{
		for (int i = 0; i < 100 && (!accu_manager->connection_ok); i++)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		}
		return (accu_manager->connection_ok);
	}

	void AcClose()
	{
		delete accu_manager;
		accu_manager = nullptr;
		//fix-me : kill the data_thread
	}



	bool _DoAccumulateAndWait(char* in_buf, uint32_t len, int timeout, uint32_t dsm_size_of, ObjectKey okey, _BufferPrepareProc func)
	{
		RcResetRemoteEvent();
		uint32_t sz = dsm_size_of * len;
		uint32_t blocks = (sz % DSM_CACHE_BLOCK_SIZE == 0) ? sz / DSM_CACHE_BLOCK_SIZE : sz / DSM_CACHE_BLOCK_SIZE + 1;
		blocks = (blocks % DogeeEnv::num_nodes == 0) ? blocks / DogeeEnv::num_nodes : blocks / DogeeEnv::num_nodes + 1;
		std::vector<uint32_t> send_idx;
		send_idx.resize(DogeeEnv::num_nodes);
		std::vector<uint32_t> send_size;
		send_size.resize(DogeeEnv::num_nodes);
		RcDataPack* cmd;
		char buf2[sizeof(RcDataPack) + BD_DATA_PROCESS_SIZE];
		cmd = (RcDataPack*)buf2;
		cmd->cmd = AcDataAccumulate;
		cmd->id = okey;
		for (int i = 0; i<DogeeEnv::num_nodes; i++)
		{
			send_idx[i] = i*blocks*DSM_CACHE_BLOCK_SIZE;
			if (blocks*DSM_CACHE_BLOCK_SIZE + send_idx[i]>sz)
				send_size[i] = sz;
			else
				send_size[i] = send_idx[i] + blocks*DSM_CACHE_BLOCK_SIZE;
		}

		AcAccumulateMsg(DogeeEnv::self_node_id, okey, current_thread_id, send_idx[DogeeEnv::self_node_id],
			(send_size[DogeeEnv::self_node_id] - send_idx[DogeeEnv::self_node_id])*sizeof(uint32_t),
			(char*)((uint32_t*)in_buf + send_idx[DogeeEnv::self_node_id]), TypeDense);
		send_idx[DogeeEnv::self_node_id] = send_size[DogeeEnv::self_node_id];

		int done = 1;
		int maxfd = 0;
		fd_set writefds;
		if (DogeeEnv::num_nodes > 0)
			maxfd = accu_manager->dataconnections[0];
#ifdef BD_ON_LINUX
		for (int i = 1; i < DogeeEnv::num_nodes; i++)
		{
			if (i != DogeeEnv::self_node_id && accu_manager->GetConnection(i)>maxfd)
				maxfd = accu_manager->GetConnection(i);
		}
		maxfd++;
#endif
		while (done < DogeeEnv::num_nodes)
		{
			FD_ZERO(&writefds);
			for (int i = 0; i < DogeeEnv::num_nodes; i++)
			{
				if (i != DogeeEnv::self_node_id)
					FD_SET(accu_manager->GetConnection(i), &writefds);
			}
			if (SOCKET_ERROR == select(maxfd, NULL, &writefds, NULL, NULL))
			{
				printf("Data Send Select Error!%d\n", RcSocketLastError());
				break;
			}
			for (int i = 0; i < DogeeEnv::num_nodes; i++)
			{
				if (send_idx[i] < send_size[i])
				{
					if (i == DogeeEnv::self_node_id)
					{
						continue;
					}

					if (FD_ISSET(accu_manager->GetConnection(i), &writefds))
					{
						unsigned int send_idx_size = func(accu_mode, in_buf,
							(char*)cmd->buf, send_idx[i], send_size[i], cmd->size, cmd->datatype);
						cmd->param0 = send_idx[i];
						send_idx[i] += send_idx_size;
						if (send_idx[i] == send_size[i])
						{
							cmd->param12 = current_thread_id;
							done++;
						}
						else
						{
							cmd->param12 = 0;
						}
						//printf("Send partition to %d, offset=%d len=%d\n",nodeid2idx(i),cmd->param0,send_idx_size);
						Socket::RcSend(accu_manager->GetConnection(i), cmd, sizeof(RcDataPack) + cmd->size);
					}
				}
			}
		}
		return RcWaitForRemoteEvent(timeout);
	}
}