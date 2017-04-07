#ifndef __DOGEE_FILE_TOOL_H_
#define __DOGEE_FILE_TOOL_H_
#include "Dogee.h"
#include <fstream>
#include <functional>
#include <string>
namespace Dogee
{
	extern void WriteFilePointerCache(std::istream& fs, const std::string path, int mark);
	extern void WriteFilePointerCache(std::istream::pos_type pos, const std::string path, int mark);
	extern bool RestoreFilePointerCache(std::istream& fs, const std::string path, int mark);

	/*
	Skip "num" lines in the file "file", at the path "path". num starts from 0.
	*/
	extern void SkipFileToLine(std::istream& file, const std::string path, unsigned int num);
	//extern void ParseCSV(const char* path, std::function<bool(const char* cell, int line, int index)> func, int skip_to_line = 0, bool use_cache = true);
}
#endif