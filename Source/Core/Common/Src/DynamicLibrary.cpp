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

/*
  All plugins from Core > Plugins are loaded and unloaded with this class when
  Dolpin is started and stopped.
*/

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

DynamicLibrary::DynamicLibrary()
{
	library = 0;
}

// Gets the last dll error as string
const char *DllGetLastError()
{
#ifdef _WIN32
	return GetLastErrorMsg();
#else // not win32 
	return dlerror();
#endif // WIN32
}

/* Loads the dll with LoadLibrary() or dlopen.  This function is called on
   start to scan for plugin, and before opening the Config and Debugging
   windows. It is also called from core to load the plugins when the 
   emulation starts.

   Returns 0 on failure and 1 on success

   TODO: think about implementing some sort of cache for the plugin info.  
*/
int DynamicLibrary::Load(const char* filename)
{
	INFO_LOG(COMMON, "DL: Loading dynamic library %s", filename);

	if (!filename || strlen(filename) == 0)	{
		ERROR_LOG(COMMON, "DL: Missing filename to load");
		return 0;
	}

	if (IsLoaded()) {
		INFO_LOG(COMMON, "DL: library %s already loaded", filename);
		return 1;
	}

#ifdef _WIN32
	library = LoadLibrary(filename);
#else
	// RTLD_NOW: resolve all symbols on load
	// RTLD_LOCAL: don't resolve symbols for other libraries
	library = dlopen(filename, RTLD_NOW | RTLD_LOCAL);
#endif

	DEBUG_LOG(COMMON, "DL: LoadLibrary: %s(%p)", filename, library);

	if (!library) {
		fprintf(stderr, "DL: Error loading DLL %s: %s", filename, 
				  DllGetLastError());
		ERROR_LOG(COMMON, "DL: Error loading DLL %s: %s", filename, 
				  DllGetLastError());
		return 0;
	}

	library_file = filename;

	INFO_LOG(COMMON, "DL: Done loading dynamic library %s", filename);
	return 1;
}

/* Frees one instances of the dynamic library. Note that on most systems use
   reference count to decide when to actually remove the library from memory.

   Return 0 on failure and 1 on success 
*/
int DynamicLibrary::Unload()
{
	INFO_LOG(COMMON, "DL: Unloading dynamic library %s", library_file.c_str());
    int retval;
    if (!IsLoaded()) { // library != null
		ERROR_LOG(COMMON, "DL: Unload failed for %s: not loaded", 
				  library_file.c_str());
        PanicAlert("DL: Unload failed %s: not loaded", 
				   library_file.c_str());
        return 0;
    }

    DEBUG_LOG(COMMON, "DL: FreeLibrary: %s %p\n", 
			  library_file.c_str(), library);        
#ifdef _WIN32
    retval = FreeLibrary(library);
#else
    retval = dlclose(library)?0:1;
#endif

    if (! retval) {
        ERROR_LOG(COMMON, "DL: Unload failed %s: %s", 
				  library_file.c_str(),
				 DllGetLastError());
    }
    library = 0;

	INFO_LOG(COMMON, "DL: Done unloading dynamic library %s", 
			 library_file.c_str());
    return retval;
}

// Returns the address where symbol funcname is loaded or NULL on failure 
void* DynamicLibrary::Get(const char* funcname) const
{
	void* retval;

	INFO_LOG(COMMON, "DL: Getting symbol %s: %s", library_file.c_str(),
			 funcname);
	
	if (!library)
	{
		ERROR_LOG(COMMON, "DL: Get failed %s - Library not loaded");
		return NULL;
	}

	#ifdef _WIN32
		retval = GetProcAddress(library, funcname);
	#else
		retval = dlsym(library, funcname);
	#endif

	if (!retval)
	{
		ERROR_LOG(COMMON, "DL: Symbol %s missing in %s (error: %s)\n", 
				  funcname, library_file.c_str(), 
				  DllGetLastError());
	}

	INFO_LOG(COMMON, "DL: Done getting symbol %s: %s", library_file.c_str(),
			 funcname);
	return retval;
}


