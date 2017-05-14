#include "DogeeCheckpoint.h"
#include <fstream>
#include "DogeeBase.h"
#include <sstream>
#include <stdio.h>
#include <mutex>
#include "DogeeLocalSync.h"


namespace Dogee
{
#define MIN(a,b) ((a<b)?a:b)
	extern void ForEachObject(std::function<void(ObjectKey key)> func);
	extern void PushObject(ObjectKey key);
	extern void DeleteObject(ObjectKey key);
	extern int GetObjectNumber();

	int checkpoint_cnt = 0;
	std::atomic<int> checkpointlock = { 0 };

	std::function<void(void)> funcCheckPoint;
	std::function<void(void)> funcRestart;
	std::function<void(std::ostream&)> funcSerialize;
	std::function<void(std::istream&)> funcDeserialize;


	void remove_checkpoint_files()
	{
		std::stringstream path;
		path << DogeeEnv::application_name << "." << DogeeEnv::self_node_id << "." << (checkpoint_cnt-1) << ".checkpoint";
		remove(path.str().c_str());
		path.str(std::string());

		path << DogeeEnv::application_name << "." << DogeeEnv::self_node_id << "." << (checkpoint_cnt - 2) << ".checkpoint";
		remove(path.str().c_str());
		path.str(std::string());

		path << DogeeEnv::application_name << ".master";
		remove(path.str().c_str());
		path.str(std::string());
		
	}

	void DumpSharedMemory(ObjectKey okey, uint32_t size,uint32_t flag, std::ostream& os)
	{
		const uint32_t fetch_size = DSM_CACHE_BLOCK_SIZE * 256;
		uint32_t buf[fetch_size];
		os.write((char*)&okey, sizeof(okey));
		os.write((char*)&size, sizeof(size));
		os.write((char*)&flag, sizeof(flag));
		for (uint32_t i = 0; i < size; i += fetch_size)
		{
			uint32_t the_size = MIN(fetch_size,size-i);
			DogeeEnv::backend->getchunk(okey, i, the_size, buf);
			for (size_t j = 0; j < the_size; j++)
			{
				os.write((char*)&buf[j], sizeof(buf[j]));
			}
		}
	}
	ObjectKey RestoreSharedMemory(std::istream& os)
	{
		ObjectKey okey;
		uint32_t size;
		uint32_t flag;
		const uint32_t fetch_size = DSM_CACHE_BLOCK_SIZE * 256;
		uint32_t buf[fetch_size];
		os.read((char*)&okey, sizeof(okey));
		os.read((char*)&size, sizeof(size));
		os.read((char*)&flag, sizeof(flag));
		for (uint32_t i = 0; i < size; i += fetch_size)
		{
			uint32_t the_size = MIN(fetch_size, size - i);
			for (size_t j = 0; j < the_size; j++)
			{
				os.read((char*)&buf[j], sizeof(buf[j]));
			}
			DogeeEnv::backend->putchunk(okey, i, the_size, buf);
		}
		DogeeEnv::backend->newobj(okey, flag, size);
		PushObject(okey);
		return okey;
	}
	extern void InitSharedConst();
	extern void DeleteSharedConstInitializer();
	void DoRestart()
	{
		if (!DogeeEnv::checkboject)
			return;
		std::stringstream path;
		path << DogeeEnv::application_name << "." << DogeeEnv::self_node_id << "." << checkpoint_cnt << ".checkpoint";
		checkpoint_cnt++;
		std::ifstream f(path.str(), std::ios::binary);
		assert(f);
		//dump the static variables
		if (DogeeEnv::isMaster())
		{
			ObjectKey modulekey=RestoreSharedMemory(f);
			assert(modulekey == 0);
			InitSharedConst();
		}
		int numobj;
		f.read((char*)&numobj, sizeof(numobj));
		for (int i = 0; i < numobj; i++)
		{
			RestoreSharedMemory(f);
		}
		//dump the checkpoint object
		funcDeserialize(f);
		funcRestart();
	}

	static bool DoCheckPoint()
	{
		if (!DogeeEnv::checkboject)
			return false;
		if (checkpointlock.exchange(1) == 1)
		{
			return false; //if another thread is doing checkpoint, return
		}
		std::stringstream path;
		path << DogeeEnv::application_name << "."<< DogeeEnv::self_node_id<<"."<< checkpoint_cnt<<".checkpoint";
		std::ofstream f(path.str(), std::ios::binary);
		assert(f);
		//dump the static variables
		if (DogeeEnv::isMaster())
			DumpSharedMemory(0, gloabl_fid, 0xFFFFFFFF, f);
		int numobj= GetObjectNumber();
		f.write((char*)&numobj, sizeof(numobj));
		ForEachObject([&](ObjectKey key){
			uint32_t flag, size;
			DogeeEnv::backend->getinfo(key, flag, size);
			DumpSharedMemory(key, size, flag,f);
		});
		funcCheckPoint();
		//dump the checkpoint object
		funcSerialize(f);
		if (DogeeEnv::isMaster())
		{
			std::stringstream pathbuf;
			pathbuf << DogeeEnv::application_name <<  ".master";
			std::ofstream outfile(pathbuf.str());
			assert(outfile);
			outfile << checkpoint_cnt;
		}
		std::stringstream removepath;
		removepath << DogeeEnv::application_name << "." << DogeeEnv::self_node_id<< "." << (checkpoint_cnt - 2) << ".checkpoint";
		remove(removepath.str().c_str());
		checkpoint_cnt++;
		return true;
	}

	static void ResetCheckpointLock()
	{
		checkpointlock = 0;
	}

	int MasterCheckCheckPoint()
	{
		std::ifstream f(DogeeEnv::application_name + ".master");
		if(!f)
			return -1;
		f >> checkpoint_cnt;
		return checkpoint_cnt;
	}

	bool DCheckpointBarrier::Enter()
	{
		/*
		1. Master waits for all slaves to DoCheckPoint
			and enter the barrier
		2. Master DoCheckPoint after all slaves has done
			calling DoCheckPoint
		3. All slaves wait for master finishing DoCheckPoint
		4. All threads released
		*/
		if (DogeeEnv::isMaster())
		{
			self->DBarrier::Enter(-1);
			bool needrelease=DoCheckPoint();
			if (needrelease)
				ResetCheckpointLock();
			self->DBarrier::Enter(-1);
		}
		else
		{
			bool needrelease = DoCheckPoint();
			self->DBarrier::Enter(-1);
			if (needrelease)
				ResetCheckpointLock();
			self->DBarrier::Enter(-1);
		}
		return true;
	}
}

