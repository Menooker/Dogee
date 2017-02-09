#ifndef __DOGEE_REMOTE_H_ 
#define __DOGEE_REMOTE_H_ 

#include "Dogee.h"
#include <vector>
#include <string>

namespace Dogee
{
	/*
	Wait for connections from Slaves and initialize the cluster. Set isMaster=true. Wait until the
	number of slaves equals to SlaveCount.
	*/
	void MasterConnect( std::vector<std::string>& Nodes, std::vector<int>& Ports);

	/*
	Called by slave nodes, will not return until the master close the cluster. Set isMaster=false.
	*/
	void SlaveMain(int SlavePort);

}

#endif