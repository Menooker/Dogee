#ifndef __DOGEE_CHECKPOINT_H_ 
#define __DOGEE_CHECKPOINT_H_
#include <iostream> 
#include <functional>
#include <string>
#include <vector>
#include "DogeeEnv.h"
#include "DogeeThreading.h"

#define SerialDef(NAME,...) __VA_ARGS__ NAME; static int __REG_MEMBER__##NAME##__;
#define SerialDecl(selfclass,NAME,...)  int selfclass::__REG_MEMBER__##NAME##__=Dogee::ClassSerializer<selfclass>::RegMember<__VA_ARGS__>(offsetof(selfclass,NAME));



namespace Dogee
{
	template<typename T>
	struct CheckPointSerializer
	{
		static void Serialize(size_t offset, std::ostream& os, void* obj)
		{
			T* buf = (T*)((char*)obj + offset);
			os.write((char*)buf,sizeof(T));
		}
		static void Deserialize(size_t offset, std::istream& os, void* obj)
		{
			T* buf = (T*)((char*)obj + offset);
			os.read((char*)buf, sizeof(T));
		}
	};
	template<typename T>
	struct CheckPointSerializer < std ::reference_wrapper<T>>
	{
		static T* getaddr(std::reference_wrapper<T>& r)
		{
			return &(T&)r;
		}
		static void Serialize(size_t offset, std::ostream& os, void* obj)
		{
			std::reference_wrapper<T>* buf = (std::reference_wrapper<T>*)((char*)obj + offset);
			os.write((char*)getaddr(*buf), sizeof(T));
		}
		static void Deserialize(size_t offset, std::istream& os, void* obj)
		{
			std::reference_wrapper<T>* buf = (std::reference_wrapper<T>*)((char*)obj + offset);
			os.read((char*)getaddr(*buf), sizeof(T));
		}
	};

	template <typename T>
	struct ClassSerializer
	{

		typedef std::function<void(std::ostream& os, void*)> outfunc;
		typedef std::function<void(std::istream& os, void*)> infunc;
		static std::vector<outfunc> & get_serialize_func_vec()
		{
			static std::vector<outfunc> myfunc;
			return myfunc;
		}

		static std::vector<infunc> & get_deserialize_func_vec()
		{
			static std::vector<infunc> myfunc2;
			return myfunc2;
		}

		template<typename T2>
		static int RegMember(size_t offset)
		{
			auto fun = std::bind(CheckPointSerializer<T2>::Serialize, offset, std::placeholders::_1, std::placeholders::_2);
			auto fun_in = std::bind(CheckPointSerializer<T2>::Deserialize, offset, std::placeholders::_1, std::placeholders::_2);
			std::vector<outfunc> & vec = get_serialize_func_vec();
			vec.push_back(fun);
			std::vector<infunc> & vec2 = get_deserialize_func_vec();
			vec2.push_back(fun_in);
			return  vec.size();
		}

		static void Serialize(std::ostream& os, const T* w)
		{
			std::vector<outfunc> & vec = get_serialize_func_vec();
			for (auto func : vec)
			{
				func(os, (void*)w);
			}
		}
		static void Deserialize(std::istream& os, T* w)
		{
			std::vector<infunc> & vec = get_deserialize_func_vec();
			for (auto func : vec)
			{
				func(os, (void*)w);
			}
		}

	};

	class CheckPoint
	{
	public:
		void DoCheckPoint()
		{}
		void DoRestart()
		{}
	};


	extern std::function<void(void)> funcCheckPoint;
	extern std::function<void(void)> funcRestart;
	extern std::function<void(std::ostream&)> funcSerialize;
	extern std::function<void(std::istream&)> funcDeserialize;

	template<typename T>
	void RegisterCheckPointFunctions(T* obj)
	{
		funcCheckPoint = std::bind(&T::DoCheckPoint, obj);
		funcRestart = std::bind(&T::DoRestart, obj);
		funcSerialize = std::bind(ClassSerializer<T>::Serialize, std::placeholders::_1,obj);
		funcDeserialize = std::bind(ClassSerializer<T>::Deserialize, std::placeholders::_1, obj);
	}

	void remove_checkpoint_files();
	int MasterCheckCheckPoint();

	template<typename MasterCheckPointType, typename SlaveCheckPointType>
	void DeleteCheckPoint()
	{
		if (!DogeeEnv::checkboject)
			return;
		if (DogeeEnv::isMaster())
		{
			MasterCheckPointType* obj = (MasterCheckPointType*)DogeeEnv::checkboject;
			delete obj;
			DogeeEnv::checkboject = nullptr;
		}
		else
		{
			SlaveCheckPointType* obj = (SlaveCheckPointType*)DogeeEnv::checkboject;
			delete obj;
			DogeeEnv::checkboject = nullptr;
		}
		remove_checkpoint_files();
		DogeeEnv::DeleteCheckpoint = nullptr;
	}

	template<typename MasterCheckPointType, typename SlaveCheckPointType, class... _Types>
	void InitCheckPoint(_Types&&... _Args)
	{
		if (DogeeEnv::isMaster())
		{
			MasterCheckPointType* obj = new MasterCheckPointType(std::forward<_Types>(_Args)...);
			DogeeEnv::checkboject = obj;
			RegisterCheckPointFunctions(obj);
		}
		else
		{
			SlaveCheckPointType* obj = new SlaveCheckPointType();
			DogeeEnv::checkboject = obj;
			RegisterCheckPointFunctions(obj);
		}
		DogeeEnv::DeleteCheckpoint = DeleteCheckPoint<MasterCheckPointType, SlaveCheckPointType>;
	}

	class DCheckpointBarrier : public DBarrier
	{
		DefBegin(DBarrier);
	public:
		DefEnd();
		DCheckpointBarrier(ObjectKey obj_id) : DBarrier(obj_id)
		{
		}
		DCheckpointBarrier(ObjectKey obj_id, int count) : DBarrier(obj_id)
		{
			self->count = count;
		}
		bool Enter();
	};

}



#endif