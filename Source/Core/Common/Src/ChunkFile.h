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

#ifndef _POINTERWRAP_H_
#define _POINTERWRAP_H_

// Extremely simple serialization framework.

// (mis)-features:
// + Super fast
// + Very simple
// + Same code is used for serialization and deserializaition (in most cases)
// - Zero backwards/forwards compatibility
// - Serialization code for anything complex has to be manually written.

#include <map>
#include <vector>
#include <deque>
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
		MODE_READ = 1, // load
		MODE_WRITE, // save
		MODE_MEASURE, // calculate size
		MODE_VERIFY, // compare
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
		case MODE_VERIFY: for(int i = 0; i < size; i++) _dbg_assert_msg_(COMMON, ((u8*)data)[i] == (*ptr)[i], "Savestate verification failure: %d (0x%X) (at %p) != %d (0x%X) (at %p).\n", ((u8*)data)[i], ((u8*)data)[i], &((u8*)data)[i], (*ptr)[i], (*ptr)[i], &(*ptr)[i]); break;
		default: break;  // throw an error?
		}
		(*ptr) += size;
	}

	template<class T>
	void Do(std::map<unsigned int, T> &x)
	{
		unsigned int number = (unsigned int)x.size();
		Do(number);
		switch (mode) {
		case MODE_READ:
			{
				x.clear();
				while (number > 0)
				{
					unsigned int first = 0;
					Do(first);
					T second;
					Do(second);
					x[first] = second;
					--number;
				}
			}
			break;
		case MODE_WRITE:
		case MODE_MEASURE:
		case MODE_VERIFY:
			{
				typename std::map<unsigned int, T>::iterator itr = x.begin();
				while (number > 0)
				{
					Do(itr->first);
					Do(itr->second);
					--number;
					++itr;
				}
			}
			break;
		}
	}

	// Store vectors.
	template<class T>
	void Do(std::vector<T> &x)
	{
		u32 vec_size = (u32)x.size();
		Do(vec_size);
		x.resize(vec_size);
		DoArray(&x[0], vec_size);
	}
	
	// Store deques.
	template<class T>
	void Do(std::deque<T> &x)
	{
		u32 deq_size = (u32)x.size();
		Do(deq_size);
		x.resize(deq_size);
		u32 i;
		for(i = 0; i < deq_size; i++)
			DoVoid(&x[i],sizeof(T));
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
		case MODE_VERIFY: _dbg_assert_msg_(COMMON, !strcmp(x.c_str(), (char*)*ptr), "Savestate verification failure: \"%s\" != \"%s\" (at %p).\n", x.c_str(), (char*)*ptr, ptr); break;
		}
		(*ptr) += stringLen;
	}

	void Do(std::wstring &x) 
	{
		int stringLen = sizeof(wchar_t)*((int)x.length() + 1);
		Do(stringLen);

		switch (mode) {
		case MODE_READ:		x = (wchar_t*)*ptr; break;
		case MODE_WRITE:	memcpy(*ptr, x.c_str(), stringLen); break;
		case MODE_MEASURE: break;
		case MODE_VERIFY: _dbg_assert_msg_(COMMON, x == (wchar_t*)*ptr, "Savestate verification failure: \"%s\" != \"%s\" (at %p).\n", x.c_str(), (wchar_t*)*ptr, ptr); break;
		}
		(*ptr) += stringLen;
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
	void DoPointer(T* &x, T*const base) {
		// pointers can be more than 2^31 apart, but you're using this function wrong if you need that much range
		s32 offset = x - base;
		Do(offset);
		if (mode == MODE_READ)
			x = base + offset;
	}

	template<class T, LinkedListItem<T>* (*TNew)(), void (*TFree)(LinkedListItem<T>*), void (*TDo)(PointerWrap&, T*)>
	void DoLinkedList(LinkedListItem<T>*& list_start, LinkedListItem<T>** list_end=0)
	{
		LinkedListItem<T>* list_cur = list_start;
		LinkedListItem<T>* prev = 0;

		while (true)
		{
			u8 shouldExist = (list_cur ? 1 : 0);
			Do(shouldExist);
			if (shouldExist == 1)
			{
				LinkedListItem<T>* cur = list_cur ? list_cur : TNew();
				TDo(*this, (T*)cur);
				if (!list_cur)
				{
					if (mode == MODE_READ)
					{
						cur->next = 0;
						list_cur = cur;
						if (prev)
							prev->next = cur;
						else
							list_start = cur;
					}
					else
					{
						TFree(cur);
						continue;
					}
				}
			}
			else
			{
				if (mode == MODE_READ)
				{
					if (prev)
						prev->next = 0;
					if (list_end)
						*list_end = prev;
					if (list_cur)
					{
						if (list_start == list_cur)
							list_start = 0;
						do
						{
							LinkedListItem<T>* next = list_cur->next;
							TFree(list_cur);
							list_cur = next;
						}
						while (list_cur);
					}
				}
				break;
			}
			prev = list_cur;
			list_cur = list_cur->next;
		}
	}

	void DoMarker(const char* prevName, u32 arbitraryNumber=0x42)
	{
		u32 cookie = arbitraryNumber;
		Do(cookie);
		if(mode == PointerWrap::MODE_READ && cookie != arbitraryNumber)
		{
			PanicAlertT("Error: After \"%s\", found %d (0x%X) instead of save marker %d (0x%X). Aborting savestate load...", prevName, cookie, cookie, arbitraryNumber, arbitraryNumber);
			mode = PointerWrap::MODE_MEASURE;
		}
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

		if (!File::Exists(_rFilename))
			return false;
				
		// Check file size
		const u64 fileSize = File::GetSize(_rFilename);
		static const u64 headerSize = sizeof(SChunkHeader);
		if (fileSize < headerSize)
		{
			ERROR_LOG(COMMON,"ChunkReader: File too small");
			return false;
		}

		File::IOFile pFile(_rFilename, "rb");
		if (!pFile)
		{
			ERROR_LOG(COMMON,"ChunkReader: Can't open file for reading");
			return false;
		}

		// read the header
		SChunkHeader header;
		if (!pFile.ReadArray(&header, 1))
		{
			ERROR_LOG(COMMON,"ChunkReader: Bad header size");
			return false;
		}
		
		// Check revision
		if (header.Revision != _Revision)
		{
			ERROR_LOG(COMMON,"ChunkReader: Wrong file revision, got %d expected %d",
				header.Revision, _Revision);
			return false;
		}
		
		// get size
		const int sz = (int)(fileSize - headerSize);
		if (header.ExpectedSize != sz)
		{
			ERROR_LOG(COMMON,"ChunkReader: Bad file size, got %d expected %d",
				sz, header.ExpectedSize);
			return false;
		}
		
		// read the state
		u8* buffer = new u8[sz];
		if (!pFile.ReadBytes(buffer, sz))
		{
			ERROR_LOG(COMMON,"ChunkReader: Error reading file");
			return false;
		}

		u8 *ptr = buffer;
		PointerWrap p(&ptr, PointerWrap::MODE_READ);
		_class.DoState(p);
		delete[] buffer;
		
		INFO_LOG(COMMON, "ChunkReader: Done loading %s" , _rFilename.c_str());
		return true;
	}
	
	// Save file template
	template<class T>
	static bool Save(const std::string& _rFilename, int _Revision, T& _class)
	{
		INFO_LOG(COMMON, "ChunkReader: Writing %s" , _rFilename.c_str());
		File::IOFile pFile(_rFilename, "wb");
		if (!pFile)
		{
			ERROR_LOG(COMMON,"ChunkReader: Error opening file for write");
			return false;
		}

		// Get data
		u8 *ptr = 0;
		PointerWrap p(&ptr, PointerWrap::MODE_MEASURE);
		_class.DoState(p);
		size_t const sz = (size_t)ptr;
		std::vector<u8> buffer(sz);
		ptr = &buffer[0];
		p.SetMode(PointerWrap::MODE_WRITE);
		_class.DoState(p);
		
		// Create header
		SChunkHeader header;
		header.Compress = 0;
		header.Revision = _Revision;
		header.ExpectedSize = (int)sz;
		
		// Write to file
		if (!pFile.WriteArray(&header, 1))
		{
			ERROR_LOG(COMMON,"ChunkReader: Failed writing header");
			return false;
		}

		if (!pFile.WriteBytes(&buffer[0], sz))
		{
			ERROR_LOG(COMMON,"ChunkReader: Failed writing data");
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

#endif  // _POINTERWRAP_H_
