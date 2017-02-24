#ifndef __DOGEE_SER_IMP_H_
#define __DOGEE_SER_IMP_H_
#include <string>

namespace Dogee
{


	template<typename T>
	class InputSerializer
	{
	public:
		uint32_t GetSize(T& pair)
		{
			return cell_divide(sizeof(T));
		}
		uint32_t Serialize(T& in, uint32_t* buf)
		{
			*(T*)buf = in;
			return cell_divide(sizeof(T));
		}
		uint32_t Deserialize(uint32_t* buf, int& out)
		{
			out = *(T*)buf;
			return cell_divide(sizeof(T));
		}
	};

	template<>
	class InputSerializer<std::string>
	{
	public:
		uint32_t GetSize(std::string& pair)
		{
			return cell_divide(pair.size()+1)+1;
		}
		uint32_t Serialize(std::string& in, uint32_t* buf)
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
			return cell_divide(sz + 1) + 1;
		}
	};
}

#endif