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
		MODE_WRITE = 2,
		MODE_MEASURE = 3,
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
		PanicAlert("Do(vector<>) does not yet work.");
	}
};

#endif  // _POINTERWRAP_H
