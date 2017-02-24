#ifndef __DOGEE_SER_IMP_H_
#define __DOGEE_SER_IMP_H_
#include <string>

namespace Dogee
{
	namespace Serializer
	{
		inline uint32_t cell_divide(uint32_t a)
		{
			const uint32_t b = sizeof(uint32_t);
			if (a % b == 0)
				return a / b;
			else
				return a / b + 1;
		}
	}


	template<typename T>
	class InputSerializer
	{
	public:
		uint32_t GetSize(const T& pair)
		{
			return Serializer::cell_divide(sizeof(T));
		}
		uint32_t Serialize(const T& in, uint32_t* buf)
		{
			*(T*)buf = in;
			return Serializer::cell_divide(sizeof(T));
		}
		uint32_t Deserialize(uint32_t* buf, int& out)
		{
			out = *(T*)buf;
			return Serializer::cell_divide(sizeof(T));
		}
	};

	template<>
	class InputSerializer<std::string>
	{
	public:
		uint32_t GetSize(const std::string& pair)
		{
			return Serializer::cell_divide(pair.size() + 1) + 1;
		}
		uint32_t Serialize(const std::string& in, uint32_t* buf)
		{
			buf[0] = in.size();
			memcpy(buf+1, in.c_str(), in.size());
			return GetSize(in);
		}
		uint32_t Deserialize(uint32_t* buf, std::string& out)
		{
			uint32_t sz = buf[0];
			char* str = (char*)(buf + 1);
			str[sz]= 0;
			out = std::string(str);
			assert(sz == out.size());
			return Serializer::cell_divide(sz + 1) + 1;
		}
	};
}

#endif