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

#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#include <stdio.h>
#endif

#include "Common.h"
#include "StringUtil.h"
#include "DynamicLibrary.h"

DynamicLibrary::DynamicLibrary()
{
	library = 0;
}

#ifdef _WIN32
std::string GetLastErrorAsString()
{
	LPVOID lpMsgBuf = 0;
	DWORD error = GetLastError();
	FormatMessage( 
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL,
		error,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
		(LPTSTR) &lpMsgBuf,
		0, NULL);
	std::string s;
	if (lpMsgBuf)
	{
		s = ((char *)lpMsgBuf);
		LocalFree(lpMsgBuf);
	} else {
		s = StringFromFormat("(unknown error %08x)", error);
	}
	return s;
}
#endif

// ------------------------------------------------------------------
/* Loading means loading the dll with LoadLibrary() to get an instance to the dll.
   This is done when Dolphin is started to determine which dlls are good, and
   before opening the Config and Debugging windowses from Plugin.cpp and
   before opening the dll for running the emulation in Video_...cpp in Core. */
// -----------------------
int DynamicLibrary::Load(const char* filename)
{
	if (!filename || strlen(filename) == 0)
	{
		LOG(MASTER_LOG, "Missing filename of dynamic library to load");
		return 0;
	}
	LOG(MASTER_LOG, "Trying to load library %s", filename);

	if (IsLoaded())
	{
		LOG(MASTER_LOG, "Trying to load already loaded library %s", filename);
		return 2;
	}

#ifdef _WIN32
	library = LoadLibrary(filename);
	if (!library) {
		LOG(MASTER_LOG, "Error loading DLL %s: %s", filename, GetLastErrorAsString().c_str());
		return 0;
	}
#else
	library = dlopen(filename, RTLD_NOW | RTLD_LOCAL);
	if (!library)
	{
		#ifdef LOGGING
		LOG(MASTER_LOG, "Error loading DLL %s: %s", filename, dlerror());
		#else
		printf("Error loading DLL %s: %s", filename, dlerror());
		#endif
		return false;
	}
#endif
	library_file = filename;
	return 1;
}


void DynamicLibrary::Unload()
{
	if (!IsLoaded())
	{
		PanicAlert("Trying to unload non-loaded library");
		return;
	}

#ifdef _WIN32
	FreeLibrary(library);
#else
	dlclose(library);
#endif
	library = 0;
}


void* DynamicLibrary::Get(const char* funcname) const
{
	void* retval;
#ifdef _WIN32
	if (!library)
	{
		PanicAlert("Can't find function %s - Library not loaded.");
	}
	retval = GetProcAddress(library, funcname);
	//if (!retval)
	//{
	//	PanicAlert("Did not find function %s in library %s.", funcname, library_file.c_str());
	//}
#else
	retval = dlsym(library, funcname);

	if (!retval)
	{
		printf("Symbol %s missing in %s (error: %s)\n", funcname, library_file.c_str(), dlerror());
	}
#endif
	return retval;
}


