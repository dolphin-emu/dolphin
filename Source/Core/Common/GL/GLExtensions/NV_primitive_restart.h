/*
** Copyright (c) 2013-2015 The Khronos Group Inc.
** SPDX-License-Identifier: MIT
*/

#include "Common/GL/GLExtensions/gl_common.h"

#define GL_PRIMITIVE_RESTART_NV 0x8558
#define GL_PRIMITIVE_RESTART_INDEX_NV 0x8559

typedef void(APIENTRYP PFNDOLPRIMITIVERESTARTINDEXNVPROC)(GLuint index);
typedef void(APIENTRYP PFNDOLPRIMITIVERESTARTNVPROC)(void);

extern PFNDOLPRIMITIVERESTARTINDEXNVPROC dolPrimitiveRestartIndexNV;
extern PFNDOLPRIMITIVERESTARTNVPROC dolPrimitiveRestartNV;

#define glPrimitiveRestartIndexNV dolPrimitiveRestartIndexNV
#define glPrimitiveRestartNV dolPrimitiveRestartNV
