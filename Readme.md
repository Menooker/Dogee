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

### Run memcached
The current version of Dogee is supported by memcached. On master and slaves, you should run mamcached in advance. Note that memcached may evict data as it is a cache service. To use it as an implementation of DSM, you should add "-M" option in the arguments when starting memcached, and adjust the memory limit by the argement "-m MEM_SIZE".

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

 * "MasterPort" is the port that master node will listen.
 * "DSMBackend" will select the kind of DSM for Dogee. Currently, we support coarse-grained mode ("ChunkMemcached") and fine-grained mode ("Memcached") of memcached as DSM. Coarse-grained mode usually works better in applications which often move large chunks of data between DSM and local memory.
 * "DSMCache" will select the kind of cache for DSM. The available options include "NoCache" (using DSM directly) and "WriteThroughCache" (using a write through cache)
 
### Run the master node and the whole cluster
Make sure the file "DogeeConfig.txt" is in the current directory. Then on master node, run
```bash
./HelloWorld input_int 23333
```
Note that the parameters following the command "./HelloWorld" will be read by the line of code "HelperGetParamInt("input_int")" in the program, and the parameter "23333" will be put into the global variable "g_int". On slave node, the program will print "The value of g_int is 23333" and then exit.

## API Mannual

### Shared Class and Object Reference
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

		self->arr = NewArray<float>(10);
		self->next = NewArray<Ref<clsa>>(10);
		self->arr[2] = a;
	}
	void Destroy()
	{
		std::cout << "destroctor" << std::endl;
		DelArray<float>(self->arr);
		DelArray<Ref<clsa>>(self->next);
	}
};

```

IMPORTANT NOTES: 
 * The macro "DefEnd();" should be delared to be public in a Dogee class. 
 * The first parameter of the consturtors should always be "ObjectKey obj_id", and the base class constructor BASE_CLASS(obj_id) should always be called
 * The constructor CLASS_NAME(ObjectKey obj_id) : BASE_CLASS_NAME(obj_id) should always be declared and have empty body.
 * Override Destroy() as the destructor.

To create Dogee class instance, use Dogee::NewObj\<CLASS_NAME>(PARAMETER_LIST), where PARAMETER_LIST is the list of parameters for the class constructor. However, the first paramter of the constructors is always "ObjectKey obj_id", which is provided by the Dogee system, and you should omit the first paramter of the constructors in PARAMETER_LIST. For example, to create a "clsa" object defined above.
```C++
Ref<clsa> ptr = NewObj<clsa>(32);
DelObj(ptr);
```
The parameter "32" is passed to the variable "i" in the constructor "clsa(ObjectKey obj_id,int a)".

We then have a reference to an object. A reference is declared by "Dogee::Ref\<CLASS_NAME,isVirtual>". There are two types of references in Dogee. The first one is "non-virtual reference". Non-virtual reference inteprets the referenced objects as "CLASS_NAME" in the declaration, and use virtual function table of the class "CLASS_NAME". Non-virtual references may not accurately intepret the object, since the wrong virtual function can be called by using Non-virtual references. However, for a class without virtual functions,  Non-virtual references are always accurate. Declare a Non-virtual reference by Dogee::Ref\<CLASS_NAME,false> or by Dogee::Ref\<CLASS_NAME> (References are non-virtual by default). Virtual references dynamically find the correct virtual function table for the refernced object, and virtual function calls are always accurately intepreted. Declare a Non-virtual reference by Dogee::Ref\<CLASS_NAME,true>.

If your class does not include any virtual functions or you know which specific class the reference exactly points to, you should use Non-virtual reference, for they are more efficient.

References can be used in the same way of pointers.
```C++
ptr->i = 0;
Ref<clsb, true> p2 = Dogee::NewObj<clsb>();
ptr->prv=p2;
```

### Shared array
The Shared array is the array that is stored on DSM. To define a reference to a shared array, you should use the template type Dogee::Array\<Type> in "DogeeBase.h", where "Type" is in any type of primitive types of C++ (like int, float, double) or a reference to shared object or array. To allocate a shared array, use API "NewArray\<Type>(NUM_ELEMENT)" where "Type" is the element type and "NUM_ELEMENT" is the number of elements in the array.

There are some advanced APIs for shared arrays, which are defined as member functions of shared arrays.
```C++
void Fill(std::function<T(uint32_t)> func, uint32_t start_index, uint32_t len);
void CopyTo(T* localarr, uint32_t start_index,uint32_t copy_len);
void CopyFrom(T* localarr, uint32_t start_index, uint32_t copy_len);
```

The "Fill" function fills the array elements starting from "start_index", and the number of elements to be filled in is "len". A std::function is passed to the API. The user-defined function takes the index to be filled in as input and returns the value to be filled in.

The "CopyTo" function copies the data in the shared array from DSM to a local buffer. The "CopyTo" function copies the data a local buffer into the shared array in DSM. 

We illustrate the APIs with the following example.
```C++
Array<float> arr = NewArray<float>(10);
arr[0]=123;
std::cout<<arr[0];
arr->Fill([](uint32_t i){
	return (float)rand();
	},0, 10); // fill the array with random values
float local[10];
arr->CopyTo(local,0,10); //copy the array to local buffer
DelArray<float>(arr);
```

### Shared Variables
You should include "DogeeBase.h" and "DogeeMacro.h" to use this feature. You can define a shared variable that is shared among the clusters by the macro "DefGlobal(NAME,TYPE)" in a global scope (where global variables are defined), where NAME is the variable name and TYPE is the variable type. TYPE is in any type of primitive types of C++ (like int, float, double) or a reference to shared object or array. If you want to use the shared variable defined in other ".cpp" files, you may declare a shared variable rather than defining one, where you should use the macro "ExternGlobal(NAME,TYPE)". The usage of shared variable is almost the same as global variables in C++. See the following example.
```C++
DefGlobal(foo,Ref<clsa>);
DefGlobal(bar,int);

void func1()
{
	foo=NewObj<clsa>(32);
	bar=10;
}

void func2()
{
	foo->i=123;
	bar=bar+2;
}
```

Sometimes, we use shared variables just for storing configurations globally, e.g. the learning rate of the distributed machine learning algorithm. In these cases, using shared variables is inefficient, because accessing shared variables involves accessing DSM, which may be time consuming. Noticing that the values of some shared variables will never change after initialization in many applications, we introduce a special kind of shared variables called Const Shared Variables. Const Shared Variables are similar to normal Shared Variables, except that they cache the value of the variable in local memory. These variables are initialized by the master and they assume that the value of the variable will never change after the first remote thread is created. Every slave node will fetch the value of the variable from DSM before the first remote thread on it is started and cache the data in local DSM. Note that the Const Shared Variables are read-only for slaves and should be initialized by master before creating remote threads. Define and declare Const Shared Variables by macros "DefConst" and "ExternConst"

### Cluster management
```C++
extern void HelperInitCluster(int argc, char* argv[]);
extern void CloseCluster();
```
The above is the main interfaces for cluster management. For simplicity, we have omitted some trivial codes in the declaration of the APIs provided in this section.

The HelperInitCluster API is responsible for initializing Dogee environment and establishing connections among the compute nodes during initialization. 
This function acts differently on the master and slaves. 
Specifically, it parses the arguments from the command line, and then decides whether the current process will run under master mode or slave mode. HelperInitCluster should be called in "main()" before any Dogee APIs are called. You should forward argc and argv parameters of main to it too. Note that the master node and the slave nodes runs the same program, but with different starting arguments. If you start a Dogee program by command "-s PORT", where PORT is the port number, it will run in slave mode.

In master mode, HelperInitCluster initializes the cluster by i) reading the settings from local configuration file, "DogeeConfig.txt" at the "current" directory, ii) connecting Dogee master to the selected slaves and key-value store servers, and iii) forwarding configuration information to all slaves. In master mode, the function will return and continue the execution of "main()". In slave mode, HelperInitCluster makes the slave node wait for the connection from the master and respond to the master's commands and it will never return to "main()" (it will call exit() when the cluster is closed).

The CloseCluster API is used to shut down the cluster, which can only be invoked by \bird master.

### Thread management
```C++
class DThread : public DEvent{
	ThreadState GetState();
	template<typename T>
	DThread(ObjectKey obj_id, T func, int node_id, uint32_t param);
	bool Join(int timeout=-1);
};
```
Dogee allows users to specify the number of working threads to be created in each slave. This is achieved by using the DThread class, a shared class in Dogee. To create a working thread on a slave node and specify the entry function of it, users need to create a DThread object using NewObj API.
An instance of DThread represents a working thread on a slave node.

The constructor of DThread takes three arguments: 
 * func is a user-defined entry function for working threads. For normal C++ function, you should wrap the function name with a THREAD_PROC macro. This parameter also accepts function objects and you do not need to wrap a function object by the macro. You need to make sure there are no local pointers contained in the function object. You can use lambda with/without capture in "func" parameter. Note that don't capture variables by reference and don't capture pointers in lambda for the parameter, or an undefiend bahavior will occur. 
 * node_id is the ID of the slave node where the working thread is created.
 * param is the parameter forwarded to the user-defined entry function.
 
Users can get the state of a thread (i.e., alive or completed) via the member function GetState. 

"Join" API will let the current thread blocked until the completion of the thread. It takes an parameter of "timeout", in milliseconds. If timeout is not -1, the current thread will be waiting at most "timeout" milliseconds. If the wait time is out, Join will return false. Otherwise, it returns true.

Note that we declare DThread as a shared class so that all its instances are stored in DSM and publicly available to Dogee cluster, which is critical for communication among Dogee master and slaves. 


### Distributed Thread Synchronization

```C++
class DBarrier : public DObject{
	DBarrier(ObjectKey obj_id, int count);
	bool Enter(int timeout = -1);
};
class DSemaphore : public DObject{
	DSemaphore(ObjectKey obj_id, int count);
	bool Acquire(int timeout = -1);
	void Release();
};
class DEvent : public DObject{
	DEvent(ObjectKey obj_id, bool auto_reset, bool is_signal);
	void Set();
	void Reset();
	bool Wait(int timeout = -1);
};
```

The above lists Dogee interfaces for synchronizing distributed threads. They are encapsulated as shared classes.

Note that just like the "Join" method of DThread, the interfaces that involves blocking the current thread has a parameter of "timeout". If timeout is -1, it will wait infinitely until the event it waits happens. Otherwise, the waiting has a timeout of "timeout" in milliseconds. If the waiting time is out, "false" will be returned.

The DBarrier class provides barrier synchronization pattern to keep distributed threads (i.e., main thread and working threads) in the same pace, which is useful in performing synchronous iterative computation. Typically, a DBarrier object is created by the main thread in the master. The constructor in DBarrier is then invoked to create the barrier and specify the total number of threads to be synchronized on the barrier. The reference to a DBarrier object can be stored in a shared global variable so that all threads in the cluster can share this barrier. After setting up all the working threads in slaves, a thread calls Enter function and waits at the barrier until all the invloved working threads reach the barrier. When the last thread arrives at the barrier, all the threads will resume their execution. 

The DSemaphore class allows a specified number of threads to access a resource. During the creation of a RSemaphore object, we set a non-negative resource count as its value. There are two ways to manipulate a semaphore. Acquire function is used to request a resource and decrement the resource count; Release function is used to release the resource and increment the resource count. Threads that request a resource with non-positive semaphore value will be blocked until other threads release that resource and the semaphore value becomes positive. 

The DEvent class acts as a distributed version of [Windows Event](https://msdn.microsoft.com/zh-cn/library/windows/desktop/ms682655(v=vs.85).aspx). It can either be signaled or un-signaled. If it is signaled, all threads waiting for the DEvent will continue executing after calling "Wait". Otherwise, threads are blocked until some other thread calls "Set". Note that in the constructor of DEvent, it takes two parameters. "auto_reset" specifies the mode of the DEvent. If it is set "true", the DEvent object is in auto reset mode, and the event will be automatically reset after "Set" is called. If there are multiple threads waiting for a "auto reset" DEvent, only one of the thread will resume working after a call to "Set". If "auto_reset" is set "false", all threads waiting for the event will continue working after a call to "Set". The parameter "is_signal" specifies the initial state of the event. The "Reset" method will reset the event.

### Accumulator
We found that many real applications require to perform vector-wise accumulation.
For example, in PageRank computation, each working thread maintains a subset of vertices with their outgoing edges and computes the PageRank credits from its own vertices to the destination vertices along the edges. In each iteration, 
credit vectors from the working threads are summed together to produce the new PageRank values for all vertices.

A straightforward way for vector accumulation with Dogee is to ask working threads to transfer local vectors to DSM and then choose one thread to fetch all vectors to perform the final accumulation. 
Let $N$ be the number of working threads. The above method incurs high network cost, i.e., the size of data to be transferred is (2N+1)*vector_size.

#### Addition Accumulator
```C++
template<typename T>
	class DAddAccumulator : public DAccumulator<T>
	{
	public:
		DAccumulator(ObjectKey k, Array<T> outarr, uint32_t outlen, uint32_t in_num_user) :DBaseAccumulator(k);
		bool AccumulateAndWait(T* buf, uint32_t len, T threshold = 0, int timeout = -1);
	}
```
Dogee provides DAddAccumulator class for users to perform vector accumulation more efficiently as well as hide data transfer details involved in vector accumulation.
Users can create a shared DAddAccumulator object and initialize it with the number N of working threads involved in the accumulation (in_num_user) and an output shared array (outarr) in DSM and its length (outlen).
The working threads can invoke AccumulateAndWait function defined in DAddAccumulator to send out their local vectors and Dogee will compute the final accumulated result and store it in the output shared array automatically.
The AccumulateAndWait function also serves as a synchronization point which will not return until all the $N$ threads send out their local vectors.
 * "buf" is the local vector to send.
 * "len" is its length. 
 * "threshold" is the "zero-threshold". Any number of absolute value less than it will be treated as zero.
 * "timeout" is the synchronization timeout, in milliseconds
 * AccumulateAndWait returns false when the waiting time is out. Otherwise, it returns true.

Our implementation of Accumulator reduces the data transfer cost to (N+1)*vector_size.

#### User Accumulator
Also, we provide a more general Accumulator class that can do any user function (other than Addition in AddAccumulator) in accumuation. Users should extend the abstract class DAccumulator.
```C++
	template<typename T>
	class DAccumulator : public DBaseAccumulator
	{
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
		bool AccumulateAndWait(T* buf, uint32_t len, T threshold = 0, int timeout = -1);

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
	}


```
There are two virtual method that should be overrided. One is DoAccumulateDense, and the other is DoAccumulateSparse.

#### Functional Accumulator
You can use DFunctionalAccumulator to write your own accumulator. You just need to write an accumlate function like:
```C++
void multiply(float in,uint32_t index,float& out)
{
	out=out*in;
}
```
Now define your own DFunctionalAccumulator by using
```C++
typedef DFunctionalAccumulator<float,multiply> MulAccumulator;
auto ptr=NewObj<MulAccumulator>(arr,len,thread_count);
```

### Fault tolerance: Checkpoint
Dogee provides fault tolerance by checkpoints via files. Dogee will peroidically write the whole DSM data and user-specified local memory data to local files. Dogee provides a special barrier DCheckpointBarrier. Every time a node enters DCheckpointBarrier, Dogee will do a checkpoint on disk.

#### Core of Dogee Fault tolerance: Checkpoint class
To use Dogee's checkpoint, you should first write two checkpoint class, one for slave nodes, one for master node. A checkpoint class specifies the local variables that a master/slave node wants to save, the bahavior of creating a checkpoint and the behavior of restoring from a checkpoint. Note that all data in DSM (including global shared variables) will be automatically dumped into the checkpoint file when Dogee is doing a checkpoint.

We now illustrate Checkpoint class by an example. Suoopose on slave nodes, we have a global variable (stored in local memory) called "slave_count" that we want to store in/restore from the checkpoints. Similarly we have a global variable (stored in local memory) called "master_count". We first create a checkpoint class for slave nodes.

```C++
class SlaveCheckPoint :public CheckPoint
{
public:
	SerialDef(slave_count, std::reference_wrapper<int>);
	SlaveCheckPoint() :slave_count(::slave_count)
	{}
};
SerialDecl(SlaveCheckPoint, slave_count, std::reference_wrapper<int>);
```

In this class, we use macro SerialDef(variable_name,type) to declare a member in the class, and macro SerialDecl(class_name,variable_name,type) outside of the class to define a member. The members of Checkpoint classes will be serialized into checkpoint files. Note that we use a std::reference_wrapper to define the member slave_count, and associate it with the gloabl variable slave_count in local memory. Dogee will automatically write the value of global variable "slave_count" into checkpoint file and restore it from checkpoint file.

In this example, we write a different checkpoint class for master node.

```C++
class MasterCheckPoint:public CheckPoint
{
public:
	SerialDef(master_count, std::reference_wrapper<int>);
	MasterCheckPoint() :master_count(::master_count)
	{}
	void DoRestart()
	{
		cout<<"Restored";
		real_main();
	}
	void DoCheckpoint()
	{
		cout<<"Do checkpoint";
	}	
};
SerialDecl(MasterCheckPoint,master_count, std::reference_wrapper<int>);
```
We similarly store a reference to "master_count" in MasterCheckPoint, and it will also automatically saved and restored. Also, in checkpoint classes for both master and slave nodes, we can override DoRestart() and DoCheckpoint() method to specify the behavior of restarting and checkpointing. Note that if a Dogee program will checkpoint on breaks down half way and gets restarted, the process on master node will invoke DoRestart() of checkpoint class of master node and will invoke exit() when DoRestart() is done.

#### Use of Dogee Fault tolerance
To start a Dogee program with checkpoint on, we call HelperInitClusterCheckPoint instead of HelperInitCluster at the start of main().
```C++
template<typename MasterCheckPointType, typename SlaveCheckPointType>
void HelperInitClusterCheckPoint(int argc, char* argv[], const char* appname = nullptr)
```
MasterCheckPointType and SlaveCheckPointType are checkpoint classes for master and slave. "appname" is the name of the current application. (appname will be used to name the checkpoint files) For the example above, we call:
```C++
HelperInitClusterCheckPoint<MasterCheckPoint, SlaveCheckPoint>(argc, argv,"Test");
```
