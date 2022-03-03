/*
** Copyright (c) 2013-2015 The Khronos Group Inc.
** SPDX-License-Identifier: MIT
*/

#include "Common/GL/GLExtensions/gl_common.h"

#define GL_PIXEL_COUNTER_BITS_NV 0x8864
#define GL_CURRENT_OCCLUSION_QUERY_ID_NV 0x8865
#define GL_PIXEL_COUNT_NV 0x8866
#define GL_PIXEL_COUNT_AVAILABLE_NV 0x8867

typedef void(APIENTRYP PFNDOLGENOCCLUSIONQUERIESNVPROC)(GLsizei n, GLuint* ids);
typedef void(APIENTRYP PFNDOLDELETEOCCLUSIONQUERIESNVPROC)(GLsizei n, const GLuint* ids);
typedef GLboolean(APIENTRYP PFNDOLISOCCLUSIONQUERYNVPROC)(GLuint id);
typedef void(APIENTRYP PFNDOLBEGINOCCLUSIONQUERYNVPROC)(GLuint id);
typedef void(APIENTRYP PFNDOLENDOCCLUSIONQUERYNVPROC)(void);
typedef void(APIENTRYP PFNDOLGETOCCLUSIONQUERYIVNVPROC)(GLuint id, GLenum pname, GLint* params);
typedef void(APIENTRYP PFNDOLGETOCCLUSIONQUERYUIVNVPROC)(GLuint id, GLenum pname, GLuint* params);

extern PFNDOLGENOCCLUSIONQUERIESNVPROC dolGenOcclusionQueriesNV;
extern PFNDOLDELETEOCCLUSIONQUERIESNVPROC dolDeleteOcclusionQueriesNV;
extern PFNDOLISOCCLUSIONQUERYNVPROC dolIsOcclusionQueryNV;
extern PFNDOLBEGINOCCLUSIONQUERYNVPROC dolBeginOcclusionQueryNV;
extern PFNDOLENDOCCLUSIONQUERYNVPROC dolEndOcclusionQueryNV;
extern PFNDOLGETOCCLUSIONQUERYIVNVPROC dolGetOcclusionQueryivNV;
extern PFNDOLGETOCCLUSIONQUERYUIVNVPROC dolGetOcclusionQueryuivNV;

#define glGenOcclusionQueriesNV dolGenOcclusionQueriesNV
#define glDeleteOcclusionQueriesNV dolDeleteOcclusionQueriesNV
#define glIsOcclusionQueryNV dolIsOcclusionQueryNV
#define glBeginOcclusionQueryNV dolBeginOcclusionQueryNV
#define glEndOcclusionQueryNV dolEndOcclusionQueryNV
#define glGetOcclusionQueryivNV dolGetOcclusionQueryivNV
#define glGetOcclusionQueryuivNV dolGetOcclusionQueryuivNV
