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

template <class T>
struct LinkedListItem : public T
{
	LinkedListItem<T> *next;
};


class PointerWrap
{
public:
	enum Mode  // also defined in pluginspecs.h. Didn't want to couple them.
	{
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
		switch (mode)
		{
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

		switch (mode)
		{
		case MODE_READ:		x = (char*)*ptr; break;
		case MODE_WRITE:	memcpy(*ptr, x.c_str(), stringLen); break;
		case MODE_MEASURE: break;
		}
		(*ptr) += stringLen;
	}
 
	void DoBuffer(u8** pBuffer, u32& _Size) 
	{
		Do(_Size);	

		if (_Size > 0)
		{
			switch (mode)
			{
			case MODE_READ:		*pBuffer = new u8[_Size]; memcpy(*pBuffer, *ptr, _Size); break;
			case MODE_WRITE:	memcpy(*ptr, *pBuffer, _Size); break;
			case MODE_MEASURE:	break;
			}
		}
		else
		{
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
	template<class T>
	static bool Load(const std::string& _rFilename, int _Revision, T& _class)
	{
		FILE* pFile = fopen(_rFilename.c_str(), "rb");
		if (pFile)
		{
			// get size
			fseek(pFile, 0, SEEK_END);
			size_t FileSize = ftell(pFile);
			fseek(pFile, 0, SEEK_SET);

			if (FileSize < sizeof(SChunkHeader))
			{
				fclose(pFile);
				return false;
			}

			// read the header
			SChunkHeader Header;
			fread(&Header, sizeof(SChunkHeader), 1, pFile);
			if (Header.Revision != _Revision)
			{
				fclose(pFile);
				return false;
			}

			// get size
			int sz = FileSize - sizeof(SChunkHeader);
			if (Header.ExpectedSize != sz)
			{
				fclose(pFile);
				return false;
			}

			// read the state
			u8* buffer = new u8[sz];
			fread(buffer, 1, sz, pFile);
			fclose(pFile);

			u8 *ptr = buffer;
			PointerWrap p(&ptr, PointerWrap::MODE_READ);
			_class.DoState(p);
			delete [] buffer;

			return true;
		}

		return false;
	}

	template<class T>
	static bool Save(const std::string& _rFilename, int _Revision, T& _class)
	{
		FILE* pFile = fopen(_rFilename.c_str(), "wb");
		if (pFile)
		{
			u8 *ptr = 0;
			PointerWrap p(&ptr, PointerWrap::MODE_MEASURE);
			_class.DoState(p);
			size_t sz = (size_t)ptr;
			u8 *buffer = new u8[sz];
			ptr = buffer;
			p.SetMode(PointerWrap::MODE_WRITE);
			_class.DoState(p);

			SChunkHeader Header;
			Header.Compress = 0;
			Header.Revision = _Revision;
			Header.ExpectedSize = sz;

			fwrite(&Header, sizeof(SChunkHeader), 1, pFile);
			fwrite(buffer, sz, 1, pFile);
			fclose(pFile);

			return true;
		}

		return false;
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
