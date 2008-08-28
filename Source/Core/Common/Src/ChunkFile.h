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

#ifndef _CHUNKFILE_H
#define _CHUNKFILE_H

// Class to read/write/verify hierarchical binary file formats.
// Grabbed from one of my older projects and modified heavily.
// Works more like a RIFF file than a Google Protocol Buffer, for example.

#include "Common.h"

#include <stdio.h>
#include <map>
#include <vector>

//TO REMEMBER WHEN USING:

//EITHER a chunk contains ONLY data
//OR it contains ONLY other chunks
//otherwise the scheme breaks...

class ChunkFile
{
public:
	enum ChunkFileMode
	{
		MODE_READ,
		MODE_WRITE,
		MODE_VERIFY,
	};
private:
	struct ChunkInfo
	{
		int startLocation;
		int parentStartLocation;
		int parentEOF;
		unsigned int ID;
		int length;
	};

	ChunkInfo stack[8];
	int stack_ptr;
	
	char *data;
	int size;
	int eof;

	ChunkFileMode mode;
	FILE *f;
	bool didFail;
	
	// Used for internal bookkeeping only.
	int ReadInt();
	void WriteInt(int x);

public:

	ChunkFile(const char *filename, ChunkFileMode mode);
	~ChunkFile();

	// Only pass 4-character IDs.
	bool Descend(const char *id);
	void Ascend();

	//void Do(int &i);
	//bool Do(std::string &s);
	bool Do(void *ptr, int size);

        bool DoArray(void *ptr, int size, int arrSize);

	// Future
	// bool DoCompressed(void *ptr, int size)

	// Store maps to file. Very useful.
	template<class T>
	bool Do(std::map<u32, T> &x) {
            return false;
	}
           
	// Store vectors.
	template<class T>
	bool Do(std::vector<T> &x) {
            return false;
	}


        // Disable size checks to save size for variable size array storing
        template<clsas T>
        bool DoArray(T *x, int size, int arrSize) {
            return DoArray((void *)x, size, arrSize);
        }
            
	// Handle everything else
	template<class T>
	bool Do(T &x) {
		return Do((void *)&x, sizeof(x));
	}

	int GetCurrentChunkSize();
	bool failed() {return didFail;}
};

#endif
