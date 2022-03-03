/*
** Copyright (c) 2013-2015 The Khronos Group Inc.
** SPDX-License-Identifier: MIT
*/

#include "Common/GL/GLExtensions/gl_common.h"

typedef void(APIENTRYP PFNDOLDRAWELEMENTSBASEVERTEXPROC)(GLenum mode, GLsizei count, GLenum type,
                                                         const void* indices, GLint basevertex);
typedef void(APIENTRYP PFNDOLDRAWRANGEELEMENTSBASEVERTEXPROC)(GLenum mode, GLuint start, GLuint end,
                                                              GLsizei count, GLenum type,
                                                              const void* indices,
                                                              GLint basevertex);
typedef void(APIENTRYP PFNDOLDRAWELEMENTSINSTANCEDBASEVERTEXPROC)(GLenum mode, GLsizei count,
                                                                  GLenum type, const void* indices,
                                                                  GLsizei instancecount,
                                                                  GLint basevertex);
typedef void(APIENTRYP PFNDOLMULTIDRAWELEMENTSBASEVERTEXPROC)(GLenum mode, const GLsizei* count,
                                                              GLenum type,
                                                              const void* const* indices,
                                                              GLsizei drawcount,
                                                              const GLint* basevertex);

extern PFNDOLDRAWELEMENTSBASEVERTEXPROC dolDrawElementsBaseVertex;
extern PFNDOLDRAWELEMENTSINSTANCEDBASEVERTEXPROC dolDrawElementsInstancedBaseVertex;
extern PFNDOLDRAWRANGEELEMENTSBASEVERTEXPROC dolDrawRangeElementsBaseVertex;
extern PFNDOLMULTIDRAWELEMENTSBASEVERTEXPROC dolMultiDrawElementsBaseVertex;

#define glDrawElementsBaseVertex dolDrawElementsBaseVertex
#define glDrawElementsInstancedBaseVertex dolDrawElementsInstancedBaseVertex
#define glDrawRangeElementsBaseVertex dolDrawRangeElementsBaseVertex
#define glMultiDrawElementsBaseVertex dolMultiDrawElementsBaseVertex
