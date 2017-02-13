// Dogee.cpp : 定义控制台应用程序的入口点。
//
#ifdef _WIN32
#include "stdafx.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <iostream>

#include "DogeeBase.h"
#include "DogeeMacro.h"
#include "DogeeStorage.h"
#include "DogeeRemote.h"
#include "DogeeThreading.h"
#include <memory>
using namespace Dogee;


class clsb : public DObject
{
	DefBegin(DObject);
public:
	DefEnd();
	virtual void aaaa() = 0;
	clsb(ObjectKey obj_id) : DObject(obj_id)
	{}
};

class clsc : public clsb
{
	DefBegin(clsb);
public:
	DefEnd();
	virtual void aaaa()
	{
		std::cout << "Class C" << std::endl;
	}
	clsc(ObjectKey obj_id) : clsb(obj_id)
	{}
};


class clsd : public clsb
{
	DefBegin(clsb);
public:
	DefEnd();
	virtual void aaaa()
	{
		std::cout << "Class D" << std::endl;
	}
	clsd(ObjectKey obj_id) : clsb(obj_id)
	{}
};

class clss : public DObject
{
	DefBegin(DObject);
public:
	//Def(int, i);
	//Def(Array<float>, arr);
	//Def(Array<Ref<clss>>, next);
	//Def(Array<Array<int>>, mat);
	//DefRef(clsb, true, prv);
	DefEnd();
	clss(ObjectKey obj_id) : DObject(obj_id)
	{
	}
};


class clsa : public DObject
{
	DefBegin(DObject);
public:
	Def(int, i);
	Def(Array<float>, arr);
	Def(Array<Ref<clsa>>, next);
	Def(Array<Array<int>>, mat);
	DefRef (clsb, true, prv);
	DefEnd();
	clsa(ObjectKey obj_id) : DObject(obj_id)
	{
	}
	clsa(ObjectKey obj_id,int a) : DObject(obj_id)
	{

		self->arr = Dogee::NewArray<float>();
		self->next = Dogee::NewArray<Ref<clsa>>();
		self->mat = Dogee::NewArray<Array<int>>();
		self->mat[0] = Dogee::NewArray<int>();
		self->arr[2] = a;
	}
};



template<typename T> void aaa(T* dummy)
{
	std::cout << "Normal" << std::endl;
}
template<> void aaa(clsa * dummy)
{
	std::cout << "Int" << std::endl;
}


DefGlobal(int, g_i);

RegVirt(clsc);
RegVirt(clsd);


DefGlobal(Ref<DSemaphore>, sem);

void threadfun(uint32_t param)
{
	std::cout << "Start" << g_i << std::endl ;
	sem->Acquire(-1);
	std::cout << "Create Thread" << g_i << std::endl<<param;
}
RegFunc(threadfun);


template <typename T>
void readtest()
{
	auto ptr2 = Dogee::NewArray<T>();
	for (int i = 0; i < 100; i++)
	{
		ptr2[i] = i;
	}
	T buf[100];
	ptr2->CopyTo(buf, 0, 100);
	for (int i = 0; i < 100; i++)
	{
		if (buf[i] != i)
		{
			std::cout << "ERR" << i << std::endl;
			break;
		}
	}
	std::cout << "OK" << std::endl;
}

void objecttest();
int main(int argc, char* argv[])
{
	if (argc == 3 && std::string(argv[1])=="-s")
	{
		RcSlave(atoi(argv[2]));
	}
	else
	{
		std::vector<std::string> hosts = { "", "127.0.0.1" };
		std::vector<int> ports = { 8080,18080 };
		std::vector<std::string> mem_hosts = { "127.0.0.1" };
		std::vector<int> mem_ports = { 11211 };
		DogeeEnv::InitStorage(BackendType::SoBackendMemcached, CacheType::SoNoCache,mem_hosts, mem_ports);

		readtest<int>();
		readtest<float>();
		readtest<double>();
		readtest<long long>();
		/*RcMaster(hosts, ports, mem_hosts, mem_ports, BackendType::SoBackendMemcached, CacheType::SoNoCache);
		g_i = 123445;
		sem = NewObj<DSemaphore>(0);
		std::cout << sem->GetObjectId();
		Ref<DThread> thread = NewObj<DThread>(threadfun, 1, 3232);
		int i;
		std::cin >> i;
		sem->Release();
		std::cin >> i;
		CloseCluster();*/
	}

	return 0;
}

void objecttest()
{
	Ref<clss> dd[1] = { 0 };
	auto ptr2 = Dogee::NewArray<Ref<clss>>();
	
	ptr2->CopyTo(dd, 0, 1);
	auto ptr = Dogee::NewObj<clsa>(12);
	//AutoRegisterObject<clsa> aaaaaa;
	ptr->next[0] = ptr;
	ptr->next[0]->i = 123;
	ptr->arr[0] = 133;
	ptr->arr[0] = ptr->arr[0] + 1;
	float buf[10];
	ptr->arr->CopyTo(buf, 0, 10);

	Array<int> arr_int[1] = { 0 };
	ptr->mat[0][2] = 123;
	ptr->mat->CopyTo(arr_int, 0, 1);
	aaa((clsb*)0);
	//Ref<clsa,true> ppp(12);
	Ref<clsc, true> p2 = Dogee::NewObj<clsc>();
	ptr->prv = p2;
	ptr->prv->aaaa();
	Ref<clsd, true> p3 = Dogee::NewObj<clsd>();
	ptr->prv = p3;
	ptr->prv->aaaa();
	g_i = 0;
	Array<int> ppp = Dogee::force_cast<Array<int>>(ptr);
	std::cout << g_i << std::endl;
	std::cout << ptr->mat[0][2] << std::endl;
	std::cout << ptr->arr[0];
}