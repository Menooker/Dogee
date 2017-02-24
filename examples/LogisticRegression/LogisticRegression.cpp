#ifdef _WIN32
#include "stdafx.h"
#endif

#include "DogeeBase.h"
#include "DogeeMacro.h"
#include "DogeeRemote.h"
#include <iostream>
#include "DogeeHelper.h"

using namespace Dogee;
using std::cin;
using std::cout;
using std::endl;

int main(int argc, char* argv[])
{
	HelperInitCluster(argc, argv);

	std::string str;
	std::cin >> str;
	CloseCluster();

	

	return 0;
}

