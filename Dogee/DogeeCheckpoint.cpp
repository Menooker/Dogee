#include "DogeeCheckpoint.h"
#include <fstream>
#include "DogeeBase.h"

namespace Dogee
{
#define MIN(a,b) ((a<b)?a:b)
	extern void ForEachObject(std::function<void(ObjectKey key)> func);
	extern void PushObject(ObjectKey key);
	extern void DeleteObject(ObjectKey key);
	extern int GetObjectNumber();


	std::function<void(void)> funcCheckPoint;
	std::function<void(void)> funcRestart;
	std::function<void(std::ostream&)> funcSerialize;
	std::function<void(std::istream&)> funcDeserialize;

	void DumpSharedMemory(ObjectKey okey, uint32_t size, std::ostream& os)
	{
		const uint32_t fetch_size = DSM_CACHE_BLOCK_SIZE * 256;
		uint32_t buf[fetch_size];
		os << okey;
		os << size;
		for (uint32_t i = 0; i < size; i += fetch_size)
		{
			uint32_t the_size = MIN(fetch_size,size-i);
			DogeeEnv::backend->getchunk(okey, i, the_size, buf);
			for (size_t j = 0; j < the_size; j++)
			{
				os << buf[j];
			}
		}
	}
	ObjectKey RestoreSharedMemory(std::istream& os)
	{
		ObjectKey okey;
		uint32_t size;
		const uint32_t fetch_size = DSM_CACHE_BLOCK_SIZE * 256;
		uint32_t buf[fetch_size];
		os >> okey;
		os >> size;
		for (uint32_t i = 0; i < size; i += fetch_size)
		{
			uint32_t the_size = MIN(fetch_size, size - i);
			for (size_t j = 0; j < the_size; j++)
			{
				os >> buf[j];
			}
			DogeeEnv::backend->putchunk(okey, i, the_size, buf);
		}
		return okey;
	}
	void DoRestart()
	{
		if (!DogeeEnv::checkboject)
			return;
		std::fstream f;
		//dump the static variables
		if (DogeeEnv::isMaster())
		{
			ObjectKey modulekey=RestoreSharedMemory(f);
			assert(modulekey == 0);
		}
		int numobj;
		f >> numobj;
		for (int i = 0; i < numobj; i++)
		{
			RestoreSharedMemory(f);
		}
		//dump the checkpoint object
		funcDeserialize(f);
		funcRestart();
	}

	void DoCheckPoint()
	{
		if (!DogeeEnv::checkboject)
			return;
		std::fstream f;
		//dump the static variables
		if (DogeeEnv::isMaster())
			DumpSharedMemory(0, gloabl_fid, f);
		f<<GetObjectNumber();
		ForEachObject([&](ObjectKey key){
			uint32_t flag, size;
			DogeeEnv::backend->getinfo(key, flag, size);
			DumpSharedMemory(key, size, f);
		});
		funcCheckPoint();
		//dump the checkpoint object
		funcSerialize(f);
	}
}

