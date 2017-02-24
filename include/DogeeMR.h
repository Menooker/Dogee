#ifndef __DOGEE_MR_H_
#define __DOGEE_MR_H_

#include <unordered_map>
#include <vector>
#include "DogeeAccumulator.h"
#include "DogeeSocket.h"
#include "DogeeSerializerImpl.h"

namespace Dogee
{
	extern void AcAccumulateMsg(int src, ObjectKey aid, int thread_id, uint32_t offset, uint32_t size_bytes, char* data, uint32_t type);
	inline uint32_t cell_divide(uint32_t a)
	{
		const uint32_t b = sizeof(uint32_t);
		if (a % b == 0)
			return a / b;
		else
			return a / b + 1;
	}

	template<typename MAP_IN_KEY_TYPE, typename MAP_IN_VALUE_TYPE, typename MAP_OUT_KEY_TYPE, typename MAP_OUT_VALUE_TYPE>
	class MapReduce : public DBaseAccumulator
	{
		InputSerializer<MAP_IN_KEY_TYPE> key_serializer;
		InputSerializer<MAP_IN_KEY_TYPE> value_serializer;

		uint32_t DecodeKeyValue(MAP_IN_KEY_TYPE& key, MAP_IN_VALUE_TYPE& value, uint32_t* buf, uint32_t len)
		{
			uint32_t mylen = key_serializer.Deserialize(buf, key);
			assert(mylen < len);
			mylen += value_serializer.Deserialize(buf, value);
			assert(mylen <= len);
			return mylen;
		}

		DefBegin(DBaseAccumulator);
	public:
		DefEnd();
		typedef std::unordered_map < MAP_OUT_KEY_TYPE, std::vector<MAP_OUT_VALUE_TYPE> > MAP;
		MapReduce(ObjectKey key) :DBaseAccumulator(key)
		{}

		MapReduce(ObjectKey key, uint32_t num_users) :DBaseAccumulator(key)
		{
			self->arr = 0;
			self->len = 0;
			self->num_users = num_users;
		}

		virtual std::pair< MAP_OUT_KEY_TYPE, MAP_OUT_VALUE_TYPE> DoMap(MAP_IN_KEY_TYPE& key, MAP_IN_VALUE_TYPE& value) = 0;

		virtual uint32_t* AllocLocalBuffer(uint32_t len)
		{
			return (uint32_t*)new MAP;
		}
		virtual void FreeLocalBuffer(uint32_t* ptr)
		{
			delete (MAP *) ptr;
		}
		virtual void BaseDoAccumulateDense(char* in_data, uint32_t in_bytes, uint32_t in_offset, uint32_t* out_data, uint32_t out_offset, uint32_t out_len)
		{
			uint32_t word_len = in_bytes / sizeof(uint32_t);
			uint32_t* word_data = (uint32_t*)word_data;
			MAP* outmap = (MAP*)out_data;
			for (uint32_t i = 0; i < word_len;)
			{
				MAP_IN_KEY_TYPE key;
				MAP_IN_VALUE_TYPE value;
				uint32_t readlen = DecodeKeyValue(key, value, word_data + i, word_len - i);
				std::pair< MAP_OUT_KEY_TYPE, MAP_OUT_VALUE_TYPE> outkv = DoMap(key, value);
				outmap[outkv.first].push_back(outkv.second);
				i += readlen;
			}
		}

		virtual void BaseDoAccumulateSparse(char* in_data, uint32_t in_bytes, uint32_t in_offset, uint32_t* out_data, uint32_t out_offset, uint32_t out_len)
		{
			assert(0);
		}

		bool Map(std::unordered_map<MAP_IN_KEY_TYPE, MAP_IN_VALUE_TYPE> in_map, int timeout=-1)
		{
			RcResetRemoteEvent();
			
			RcDataPack* cmd;
			char buf2[sizeof(RcDataPack) + BD_DATA_PROCESS_SIZE];
			cmd = (RcDataPack*)buf2;
			cmd->cmd = AcDataAccumulate;
			cmd->id = this->GetObjectId();

			uint32_t cnt = in_map.size();
			uint32_t* mybuf=(uint32_t*)cmd->buf;
			const uint32_t buflen = BD_DATA_PROCESS_SIZE / sizeof(uint32_t);

			int maxfd = 0;
			fd_set writefds;
			if (DogeeEnv::num_nodes > 0)
				maxfd = accu_manager->dataconnections[0];
#ifndef _WIN32
			for (int i = 1; i < DogeeEnv::num_nodes; i++)
			{
				if (i != DogeeEnv::self_node_id && accu_manager->GetConnection(i)>maxfd)
					maxfd = accu_manager->GetConnection(i);
			}
			maxfd++;
#endif
			auto itr = in_map.begin();
			bool done = false;

			auto PrepareBuf = [&](){
				uint32_t elements = 0;
				uint32_t idx = 0;
				while (idx < buflen && itr != in_map.end())
				{
					uint32_t size_to_add = key_serializer.GetSize(itr->first) + value_serializer.GetSize(itr->second);
					if (idx + size_to_add <= buflen)
					{
						uint32_t ret;
						ret = key_serializer.Serialize(itr->first, mybuf + idx);
						idx += ret;
						ret = value_serializer.Serialize(itr->second, mybuf + idx);
						idx += ret;
					}
					else
					{
						break;
					}
					itr++;
					elements++;
					if (elements >= cnt / DogeeEnv::num_nodes + 1)
						break;
				}
				return idx;
			};

			int turns = 0;

			while ( itr != in_map.end())
			{
				if (turns % DogeeEnv::num_nodes == DogeeEnv::self_node_id)
				{
					uint32_t sz=PrepareBuf();
					AcAccumulateMsg(DogeeEnv::self_node_id, GetObjectId(), 0, 0,
						sz*sizeof(uint32_t), (char*)mybuf, TypeDense);
					turns++;
					continue;
				}
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
					if (FD_ISSET(accu_manager->GetConnection(i), &writefds))
					{
						turns++;
						unsigned int send_idx_size = func(accu_mode, in_buf,
							(char*)cmd->buf, send_idx[i], send_size[i], cmd->size, cmd->datatype);
						uint32_t sz = PrepareBuf();
						cmd->size = sz*sizeof(uint32_t);
						cmd->datatype = TypeDense;
						cmd->param0 = 0;//offset
						cmd->param12 = 0;//thread_id
						Socket::RcSend(accu_manager->GetConnection(i), cmd, sizeof(RcDataPack) + cmd->size);
					}
					
				}
			}
			for (int i = 0; i < DogeeEnv::num_nodes; i++)
			{
				if (i==DogeeEnv::self_node_id)
					AcAccumulateMsg(DogeeEnv::self_node_id, GetObjectId(), current_thread_id, 0,
						0, nullptr, TypeDense);
				else
				{
					cmd->size = 0;
					cmd->datatype = TypeDense;
					cmd->param0 = 0;//offset
					cmd->param12 = current_thread_id;//thread_id
					Socket::RcSend(accu_manager->GetConnection(i), cmd, sizeof(RcDataPack) + cmd->size);
				}

			}

			return RcWaitForRemoteEvent(timeout);
		}

	};

	template<class IN_VALUE, class OUT_VALUE>
	class Reducer : DObject
	{
		DefBegin(DObject);
		Def(out, Array<Array<OUT_VALUE>>);
		Def(counters, Array<int>);
		Def(sem, Ref<DSemaphore>);
	public:
		DefEnd();

		Mapper(ObjectKey key) :DObject(key)
		{}

		Mapper(ObjectKey key, int dummy) :DObject(key)
		{
			out = NewArray<Array<OUT_VALUE>>();
			counters = NewArray<int>();
			sem = NewObj(DSemaphore)(1);
		}

		void Write(uint32_t key, OUT_VALUE* v, uint32_t len)
		{
			sem->Acquire();
			Array<OUT_VALUE> arr = out[key];
			if (arr.GetObjectId() == 0)
			{

				arr = NewArray<OUT_VALUE>();
				out[key] = arr;
				counters[key] = 0;
			}
			int cnt = counters[key];
			counters[key] = cnt + len;
			arr.CopyFrom(v, cnt, len);
			sem->Release();
		}

		virtual void Map(uint32_t in_key, IN_VALUE in_value) = 0;

	};

}

#endif