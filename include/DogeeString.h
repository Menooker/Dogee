#ifndef __DOGEE_STRING_H_ 
#define __DOGEE_STRING_H_ 

#include "DogeeBase.h"
#include "DogeeMacro.h"
#include "DogeeEnv.h"
#include <string>

namespace Dogee
{
	class DString : public DObject
	{
		bool cached;
		std::string cached_string;
		Value<uint32_t, 0> m_size;
	public:
		uint32_t size()
		{
			return m_size;
		}
		DString(ObjectKey obj_id) : DObject(obj_id), cached(false)
		{
		}

		const std::string& getstr()
		{
			if (!cached)//fix-me : not safe in multithreading env
			{
				cached = true;
				uint32_t sz = self->m_size;
				bool has_tail = (sz % sizeof(uint32_t) != 0);
				uint32_t size = sz / sizeof(uint32_t) + ( has_tail ? 1 : 0);
				char* buffer = new char[(size + 1)*sizeof(uint32_t)];

				DogeeEnv::cache->getchunk(this->GetObjectId(), 1, size, (uint32_t*)buffer);
				buffer[sz] = 0;
				cached_string = std::string(buffer);
				delete[]buffer;
			}
			return cached_string;
		}
		const std::string& operator*()
		{
			return getstr();
		}

		operator const std::string()
		{
			return getstr();
		}

		const std::string* operator->()
		{
			return &getstr();
		}
		DString(const std::string& str) : DObject(0), cached(false)
		{
			bool has_tail = (str.size() % sizeof(uint32_t) != 0);
			uint32_t size = str.size() / sizeof(uint32_t) + (has_tail ? 1 : 0);
			ObjectKey okey = AllocObjectId(AutoRegisterObject<DString>::id, 1 + size);
			this->SetObjectId(okey);
			const char* pstr = str.c_str();
			uint32_t* ptr = (uint32_t*)pstr;
			DogeeEnv::cache->putchunk(okey, 1, size, ptr);
			self->m_size = str.size();
		}
	};

	template<>
	struct NewObjImp<DString>
	{
		template<class... _Types> inline
			ObjectKey NewObj(_Types&&... _Args)
		{
			DString ret(std::forward<_Types>(_Args)...);
			return (ret.GetObjectId());
		}
	};
}

#endif