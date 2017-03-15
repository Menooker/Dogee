#ifndef __DOGEE_ACCUMULATOR_H_ 
#define __DOGEE_ACCUMULATOR_H_

#include "DogeeBase.h"
#include "DogeeMacro.h"
#include "DogeeAPIWrapping.h"
#include <assert.h>
#include <unordered_map>
#define BD_DATA_PROCESS_SIZE (16*1024)
namespace Dogee
{

#pragma pack(push)
#pragma pack(4)
	template<typename T>
	struct SparseElement
	{
		uint32_t index;
		T data;
	};
#pragma pack(pop)

	enum AcDataType
	{
		TypeDense = 1,
		TypeSparse,
	};

	enum AccumulatorMode
	{
		DenseMode,
		SparseMode,
		AutoMode,
	};
	extern AccumulatorMode accu_mode;
	inline void SetAccumulatorMode(AccumulatorMode mode)
	{
		accu_mode = mode;
	}


	typedef std::function<uint32_t(AccumulatorMode, void* src, char* dest,
		int idx, int size, unsigned int& outsize, uint32_t & type)> _BufferPrepareProc;

	extern bool _DoAccumulateAndWait(char* in_buf, uint32_t len, int timeout,
		uint32_t dsm_size_of, ObjectKey okey, _BufferPrepareProc func);

	template<typename T> bool IsProfitableToCompress(T* vec, int vsize, T threshold)
	{
		unsigned int zeros = 0;
		unsigned int used = 0;
		int i;
		for (i = 0; i < vsize && used < BD_DATA_PROCESS_SIZE; i++)
		{
			T val = vec[i];
			if ((val >= 0 && val <= threshold) || (val<0 && val>= -threshold))
			{
				zeros++;
			}
			else
			{
				used += sizeof(SparseElement<T>);
			}
		}
		return ((double)zeros / i > 0.52);
	}

	/*
	return the number of elements in the buffer.
	outsize returns size of the elements in bytes
	*/
	template<typename T>
	unsigned int MakeSparseArray(T* vec, int src, int vsize, T threshold, char* dest, unsigned int& used)
	{
		used = 0;
		int i;
		for (i = src; i < vsize; i++)
		{
			if (used + sizeof(SparseElement<T>) >= BD_DATA_PROCESS_SIZE)
				break;
			T val = vec[i];
			if (val >= 0 && val < threshold)
				val = 0;
			else if (val<0 && val> -threshold)
				val = 0;
			if (val != 0)
			{
				SparseElement<T>* ptr = (SparseElement<T>*)(dest + used);
				ptr->index = i;
				ptr->data = val;
				used += sizeof(SparseElement<T>);
			}
		}
		return (i - src);
	}

	/*
	return the number of elements in the buffer.
	outsize returns size of the elements in bytes
	int idx, int size -- start index and end index in words
	*/
	template<typename T>
	unsigned int AcAccumulatePrepareBuffer(AccumulatorMode config_mode, T threshold,
		T* src, char* dest, uint32_t idx, uint32_t size, unsigned int& outsize, uint32_t & type)
	{
		unsigned int send_idx_size;
		const int dsm_size_of = DSMInterface<T>::dsm_size_of;
		if (config_mode == AutoMode)
		{
			config_mode = IsProfitableToCompress(src + (idx / dsm_size_of), (size - idx) / dsm_size_of, threshold) ? SparseMode : DenseMode;
		}
		if (config_mode == DenseMode)
		{

			if (idx + BD_DATA_PROCESS_SIZE / sizeof(uint32_t) > size)
				send_idx_size = size - idx;
			else
				send_idx_size = BD_DATA_PROCESS_SIZE / sizeof(uint32_t);
			outsize = send_idx_size*sizeof(uint32_t);
			memcpy(dest, (char*)src + idx*sizeof(uint32_t), outsize);
			type = TypeDense;
		}
		else if (config_mode == SparseMode)
		{
			send_idx_size = dsm_size_of * MakeSparseArray(src, idx / dsm_size_of, size / dsm_size_of, threshold, dest, outsize);
			type = TypeSparse;
		}

		return send_idx_size;
	}



	class DBaseAccumulator : public DObject
	{
		DefBegin(DObject);
	public:
		Def(arr, ObjectKey);
		// "len" is the size of the output/input buffer in "words"
		Def(len, uint32_t);
		Def(num_users, uint32_t);
		DefEnd();
		DBaseAccumulator(ObjectKey key) :DObject(key)
		{}
		virtual uint32_t* AllocLocalBuffer(uint32_t len)=0;
		virtual void FreeLocalBuffer(uint32_t* ptr) = 0;
		virtual void BaseDoAccumulateDense(char*__RESTRICT in_data, uint32_t in_bytes, uint32_t in_offset, uint32_t*__RESTRICT out_data, uint32_t out_offset, uint32_t out_len) = 0;
		virtual void BaseDoAccumulateSparse(char*__RESTRICT in_data, uint32_t in_bytes, uint32_t in_offset, uint32_t*__RESTRICT out_data, uint32_t out_offset, uint32_t out_len) = 0;

	};



	template<typename T>
	class DAccumulator : public DBaseAccumulator
	{
		DefBegin(DBaseAccumulator);
	public:
		DefEnd();

		DAccumulator(ObjectKey k) :DBaseAccumulator(k)
		{}

		virtual uint32_t* AllocLocalBuffer(uint32_t len)
		{
			return new uint32_t[len];
		}

		virtual void FreeLocalBuffer(uint32_t* ptr)
		{
			delete[]ptr;
		}

		/*
		param :
		- outarr : the output shared array
		- outlen : the count of elements in output/input array 
		- in_num_user : the number of nodes that will send the vector for accumulation
		*/
		DAccumulator(ObjectKey k, Array<T> outarr, uint32_t outlen, uint32_t in_num_user) :DBaseAccumulator(k)
		{
			self->arr = outarr.GetObjectId();
			self->len = outlen * DSMInterface<T>::dsm_size_of;
			self->num_users = in_num_user;
		}

		/*
		input a local array "buf" of length "len"(defined in class member), dispatch it to each node of the
		cluster. Then wait for the accumulation is ready.
		param :
		- buf : local input buffer
		- timeout : the timeout for waiting for the accumulation
		*/
		bool AccumulateAndWait(T* buf, uint32_t len, T threshold = 0, int timeout = -1)
		{		
			return _DoAccumulateAndWait((char*)buf, len, timeout, DSMInterface<T>::dsm_size_of, this->GetObjectId(),
				[&](AccumulatorMode mode, void* src, char* dest, int idx, int size, unsigned int& outsize, uint32_t & type){
				return AcAccumulatePrepareBuffer(mode, threshold, (T*)src, dest, idx, size, outsize, type);
			});
		}


		/*
		input a local array "buf" of length "len"(defined in class member), dispatch it to each node of the
		cluster. Return without waiting for the accumulation is ready.
		param :
		- buf : local input buffer
		*/
		void Accumulate(T* buf, uint32_t len)
		{
			_BreakPoint;
			
		}

		/*
		User defined function to do the acutal accumulation for dense vector. See the input of the Accumulator as a vector. 
		Each node is responsible for a part of the vector. This function will be called when a node has
		received a part of its responsible vector from the peer nodes.
		param:
		- in_data : the input data buffer
		- in_len : the input buffer length (count of elements)
		- in_index : the offset of in_data within the output array "arr"
		- out_data : the output data buffer, may have the accumulation results from previously received vectors
		- out_index : the offset of out_data within the output array "arr"
		- out_len : the length of output buffer
		*/
		virtual void DoAccumulateDense(T*__RESTRICT in_data, uint32_t in_len, uint32_t in_index, T*__RESTRICT out_data, uint32_t out_index, uint32_t out_len) = 0;


		/*
		User defined function to do the acutal accumulation for sparse vector. See the input of the Accumulator as a vector.
		Each node is responsible for a part of the vector. This function will be called when a node has
		received a part of its responsible vector from the peer nodes.
		param:
		- in_data : the input data buffer
		- in_len : the input buffer length (count of elements)
		- in_index : the offset of in_data within the output array "arr"
		- out_data : the output data buffer, may have the accumulation results from previously received vectors
		- out_index : the offset of out_data within the output array "arr"
		- out_len : the length of output buffer
		*/
		virtual void DoAccumulateSparse(SparseElement<T>*__RESTRICT in_data, uint32_t in_len, uint32_t in_index, T*__RESTRICT out_data, uint32_t out_index, uint32_t out_len) = 0;


		virtual void BaseDoAccumulateDense(char*__RESTRICT in_data, uint32_t in_bytes, uint32_t in_offset, uint32_t*__RESTRICT out_data, uint32_t out_offset, uint32_t out_len)
		{
			DoAccumulateDense((T*)in_data, in_bytes / sizeof(T), in_offset / DSMInterface<T>::dsm_size_of,
				(T*)out_data, out_offset / DSMInterface<T>::dsm_size_of, out_len /  DSMInterface<T>::dsm_size_of);
		}

		virtual void BaseDoAccumulateSparse(char*__RESTRICT in_data, uint32_t in_bytes, uint32_t in_offset, uint32_t*__RESTRICT out_data, uint32_t out_offset, uint32_t out_len)
		{
			DoAccumulateSparse((SparseElement<T>*)in_data, in_bytes / sizeof(SparseElement<T>),
				in_offset / DSMInterface<T>::dsm_size_of, (T*)out_data, out_offset / DSMInterface<T>::dsm_size_of, out_len / DSMInterface<T>::dsm_size_of);
		}

	};

	template<typename T>
	class DAddAccumulator : public DAccumulator<T>
	{
		
		DefBegin(DAccumulator<T>);
	public:
		DefEnd();
		DAddAccumulator(ObjectKey k) :DAccumulator<T>(k)
		{}

		/*
		param :
		- outarr : the output shared array
		- outlen : the count of elements in output/input array
		- in_num_user : the number of nodes that will send the vector for accumulation
		*/
		DAddAccumulator(ObjectKey k, Array<T> outarr, uint32_t outlen, uint32_t in_num_user) :DAccumulator<T>(k, outarr, outlen, in_num_user)
		{}

		virtual void DoAccumulateDense(T*__RESTRICT in_data, uint32_t in_len, uint32_t in_index, T*__RESTRICT out_data, uint32_t out_index, uint32_t out_len)
		{
			const int offset = in_index - out_index;
			assert(offset >= 0 && offset + in_len <= out_len);
			for (uint32_t i = 0; i < in_len; i++)
			{
				out_data[offset + i] += in_data[i];
			}
		}

		virtual void DoAccumulateSparse(SparseElement<T>*__RESTRICT in_data, uint32_t in_len, uint32_t in_index, T*__RESTRICT out_data, uint32_t out_index, uint32_t out_len)
		{
			for (uint32_t i = 0; i < in_len; i++)
			{
				uint32_t idx = in_data[i].index;
				assert(idx >= out_index && idx < out_index + out_len);
				out_data[idx - out_index] += in_data[i].data;
			}
		}
	};

	template<typename T>
	struct AccumulatorFuntionHelper
	{
		typedef void (*tyfun)(T in,uint32_t index,T& out);
		//typedef uint32_t(*tymapkeyfun)(T in, uint32_t index, T& out);
	};

	template<typename T, typename AccumulatorFuntionHelper<T>::tyfun Func>
	class DFunctionalAccumulator : public DAccumulator<T>
	{

		DefBegin(DAccumulator<T>);
	public:
		DefEnd();

		DFunctionalAccumulator(ObjectKey k) :DAccumulator<T>(k)
		{}

		DFunctionalAccumulator(ObjectKey k, Array<T> outarr, uint32_t outlen, uint32_t in_num_user) :DAccumulator<T>(k, outarr, outlen, in_num_user)
		{}

		virtual void DoAccumulateDense(T*__RESTRICT in_data, uint32_t in_len, uint32_t in_index, T*__RESTRICT out_data, uint32_t out_index, uint32_t out_len)
		{
			const int offset = in_index - out_index;
			assert(offset >= 0 && offset + in_len <= out_len);
			for (uint32_t i = 0; i < in_len; i++)
			{
				Func(in_data[i], i + in_index, out_data[offset + i]);
			}
		}

		virtual void DoAccumulateSparse(SparseElement<T>*__RESTRICT in_data, uint32_t in_len, uint32_t in_index, T*__RESTRICT out_data, uint32_t out_index, uint32_t out_len)
		{
			for (uint32_t i = 0; i < in_len; i++)
			{
				uint32_t idx = in_data[i].index;
				assert(idx >= out_index && idx < out_index + out_len);
				Func(in_data[i].data, idx, out_data[idx - out_index]);
			}
		}
	};
}

#endif