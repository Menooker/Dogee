#ifndef __DOGEE_DIR_CACHE_H_
#define __DOGEE_DIR_CACHE_H_
#include "DogeeStorage.h"
#include <unordered_map>
#include <queue>
#include "DogeeAPIWrapping.h"
#include <thread>
#include "DogeeSocket.h"
#include "DogeeUtil.h"
#include "DogeeEnv.h"

#define CACHE_HELLO_MAGIC (0x2e3a4f01)
#define CACHE_MAX_CHUNK 4096

namespace Dogee
{
	struct CacheBlock
	{
		uint32_t cache[1 << DSM_CACHE_BITS];
		unsigned long lru;
		uint64_t key;
		BD_RWLOCK lock;
	};
#pragma pack(push)
#pragma pack(4)
	struct CacheHelloPackage
	{
		int magic;
		int cacheid;
	};
#pragma pack(pop)

	class DSMDirectoryCache : public DSMCache
	{
	private:

		void doget(LongKey k, std::function<void(CacheBlock*)> func);

		//get data within a cache block
		void getblockdata(LongKey k, uint32_t len,uint32_t* buf);

		SoStatus doput(LongKey k, uint32_t* v, uint32_t len);

		//put data within a cache block
		SoStatus putblockdata(LongKey k, uint32_t len, uint32_t* buf);


		SoStorage* backend;
		/*
		cacheline's key:
		|object_id|block_id|
		|  32-bit | 28-bit |
		The lower DSM_CACHE_BITS bit of the original 64bit key is not used.
		Every cache block has 2^DSM_CACHE_BITS entries.
		*/

		typedef std::unordered_map<uint64_t, CacheBlock*>::iterator hash_iterator;
		CacheBlock block_cache[DSM_CACHE_SIZE];
		std::queue<CacheBlock*> block_queue;
		std::unordered_map<uint64_t, CacheBlock*> cache;
		BD_LOCK queue_lock;
		BD_RWLOCK hash_lock;

		std::vector<std::string> hosts;
		std::vector<int> ports;
		int cache_id;
		SOCKET controllisten;
		SOCKET datalisten;
		CacheBlock* find_block(uint64_t key);

		class DSMCacheProtocal;
		DSMCacheProtocal* protocal;

		class DSMCacheProtocal
		{
		private:
			std::unordered_map<uint64_t, uint64_t> directory;
			BD_RWLOCK dir_lock;
			typedef std::unordered_map<uint64_t, uint64_t>::iterator dir_iterator;

			SOCKET* controlsockets;
			SOCKET* datasockets;
			BD_LOCK*   datasocketlocks;
			std::thread* threads;
			DSMDirectoryCache* ths;
			int caches;
			enum CacheMessageKind
			{
				MsgReplyOK = 1,
				MsgReplyBadAddress,
				MsgWriteMiss,
				MsgReadMiss,
				MsgWriteChunkMiss,
				MsgWrite,
				MsgRenew,
				MsgWriteback,
				MsgWriteChunk,
				MsgRenewChunk,
			};

			struct Params
			{
				DSMCacheProtocal* ths;
				int target_id;
			};



			static void ListenSocketProc(DSMCacheProtocal* ths )
			{
				for (int i = 0; i < ths->ths->cache_id; i++)
				{
					for (int j = 0; j < 2; j++)
					{
						sockaddr_in remoteAddr;
#ifdef _WIN32
						int nAddrlen = sizeof(remoteAddr);
#else
						unsigned int nAddrlen = sizeof(remoteAddr);
#endif
						SOCKET mylisten = (j % 2 == 0) ? ths->ths->controllisten : ths->ths->datalisten;
						SOCKET sClient = accept((SOCKET)mylisten, (LPSOCKADDR)&remoteAddr, &nAddrlen);
						if (sClient == INVALID_SOCKET)
						{
							printf("cache socket accept error !");
							return ;
						}
						Socket::RcSetTCPNoDelay(sClient);
						SOCKET sock = (SOCKET)sClient;
						CacheHelloPackage pack;
						if (Socket::RcRecv(sock, &pack, sizeof(CacheHelloPackage)) != sizeof(CacheHelloPackage) || pack.magic != CACHE_HELLO_MAGIC)
						{
							printf("cache socket handshake error !");
							return ;
						}
						if (pack.cacheid >= ths->ths->cache_id)
						{
							printf("bad cache id %d!", pack.cacheid);
							return ;
						}
						
						if (j % 2 == 0)
							ths->controlsockets[pack.cacheid] = sock;
						else
							ths->datasockets[pack.cacheid] = sock;

						if (j % 2 == 0)
						{
							std::thread th(CacheProtocalProc, ths, pack.cacheid);
							th.detach();
							ths->threads[pack.cacheid] = std::move(th);
						}
						CacheHelloPackage pack2 = { CACHE_HELLO_MAGIC, ths->ths->cache_id };
						Socket::RcSend(sock, &pack2, sizeof(pack2));

					}
				}
				return ;
			}
#pragma pack(push)
#pragma pack(4)
			struct DataPack
			{
				uint64_t addr;
				CacheMessageKind kind;
				uint32_t len;
				uint32_t buf[DSM_CACHE_BLOCK_SIZE];
			};
			struct ServerWriteReply
			{
				uint64_t addr;
			};
#pragma pack(pop)

			void ServerRenewChunk(uint64_t addr, int src_id, uint32_t* v);
			void ServerRenew(uint64_t addr, int src_id, uint32_t* v, uint32_t len);
			void ServerWrite(uint64_t addr, int src_id, uint32_t* v, uint32_t len);
			void ServerWriteback(uint64_t addr, int src_id);
			CacheMessageKind ServerWriteMiss(uint64_t addr, int src_id, uint32_t* v,uint32_t in_len, uint32_t* outbuf);
			CacheMessageKind ServerReadMiss(uint64_t addr, int src_id, uint32_t* outbuf);

			static void CacheProtocalProc(DSMCacheProtocal* ths, int target_id)
			{
				DogeeEnv::InitCurrentThread();
				DataPack pack;
				for (;;)
				{
					if (Socket::RcRecv(ths->controlsockets[target_id], &pack, sizeof(pack)) != sizeof(pack))
					{
						printf("Cache server socket error %d\n", RcSocketLastError());
						//_BreakPoint;
						break;
					}
					switch (pack.kind)
					{
					case MsgReadMiss:
						ths->ServerReadMiss(pack.addr, target_id, NULL);
						break;
					case MsgWriteMiss:
						ths->ServerWriteMiss(pack.addr, target_id, pack.buf,pack.len, NULL);
						break;
					case MsgWriteChunkMiss:
						ths->ServerWriteMiss(pack.addr, target_id, pack.buf, sizeof(pack.buf), NULL);
						break;
					case MsgWrite:
						ths->ServerWrite(pack.addr, target_id, pack.buf,pack.len);
						break;
					case MsgRenew:
						ths->ServerRenew(pack.addr, target_id, pack.buf, pack.len);
						break;
					case MsgWriteback:
						ths->ServerWriteback(pack.addr, target_id);
						break;
					case MsgRenewChunk:
						ths->ServerRenewChunk(pack.addr, target_id, pack.buf);
						break;
					default:
						printf("Bad cache server message %d\n", pack.kind);
					}
				}
				return ;
			}
		public:
			void Writeback(uint64_t addr);
			void Write(uint64_t addr, uint32_t* v,uint32_t len);

			/*
			Send a write message and fetch the written block
			params:
			addr : the exact address of the miss
			v : the new value
			blk : the fetched block will be store here
			*/
			SoStatus WriteMiss(uint64_t addr, uint32_t * v, uint32_t len, CacheBlock* blk);
			SoStatus ReadMiss(uint64_t addr, CacheBlock* blk);


			DSMCacheProtocal(DSMDirectoryCache* t) : ths(t)
			{
				caches = ths->hosts.size();
				controlsockets = new SOCKET[caches];
				datasockets = new SOCKET[caches];
				threads = new std::thread[caches];
				datasocketlocks = new BD_LOCK[caches];
				for (int i = 0; i < caches; i++)
				{
					UaInitLock(&datasocketlocks[i]);
				}
				UaInitRWLock(&dir_lock);
				std::thread th(ListenSocketProc, this);
				for (int i = ths->cache_id + 1; i < caches; i++)
				{
					SOCKET sock;
					for (int j = 0; j < 3; j++)
					{
						sock = Socket::RcConnect((char*)ths->hosts[i].c_str(), ths->ports[i] + 1);
						if (sock == (SOCKET)0)
						{
							printf("cache socket connect error %d!\n", RcSocketLastError());
							std::this_thread::sleep_for(std::chrono::milliseconds( 500));
						}
						else
							break;
					}
					if (sock == (SOCKET)0)
					{
						printf("cache socket connect error %d!\n", RcSocketLastError());
						_BreakPoint;
					}
					Socket::RcSetTCPNoDelay(sock);
					CacheHelloPackage pack = { CACHE_HELLO_MAGIC, ths->cache_id };
					Socket::RcSend(sock, &pack, sizeof(pack));
					if (Socket::RcRecv(sock, &pack, sizeof(CacheHelloPackage)) != sizeof(CacheHelloPackage) || pack.magic != CACHE_HELLO_MAGIC
						|| pack.cacheid != i)
					{
						printf("cache socket handshake error %d !", i);
						_BreakPoint;
					}
					controlsockets[i] = sock;
					/////////////////////////////////////////////////////////////

					sock = Socket::RcConnect((char*)ths->hosts[i].c_str(), ths->ports[i] + 2); //data sockets
					if (sock == (SOCKET)0)
					{
						printf("cache socket connect error %d!\n", i);
						_BreakPoint;
					}
					CacheHelloPackage pack2 = { CACHE_HELLO_MAGIC, ths->cache_id };
					Socket::RcSend(sock, &pack2, sizeof(pack2));
					if (Socket::RcRecv(sock, &pack, sizeof(CacheHelloPackage)) != sizeof(CacheHelloPackage) || pack.magic != CACHE_HELLO_MAGIC
						|| pack.cacheid != i)
					{
						printf("cache socket handshake error %d !", i);
						_BreakPoint;
					}
					datasockets[i] = sock;
				}
				th.join();
				for (int i = 0; i < caches; i++)
				{
					if (i == ths->cache_id)
						continue;
					std::thread th(CacheProtocalProc, this, i);
					th.detach();
					threads[i] = std::move(th);
				}
				printf("LISTEN OK\n");
			}

			~DSMCacheProtocal()
			{
				for (int i = 0; i < caches; i++)
				{
					if (i == ths->cache_id)
						continue;
					closesocket((SOCKET)controlsockets[i]);
					closesocket((SOCKET)datasockets[i]);
					UaKillLock(&datasocketlocks[i]);
					//fix-me : kill the thread
					//UaStopThread(threads[i]);
				}
				delete[]controlsockets;
				delete[]datasockets;
				delete[]datasocketlocks;
				delete[]threads;
				UaKillRWLock(&dir_lock);
			}
		};
		//end of class DSMCacheProtocal


		inline void mapput(uint64_t key, CacheBlock* blk)
		{
			UaEnterWriteRWLock(&hash_lock);
			cache[key] = blk;
			UaLeaveWriteRWLock(&hash_lock);
		}

		/*
		return a cache block from free list. if no free block is
		available, swap out a block. Then this function puts the
		new block to the cache, using the key "k".
		input: k - the key of the new block
		output: return val - new cache block
		is_pending - if true, there is a prevoius pending
		block request of the same address. You should
		wait for the lock of the block until the block
		is ready.
		if false, the returned block is a new block,
		and the block's lock is held. RELEASE it when
		the block is ready!!!!
		*/
		CacheBlock* getblock(uint64_t k, bool& is_pending);

		inline void freeblock(CacheBlock* blk)
		{
			UaEnterWriteRWLock(&blk->lock);
			blk->key = DSM_CACHE_BAD_KEY;
			UaLeaveWriteRWLock(&blk->lock);

			UaEnterLock(&queue_lock);
			block_queue.push(blk);
			UaLeaveLock(&queue_lock);
		}

	public:

		DSMDirectoryCache(SoStorage* back, std::vector<std::string> mhosts, std::vector<int> mports, int mcache_id,
			SOCKET mcontrollisten, SOCKET mdatalisten)
			: backend(back), hosts(mhosts), ports(mports), cache_id(mcache_id), controllisten(mcontrollisten), datalisten(mdatalisten)
		{
#ifdef BD_DSM_STAT
			writes = 0;
			whit = 0;
			reads = 0;
			rhit = 0;
#endif
			for (int i = 0; i < DSM_CACHE_SIZE; i++)
			{
				block_cache[i].key = DSM_CACHE_BAD_KEY;
				UaInitRWLock(&block_cache[i].lock);
				block_queue.push(&block_cache[i]);
			}
			UaInitRWLock(&hash_lock);
			UaInitLock(&queue_lock);
			protocal = new DSMCacheProtocal(this);
		}

		~DSMDirectoryCache()
		{
			delete protocal;
			for (int i = 0; i < DSM_CACHE_SIZE; i++)
			{
				UaKillRWLock(&block_cache[i].lock);
			}
			UaKillLock(&queue_lock);
			UaKillRWLock(&hash_lock);
		}

		SoStatus putchunk(ObjectKey key, FieldKey fldid, uint32_t len, uint64_t* v);
		SoStatus putchunk(ObjectKey key, FieldKey fldid, uint32_t len, uint32_t* v);

		SoStatus getchunk(ObjectKey key, FieldKey fldid, uint32_t len, uint64_t* v);
		SoStatus getchunk(ObjectKey key, FieldKey fldid, uint32_t len, uint32_t* v);

		SoStatus put(ObjectKey key, FieldKey fldid, uint32_t v);
		SoStatus put(ObjectKey key, FieldKey fldid, uint64_t v);
		uint32_t get(ObjectKey key, FieldKey fldid);
	};
}

#endif
