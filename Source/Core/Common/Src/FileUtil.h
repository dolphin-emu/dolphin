#ifndef _FILEUTIL_H
#define _FILEUTIL_H

#include <string>

class File
{
public:
	static bool Exists(const std::string &filename);
	static void Launch(const std::string &filename);
	static void Explore(const std::string &path);
	static bool IsDirectory(const std::string &filename);
	static bool CreateDir(const std::string &filename);
        static std::string GetUserDirectory();
};

#endif
