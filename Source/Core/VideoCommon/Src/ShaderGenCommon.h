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

#ifndef _SHADERGENCOMMON_H
#define _SHADERGENCOMMON_H

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "CommonTypes.h"

template<class uid_data>
class ShaderUid
{
public:
	ShaderUid()
	{
		// TODO: Move to Shadergen => can be optimized out
		memset(values, 0, sizeof(values));
	}

	void Write(const char* fmt, ...) {}
	const char* GetBuffer() { return NULL; }
	void SetBuffer(char* buffer) { }

	bool operator == (const ShaderUid& obj) const
	{
		return memcmp(this->values, obj.values, sizeof(values)) == 0;
	}

	// TODO: Store last frame used and order by that? makes much more sense anyway...
	bool operator < (const ShaderUid& obj) const
	{
		for (unsigned int i = 0; i < sizeof(uid_data) / sizeof(u32); ++i)
		{
			if (this->values[i] < obj.values[i])
				return true;
			else if (this->values[i] > obj.values[i])
				return false;
		}
		return false;
	}

	uid_data& GetUidData() { return data; }

private:
	union
	{
		uid_data data;
		u32 values[sizeof(uid_data) / sizeof(u32)];
	};
};

// Needs to be a template for hacks...
template<class uid_data>
class ShaderCode
{
public:
	ShaderCode() : buf(NULL), write_ptr(NULL)
	{

	}

	void Write(const char* fmt, ...)
	{
		va_list arglist;
		va_start(arglist, fmt);
		write_ptr += vsprintf(write_ptr, fmt, arglist);
		va_end(arglist);
	}

	const char* GetBuffer() { return buf; }
	void SetBuffer(char* buffer) { buf = buffer; write_ptr = buffer; }
	uid_data& GetUidData() { return *(uid_data*)NULL; }

private:
	const char* buf;
	char* write_ptr;
};

enum GenOutput
{
	GO_ShaderCode,
	GO_ShaderUid,
};

#endif // _SHADERGENCOMMON_H
