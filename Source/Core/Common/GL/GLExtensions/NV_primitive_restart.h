// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/GL/GLExtensions/gl_common.h"

#define GL_PRIMITIVE_RESTART_NV 0x8558
#define GL_PRIMITIVE_RESTART_INDEX_NV 0x8559

typedef void (GLAPIENTRY * PFNGLPRIMITIVERESTARTINDEXNVPROC) (GLuint index);
typedef void (GLAPIENTRY * PFNGLPRIMITIVERESTARTNVPROC) (void);

extern PFNGLPRIMITIVERESTARTINDEXNVPROC glPrimitiveRestartIndexNV;
extern PFNGLPRIMITIVERESTARTNVPROC glPrimitiveRestartNV;

