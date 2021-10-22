/*
** Copyright (c) 2013-2015 The Khronos Group Inc.
** SPDX-License-Identifier: MIT
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
