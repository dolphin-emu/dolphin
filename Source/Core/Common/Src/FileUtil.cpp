#include "Common.h"
#include "FileUtil.h"

bool File::Exists(const std::string &filename)
{
#ifdef _WIN32
	return GetFileAttributes(filename.c_str()) != INVALID_FILE_ATTRIBUTES;
#else
	return true; //TODO
#endif
}