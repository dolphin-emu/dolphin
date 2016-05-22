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

#define GL_PIXEL_COUNTER_BITS_NV          0x8864
#define GL_CURRENT_OCCLUSION_QUERY_ID_NV  0x8865
#define GL_PIXEL_COUNT_NV                 0x8866
#define GL_PIXEL_COUNT_AVAILABLE_NV       0x8867

typedef void (APIENTRYP PFNDOLGENOCCLUSIONQUERIESNVPROC) (GLsizei n, GLuint *ids);
typedef void (APIENTRYP PFNDOLDELETEOCCLUSIONQUERIESNVPROC) (GLsizei n, const GLuint *ids);
typedef GLboolean (APIENTRYP PFNDOLISOCCLUSIONQUERYNVPROC) (GLuint id);
typedef void (APIENTRYP PFNDOLBEGINOCCLUSIONQUERYNVPROC) (GLuint id);
typedef void (APIENTRYP PFNDOLENDOCCLUSIONQUERYNVPROC) (void);
typedef void (APIENTRYP PFNDOLGETOCCLUSIONQUERYIVNVPROC) (GLuint id, GLenum pname, GLint *params);
typedef void (APIENTRYP PFNDOLGETOCCLUSIONQUERYUIVNVPROC) (GLuint id, GLenum pname, GLuint *params);

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
