// Copyright (C) 2003 Dolphin Project.

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

#ifndef _DYNAMICLIBRARY_H_
#define _DYNAMICLIBRARY_H_

#ifdef _WIN32
#include <windows.h>
#endif

#include <string>

/* Abstracts the (few) differences between dynamically loading DLLs under
   Windows and .so / .dylib under Linux/MacOSX. */
class DynamicLibrary
{
public:
	DynamicLibrary();

	// Loads the library filename
	int Load(const char *filename);

	// Unload the current library
	int Unload();

	// Gets a pointer to the function symbol of funcname by getting it from the
	// share object
	void *Get(const char *funcname) const;

	// Returns true is the library is loaded
	bool IsLoaded() const { return library != 0; }

private:
	// name of the library file
	std::string library_file;

	// Library handle
#ifdef _WIN32
	HINSTANCE library;
#else
	void *library;
#endif
};

#endif // _DYNAMICLIBRARY_H_
