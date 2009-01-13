// Copyright (C) 2003-2008 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#ifndef _WIN32
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif

#include "Common.h"
#include "MappedFile.h"

namespace Common
{
class CMappedFile
	: public IMappedFile
{
	public:

		CMappedFile(void);
		~CMappedFile(void);
		bool Open(const char* _szFilename);
		bool IsOpen(void);
		void Close(void);
		u64 GetSize(void);
		u8* Lock(u64 _offset, u64 _size);
		void Unlock(u8* ptr);


	private:

		u64 size;

		typedef std::map<u8*, u8*>Lockmap;
		Lockmap lockMap;
#ifdef _WIN32
		HANDLE hFile;
		HANDLE hFileMapping;
#elif POSIX
		int fd;
		typedef std::map<u8*, size_t>Sizemap;
		Sizemap sizeMap;
#endif

		int granularity;
};


CMappedFile::CMappedFile()
{
#ifdef _WIN32
	hFile = INVALID_HANDLE_VALUE;
	SYSTEM_INFO info;
	GetSystemInfo(&info);
	granularity = (int)info.dwAllocationGranularity;
#elif POSIX
	fd = -1;
	granularity = getpagesize(); //sysconf(_SC_PAGE_SIZE);
#endif
}


CMappedFile::~CMappedFile()
{
	Close();
}


bool CMappedFile::Open(const char* filename)
{
	Close();
#ifdef _WIN32
	hFile = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, 0,   OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

	if (hFile == INVALID_HANDLE_VALUE)
	{
		return(false);
	}

	hFileMapping = CreateFileMapping(hFile, 0, PAGE_READONLY, 0, 0, NULL);

	if (hFileMapping == NULL)
	{
		CloseHandle(hFile);
		hFile = 0;
		return(false);
	}

	u32 high = 0;
	u32 low = GetFileSize(hFile, (LPDWORD)&high);
	size = (u64)low | ((u64)high << 32);
#elif POSIX
	fd   = open(filename, O_RDONLY);
	size = lseek(fd, 0, SEEK_END);
	lseek(fd, 0, SEEK_SET);
#endif

	return(true);
}


bool CMappedFile::IsOpen()
{
#ifdef _WIN32
	return(hFile != INVALID_HANDLE_VALUE);

#elif POSIX
	return(fd != -1);
#endif

}


u64 CMappedFile::GetSize()
{
	return(size);
}


void CMappedFile::Close()
{
#ifdef _WIN32
	if (hFile != INVALID_HANDLE_VALUE)
	{
		CloseHandle(hFileMapping);
		CloseHandle(hFile);
		lockMap.clear();
		hFile = INVALID_HANDLE_VALUE;
	}

#elif POSIX
	if (fd != -1)
	{
		lockMap.clear();
		sizeMap.clear();
		close(fd);
	}

	fd = -1;
#endif
}


u8* CMappedFile::Lock(u64 offset, u64 _size)
{
#ifdef _WIN32
	if (hFile != INVALID_HANDLE_VALUE)
#elif POSIX
	if (fd != -1)
#endif
	{
		u64 realOffset = offset & ~(granularity - 1);
		s64 difference = offset - realOffset;
		u64 fake_size = (difference + _size + granularity - 1) & ~(granularity - 1);
#ifdef _WIN32
		u8* realPtr = (u8*)MapViewOfFile(hFileMapping, FILE_MAP_READ, (DWORD)(realOffset >> 32), (DWORD)realOffset, (SIZE_T)(_size));

		if (realPtr == NULL)
		{
			return(NULL);
		}

#elif POSIX
		// TODO
		u8* realPtr = (u8*)mmap(0, fake_size, PROT_READ, MAP_PRIVATE, fd, (off_t)realOffset);

		if (!realPtr)
		{
			PanicAlert("Map Failed");
			exit(0);
		}
#endif

		u8* fakePtr = realPtr + difference;
		//add to map
		lockMap[fakePtr] = realPtr;
#ifndef _WIN32
		sizeMap[fakePtr] = _size + difference;
#endif
		return(fakePtr);
	}
	else
	{
		return(0);
	}
}


void CMappedFile::Unlock(u8* ptr)
{
	if (ptr != 0)
	{
		Lockmap::iterator iter = lockMap.find(ptr);

		if (iter != lockMap.end())
		{
#ifdef _WIN32
			UnmapViewOfFile((*iter).second);
#else
			munmap((*iter).second, sizeMap[ptr]);
#endif
			lockMap.erase(iter);
		}
		else
		{
			PanicAlert("CMappedFile : Unlock failed");
		}
	}
}


IMappedFile* IMappedFile::CreateMappedFileDEPRECATED(void)
{
	return(new CMappedFile);
}
} // end of namespace Common
