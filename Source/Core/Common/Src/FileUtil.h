#ifndef _FILEUTIL_H
#define _FILEUTIL_H

#include <string>

class File
{
public:
	static bool Exists(const std::string &filename);
};

#endif
