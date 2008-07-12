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
#include "DynamicLibrary.h"

DynamicLibrary::DynamicLibrary()
{
	library = 0;
}


bool DynamicLibrary::Load(const char* filename)
{
	if (strlen(filename) == 0)
	{
		PanicAlert("DynamicLibrary : Missing filename");
		return(false);
	}

	if (IsLoaded())
	{
		PanicAlert("Trying to load already loaded library %s", filename);
		return(false);
	}

#ifdef _WIN32
	library = LoadLibrary(filename);
#else
	library = dlopen(filename, RTLD_NOW | RTLD_LOCAL);

	if (!library)
	{
		PanicAlert(dlerror());
	}
	else
	{
		printf("Successfully loaded %s", filename);
	}
#endif

	return(library != 0);
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
	retval = GetProcAddress(library, funcname);

	if (!retval)
	{
		// PanicAlert("Did not find function %s in DLL", funcname);
	}

	return(retval);

#else
	retval = dlsym(library, funcname);

	if (!retval)
	{
		printf("%s\n", dlerror());
	}
#endif

}


