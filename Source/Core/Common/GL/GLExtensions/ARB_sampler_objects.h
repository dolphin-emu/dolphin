/*
** Copyright (c) 2013-2015 The Khronos Group Inc.
**
** Permission is hereby granted, free of charge, to any person obtaining a
** copy of this software and/or associated documentation files (the
** "Materials"), to deal in the Materials without restriction, including
** without limitation the rights to use, copy, modify, merge, publish,
** distribute, sublicense, and/or sell copies of the Materials, and to
** permit persons to whom the Materials are furnished to do so, subject to
** the following conditions:
**
** The above copyright notice and this permission notice shall be included
** in all copies or substantial portions of the Materials.
**
** THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
** EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
** MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
** IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
** CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
** TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
** MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
*/

#include "Common/GL/GLExtensions/gl_common.h"

typedef void (APIENTRYP PFNDOLGENSAMPLERSPROC) (GLsizei count, GLuint *samplers);
typedef void (APIENTRYP PFNDOLDELETESAMPLERSPROC) (GLsizei count, const GLuint *samplers);
typedef GLboolean (APIENTRYP PFNDOLISSAMPLERPROC) (GLuint sampler);
typedef void (APIENTRYP PFNDOLBINDSAMPLERPROC) (GLuint unit, GLuint sampler);
typedef void (APIENTRYP PFNDOLSAMPLERPARAMETERIPROC) (GLuint sampler, GLenum pname, GLint param);
typedef void (APIENTRYP PFNDOLSAMPLERPARAMETERIVPROC) (GLuint sampler, GLenum pname, const GLint *param);
typedef void (APIENTRYP PFNDOLSAMPLERPARAMETERFPROC) (GLuint sampler, GLenum pname, GLfloat param);
typedef void (APIENTRYP PFNDOLSAMPLERPARAMETERFVPROC) (GLuint sampler, GLenum pname, const GLfloat *param);
typedef void (APIENTRYP PFNDOLSAMPLERPARAMETERIIVPROC) (GLuint sampler, GLenum pname, const GLint *param);
typedef void (APIENTRYP PFNDOLSAMPLERPARAMETERIUIVPROC) (GLuint sampler, GLenum pname, const GLuint *param);
typedef void (APIENTRYP PFNDOLGETSAMPLERPARAMETERIVPROC) (GLuint sampler, GLenum pname, GLint *params);
typedef void (APIENTRYP PFNDOLGETSAMPLERPARAMETERIIVPROC) (GLuint sampler, GLenum pname, GLint *params);
typedef void (APIENTRYP PFNDOLGETSAMPLERPARAMETERFVPROC) (GLuint sampler, GLenum pname, GLfloat *params);
typedef void (APIENTRYP PFNDOLGETSAMPLERPARAMETERIUIVPROC) (GLuint sampler, GLenum pname, GLuint *params);

extern PFNDOLBINDSAMPLERPROC dolBindSampler;
extern PFNDOLDELETESAMPLERSPROC dolDeleteSamplers;
extern PFNDOLGENSAMPLERSPROC dolGenSamplers;
extern PFNDOLGETSAMPLERPARAMETERIIVPROC dolGetSamplerParameterIiv;
extern PFNDOLGETSAMPLERPARAMETERIUIVPROC dolGetSamplerParameterIuiv;
extern PFNDOLGETSAMPLERPARAMETERFVPROC dolGetSamplerParameterfv;
extern PFNDOLGETSAMPLERPARAMETERIVPROC dolGetSamplerParameteriv;
extern PFNDOLISSAMPLERPROC dolIsSampler;
extern PFNDOLSAMPLERPARAMETERIIVPROC dolSamplerParameterIiv;
extern PFNDOLSAMPLERPARAMETERIUIVPROC dolSamplerParameterIuiv;
extern PFNDOLSAMPLERPARAMETERFPROC dolSamplerParameterf;
extern PFNDOLSAMPLERPARAMETERFVPROC dolSamplerParameterfv;
extern PFNDOLSAMPLERPARAMETERIPROC dolSamplerParameteri;
extern PFNDOLSAMPLERPARAMETERIVPROC dolSamplerParameteriv;

#define glBindSampler dolBindSampler
#define glDeleteSamplers dolDeleteSamplers
#define glGenSamplers dolGenSamplers
#define glGetSamplerParameterIiv dolGetSamplerParameterIiv
#define glGetSamplerParameterIuiv dolGetSamplerParameterIuiv
#define glGetSamplerParameterfv dolGetSamplerParameterfv
#define glGetSamplerParameteriv dolGetSamplerParameteriv
#define glIsSampler dolIsSampler
#define glSamplerParameterIiv dolSamplerParameterIiv
#define glSamplerParameterIuiv dolSamplerParameterIuiv
#define glSamplerParameterf dolSamplerParameterf
#define glSamplerParameterfv dolSamplerParameterfv
#define glSamplerParameteri dolSamplerParameteri
#define glSamplerParameteriv dolSamplerParameteriv
