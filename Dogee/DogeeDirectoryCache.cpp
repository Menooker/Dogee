#include "DogeeDirectoryCache.h"
#include <assert.h>
#include <atomic>

namespace Dogee
{
	
	using namespace Socket;

	static inline uint32_t CacheClock()
	{
		static std::atomic<uint32_t> cnt = ATOMIC_VAR_INIT(0);
		return cnt++;
	}

	void DSMDirectoryCache::DSMCacheProtocal::ServerRenew(uint64_t addr, int src_id, uint32_t * v, uint32_t len)
	{


		//	printf("renew : %lld[%lld]=%d\n",addr>>32,addr &0xffffffff,v.vi);

		UaEnterReadRWLock(&ths->hash_lock);
		hash_iterator itr=ths->cache.find(addr & DSM_CACHE_HIGH_MASK_64);
		bool found=(itr!=ths->cache.end());
		CacheBlock* blk=found?itr->second:NULL;
		UaLeaveReadRWLock(&ths->hash_lock);
		if(found)
		{
			if(UaTryEnterReadRWLock(&blk->lock))
			{
				if (blk->key == (addr & DSM_CACHE_HIGH_MASK_64))
				{
					memcpy(blk->cache + (addr & DSM_CACHE_LOW_MASK_64), v, sizeof(blk->cache[0])*len);
				}
				else
					printf("Discard write changed key : [%lu->%lu]=%u->%u\n",addr &0xffffffff,blk->key,blk->cache[addr & DSM_CACHE_LOW_MASK],*v);
				UaLeaveReadRWLock(&blk->lock);
			}
			else
            {
                printf("Discard write : [%lu]=%u->%u\n",addr &0xffffffff,blk->cache[addr & DSM_CACHE_LOW_MASK],*v);
            }
			//printf("Renew!!! index=%llx,value=%d\n",addr ,v.vi);
		}
	}

	void DSMDirectoryCache::DSMCacheProtocal::ServerRenewChunk(uint64_t addr, int src_id, uint32_t* v)
	{
		UaEnterReadRWLock(&ths->hash_lock);
		hash_iterator itr=ths->cache.find(addr & DSM_CACHE_HIGH_MASK_64);
		bool found=(itr!=ths->cache.end());
		CacheBlock* blk=found?itr->second:NULL;
		UaLeaveReadRWLock(&ths->hash_lock);
		if(found)
		{
			if(UaTryEnterReadRWLock(&blk->lock))
			{
				if(blk->key== (addr & DSM_CACHE_HIGH_MASK_64))
					memcpy(blk->cache, v, sizeof(blk->cache[0])*DSM_CACHE_BLOCK_SIZE);
				else
					printf("Discard write changed key : [%lu->%lu]=%u->\n",addr &0xffffffff,blk->key,blk->cache[addr & DSM_CACHE_LOW_MASK]);
				UaLeaveReadRWLock(&blk->lock);
			}
			else
            {
                printf("Discard write : [%lu]=%u-\n",addr &0xffffffff,blk->cache[addr & DSM_CACHE_LOW_MASK]);
            }
			//printf("Renew!!! index=%llx,value=%d\n",addr ,v.vi);
		}
	}

	void DSMDirectoryCache::DSMCacheProtocal::ServerWrite(uint64_t addr,int src_id,uint32_t* v,uint32_t len)
	{
		bool islocal= (src_id==ths->cache_id);
		uint64_t baddr=addr & DSM_CACHE_HIGH_MASK_64;
		if(!islocal &&  ((addr>>DSM_CACHE_BITS) % caches!=ths->cache_id))
		{
			//RcSend(datasockets[src_id],&status,sizeof(status));
			printf("Cache server bad address!!!%lx\n",addr);
			_BreakPoint;
			return;
		}
		ths->backend->putchunk(addr>>32,(addr & 0xffffffff),len,v);
		//printf("WRITE!!!!! [%llx]=%d\n",addr,v.vi);
		UaEnterReadRWLock(&dir_lock);
		dir_iterator itr=directory.find(baddr);
		uint64_t old=0;
		if(itr!=directory.end())
		{
			old=itr->second;
		}
		if(old)
		{
			DataPack sendpack;
			sendpack.kind=MsgRenew;
			sendpack.addr=addr;
			sendpack.len = len;
			memcpy(sendpack.buf, v, sizeof(sendpack.buf[0])*DSM_CACHE_BLOCK_SIZE);
			for(int i=0;i<caches;i++)
			{
				if((old & 1) && src_id!=i)
				{
					if(i==ths->cache_id)
					{
						ServerRenew(addr,i,v,len);
					}
					else
					{
						RcSend(controlsockets[i],&sendpack,sizeof(sendpack));
					}
				}
				old=old>>1;
			}
		}
		if(!islocal)
		{
			ServerWriteReply reply={addr};
			RcSend(datasockets[src_id],&reply,sizeof(reply));
		}
		UaLeaveReadRWLock(&dir_lock);
	}

	void DSMDirectoryCache::DSMCacheProtocal::ServerWriteback(uint64_t addr,int src_id)
	{
		if(((addr>>DSM_CACHE_BITS) % caches!=ths->cache_id))
		{
			printf("Cache server bad address!!!%lx\n",addr);
			_BreakPoint;
		}
		UaEnterWriteRWLock(&dir_lock);
		dir_iterator itr=directory.find(addr);
		if(itr!=directory.end())
		{
			uint64_t old;
			old=itr->second;
			uint64_t newv = old & ~( ((uint64_t)1)<< src_id);
			if(newv)
				directory[addr]=newv;
			else
				directory.erase(addr);
		}

		UaLeaveWriteRWLock(&dir_lock);

	}


	DSMDirectoryCache::DSMCacheProtocal::CacheMessageKind DSMDirectoryCache::DSMCacheProtocal::ServerWriteMiss(uint64_t addr,int src_id,uint32_t* v,uint32_t in_len,uint32_t* outbuf)
	{
		bool islocal= (src_id==ths->cache_id);
		uint64_t baddr=addr & DSM_CACHE_HIGH_MASK_64;
		CacheMessageKind status=MsgReplyOK;
		if(!islocal &&  ((addr>>DSM_CACHE_BITS) % caches!=ths->cache_id))
		{
			printf("Cache server bad address!!!%lx\n",addr);
			_BreakPoint;
		}

		ths->backend->putchunk( addr>>32,(addr&0xffffffff),in_len,v);
		//printf("WRITE Miss!!!!! [%llx]=%d\n",addr,v.vi);

		UaEnterWriteRWLock(&dir_lock);
		dir_iterator itr=directory.find(baddr);
		uint64_t old=0;
		if(itr!=directory.end())
		{
			old=itr->second;
		}
		uint64_t newv = old | ( ((uint64_t)1)<< src_id);
		directory[baddr]=newv;

		if(old)
		{
			DataPack sendpack;
			sendpack.kind=MsgRenew;
			sendpack.addr=addr;
			sendpack.len = in_len;
			memcpy(sendpack.buf, v, sizeof(sendpack.buf[0])*DSM_CACHE_BLOCK_SIZE);
			for(int i=0;i<caches;i++)
			{
				if((old & 1) && src_id!=i)
				{
					if(i==ths->cache_id)
					{
						ServerRenew(addr,i,v,in_len);
					}
					else
					{
						RcSend(controlsockets[i],&sendpack,sizeof(sendpack));
					}
				}
				old=old>>1;
			}
		}

		////////////////////////////////////////////////////

		if(islocal)
		{
			if(ths->backend->getblock(baddr,outbuf)!=SoOK)
			{
				UaLeaveWriteRWLock(&dir_lock);
				return MsgReplyBadAddress;
			}
		}
		else
		{
			DataPack sendpack;
			sendpack.kind=MsgReplyOK;
			sendpack.addr=addr;
			if(ths->backend->getblock(baddr,sendpack.buf)!=SoOK)
			{
				sendpack.kind=MsgReplyBadAddress;
			}
			RcSend(datasockets[src_id],&sendpack,sizeof(sendpack));
		}
		UaLeaveWriteRWLock(&dir_lock);
		return status;
	}

	DSMDirectoryCache::DSMCacheProtocal::CacheMessageKind DSMDirectoryCache::DSMCacheProtocal::ServerReadMiss(uint64_t addr,int src_id,uint32_t* outbuf)
	{
		bool islocal= (src_id==ths->cache_id);
		CacheMessageKind status=MsgReplyOK;
		if(!islocal &&  ((addr>>DSM_CACHE_BITS) % caches!=ths->cache_id))
		{
			printf("Cache server bad address!!!%lx\n",addr);
			_BreakPoint;
		}
		UaEnterWriteRWLock(&dir_lock);
		dir_iterator itr=directory.find(addr);
		uint64_t old=0;
		if(itr!=directory.end())
		{
			old=itr->second;
		}
		uint64_t newv = old | ( ((uint64_t)1)<< src_id);
		directory[addr]=newv;

		if(islocal)
		{
			if(ths->backend->getblock(addr,outbuf)!=SoOK)
			{
				UaLeaveWriteRWLock(&dir_lock);
				return MsgReplyBadAddress;
			}
		}
		else
		{
			DataPack sendpack;
			sendpack.kind=MsgReplyOK;
			sendpack.addr=addr;
			if(ths->backend->getblock(addr,sendpack.buf)!=SoOK)
			{
				sendpack.kind=MsgReplyBadAddress;
			}
			RcSend(datasockets[src_id],&sendpack,sizeof(sendpack));
		}
		UaLeaveWriteRWLock(&dir_lock);

		return status;
	}


	void DSMDirectoryCache::DSMCacheProtocal::Writeback(uint64_t addr)
	{
		//printf("WriteBack %u\n",addr);
		int target_cache_id=(addr>>DSM_CACHE_BITS) % caches;
		if(target_cache_id==ths->cache_id)
		{
			ServerWriteback(addr,ths->cache_id);
		}
		else
		{
			DataPack pack = { addr, MsgWriteback };
			RcSend(controlsockets[target_cache_id],&pack,sizeof(pack));
		}
	}

	void DSMDirectoryCache::DSMCacheProtocal::Write(uint64_t addr, uint32_t* v, uint32_t len)
	{
		int target_cache_id=(addr>>DSM_CACHE_BITS) % caches;
		if(target_cache_id==ths->cache_id)
		{
			ServerWrite(addr,ths->cache_id,v,len);
		}
		else
		{
			UaEnterLock(&datasocketlocks[target_cache_id]);
			DataPack pack = { addr, MsgWrite,len };
			memcpy(pack.buf, v, sizeof(pack.buf[0])*len);
			RcSend(controlsockets[target_cache_id],&pack,sizeof(pack));
			ServerWriteReply reply;
			if(RcRecv(datasockets[target_cache_id],&reply,sizeof(reply))!=sizeof(reply))
            {
				UaLeaveLock(&datasocketlocks[target_cache_id]);
                printf("Write receive error %d\n",RcSocketLastError());
                return;
            }
			if(reply.addr!=addr)
            {
                printf("Write return a bad address\n");
				_BreakPoint;
                return;
            }
			UaLeaveLock(&datasocketlocks[target_cache_id]);
		}
	}

	SoStatus DSMDirectoryCache::DSMCacheProtocal::WriteMiss(uint64_t addr, uint32_t * v, uint32_t len, CacheBlock* blk)
	{
		int target_cache_id=(addr>>DSM_CACHE_BITS) % caches;
		if(target_cache_id==ths->cache_id)
		{
			if(ServerWriteMiss(addr,ths->cache_id,v,1,blk->cache)!=MsgReplyOK)
				return SoFail;
		}
		else
		{
			DataPack pack = { addr, MsgWriteMiss, len };
			memcpy(pack.buf, v, sizeof(pack.buf[0])*len);
			UaEnterLock(&datasocketlocks[target_cache_id]);
			RcSend(controlsockets[target_cache_id],&pack,sizeof(pack));
			if(RcRecv(datasockets[target_cache_id],&pack,sizeof(pack))!=sizeof(pack))
            {
				UaLeaveLock(&datasocketlocks[target_cache_id]);
                printf("Write miss receive error %d\n",RcSocketLastError());
                return SoFail;
            }
			UaLeaveLock(&datasocketlocks[target_cache_id]);
			if(pack.kind!=MsgReplyOK)
			{
				return SoFail;
			}
			if(pack.addr!=addr)
			{
				printf("Write miss return a bad addr\n");
				return SoFail;
			}
			memcpy(blk->cache,pack.buf,sizeof(blk->cache));
		}
		blk->key=addr & DSM_CACHE_HIGH_MASK_64;
		return SoOK;
	}


	SoStatus DSMDirectoryCache::DSMCacheProtocal::ReadMiss(uint64_t addr,CacheBlock* blk)
	{
		int target_cache_id=(addr>>DSM_CACHE_BITS) % caches;
		if(target_cache_id==ths->cache_id)
		{
			if(ServerReadMiss(addr,ths->cache_id,blk->cache)!=MsgReplyOK)
				return SoFail;
		}
		else
		{
			DataPack pack = { addr, MsgReadMiss };
			UaEnterLock(&datasocketlocks[target_cache_id]);
			RcSend(controlsockets[target_cache_id],&pack,sizeof(pack));
			if(RcRecv(datasockets[target_cache_id],&pack,sizeof(pack))!=sizeof(pack))
            {
                printf("Read miss receive error %d\n",RcSocketLastError());
                return SoFail;
            }
			UaLeaveLock(&datasocketlocks[target_cache_id]);
			if(pack.kind!=MsgReplyOK)
				return SoFail;
			if(pack.addr!=addr)
			{
				printf("Read miss return a bad addr\n");
				_BreakPoint;
				return SoFail;
			}
			memcpy(blk->cache,pack.buf,sizeof(blk->cache));
		}
		blk->key=addr;
		return SoOK;
	}
//end of class DSMCacheProtocal





CacheBlock* DSMDirectoryCache::getblock(uint64_t k,bool& is_pending)
{
	UaEnterLock(&queue_lock);

	UaEnterReadRWLock(&hash_lock);
	hash_iterator itr=cache.find(k);
	if(itr!=cache.end())
	{
		/*if a pending block is already in the cache,
		we just return the new block.
		(which means another block request for the same address
		has already sent)
		*/
		CacheBlock* blk=itr->second;
		UaLeaveReadRWLock(&hash_lock);
		UaLeaveLock(&queue_lock);
		is_pending=true;
		return blk;
	}
	UaLeaveReadRWLock(&hash_lock);

	is_pending=false;
	CacheBlock* ret;
	if(block_queue.empty())
	{
		//if there is no free block,first find the oldest block not referenced
		unsigned long minlru=0xffffffff;
		int mini=-1;
		for(int i=0;i<DSM_CACHE_SIZE;i++)
		{
			if(block_cache[i].key!=DSM_CACHE_BAD_KEY && block_cache[i].lru<minlru)
			{
				minlru=block_cache[i].lru;
				mini=i;
			}
		}
		assert(mini!=-1);
		//acquire the control over the block and swap it out
		UaEnterWriteRWLock(&block_cache[mini].lock);

		uint64_t oldkey=block_cache[mini].key;
		block_cache[mini].key=DSM_CACHE_BAD_KEY;
		block_cache[mini].lru=0xffffffff;
		UaEnterWriteRWLock(&hash_lock);
		cache.erase(oldkey);
		cache[k]=&block_cache[mini];
		UaLeaveWriteRWLock(&hash_lock);

		//release the queue lock before time consuming operations
		UaLeaveLock(&queue_lock);

		//we don't release the block's lock here. we should release it when the block is finally ready
		protocal->Writeback(oldkey);

		ret=&block_cache[mini];
	}
	else
	{
		ret=block_queue.front();
		block_queue.pop();
		UaEnterWriteRWLock(&hash_lock);
		cache[k]=ret;
		UaLeaveWriteRWLock(&hash_lock);
		UaEnterWriteRWLock(&ret->lock);
		UaLeaveLock(&queue_lock);
	}

	return ret;
}

SoStatus DSMDirectoryCache::put(ObjectKey okey, FieldKey fldid, uint64_t v)
{
	return putchunk(okey, fldid, 2, (uint32_t*)&v);
}

SoStatus DSMDirectoryCache::put(ObjectKey okey, FieldKey fldid, uint32_t v)
{
	return doput(MAKE64(okey, fldid),&v,1);
}

SoStatus DSMDirectoryCache::putblockdata(LongKey k, uint32_t len, uint32_t* buf)
{
	return doput(k, buf, len);
}

SoStatus DSMDirectoryCache::doput(LongKey addr,uint32_t* v,uint32_t len)
{
#ifdef BD_DSM_STAT
	writes++;
#endif
	uint64_t k = addr & DSM_CACHE_HIGH_MASK_64;
	
	UaEnterReadRWLock(&hash_lock);
	hash_iterator itr=cache.find(k);
	bool found=(itr!=cache.end());
	CacheBlock* foundblock=found?itr->second:NULL;
	UaLeaveReadRWLock(&hash_lock);

	if(found)
	{
		UaEnterReadRWLock(&foundblock->lock);
		if(foundblock->key!=k) //in case that the block is swapped out and reused
		{
			UaLeaveReadRWLock(&foundblock->lock);
			goto MISS;
		}
#ifdef BD_DSM_STAT
		whit++;
#endif
		foundblock->lru=CacheClock();
		//foundblock->cache[fldid & DSM_CACHE_LOW_MASK]=v;
		memcpy(&foundblock->cache[addr & DSM_CACHE_LOW_MASK_64], v, sizeof(foundblock->cache[0])*len);
		protocal->Write(addr,v,len);
		UaLeaveReadRWLock(&foundblock->lock);
		
		return SoOK;
	}
MISS:
	bool is_pending;
	CacheBlock* blk=getblock(k,is_pending);
	if(is_pending)
	{
		UaEnterReadRWLock(&blk->lock);
		if(blk->key!=k) //in case that the block is swapped out and reused AGAIN!
		{
			UaLeaveReadRWLock(&blk->lock);
			goto MISS;
		}
		blk->lru=CacheClock();
		//blk->cache[fldid & DSM_CACHE_LOW_MASK]=v;
		memcpy(&blk->cache[addr & DSM_CACHE_LOW_MASK_64], v, sizeof(blk->cache[0])*len);
		protocal->Write(addr, v, len);
		UaLeaveReadRWLock(&blk->lock);
		
		return SoOK;
	}
	else
	{
		protocal->WriteMiss(addr, v, len, blk);
		blk->lru=CacheClock();
		if(blk->key!=k)
		{
			printf("Write Miss key error");
			_BreakPoint;
		}
		////deleted//blk->cache[fldid & DSM_CACHE_LOW_MASK]=v;
		UaLeaveWriteRWLock(&blk->lock);
		
		return SoOK;
	}
}

void DSMDirectoryCache::doget(LongKey k, std::function<void(CacheBlock*)> func)
{
#ifdef BD_DSM_STAT
	reads++;
#endif
	k = k & DSM_CACHE_HIGH_MASK_64;

	UaEnterReadRWLock(&hash_lock);
	hash_iterator itr = cache.find(k);
	bool found = (itr != cache.end());
	CacheBlock* foundblock = found ? itr->second : NULL;
	UaLeaveReadRWLock(&hash_lock);

	if (found)
	{
		UaEnterReadRWLock(&foundblock->lock);
		if (foundblock->key != k) //in case that the block is swapped out and reused
		{
			UaLeaveReadRWLock(&foundblock->lock);
			goto MISS;
		}
#ifdef BD_DSM_STAT
		rhit++;
#endif
		foundblock->lru = CacheClock();
		func(foundblock);
		//ret = foundblock->cache[fldid & DSM_CACHE_LOW_MASK];
		UaLeaveReadRWLock(&foundblock->lock);
		return ;
	}
MISS:
	bool is_pending;
	CacheBlock* blk = getblock(k, is_pending);
	if (is_pending)
	{
		UaEnterReadRWLock(&blk->lock);
		if (blk->key != k) //in case that the block is swapped out and reused AGAIN!
		{
			UaLeaveReadRWLock(&blk->lock);
			goto MISS;
		}
		blk->lru = CacheClock();
		func(blk);
		//ret = blk->cache[fldid & DSM_CACHE_LOW_MASK];
		UaLeaveReadRWLock(&blk->lock);

		return ;
	}
	else
	{
		protocal->ReadMiss(k, blk);
		blk->lru = CacheClock();
		if (blk->key != k)
		{
			printf("Write Miss key error");
			_BreakPoint;
		}
		func(blk);
		//ret = blk->cache[fldid & DSM_CACHE_LOW_MASK];
		UaLeaveWriteRWLock(&blk->lock);

		return;
	}
}

uint32_t DSMDirectoryCache::get(ObjectKey okey,FieldKey fldid)
{
	uint32_t ret;
	doget(MAKE64(okey, fldid), [&](CacheBlock* blk){ret = blk->cache[fldid & DSM_CACHE_LOW_MASK]; });
	return ret;
}

void DSMDirectoryCache::getblockdata(LongKey k, uint32_t len, uint32_t* buf)
{
	doget(k, [&](CacheBlock* blk){memcpy(buf, blk->cache + (k & DSM_CACHE_LOW_MASK_64), len*sizeof(blk->cache[0])); });
}

CacheBlock* DSMDirectoryCache::find_block(uint64_t k)
{
	UaEnterReadRWLock(&hash_lock);
	hash_iterator itr=cache.find(k);
	bool found=(itr!=cache.end());
	CacheBlock* foundblock=found?itr->second:NULL;
	UaLeaveReadRWLock(&hash_lock);

	if(found)
	{
		UaEnterReadRWLock(&foundblock->lock);
		if(foundblock->key!=k) //in case that the block is swapped out and reused
		{
			UaLeaveReadRWLock(&foundblock->lock);
			return NULL;
		}
		foundblock->lru=CacheClock();

		return foundblock;
	}
	return NULL;
}

SoStatus DSMDirectoryCache::putchunk(ObjectKey okey, FieldKey fldid, uint32_t len, uint32_t* v)
{
	uint64_t k=MAKE64(okey,fldid);
	uint64_t k_start,k_end,k_tail;

	k_tail=k+len;
	if(k & DSM_CACHE_LOW_MASK_64)
		k_start= (k & DSM_CACHE_HIGH_MASK_64) + DSM_CACHE_BLOCK_SIZE;
	else
		k_start=k;

	k_end= k_tail & DSM_CACHE_HIGH_MASK_64;
	SoStatus ret;
	CacheBlock* foundblock=NULL;
	
	uint32_t idx=0;
	uint64_t i;

	uint32_t copylen;
	copylen = k_start < k_tail ? k_start - k : len;
	assert(copylen <= DSM_CACHE_BLOCK_SIZE);
	if (copylen > 0)
	{
		if (putblockdata(k, copylen, v) != SoOK)
			ret = SoFail;
	}
	idx = copylen;

	for(i=k_start;i<k_end;i+=DSM_CACHE_BLOCK_SIZE)
	{
		if (putblockdata(i, DSM_CACHE_BLOCK_SIZE, v + idx) != SoOK)
			ret = SoFail;
		idx += DSM_CACHE_BLOCK_SIZE;
	}
	if(idx<len)
	{
		if (putblockdata(k_end, len - idx, v + idx) != SoOK)
			ret = SoFail;
	}
	
	return ret;
}

SoStatus DSMDirectoryCache::putchunk(ObjectKey okey,FieldKey fldid,uint32_t len,uint64_t* v)
{
	return putchunk(okey, fldid, len * 2, (uint32_t*)v);
}


SoStatus DSMDirectoryCache::getchunk(ObjectKey okey, FieldKey fldid, uint32_t len, uint64_t* v)
{
	return getchunk(okey, fldid, len * 2, (uint32_t*)v);
}

SoStatus DSMDirectoryCache::getchunk(ObjectKey okey, FieldKey fldid, uint32_t len, uint32_t* v)
{
	uint64_t k = MAKE64(okey, fldid);
	uint64_t k_start, k_end, k_tail;

	k_tail = k + len;
	if (k & DSM_CACHE_LOW_MASK_64)
		k_start = (k & DSM_CACHE_HIGH_MASK_64) + DSM_CACHE_BLOCK_SIZE;
	else
		k_start = k;

	k_end = k_tail & DSM_CACHE_HIGH_MASK_64;
	SoStatus ret=SoOK;
	CacheBlock* foundblock = NULL;

	uint32_t idx = 0;
	uint64_t i;
	
	uint32_t copylen;
	copylen = k_start < k_tail ? k_start - k : len;
	assert(copylen <= DSM_CACHE_BLOCK_SIZE);
	if (copylen > 0)
	{
		getblockdata(k, copylen, v);
	}
	idx = copylen;
	for (i = k_start; i<k_end; i += DSM_CACHE_BLOCK_SIZE)
	{
		getblockdata(i, DSM_CACHE_BLOCK_SIZE, v + idx);
		idx += DSM_CACHE_BLOCK_SIZE;
	}
	if (idx<len)
	{
		getblockdata(k_end, len - idx, v + idx);
	}

	return ret;
}


}