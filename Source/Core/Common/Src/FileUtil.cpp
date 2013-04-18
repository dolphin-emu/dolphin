// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.


#include "Common.h"
#include "CommonPaths.h"
#include "FileUtil.h"
#include "StringUtil.h"

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>		// for SHGetFolderPath
#include <shellapi.h>
#include <commdlg.h>	// for GetSaveFileName
#include <io.h>
#include <direct.h>		// getcwd
#else
#include <sys/param.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#endif

#if defined(__APPLE__)
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFURL.h>
#include <CoreFoundation/CFBundle.h>
#endif

#include <algorithm>
#include <sys/stat.h>

#include "StringUtil.h"

#ifndef S_ISDIR
#define S_ISDIR(m)  (((m)&S_IFMT) == S_IFDIR)
#endif

#ifdef BSD4_4
#define stat64 stat
#define fstat64 fstat
#endif

// This namespace has various generic functions related to files and paths.
// The code still needs a ton of cleanup.
// REMEMBER: strdup considered harmful!
namespace File
{

// Remove any ending forward slashes from directory paths
// Modifies argument.
static void StripTailDirSlashes(std::string &fname)
{
	if (fname.length() > 1)
	{
		size_t i = fname.length() - 1;
		while (fname[i] == DIR_SEP_CHR)
			fname[i--] = '\0';
	}
	return;
}

// Returns true if file filename exists
bool Exists(const std::string &filename)
{
	struct stat64 file_info;

	std::string copy(filename);
	StripTailDirSlashes(copy);

#ifdef _WIN32
	int result = _tstat64(UTF8ToTStr(copy).c_str(), &file_info);
#else
	int result = stat64(copy.c_str(), &file_info);
#endif

	return (result == 0);
}

// Returns true if filename is a directory
bool IsDirectory(const std::string &filename)
{
	struct stat64 file_info;

	std::string copy(filename);
	StripTailDirSlashes(copy);

#ifdef _WIN32
	int result = _tstat64(UTF8ToTStr(copy).c_str(), &file_info);
#else
	int result = stat64(copy.c_str(), &file_info);
#endif

	if (result < 0) {
		WARN_LOG(COMMON, "IsDirectory: stat failed on %s: %s", 
				 filename.c_str(), GetLastErrorMsg());
		return false;
	}

	return S_ISDIR(file_info.st_mode);
}

// Deletes a given filename, return true on success
// Doesn't supports deleting a directory
bool Delete(const std::string &filename)
{
	INFO_LOG(COMMON, "Delete: file %s", filename.c_str());

	// Return true because we care about the file no 
	// being there, not the actual delete.
	if (!Exists(filename))
	{
		WARN_LOG(COMMON, "Delete: %s does not exist", filename.c_str());
		return true;
	}

	// We can't delete a directory
	if (IsDirectory(filename))
	{
		WARN_LOG(COMMON, "Delete failed: %s is a directory", filename.c_str());
		return false;
	}

#ifdef _WIN32
	if (!DeleteFile(UTF8ToTStr(filename).c_str()))
	{
		WARN_LOG(COMMON, "Delete: DeleteFile failed on %s: %s", 
				 filename.c_str(), GetLastErrorMsg());
		return false;
	}
#else
	if (unlink(filename.c_str()) == -1) {
		WARN_LOG(COMMON, "Delete: unlink failed on %s: %s", 
				 filename.c_str(), GetLastErrorMsg());
		return false;
	}
#endif

	return true;
}

// Returns true if successful, or path already exists.
bool CreateDir(const std::string &path)
{
	INFO_LOG(COMMON, "CreateDir: directory %s", path.c_str());
#ifdef _WIN32
	if (::CreateDirectory(UTF8ToTStr(path).c_str(), NULL))
		return true;
	DWORD error = GetLastError();
	if (error == ERROR_ALREADY_EXISTS)
	{
		WARN_LOG(COMMON, "CreateDir: CreateDirectory failed on %s: already exists", path.c_str());
		return true;
	}
	ERROR_LOG(COMMON, "CreateDir: CreateDirectory failed on %s: %i", path.c_str(), error);
	return false;
#else
	if (mkdir(path.c_str(), 0755) == 0)
		return true;

	int err = errno;

	if (err == EEXIST)
	{
		WARN_LOG(COMMON, "CreateDir: mkdir failed on %s: already exists", path.c_str());
		return true;
	}

	ERROR_LOG(COMMON, "CreateDir: mkdir failed on %s: %s", path.c_str(), strerror(err));
	return false;
#endif
}

// Creates the full path of fullPath returns true on success
bool CreateFullPath(const std::string &fullPath)
{
	int panicCounter = 100;
	INFO_LOG(COMMON, "CreateFullPath: path %s", fullPath.c_str());

	if (File::Exists(fullPath))
	{
		INFO_LOG(COMMON, "CreateFullPath: path exists %s", fullPath.c_str());
		return true;
	}

	size_t position = 0;
	while (true)
	{
		// Find next sub path
		position = fullPath.find(DIR_SEP_CHR, position);

		// we're done, yay!
		if (position == fullPath.npos)
			return true;

		// Include the '/' so the first call is CreateDir("/") rather than CreateDir("")
		std::string const subPath(fullPath.substr(0, position + 1));
		if (!File::IsDirectory(subPath))
			File::CreateDir(subPath);

		// A safety check
		panicCounter--;
		if (panicCounter <= 0)
		{
			ERROR_LOG(COMMON, "CreateFullPath: directory structure is too deep");
			return false;
		}
		position++;
	}
}


// Deletes a directory filename, returns true on success
bool DeleteDir(const std::string &filename)
{
	INFO_LOG(COMMON, "DeleteDir: directory %s", filename.c_str());

	// check if a directory
	if (!File::IsDirectory(filename))
	{
		ERROR_LOG(COMMON, "DeleteDir: Not a directory %s", filename.c_str());
		return false;
	}

#ifdef _WIN32
	if (::RemoveDirectory(UTF8ToTStr(filename).c_str()))
		return true;
#else
	if (rmdir(filename.c_str()) == 0)
		return true;
#endif
	ERROR_LOG(COMMON, "DeleteDir: %s: %s", filename.c_str(), GetLastErrorMsg());

	return false;
}

// renames file srcFilename to destFilename, returns true on success 
bool Rename(const std::string &srcFilename, const std::string &destFilename)
{
	INFO_LOG(COMMON, "Rename: %s --> %s", 
			srcFilename.c_str(), destFilename.c_str());
	if (rename(srcFilename.c_str(), destFilename.c_str()) == 0)
		return true;
	ERROR_LOG(COMMON, "Rename: failed %s --> %s: %s", 
			  srcFilename.c_str(), destFilename.c_str(), GetLastErrorMsg());
	return false;
}

// copies file srcFilename to destFilename, returns true on success 
bool Copy(const std::string &srcFilename, const std::string &destFilename)
{
	INFO_LOG(COMMON, "Copy: %s --> %s", 
			srcFilename.c_str(), destFilename.c_str());
#ifdef _WIN32
	if (CopyFile(UTF8ToTStr(srcFilename).c_str(), UTF8ToTStr(destFilename).c_str(), FALSE))
		return true;

	ERROR_LOG(COMMON, "Copy: failed %s --> %s: %s", 
			srcFilename.c_str(), destFilename.c_str(), GetLastErrorMsg());
	return false;
#else

	// buffer size
#define BSIZE 1024

	char buffer[BSIZE];

	// Open input file
	FILE *input = fopen(srcFilename.c_str(), "rb");
	if (!input)
	{
		ERROR_LOG(COMMON, "Copy: input failed %s --> %s: %s", 
				srcFilename.c_str(), destFilename.c_str(), GetLastErrorMsg());
		return false;
	}

	// open output file
	FILE *output = fopen(destFilename.c_str(), "wb");
	if (!output)
	{
		fclose(input);
		ERROR_LOG(COMMON, "Copy: output failed %s --> %s: %s", 
				srcFilename.c_str(), destFilename.c_str(), GetLastErrorMsg());
		return false;
	}

	// copy loop
	while (!feof(input))
	{
		// read input
		int rnum = fread(buffer, sizeof(char), BSIZE, input);
		if (rnum != BSIZE)
		{
			if (ferror(input) != 0)
			{
				ERROR_LOG(COMMON, 
						"Copy: failed reading from source, %s --> %s: %s", 
						srcFilename.c_str(), destFilename.c_str(), GetLastErrorMsg());
				goto bail;
			}
		}

		// write output
		int wnum = fwrite(buffer, sizeof(char), rnum, output);
		if (wnum != rnum)
		{
			ERROR_LOG(COMMON, 
					"Copy: failed writing to output, %s --> %s: %s", 
					srcFilename.c_str(), destFilename.c_str(), GetLastErrorMsg());
			goto bail;
		}
	}
	// close files
	fclose(input);
	fclose(output);
	return true;
bail:
	if (input)
		fclose(input);
	if (output)
		fclose(output);
	return false;
#endif
}

// Returns the size of filename (64bit)
u64 GetSize(const std::string &filename)
{
	if (!Exists(filename))
	{
		WARN_LOG(COMMON, "GetSize: failed %s: No such file", filename.c_str());
		return 0;
	}

	if (IsDirectory(filename))
	{
		WARN_LOG(COMMON, "GetSize: failed %s: is a directory", filename.c_str());
		return 0;
	}
	
	struct stat64 buf;
#ifdef _WIN32
	if (_tstat64(UTF8ToTStr(filename).c_str(), &buf) == 0)
#else
	if (stat64(filename.c_str(), &buf) == 0)
#endif
	{
		DEBUG_LOG(COMMON, "GetSize: %s: %lld",
				filename.c_str(), (long long)buf.st_size);
		return buf.st_size;
	}

	ERROR_LOG(COMMON, "GetSize: Stat failed %s: %s",
			filename.c_str(), GetLastErrorMsg());
	return 0;
}

// Overloaded GetSize, accepts file descriptor
u64 GetSize(const int fd)
{
	struct stat64 buf;
	if (fstat64(fd, &buf) != 0) {
		ERROR_LOG(COMMON, "GetSize: stat failed %i: %s",
			fd, GetLastErrorMsg());
		return 0;
	}
	return buf.st_size;
}

// Overloaded GetSize, accepts FILE*
u64 GetSize(FILE *f)
{
	// can't use off_t here because it can be 32-bit
	u64 pos = ftello(f);
	if (fseeko(f, 0, SEEK_END) != 0) {
		ERROR_LOG(COMMON, "GetSize: seek failed %p: %s",
			  f, GetLastErrorMsg());
		return 0;
	}
	u64 size = ftello(f);
	if ((size != pos) && (fseeko(f, pos, SEEK_SET) != 0)) {
		ERROR_LOG(COMMON, "GetSize: seek failed %p: %s",
			  f, GetLastErrorMsg());
		return 0;
	}
	return size;
}

// creates an empty file filename, returns true on success 
bool CreateEmptyFile(const std::string &filename)
{
	INFO_LOG(COMMON, "CreateEmptyFile: %s", filename.c_str()); 

	if (!File::IOFile(filename, "wb"))
	{
		ERROR_LOG(COMMON, "CreateEmptyFile: failed %s: %s",
				  filename.c_str(), GetLastErrorMsg());
		return false;
	}

	return true;
}


// Scans the directory tree gets, starting from _Directory and adds the
// results into parentEntry. Returns the number of files+directories found
u32 ScanDirectoryTree(const std::string &directory, FSTEntry& parentEntry)
{
	INFO_LOG(COMMON, "ScanDirectoryTree: directory %s", directory.c_str());
	// How many files + directories we found
	u32 foundEntries = 0;
#ifdef _WIN32
	// Find the first file in the directory.
	WIN32_FIND_DATA ffd;

	HANDLE hFind = FindFirstFile(UTF8ToTStr(directory + "\\*").c_str(), &ffd);
	if (hFind == INVALID_HANDLE_VALUE)
	{
		FindClose(hFind);
		return foundEntries;
	}
	// windows loop
	do
	{
		FSTEntry entry;
		const std::string virtualName(TStrToUTF8(ffd.cFileName));
#else
	struct dirent dirent, *result = NULL;

	DIR *dirp = opendir(directory.c_str());
	if (!dirp)
		return 0;

	// non windows loop
	while (!readdir_r(dirp, &dirent, &result) && result)
	{
		FSTEntry entry;
		const std::string virtualName(result->d_name);
#endif
		// check for "." and ".."
		if (((virtualName[0] == '.') && (virtualName[1] == '\0')) ||
				((virtualName[0] == '.') && (virtualName[1] == '.') && 
				 (virtualName[2] == '\0')))
			continue;
		entry.virtualName = virtualName;
		entry.physicalName = directory;
		entry.physicalName += DIR_SEP + entry.virtualName;

		if (IsDirectory(entry.physicalName.c_str()))
		{
			entry.isDirectory = true;
			// is a directory, lets go inside
			entry.size = ScanDirectoryTree(entry.physicalName, entry);
			foundEntries += (u32)entry.size;
		}
		else
		{ // is a file 
			entry.isDirectory = false;
			entry.size = GetSize(entry.physicalName.c_str());
		}
		++foundEntries;
		// Push into the tree
		parentEntry.children.push_back(entry);		
#ifdef _WIN32 
	} while (FindNextFile(hFind, &ffd) != 0);
	FindClose(hFind);
#else
	}
	closedir(dirp);
#endif
	// Return number of entries found.
	return foundEntries;
}


// Deletes the given directory and anything under it. Returns true on success.
bool DeleteDirRecursively(const std::string &directory)
{
	INFO_LOG(COMMON, "DeleteDirRecursively: %s", directory.c_str());
#ifdef _WIN32
	// Find the first file in the directory.
	WIN32_FIND_DATA ffd;
	HANDLE hFind = FindFirstFile(UTF8ToTStr(directory + "\\*").c_str(), &ffd);

	if (hFind == INVALID_HANDLE_VALUE)
	{
		FindClose(hFind);
		return false;
	}

	// windows loop
	do
	{
		const std::string virtualName(TStrToUTF8(ffd.cFileName));
#else
	struct dirent dirent, *result = NULL;
	DIR *dirp = opendir(directory.c_str());
	if (!dirp)
		return false;

	// non windows loop
	while (!readdir_r(dirp, &dirent, &result) && result)
	{
		const std::string virtualName = result->d_name;
#endif

		// check for "." and ".."
		if (((virtualName[0] == '.') && (virtualName[1] == '\0')) ||
			((virtualName[0] == '.') && (virtualName[1] == '.') && 
			 (virtualName[2] == '\0')))
			continue;

		std::string newPath = directory + DIR_SEP_CHR + virtualName;
		if (IsDirectory(newPath))
		{
			if (!DeleteDirRecursively(newPath))
			{
				#ifndef _WIN32
				closedir(dirp);
				#endif

				return false;
			}
		}
		else
		{
			if (!File::Delete(newPath))
			{
				#ifndef _WIN32
				closedir(dirp);
				#endif

				return false;
			}
		}

#ifdef _WIN32
	} while (FindNextFile(hFind, &ffd) != 0);
	FindClose(hFind);
#else
	}
	closedir(dirp);
#endif
	File::DeleteDir(directory);
		
	return true;
}

// Create directory and copy contents (does not overwrite existing files)
void CopyDir(const std::string &source_path, const std::string &dest_path)
{
#ifndef _WIN32
	if (source_path == dest_path) return;
	if (!File::Exists(source_path)) return;
	if (!File::Exists(dest_path)) File::CreateFullPath(dest_path);

	struct dirent dirent, *result = NULL;
	DIR *dirp = opendir(source_path.c_str());
	if (!dirp) return;

	while (!readdir_r(dirp, &dirent, &result) && result)
	{
		const std::string virtualName(result->d_name);
		// check for "." and ".."
		if (((virtualName[0] == '.') && (virtualName[1] == '\0')) ||
			((virtualName[0] == '.') && (virtualName[1] == '.') &&
			(virtualName[2] == '\0')))
			continue;

		std::string source, dest;
		source = source_path + virtualName;
		dest = dest_path + virtualName;
		if (IsDirectory(source))
		{
			source += '/';
			dest += '/';
			if (!File::Exists(dest)) File::CreateFullPath(dest);
			CopyDir(source, dest);
		}
		else if (!File::Exists(dest)) File::Copy(source, dest);
	}
	closedir(dirp);
#endif
}

// Returns the current directory
std::string GetCurrentDir()
{
	char *dir;
	// Get the current working directory (getcwd uses malloc) 
	if (!(dir = __getcwd(NULL, 0))) {

		ERROR_LOG(COMMON, "GetCurrentDirectory failed: %s",
				GetLastErrorMsg());
		return NULL;
	}
	std::string strDir = dir;
	free(dir);
	return strDir;
}

// Sets the current directory to the given directory
bool SetCurrentDir(const std::string &directory)
{
	return __chdir(directory.c_str()) == 0;
}

#if defined(__APPLE__)
std::string GetBundleDirectory() 
{
	CFURLRef BundleRef;
	char AppBundlePath[MAXPATHLEN];
	// Get the main bundle for the app
	BundleRef = CFBundleCopyBundleURL(CFBundleGetMainBundle());
	CFStringRef BundlePath = CFURLCopyFileSystemPath(BundleRef, kCFURLPOSIXPathStyle);
	CFStringGetFileSystemRepresentation(BundlePath, AppBundlePath, sizeof(AppBundlePath));
	CFRelease(BundleRef);
	CFRelease(BundlePath);

	return AppBundlePath;
}
#endif

#ifdef _WIN32
std::string& GetExeDirectory()
{
	static std::string DolphinPath;
	if (DolphinPath.empty())
	{
		TCHAR Dolphin_exe_Path[2048];
		GetModuleFileName(NULL, Dolphin_exe_Path, 2048);
		DolphinPath = TStrToUTF8(Dolphin_exe_Path);
		DolphinPath = DolphinPath.substr(0, DolphinPath.find_last_of('\\'));
	}
	return DolphinPath;
}
#endif

std::string GetSysDirectory()
{
	std::string sysDir;

#if defined (__APPLE__)
	sysDir = GetBundleDirectory();
	sysDir += DIR_SEP;
	sysDir += SYSDATA_DIR;
#else
	sysDir = SYSDATA_DIR;
#endif
	sysDir += DIR_SEP;

	INFO_LOG(COMMON, "GetSysDirectory: Setting to %s:", sysDir.c_str());
	return sysDir;
}

// Returns a string with a Dolphin data dir or file in the user's home
// directory. To be used in "multi-user" mode (that is, installed).
const std::string& GetUserPath(const unsigned int DirIDX, const std::string &newPath)
{
	static std::string paths[NUM_PATH_INDICES];

	// Set up all paths and files on the first run
	if (paths[D_USER_IDX].empty())
	{
#ifdef _WIN32
		paths[D_USER_IDX] = GetExeDirectory() + DIR_SEP USERDATA_DIR DIR_SEP;
#else
		if (File::Exists(ROOT_DIR DIR_SEP USERDATA_DIR))
			paths[D_USER_IDX] = ROOT_DIR DIR_SEP USERDATA_DIR DIR_SEP;
		else
			paths[D_USER_IDX] = std::string(getenv("HOME") ? 
				getenv("HOME") : getenv("PWD") ? 
				getenv("PWD") : "") + DIR_SEP DOLPHIN_DATA_DIR DIR_SEP;
#endif

		paths[D_GCUSER_IDX]			= paths[D_USER_IDX] + GC_USER_DIR DIR_SEP;
		paths[D_WIIROOT_IDX]		= paths[D_USER_IDX] + WII_USER_DIR;
		paths[D_WIIUSER_IDX]		= paths[D_WIIROOT_IDX] + DIR_SEP;
		paths[D_CONFIG_IDX]			= paths[D_USER_IDX] + CONFIG_DIR DIR_SEP;
		paths[D_GAMECONFIG_IDX]		= paths[D_USER_IDX] + GAMECONFIG_DIR DIR_SEP;
		paths[D_MAPS_IDX]			= paths[D_USER_IDX] + MAPS_DIR DIR_SEP;
		paths[D_CACHE_IDX]			= paths[D_USER_IDX] + CACHE_DIR DIR_SEP;
		paths[D_SHADERCACHE_IDX]	= paths[D_USER_IDX] + SHADERCACHE_DIR DIR_SEP;
		paths[D_SHADERS_IDX]		= paths[D_USER_IDX] + SHADERS_DIR DIR_SEP;
		paths[D_STATESAVES_IDX]		= paths[D_USER_IDX] + STATESAVES_DIR DIR_SEP;
		paths[D_SCREENSHOTS_IDX]	= paths[D_USER_IDX] + SCREENSHOTS_DIR DIR_SEP;
		paths[D_OPENCL_IDX]			= paths[D_USER_IDX] + OPENCL_DIR DIR_SEP;
		paths[D_HIRESTEXTURES_IDX]	= paths[D_USER_IDX] + HIRES_TEXTURES_DIR DIR_SEP;
		paths[D_DUMP_IDX]			= paths[D_USER_IDX] + DUMP_DIR DIR_SEP;
		paths[D_DUMPFRAMES_IDX]		= paths[D_USER_IDX] + DUMP_FRAMES_DIR DIR_SEP;
		paths[D_DUMPAUDIO_IDX]		= paths[D_USER_IDX] + DUMP_AUDIO_DIR DIR_SEP;
		paths[D_DUMPTEXTURES_IDX]	= paths[D_USER_IDX] + DUMP_TEXTURES_DIR DIR_SEP;
		paths[D_DUMPDSP_IDX]		= paths[D_USER_IDX] + DUMP_DSP_DIR DIR_SEP;
		paths[D_LOGS_IDX]			= paths[D_USER_IDX] + LOGS_DIR DIR_SEP;
		paths[D_MAILLOGS_IDX]		= paths[D_USER_IDX] + MAIL_LOGS_DIR DIR_SEP;
		paths[D_THEMES_IDX]			= paths[D_USER_IDX] + THEMES_DIR DIR_SEP;
		paths[D_WIISYSCONF_IDX]		= paths[D_WIIUSER_IDX] + WII_SYSCONF_DIR DIR_SEP;
		paths[F_DOLPHINCONFIG_IDX]	= paths[D_CONFIG_IDX] + DOLPHIN_CONFIG;
		paths[F_DEBUGGERCONFIG_IDX]	= paths[D_CONFIG_IDX] + DEBUGGER_CONFIG;
		paths[F_LOGGERCONFIG_IDX]	= paths[D_CONFIG_IDX] + LOGGER_CONFIG;
		paths[F_MAINLOG_IDX]		= paths[D_LOGS_IDX] + MAIN_LOG;
		paths[F_WIISYSCONF_IDX]		= paths[D_WIISYSCONF_IDX] + WII_SYSCONF;
		paths[F_RAMDUMP_IDX]		= paths[D_DUMP_IDX] + RAM_DUMP;
		paths[F_ARAMDUMP_IDX]		= paths[D_DUMP_IDX] + ARAM_DUMP;
		paths[F_FAKEVMEMDUMP_IDX]	= paths[D_DUMP_IDX] + FAKEVMEM_DUMP;
		paths[F_GCSRAM_IDX]			= paths[D_GCUSER_IDX] + GC_SRAM;
	}

	if (!newPath.empty())
	{
		if(DirIDX != D_WIIROOT_IDX)
			PanicAlert("Trying to change user path other than Wii root");

		if (!File::IsDirectory(newPath))
		{
			WARN_LOG(COMMON, "Invalid path specified %s, Wii user path will be set to default", newPath.c_str());
			paths[D_WIIROOT_IDX] = paths[D_USER_IDX] + WII_USER_DIR;
		}
		else
		{
			paths[D_WIIROOT_IDX] = newPath;
		}

		paths[D_WIIUSER_IDX] = paths[D_WIIROOT_IDX] + DIR_SEP;
		paths[D_WIISYSCONF_IDX]	= paths[D_WIIUSER_IDX] + WII_SYSCONF_DIR + DIR_SEP; 
		paths[F_WIISYSCONF_IDX]	= paths[D_WIISYSCONF_IDX] + WII_SYSCONF;
	}
	return paths[DirIDX];
}

std::string GetThemeDir(const std::string& theme_name)
{
	std::string dir = File::GetUserPath(D_THEMES_IDX) + theme_name + "/";

#if !defined(_WIN32)
	// If theme does not exist in user's dir load from shared directory
	if (!File::Exists(dir))
		dir = SHARED_USER_DIR THEMES_DIR "/" + theme_name + "/";
#endif
	
	return dir;
}

bool WriteStringToFile(bool text_file, const std::string &str, const char *filename)
{
	return File::IOFile(filename, text_file ? "w" : "wb").WriteBytes(str.data(), str.size());
}

bool ReadFileToString(bool text_file, const char *filename, std::string &str)
{
	File::IOFile file(filename, text_file ? "r" : "rb");
	auto const f = file.GetHandle();

	if (!f)
		return false;

	str.resize(GetSize(f));
	return file.ReadArray(&str[0], str.size());
}

IOFile::IOFile()
	: m_file(NULL), m_good(true)
{}

IOFile::IOFile(std::FILE* file)
	: m_file(file), m_good(true)
{}

IOFile::IOFile(const std::string& filename, const char openmode[])
	: m_file(NULL), m_good(true)
{
	Open(filename, openmode);
}

IOFile::~IOFile()
{
	Close();
}

IOFile::IOFile(IOFile&& other)
	: m_file(NULL), m_good(true)
{
	Swap(other);
}

IOFile& IOFile::operator=(IOFile&& other)
{
	IOFile((IOFile&&)other).Swap(*this);
	return *this;
}

void IOFile::Swap(IOFile& other)
{
	std::swap(m_file, other.m_file);
	std::swap(m_good, other.m_good);
}

bool IOFile::Open(const std::string& filename, const char openmode[])
{
	Close();
#ifdef _WIN32
	_tfopen_s(&m_file, UTF8ToTStr(filename).c_str(), UTF8ToTStr(openmode).c_str());
#else
	m_file = fopen(filename.c_str(), openmode);
#endif

	m_good = IsOpen();
	return m_good;
}

bool IOFile::Close()
{
	if (!IsOpen() || 0 != std::fclose(m_file))
		m_good = false;

	m_file = NULL;
	return m_good;
}

std::FILE* IOFile::ReleaseHandle()
{
	std::FILE* const ret = m_file;
	m_file = NULL;
	return ret;
}

void IOFile::SetHandle(std::FILE* file)
{
	Close();
	Clear();
	m_file = file;
}

u64 IOFile::GetSize()
{
	if (IsOpen())
		return File::GetSize(m_file);
	else
		return 0;
}

bool IOFile::Seek(s64 off, int origin)
{
	if (!IsOpen() || 0 != fseeko(m_file, off, origin))
		m_good = false;

	return m_good;
}

u64 IOFile::Tell()
{
	if (IsOpen())
		return ftello(m_file);
	else
		return -1;
}

bool IOFile::Flush()
{
	if (!IsOpen() || 0 != std::fflush(m_file))
		m_good = false;

	return m_good;
}

bool IOFile::Resize(u64 size)
{
	if (!IsOpen() || 0 !=
#ifdef _WIN32
		// ector: _chsize sucks, not 64-bit safe
		// F|RES: changed to _chsize_s. i think it is 64-bit safe
		_chsize_s(_fileno(m_file), size)
#else
		// TODO: handle 64bit and growing
		ftruncate(fileno(m_file), size)
#endif
	)
		m_good = false;

	return m_good;
}

} // namespace
