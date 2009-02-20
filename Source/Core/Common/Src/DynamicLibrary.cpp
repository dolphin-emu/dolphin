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

// All plugins from Core > Plugins are loaded and unloaded with this class when
// Dolphin is started and stopped.

#include <string.h> // System
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#include <stdio.h>
#endif

#include "Common.h" // Local
#include "FileUtil.h"
#include "StringUtil.h"
#include "DynamicLibrary.h"
#include "ConsoleWindow.h"

DynamicLibrary::DynamicLibrary()
{
	library = 0;
}

std::string GetLastErrorAsString()
{
#ifdef _WIN32
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
#else
	static std::string errstr;
	char *tmp = dlerror();
	if (tmp)
		errstr = tmp;
	
	return errstr;
#endif
}

/* Function: Loading means loading the dll with LoadLibrary() to get an
   instance to the dll.  This is done when Dolphin is started to determine
   which dlls are good, and before opening the Config and Debugging windows
   from Plugin.cpp and before opening the dll for running the emulation in
   Video_...cpp in Core. Since this is fairly slow, TODO: think about
   implementing some sort of cache.
   
   Called from: The Dolphin Core */
int DynamicLibrary::Load(const char* filename)
{
	if (!filename || strlen(filename) == 0)
	{
		LOG(MASTER_LOG, "Missing filename of dynamic library to load");
		PanicAlert("Missing filename of dynamic library to load");
		return 0;
	}
	if (IsLoaded())
	{
		LOG(MASTER_LOG, "Trying to load already loaded library %s", filename);
		return 2;
	}

	Console::Print("LoadLibrary: %s", filename);
#ifdef _WIN32
	library = LoadLibrary(filename);
#else
	library = dlopen(filename, RTLD_NOW | RTLD_LOCAL);
#endif
	Console::Print(" %p\n", library); 
	if (!library)
	{
		LOG(MASTER_LOG, "Error loading DLL %s: %s", filename, GetLastErrorAsString().c_str());
		PanicAlert("Error loading DLL %s: %s\n", filename, GetLastErrorAsString().c_str());
		return 0;
	}

	library_file = filename;
	return 1;
}

int DynamicLibrary::Unload()
{
	int retval;
	if (!IsLoaded())
	{
		PanicAlert("Error unloading DLL %s: not loaded", library_file.c_str());
		return 0;
	}

	Console::Print("FreeLibrary: %s %p\n", library_file.c_str(), library);        
#ifdef _WIN32
	retval = FreeLibrary(library);
#else
	retval = dlclose(library)?0:1;
#endif

	if (!retval)
	{
		PanicAlert("Error unloading DLL %s: %s", library_file.c_str(),
				   GetLastErrorAsString().c_str());
	}
	library = 0;
	return retval;
}

void* DynamicLibrary::Get(const char* funcname) const
{
	void* retval;
	if (!library)
	{
		PanicAlert("Can't find function %s - Library not loaded.");
		return NULL;
	}

	#ifdef _WIN32
		retval = GetProcAddress(library, funcname);
	#else
		retval = dlsym(library, funcname);
	#endif

	if (!retval)
	{
		LOG(MASTER_LOG, "Symbol %s missing in %s (error: %s)\n", funcname, library_file.c_str(), GetLastErrorAsString().c_str());
		PanicAlert("Symbol %s missing in %s (error: %s)\n", funcname, library_file.c_str(), GetLastErrorAsString().c_str());
	}

	return retval;
}
