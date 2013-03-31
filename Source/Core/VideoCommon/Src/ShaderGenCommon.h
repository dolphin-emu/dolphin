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
#include <vector>

#include "CommonTypes.h"
#include "VideoCommon.h"

class ShaderGeneratorInterface
{
public:
	void Write(const char* fmt, ...) {}
	const char* GetBuffer() { return NULL; }
	void SetBuffer(char* buffer) { }
	inline void SetConstantsUsed(unsigned int first_index, unsigned int last_index) {}

	template<class uid_data>
	uid_data& GetUidData() { return *(uid_data*)NULL; }
};

template<class uid_data>
class ShaderUid : public ShaderGeneratorInterface
{
public:
	ShaderUid()
	{
		// TODO: Move to Shadergen => can be optimized out
		memset(values, 0, sizeof(values));
	}

	bool operator == (const ShaderUid& obj) const
	{
		return memcmp(this->values, obj.values, sizeof(values)) == 0;
	}

	bool operator != (const ShaderUid& obj) const
	{
		return memcmp(this->values, obj.values, sizeof(values)) != 0;
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

	template<class T>
	inline T& GetUidData() { return data; }

private:
	union
	{
		uid_data data;
		u32 values[sizeof(uid_data) / sizeof(u32)];
	};
};

class ShaderCode : public ShaderGeneratorInterface
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

private:
	const char* buf;
	char* write_ptr;
};

class ShaderConstantProfile : public ShaderGeneratorInterface
{
public:
	ShaderConstantProfile(int num_constants) { constant_usage.resize(num_constants); }

	inline void SetConstantsUsed(unsigned int first_index, unsigned int last_index)
	{
		for (unsigned int i = first_index; i < last_index+1; ++i)
			constant_usage[i] = true;
	}

	inline bool ConstantIsUsed(unsigned int index)
	{
		return true;
//		return constant_usage[index];
	}
private:
	std::vector<bool> constant_usage; // TODO: Is vector<bool> appropriate here?
};

template<class T>
static void WriteRegister(T& object, API_TYPE ApiType, const char *prefix, const u32 num)
{
	if (ApiType == API_OPENGL)
		return; // Nothing to do here

	object.Write(" : register(%s%d)", prefix, num);
}

template<class T>
static void WriteLocation(T& object, API_TYPE ApiType, bool using_ubos)
{
	if (using_ubos)
		return;

	object.Write("uniform ");
}

template<class T>
static void DeclareUniform(T& object, API_TYPE api_type, bool using_ubos, const u32 num, const char* type, const char* name)
{
	WriteLocation(object, api_type, using_ubos);
	object.Write("%s %s ", type, name);
	WriteRegister(object, api_type, "c", num);
	object.Write(";\n");
}

#endif // _SHADERGENCOMMON_H
