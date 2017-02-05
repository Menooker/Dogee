// Dogee.cpp : 定义控制台应用程序的入口点。
//
#ifdef _WIN32
#include "stdafx.h"
#endif

#include <stdio.h>
#include <iostream>

#include "DogeeBase.h"

#include "DogeeMacro.h"

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
	Def(Array<Reference<clsa>>, next);
	DefRef (clsb, true, prv);
	DefEnd();
	clsa(ObjectKey obj_id) : DObject(obj_id)
	{
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


RegVirt(clsc);
RegVirt(clsd);
int main(int argc, char* argv[])
{
	
	auto ptr = Dogee::Alloc<clsa>::tnew();
	ptr->arr = Dogee::Alloc<float>::newarray();
	ptr->next = Dogee::Alloc<Reference<clsa>>::newarray();

	//AutoRegisterObject<clsa> aaaaaa;

	ptr->next[0] = ptr;
	ptr->next[0]->i = 123;
	ptr->arr[0] = 133;
	ptr->arr[0]=ptr->arr[0] + 1;
	aaa((clsb*)0);
	//Reference<clsa,true> ppp(12);
	Reference<clsc, true> p2 = Dogee::Alloc<clsc>::tnew();
	ptr->prv=p2;
	ptr->prv->aaaa();
	Reference<clsd, true> p3 = Dogee::Alloc<clsd>::tnew();
	ptr->prv = p3;
	ptr->prv->aaaa();
	Array<int> ppp = Dogee::force_cast<decltype(ptr), Array<int>>(ptr);
	std::cout << clsa::CLASS_ID;

	return 0;
}

