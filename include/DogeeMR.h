#ifndef __DOGEE_MR_H_
#define __DOGEE_MR_H_

#include <unordered_map>
#include <vector>
#include "DogeeAccumulator.h"
#include "DogeeSocket.h"
#include "DogeeSerializerImpl.h"

namespace Dogee
{
	extern bool _Reduce(ObjectKey key, int timeout);
	extern bool _Map(ObjectKey key, std::function<bool()> is_done, std::function<uint32_t(uint32_t*)> PrepareBuf, int timeout);

	class DBaseMapReduce : public DBaseAccumulator
	{
		DefBegin(DBaseAccumulator);
	public:
		DefEnd();

		virtual void BaseDoReduce(uint32_t* buf) = 0;

		DBaseMapReduce(ObjectKey key) :DBaseAccumulator(key)
		{}
	};

	template<typename MAP_IN_KEY_TYPE, typename MAP_IN_VALUE_TYPE, typename MAP_OUT_KEY_TYPE, typename MAP_OUT_VALUE_TYPE>
	class DMapReduce : public DBaseMapReduce
	{
		InputSerializer<MAP_IN_KEY_TYPE> key_serializer;
		InputSerializer<MAP_IN_VALUE_TYPE> value_serializer;

		uint32_t DecodeKeyValue(MAP_IN_KEY_TYPE& key, MAP_IN_VALUE_TYPE& value, uint32_t* buf, uint32_t len)
		{
			uint32_t mylen = key_serializer.Deserialize(buf, key);
			assert(mylen < len);
			mylen += value_serializer.Deserialize(buf + mylen, value);
			assert(mylen <= len);
			return mylen;
		}

		DefBegin(DBaseMapReduce);
	public:
		DefEnd();
		typedef std::unordered_map < MAP_OUT_KEY_TYPE, std::vector<MAP_OUT_VALUE_TYPE> > MAP;
		typedef void (*pDoMapFunc)(MAP_IN_KEY_TYPE& key, MAP_IN_VALUE_TYPE& value, MAP& localmap);
		typedef void (*pDoReduceFunc)(const MAP_OUT_KEY_TYPE& key, const std::vector<MAP_OUT_VALUE_TYPE>& values);

		
		DMapReduce(ObjectKey key) :DBaseMapReduce(key)
		{}

		DMapReduce(ObjectKey key, uint32_t num_users) :DBaseMapReduce(key)
		{
			self->arr = 0;
			self->len = 0;
			self->num_users = num_users;
		}

		virtual void DoMap(MAP_IN_KEY_TYPE& key, MAP_IN_VALUE_TYPE& value,MAP& localmap) = 0;
		virtual void DoReduce(const MAP_OUT_KEY_TYPE& key, const std::vector<MAP_OUT_VALUE_TYPE>& values) = 0;


		virtual void BaseDoReduce(uint32_t* buf)
		{
			MAP* outmap = (MAP*)buf;
			for (auto itr = outmap->begin(); itr != outmap->end(); itr++)
			{
				DoReduce(itr->first, itr->second);
			}
		}

		virtual uint32_t* AllocLocalBuffer(uint32_t len)
		{
			return (uint32_t*)new MAP;
		}
		virtual void FreeLocalBuffer(uint32_t* ptr)
		{
			delete (MAP *) ptr;
		}
		virtual void BaseDoAccumulateDense(char*__RESTRICT in_data, uint32_t in_bytes, uint32_t in_offset, uint32_t*__RESTRICT out_data, uint32_t out_offset, uint32_t out_len)
		{
			uint32_t word_len = in_bytes / sizeof(uint32_t);
			uint32_t* word_data = (uint32_t*)in_data;
			MAP* outmap = (MAP*)out_data;
			for (uint32_t i = 0; i < word_len;)
			{
				MAP_IN_KEY_TYPE key;
				MAP_IN_VALUE_TYPE value;
				uint32_t readlen = DecodeKeyValue(key, value, word_data + i, word_len - i);
				DoMap(key, value,*outmap);
				i += readlen;
			}
		}

		virtual void BaseDoAccumulateSparse(char*__RESTRICT in_data, uint32_t in_bytes, uint32_t in_offset, uint32_t*__RESTRICT out_data, uint32_t out_offset, uint32_t out_len)
		{
			assert(0);
		}

		bool Reduce(int timeout = -1)
		{
			return _Reduce(GetObjectId(), -1);
		}

		bool Map(std::unordered_map<MAP_IN_KEY_TYPE, MAP_IN_VALUE_TYPE> in_map, int timeout=-1)
		{
			uint32_t cnt = in_map.size();
			auto itr = in_map.begin();
			const uint32_t buflen = BD_DATA_PROCESS_SIZE / sizeof(uint32_t);
			auto PrepareBuf = [&](uint32_t* mybuf){
				uint32_t elements = 0;
				uint32_t idx = 0;
				while (idx < buflen && itr != in_map.end())
				{
					uint32_t size_to_add = key_serializer.GetSize(itr->first) +  value_serializer.GetSize(itr->second);
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
			return _Map(GetObjectId(), [&](){return itr != in_map.end(); }, PrepareBuf, timeout); 
		}

	};

	
	template<typename MAP_IN_KEY_TYPE, typename MAP_IN_VALUE_TYPE,
		typename MAP_OUT_KEY_TYPE, typename MAP_OUT_VALUE_TYPE,
		typename DMapReduce<MAP_IN_KEY_TYPE, MAP_IN_VALUE_TYPE, MAP_OUT_KEY_TYPE, MAP_OUT_VALUE_TYPE>::pDoMapFunc MapFunc,
		typename DMapReduce<MAP_IN_KEY_TYPE, MAP_IN_VALUE_TYPE, MAP_OUT_KEY_TYPE, MAP_OUT_VALUE_TYPE>::pDoReduceFunc ReduceFunc>
	class DFunctionMapReduce : public DMapReduce<MAP_IN_KEY_TYPE, MAP_IN_VALUE_TYPE, MAP_OUT_KEY_TYPE,  MAP_OUT_VALUE_TYPE>
	{
		typedef DMapReduce<MAP_IN_KEY_TYPE, MAP_IN_VALUE_TYPE, MAP_OUT_KEY_TYPE, MAP_OUT_VALUE_TYPE> Parent;
	public:
		DFunctionMapReduce(ObjectKey key) :Parent(key)
		{}

		DFunctionMapReduce(ObjectKey key, uint32_t num_users) :Parent(key, num_users)
		{}

		virtual void DoMap(MAP_IN_KEY_TYPE& key, MAP_IN_VALUE_TYPE& value, typename Parent::MAP & localmap)
		{
			MapFunc(key, value, localmap);
		}
		virtual void DoReduce(const MAP_OUT_KEY_TYPE& key, const std::vector<MAP_OUT_VALUE_TYPE>& values)
		{
			ReduceFunc(key, values);
		}
	};
}

#endif