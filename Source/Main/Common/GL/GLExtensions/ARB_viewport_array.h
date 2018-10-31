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

typedef void(APIENTRYP PFNDOLVIEWPORTARRAYVPROC)(GLuint first, GLsizei count, const GLfloat* v);
typedef void(APIENTRYP PFNDOLVIEWPORTINDEXEDFPROC)(GLuint index, GLfloat x, GLfloat y, GLfloat w,
                                                   GLfloat h);
typedef void(APIENTRYP PFNDOLVIEWPORTINDEXEDFVPROC)(GLuint index, const GLfloat* v);
typedef void(APIENTRYP PFNDOLSCISSORARRAYVPROC)(GLuint first, GLsizei count, const GLint* v);
typedef void(APIENTRYP PFNDOLSCISSORINDEXEDPROC)(GLuint index, GLint left, GLint bottom,
                                                 GLsizei width, GLsizei height);
typedef void(APIENTRYP PFNDOLSCISSORINDEXEDVPROC)(GLuint index, const GLint* v);
typedef void(APIENTRYP PFNDOLDEPTHRANGEARRAYVPROC)(GLuint first, GLsizei count, const GLdouble* v);
typedef void(APIENTRYP PFNDOLDEPTHRANGEINDEXEDPROC)(GLuint index, GLdouble n, GLdouble f);
typedef void(APIENTRYP PFNDOLGETFLOATI_VPROC)(GLenum target, GLuint index, GLfloat* data);
typedef void(APIENTRYP PFNDOLGETDOUBLEI_VPROC)(GLenum target, GLuint index, GLdouble* data);

extern PFNDOLDEPTHRANGEARRAYVPROC dolDepthRangeArrayv;
extern PFNDOLDEPTHRANGEINDEXEDPROC dolDepthRangeIndexed;
extern PFNDOLGETDOUBLEI_VPROC dolGetDoublei_v;
extern PFNDOLGETFLOATI_VPROC dolGetFloati_v;
extern PFNDOLSCISSORARRAYVPROC dolScissorArrayv;
extern PFNDOLSCISSORINDEXEDPROC dolScissorIndexed;
extern PFNDOLSCISSORINDEXEDVPROC dolScissorIndexedv;
extern PFNDOLVIEWPORTARRAYVPROC dolViewportArrayv;
extern PFNDOLVIEWPORTINDEXEDFPROC dolViewportIndexedf;
extern PFNDOLVIEWPORTINDEXEDFVPROC dolViewportIndexedfv;

#define glDepthRangeArrayv dolDepthRangeArrayv
#define glDepthRangeIndexed dolDepthRangeIndexed
#define glGetDoublei_v dolGetDoublei_v
#define glGetFloati_v dolGetFloati_v
#define glScissorArrayv dolScissorArrayv
#define glScissorIndexed dolScissorIndexed
#define glScissorIndexedv dolScissorIndexedv
#define glViewportArrayv dolViewportArrayv
#define glViewportIndexedf dolViewportIndexedf
#define glViewportIndexedfv dolViewportIndexedfv
