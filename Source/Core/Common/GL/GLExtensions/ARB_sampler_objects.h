/*
** Copyright (c) 2013-2015 The Khronos Group Inc.
** SPDX-License-Identifier: MIT
*/

#include "Common/GL/GLExtensions/gl_common.h"

typedef void(APIENTRYP PFNDOLGENSAMPLERSPROC)(GLsizei count, GLuint* samplers);
typedef void(APIENTRYP PFNDOLDELETESAMPLERSPROC)(GLsizei count, const GLuint* samplers);
typedef GLboolean(APIENTRYP PFNDOLISSAMPLERPROC)(GLuint sampler);
typedef void(APIENTRYP PFNDOLBINDSAMPLERPROC)(GLuint unit, GLuint sampler);
typedef void(APIENTRYP PFNDOLSAMPLERPARAMETERIPROC)(GLuint sampler, GLenum pname, GLint param);
typedef void(APIENTRYP PFNDOLSAMPLERPARAMETERIVPROC)(GLuint sampler, GLenum pname,
                                                     const GLint* param);
typedef void(APIENTRYP PFNDOLSAMPLERPARAMETERFPROC)(GLuint sampler, GLenum pname, GLfloat param);
typedef void(APIENTRYP PFNDOLSAMPLERPARAMETERFVPROC)(GLuint sampler, GLenum pname,
                                                     const GLfloat* param);
typedef void(APIENTRYP PFNDOLSAMPLERPARAMETERIIVPROC)(GLuint sampler, GLenum pname,
                                                      const GLint* param);
typedef void(APIENTRYP PFNDOLSAMPLERPARAMETERIUIVPROC)(GLuint sampler, GLenum pname,
                                                       const GLuint* param);
typedef void(APIENTRYP PFNDOLGETSAMPLERPARAMETERIVPROC)(GLuint sampler, GLenum pname,
                                                        GLint* params);
typedef void(APIENTRYP PFNDOLGETSAMPLERPARAMETERIIVPROC)(GLuint sampler, GLenum pname,
                                                         GLint* params);
typedef void(APIENTRYP PFNDOLGETSAMPLERPARAMETERFVPROC)(GLuint sampler, GLenum pname,
                                                        GLfloat* params);
typedef void(APIENTRYP PFNDOLGETSAMPLERPARAMETERIUIVPROC)(GLuint sampler, GLenum pname,
                                                          GLuint* params);

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
