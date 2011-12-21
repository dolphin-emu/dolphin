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

#ifndef _ProgramShaderCache_H_
#define _ProgramShaderCache_H_

#include "GLUtil.h"

#include "VertexShaderCache.h"
#include "PixelShaderCache.h"
#include "PixelShaderGen.h"
#include "VertexShaderGen.h"

#include "LinearDiskCache.h"
#include "ConfigManager.h"

	union PID
	{
		struct {
		GLuint vsid, psid;
		};
		u64 id;
	};

class PROGRAMUID
{
public:

	PID uid;

	PROGRAMUID()
	{
		uid.id = 0;
	}

	PROGRAMUID(const PROGRAMUID& r)
	{
		uid.id = r.uid.id;
	}
	PROGRAMUID(GLuint _v, GLuint _p)
	{
		uid.vsid = _v;
		uid.psid = _p;
	}

	int GetNumValues() const
	{
		return uid.id;
	}
};
void GetProgramShaderId(PROGRAMUID *uid, GLuint _v, GLuint _p);

namespace OGL
{
#define NUM_UNIFORMS 27
extern const char *UniformNames[NUM_UNIFORMS];
extern GLenum ProgramFormat;

struct PROGRAMSHADER
{
	PROGRAMSHADER() : glprogid(0), vsid(0), psid(0), binaryLength(0){}
	GLuint glprogid; // opengl program id
	GLuint vsid, psid;
	GLint UniformLocations[NUM_UNIFORMS];
	GLint binaryLength;
	u8 *Data()
	{
		glGetProgramiv(glprogid, GL_PROGRAM_BINARY_LENGTH, &binaryLength);
        u8* binary = (u8*)malloc(binaryLength);
        glGetProgramBinary(glprogid, binaryLength, NULL, &ProgramFormat, binary);
        return binary;
	}
	GLint Size()
	{
		if(!binaryLength)
			glGetProgramiv(glprogid, GL_PROGRAM_BINARY_LENGTH, &binaryLength);
		return binaryLength;
	}
};


class ProgramShaderCache
{
	struct PCacheEntry
	{
		PROGRAMSHADER program;
		int frameCount;
		PCacheEntry() : frameCount(0) {}
		void Destroy() {
			glDeleteProgram(program.glprogid);
			program.glprogid = 0;
		}
		u8* Data()
		{
			return program.Data();
		}
		GLint Size()
		{
			return program.Size();
		}
	};
	
	class ProgramShaderCacheInserter : public LinearDiskCacheReader<PROGRAMUID, u8>
	{
		public:
			void Read(const PROGRAMUID &key, const u8 *value, u32 value_size)
			{
				PCacheEntry entry;

				// The two shaders might not even exist anymore
				// But it is fine, no need to worry about that
				entry.program.vsid = key.uid.vsid;
				entry.program.psid = key.uid.psid;
				
				entry.program.glprogid = glCreateProgram();

				glProgramBinary(entry.program.glprogid, ProgramFormat, value, value_size);
				
				GLint success;
				glGetProgramiv(entry.program.glprogid, GL_LINK_STATUS, &success);

				if (success)
				{
					pshaders[std::make_pair(key.uid.psid, key.uid.vsid)] = entry;
					glUseProgram(entry.program.glprogid);
					SetProgramVariables(entry, key);
				}
			}
	};
	
	typedef std::map<std::pair<u64, u64>, PCacheEntry> PCache;

	static PCache pshaders;
	static GLuint CurrentFShader, CurrentVShader, CurrentProgram;
	static std::pair<u64, u64> CurrentShaderProgram;

	static GLuint s_ps_vs_ubo;
	static GLintptr s_vs_data_offset;
	static void SetProgramVariables(PCacheEntry &entry, const PROGRAMUID &uid);
	
public:
	static PROGRAMSHADER GetShaderProgram(void);
	static void SetBothShaders(GLuint PS, GLuint VS);
	static GLuint GetCurrentProgram(void);

	static void SetMultiPSConstant4fv(unsigned int offset, const float *f, unsigned int count);
	static void SetMultiVSConstant4fv(unsigned int offset, const float *f, unsigned int count);
	
	static void Init(void);
	static void Shutdown(void);

};

}  // namespace OGL

#endif
