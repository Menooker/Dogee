#include "DogeeBase.h"
#include "DogeeEnv.h"
namespace Dogee
{
	ObjectKey objid=1;
	uint64_t data[4096 * 32];
	THREAD_LOCAL DObject* lastobject = nullptr;
	SoStorage* DogeeEnv::backend=nullptr;
	DSMCache* DogeeEnv::cache=nullptr;
	bool DogeeEnv::_isMaster = false;

	bool isMaster()
	{
		return DogeeEnv::isMaster();
	}
}
