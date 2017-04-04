// Dogee.cpp : 定义控制台应用程序的入口点。
//
#ifdef _WIN32
#include "stdafx.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <iostream>

#include "DogeeHelper.h" //there is a GCC bug. DogeeHelper.h should be included before DogeeMacro.h
#include "DogeeBase.h"
#include "DogeeStorage.h"
#include "DogeeRemote.h"
#include "DogeeThreading.h"
#include "DogeeMacro.h"

#include <memory>
using namespace Dogee;

int main(int argc, char* argv[])
{
	HelperInitCluster(argc, argv);

	CloseCluster();

	return 0;
}
