#include "DogeeBase.h"
#include "DogeeEnv.h"
namespace Dogee
{
	ObjectKey objid=1;
	THREAD_LOCAL DObject* lastobject = nullptr;
	SoStorage* DogeeEnv::backend=nullptr;
	DSMCache* DogeeEnv::cache=nullptr;
	bool DogeeEnv::_isMaster = false;
	int DogeeEnv::self_node_id=-1;
	int DogeeEnv::num_nodes=0;
	DogeeEnv::InitStorageCurrentThreadProc DogeeEnv::InitStorageCurrentThread = nullptr;
	bool isMaster()
	{
		return DogeeEnv::isMaster();
	}
}
