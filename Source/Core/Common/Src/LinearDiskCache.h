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

#ifndef _LINEAR_DISKCACHE
#define _LINEAR_DISKCACHE

#include "Common.h"

#include <stdlib.h>
#include <stdio.h>

// On disk format:
// uint32 'DCAC'
// uint32 version;  // svn_rev
// uint32 key_length;
// uint32 value_length;
// ....   key;
// ....   value;

class LinearDiskCacheReader {
public:
	virtual void Read(const u8 *key, int key_size, const u8 *value, int value_size) = 0;
};

// Dead simple unsorted key-value store with append functionality.
// No random read functionality, all reading is done in OpenAndRead.
// Keys and values can contain any characters, including \0.
//
// Suitable for caching generated shader bytecode between executions.
// Not tuned for extreme performance but should be reasonably fast.
// Does not support keys or values larger than 2GB, which should be reasonable.
// Keys must have non-zero length; values can have zero length.
class LinearDiskCache {
public:
	LinearDiskCache();

	// Returns the number of items read from the cache.
	int OpenAndRead(const char *filename, LinearDiskCacheReader *reader);
	void Close();
	void Sync();

	// Appends a key-value pair to the store.
	void Append(const u8 *key, int key_size, const u8 *value, int value_size);

private:
	void WriteHeader();
	bool ValidateHeader();

	FILE *file_;
	int num_entries_;
};

#endif  // _LINEAR_DISKCACHE