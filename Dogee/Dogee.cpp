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


void threadfun(uint32_t param)
{
	std::cout << "Create Thread" << g_i << std::endl<<param;
}
RegFunc(threadfun);


int main(int argc, char* argv[])
{
	if (argc == 3 && std::string(argv[1])=="-s")
	{
		RcSlave(atoi(argv[2]));
	}
	else
	{
		std::vector<std::string> hosts = { "", "127.0.0.1" };
		std::vector<int> ports = { 8090,18090 };
		std::vector<std::string> mem_hosts = { "127.0.0.1" };
		std::vector<int> mem_ports = { 11211 };
		RcMaster(hosts, ports, mem_hosts, mem_ports, BackendType::SoBackendMemcached, CacheType::SoNoCache);
		g_i = 123445;
		Ref<DThread> thread = NewObj<DThread>(threadfun, 1, 3232);
		int i;
		std::cin >> i;
		CloseCluster();
	}

	return 0;
}

void objecttest()
{
	auto ptr = Dogee::NewObj<clsa>(12);
	//AutoRegisterObject<clsa> aaaaaa;
	ptr->next[0] = ptr;
	ptr->next[0]->i = 123;
	ptr->arr[0] = 133;
	ptr->arr[0] = ptr->arr[0] + 1;
	ptr->mat[0][2] = 123;
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