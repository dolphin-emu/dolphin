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


struct PROGRAMSHADER
{
        PROGRAMSHADER() : glprogid(0), vsid(0), psid(0){}
        GLuint glprogid; // opengl program id
        GLuint vsid, psid;
        GLint attrLoc[3];
        GLint UniformLocations[NUM_UNIFORMS];
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
        };
        typedef std::map<std::pair<u64, u64>, PCacheEntry> PCache;

        static PCache pshaders;
        static GLuint CurrentFShader, CurrentVShader, CurrentProgram;
        static std::pair<u64, u64> CurrentShaderProgram;

        // For UBOS
        static GLuint UBOBuffers[2]; // PS is 0, VS is 1
public:
        static PROGRAMSHADER GetShaderProgram(void);
        static GLint GetAttr(int num);
        static void SetBothShaders(GLuint PS, GLuint VS);
        static GLuint GetCurrentProgram(void);
        static void SetUniformObjects(int Buffer, unsigned int offset, const float *f, unsigned int count = 1);
        
        static void Init(void);
        static void Shutdown(void);

};

}  // namespace OGL

#endif
