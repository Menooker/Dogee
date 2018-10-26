
#include <DMap.h>
#include <DogeeString.h>
#include <vector>
#include <iostream>

using namespace Dogee;


class Account;
class Branch : public DObject {
public:
	DefBegin(DObject);
	Def(name, Ref<DString>);
	Def(accounts, Array<Ref<Account>>);
	DefEnd();

	Branch(ObjectKey okey) :DObject(okey) {}
	Branch(ObjectKey okey,std::string name, Array<Ref<Account>> accu) :DObject(okey) 
	{
		self->name = NewObj<DString>(name);
		self->accounts = accu;
	}
};

class Account : public DObject {
public:
	DefBegin(DObject);
	Def(name, Ref<DString>);
	Def(deposit, int);
	Def(branch, Ref<Branch>);
	DefEnd();

	Account(ObjectKey okey) :DObject(okey) {}
	Account(ObjectKey okey, std::string name, int deposit, Ref<Branch> branch) :DObject(okey)
	{
		self->name = NewObj<DString>(name);
		self->deposit = deposit;
		self->branch = branch;
	}
};

int main()
{
	DogeeEnv::InitStorage(BackendType::SoBackendChunkMemcached, CacheType::SoNoCache, *new std::vector<std::string>{}, *new std::vector<int>{},
		*new std::vector<std::string>{ "127.0.0.1" }, *new std::vector<int>{ 11211 }, 0);
	DogeeEnv::InitStorageCurrentThread();

	auto indexer = NewObj<DMap<int, Ref<Account>>>();
	auto arr = NewArray<Ref<Account>>(3);
	auto b1 = NewObj<Branch>("BranchA", arr);
	int idx = 0;
	auto push = [&](std::string s, int dep)
	{
		auto acc = NewObj<Account>(s, dep, b1);
		arr[idx++] = acc;
		indexer->insert(dep, acc);
	};

	push("A", 1000);
	push("B", 1001);
	push("C", 30000);

	arr = NewArray<Ref<Account>>(2);
	b1 = NewObj<Branch>("BranchB", arr);
	idx = 0;

	push("A", 1);
	push("B", 2);

	arr = NewArray<Ref<Account>>(2);
	b1 = NewObj<Branch>("BranchC", arr);
	idx = 0;

	push("A", 10001);

	std::cout << "Account Name\tBranch Name\n";
	for (auto itr = indexer->lowerBound(10000); itr != nullptr; itr = indexer->successor(itr))
	{
		std::cout << itr->value->name->getstr()<< "\t" << itr->value->branch->name->getstr() << "\n";
	}
	
}


void numbertest()
{
	auto dmap = NewObj<DMap<int, Ref<DString>>>();
	dmap->insert(123, NewObj<DString>("11111"));
	dmap->insert(124, NewObj<DString>("22222"));
	dmap->insert(130, NewObj<DString>("333"));
	dmap->insert(141, NewObj<DString>("334"));

	std::cout << dmap->search(120).GetObjectId() << "\n";
	std::cout << dmap->search(123)->value->getstr() << "\n";

	auto itr = dmap->lowerBound(125);
	while (itr != nullptr)
	{
		std::cout << itr->value->getstr() << "\n";
		itr = dmap->successor(itr);
	}
}