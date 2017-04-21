# Dogee
Dogee is a C++ library for distributed programming on distributed shared memory (DSM) by shared memory multithreading model. Usually, DSM systems provide developers "get" and "set" APIs to use the shared memory. Dogee allows developers to operate the distributed shared memory in a similar way they operate local memory by C++, without using "get" and "set" explicitly. By using Birdee, developers can create arrays, shared variables and objects in DSM.

[Birdee](https://github.com/Menooker/Birdee/) is a sister project, which is a new distributed programming language. Dogee can be viewed as Birdee in C++.

## Build instructions

### Build Dogee on Ubuntu
Make sure your g++ compiler supports c++11 features.
```bash
sudo apt-get install libmemcached-dev
make
```
Now the binary files will be ready in the "bin" directory.

### Build Dogee on Windows
Dogee is based on libmemcached. This repository has included the libmemcached library (.lib and .dll) for Windows (for both debug and release mode and both x86 and x64 mode). You just need to open "Dogee.sln" and build Dogee with Visual Studio 2013 (or newer). Note that there are some bugs in the compiler of original version of VS2013, and you should update VS2013 to make Dogee compile.

## Write a Hello World program
We now show that how to write a simple program with Dogee.
```C++
#include <stdio.h>
#include <stdlib.h>
#include <iostream>

#include "DogeeHelper.h" 
#include "DogeeBase.h"
#include "DogeeRemote.h"
#include "DogeeThreading.h"
#include "DogeeMacro.h"

using namespace Dogee;

DefGlobal(g_int, int);

void threadfun(uint32_t param)
{
	std::cout << "The value of g_int is " << g_int << std::endl;
}
int main(int argc, char* argv[])
{
	HelperInitCluster(argc, argv);

	//initialize the shared variables
	g_int = HelperGetParamInt("input_int");

	//create a remote thread on node 1, give the parameter "3232"
	Ref<DThread> thread = NewObj<DThread>(THREAD_PROC(threadfun), 1, 3232);
	std::string dummy;
	std::cout << "Input any string to close the cluster" << std::endl;
	std::cin >> dummy;
	CloseCluster();
	
	return 0;
}

```
This program defines a global variable "g_int" which can be shared between master and slaves. And it creates a thread on slave node 1, and the thread on the slave node gets and prints the value of "g_int". The value of "g_int" is set on master by the command line argument. To compile the program using gcc, you may link it with "-lmemcached -pthread" and "libBirdee.a". On windows you may only need to link it with "Birdee.lib".

## Execution instructions

In this section, we take the Hello World program above as an example. After you build it, we assume you get a binary "HelloWorld". Copy the binary file to all machines in the cluster.

### Start the slave node
```bash
./HelloWorld -s 18080
```
This command will run the program in slave mode and let it wait for the connections from the master, listening port "18080".

### Write a config file on the master
Create a file "DogeeConfig.txt" on the master node. The content of the file should be like:
```bash
DogeeConfigVer= 1
MasterPort= 9090
NumSlaves= 2
NumMemServers= 1
DSMBackend= ChunkMemcached
DSMCache= NoCache
Slaves= 
127.0.0.1 18080
127.0.0.1 18090
MemServers=
127.0.0.1 11211
```
### Run the master node and the whole cluster
Make sure the file "DogeeConfig.txt" is in the current directory. Then on master node, run
```bash
./HelloWorld input_int 23333
```
Note that the parameters following the command "./HelloWorld" will be read by the line of code "HelperGetParamInt("input_int")" in the program, and the parameter "23333" will be put into the global variable "g_int". On slave node, the program will print "The value of g_int is 23333" and then exit.

## API Mannual

### Dogee Class and Object Reference
Include header file "DogeeBase.h" and "DogeeMacro.h" to use the features.

To define a class to be stored in DSM, the class or its base class should extend the Dogee base class "Dogee::DObject". A "referenece" in Dogee is the counterpart of pointers in C++, while pointers point to objects in local memory, and refereneces point to objects in shared memory.

To declare member variables in the class, you should first "call" the macro "DefBegin(BASE_CLASS_NAME);". Then define the members by macro "Def(VARIABLE_NAME,TYPE)". Define referenece by "DefRef(Type,isVirtual,Name)", or by "Def(Dogee::Ref\<TYPE>,VARIABLE_NAME)". Use "self" instead of "this" in the class's member functions. "Call" macro "DefEnd();" after defining the last member variable of a class. An example of defining a Dogee class.

```C++
class clsa : public DObject
{
	DefBegin(DObject);
public:
	Def(i,int);
	Def(arr,Array<float>);
	Def(next,Array<Ref<clsa>>);
	Def(prv,Ref<clsb, true>);
	DefEnd();
	clsa(ObjectKey obj_id) : DObject(obj_id)
	{
	}
	clsa(ObjectKey obj_id,int a) : DObject(obj_id)
	{

		self->arr = NewArray<float>();
		self->next = NewArray<Ref<clsa>>();
		self->arr[2] = a;
	}
};

```

IMPORTANT NOTES: 
 * The macro "DefEnd();" should be delared to be public in a Dogee class. 
 * The first parameter of the consturtors should always be "ObjectKey obj_id", and the base class constructor BASE_CLASS(obj_id) should always be called
 * The constructor CLASS_NAME(ObjectKey obj_id) : BASE_CLASS_NAME(obj_id) should always be declared and have empty body.

To create Dogee class instance, use Dogee::NewObj\<CLASS_NAME>(PARAMETER_LIST), where PARAMETER_LIST is the list of parameters for the class constructor. However, the first paramter of the constructors is always "ObjectKey obj_id", which is provided by the Dogee system, and you should omit the first paramter of the constructors in PARAMETER_LIST. For example, to create a "clsa" object defined above.
```C++
Ref<clsa> ptr = NewObj<clsa>(32);
```
The parameter "32" is passed to the variable "i" in the constructor "clsa(ObjectKey obj_id,int a)".

We then have a reference to an object. A reference is declared by "Dogee:Ref\<CLASS_NAME,isVirtual>". There are two types of references in Dogee. The first one is "non-virtual reference". Non-virtual reference inteprets the referenced objects as "CLASS_NAME" in the declaration, and use virtual function table of the class "CLASS_NAME". Non-virtual references may not accurately intepret the object, since the wrong virtual function can be called by using Non-virtual references. However, for a class without virtual functions,  Non-virtual references are always accurate. Declare a Non-virtual reference by Dogee:Ref\<CLASS_NAME,false> or by Dogee:Ref\<CLASS_NAME> (References are non-virtual by default). Virtual references dynamically find the correct virtual function table for the refernced object, and virtual function calls are always accurately intepreted. Declare a Non-virtual reference by Dogee:Ref\<CLASS_NAME,true>.

References can be used in the same way of pointers.
```C++
ptr->i = 0;
Ref<clsb, true> p2 = Dogee::NewObj<clsb>();
ptr->prv=p2;
```
