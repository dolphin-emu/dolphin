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

#define GL_DEPTH_COMPONENT32F_NV 0x8DAB
#define GL_DEPTH32F_STENCIL8_NV 0x8DAC
#define GL_FLOAT_32_UNSIGNED_INT_24_8_REV_NV 0x8DAD
#define GL_DEPTH_BUFFER_FLOAT_MODE_NV 0x8DAF

typedef void(APIENTRYP PFNDOLDEPTHRANGEDNVPROC)(GLdouble zNear, GLdouble zFar);
typedef void(APIENTRYP PFNDOLCLEARDEPTHDNVPROC)(GLdouble depth);
typedef void(APIENTRYP PFNDOLDEPTHBOUNDSDNVPROC)(GLdouble zmin, GLdouble zmax);

extern PFNDOLDEPTHRANGEDNVPROC dolDepthRangedNV;
extern PFNDOLCLEARDEPTHDNVPROC dolClearDepthdNV;
extern PFNDOLDEPTHBOUNDSDNVPROC dolDepthBoundsdNV;

#define glDepthRangedNV dolDepthRangedNV
#define glClearDepthdNV dolClearDepthdNV
#define glDepthBoundsdNV dolDepthBoundsdNV
