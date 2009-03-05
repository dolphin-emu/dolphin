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

#ifndef _POINTERWRAP_H
#define _POINTERWRAP_H

// Extremely simple serialization framework.

// (mis)-features:
// + Super fast
// + Very simple
// + Same code is used for serialization and deserializaition (in most cases)
// - Zero backwards/forwards compatibility
// - Serialization code for anything complex has to be manually written.

#include <stdio.h>

#include <map>
#include <vector>
#include <string>

#include "Common.h"
#include "FileUtil.h"

template <class T>
struct LinkedListItem : public T
{
	LinkedListItem<T> *next;
};

// Wrapper class
class PointerWrap
{
public:
	enum Mode { 		
		MODE_READ = 1,
		MODE_WRITE,
		MODE_MEASURE,
	};

	u8 **ptr;
	Mode mode;

public:
	PointerWrap(u8 **ptr_, Mode mode_) : ptr(ptr_), mode(mode_) {}
	PointerWrap(unsigned char **ptr_, int mode_) : ptr((u8**)ptr_), mode((Mode)mode_) {}

	void SetMode(Mode mode_) {mode = mode_;}
	Mode GetMode() const {return mode;}
	u8 **GetPPtr() {return ptr;}

	void DoVoid(void *data, int size)
	{
		switch (mode) {
		case MODE_READ:	memcpy(data, *ptr, size); break;
		case MODE_WRITE: memcpy(*ptr, data, size); break;
		case MODE_MEASURE: break;  // MODE_MEASURE - don't need to do anything
		default: break;  // throw an error?
		}
		(*ptr) += size;
	}

	// Store maps to file. Very useful.
	template<class T>
	void Do(std::map<unsigned int, T> &x) {
		// TODO
		PanicAlert("Do(map<>) does not yet work.");
	}
	
	// Store vectors.
	template<class T>
	void Do(std::vector<T> &x) {
		// TODO
		PanicAlert("Do(vector<>) does not yet work.");
	}
	
	// Store strings.
	void Do(std::string &x) 
	{
		int stringLen = (int)x.length() + 1;
		Do(stringLen);
		
		switch (mode) {
		case MODE_READ:		x = (char*)*ptr; break;
		case MODE_WRITE:	memcpy(*ptr, x.c_str(), stringLen); break;
		case MODE_MEASURE: break;
		}
		(*ptr) += stringLen;
	}
 
	void DoBuffer(u8** pBuffer, u32& _Size) 
	{
		Do(_Size);	
		
		if (_Size > 0) {
			switch (mode) {
			case MODE_READ:		*pBuffer = new u8[_Size]; memcpy(*pBuffer, *ptr, _Size); break;
			case MODE_WRITE:	memcpy(*ptr, *pBuffer, _Size); break;
			case MODE_MEASURE:	break;
			}
		} else 	{
			*pBuffer = NULL;
		}
		(*ptr) += _Size;
	}
	
    template<class T>
	void DoArray(T *x, int count) {
        DoVoid((void *)x, sizeof(T) * count);
    }
	
	template<class T>
	void Do(T &x) {
		DoVoid((void *)&x, sizeof(x));
	}
	
	template<class T>
	void DoLinkedList(LinkedListItem<T> **list_start) {
		// TODO
		PanicAlert("Do(linked list<>) does not yet work.");
	}
};


class CChunkFileReader
{
public:
	// Load file template
	template<class T>
	static bool Load(const std::string& _rFilename, int _Revision, T& _class) 
	{
		
		INFO_LOG(COMMON, "ChunkReader: Loading %s" , _rFilename.c_str());

		if (! File::Exists(_rFilename.c_str()))
			return false;
				
		// Check file size
		u64 fileSize = File::GetSize(_rFilename.c_str());
		u64 headerSize = sizeof(SChunkHeader);
		if (fileSize < headerSize) {
			ERROR_LOG(COMMON,"ChunkReader: File too small");
			return false;
		}

		FILE* pFile = fopen(_rFilename.c_str(), "rb");
		if (!pFile) { 
			ERROR_LOG(COMMON,"ChunkReader: Can't open file for reading");
			return false;
		}

		// read the header
		SChunkHeader header;
		if (fread(&header, 1, headerSize, pFile) != headerSize) {
			fclose(pFile);
			ERROR_LOG(COMMON,"ChunkReader: Bad header size");
			return false;
		}
		
		// Check revision
		if (header.Revision != _Revision) {
			fclose(pFile);
			ERROR_LOG(COMMON,"ChunkReader: Wrong file revision, got %d expected %d",
					  header.Revision, _Revision);
			return false;
		}
		
		// get size
		int sz = (int)(fileSize - headerSize);
		if (header.ExpectedSize != sz) {
			fclose(pFile);
			ERROR_LOG(COMMON,"ChunkReader: Bad file size, got %d expected %d",
					  sz, header.ExpectedSize);
			return false;
		}
		
		// read the state
		u8* buffer = new u8[sz];
		if ((int)fread(buffer, 1, sz, pFile) != sz) {
			fclose(pFile);
			ERROR_LOG(COMMON,"ChunkReader: Error reading file");
			return false;
		}

		// done reading
		if (fclose(pFile)) {
			ERROR_LOG(COMMON,"ChunkReader: Error closing file! might be corrupted!");
			// should never happen!
			return false;
		}

		u8 *ptr = buffer;
		PointerWrap p(&ptr, PointerWrap::MODE_READ);
		_class.DoState(p);
		delete [] buffer;
		
		INFO_LOG(COMMON, "ChunkReader: Done loading %s" , _rFilename.c_str());
		return true;
	}
	
	// Save file template
	template<class T>
	static bool Save(const std::string& _rFilename, int _Revision, T& _class)
	{
		FILE* pFile;

		INFO_LOG(COMMON, "ChunkReader: Writing %s" , _rFilename.c_str());
		pFile = fopen(_rFilename.c_str(), "wb");
		if (!pFile) {
			ERROR_LOG(COMMON,"ChunkReader: Error opening file for write");
			return false;
		}

		// Get data
		u8 *ptr = 0;
		PointerWrap p(&ptr, PointerWrap::MODE_MEASURE);
		_class.DoState(p);
		size_t sz = (size_t)ptr;
		u8 *buffer = new u8[sz];
		ptr = buffer;
		p.SetMode(PointerWrap::MODE_WRITE);
		_class.DoState(p);
		
		// Create header
		SChunkHeader header;
		header.Compress = 0;
		header.Revision = _Revision;
		header.ExpectedSize = (int)sz;
		
		// Write to file
		if (fwrite(&header, sizeof(SChunkHeader), 1, pFile) != 1) {
			ERROR_LOG(COMMON,"ChunkReader: Failed writing header");
			return false;
		}

		if (fwrite(buffer, sz, 1, pFile) != 1) {
			ERROR_LOG(COMMON,"ChunkReader: Failed writing data");
			return false;
		}

		if (fclose(pFile)) {
			ERROR_LOG(COMMON,"ChunkReader: Error closing file! might be corrupted!");
			return false;
		}
		
		INFO_LOG(COMMON,"ChunkReader: Done writing %s", 
				 _rFilename.c_str());
		return true;
	}
	
private:
	struct SChunkHeader
	{
		int Revision;
		int Compress;
		int ExpectedSize;
	};
};

#endif  // _POINTERWRAP_H
