/*
** Copyright (c) 2013-2015 The Khronos Group Inc.
** SPDX-License-Identifier: MIT
*/

#include "Common/GL/GLExtensions/gl_common.h"

typedef void(APIENTRYP PFNDOLMINSAMPLESHADINGARBPROC)(GLfloat value);

extern PFNDOLMINSAMPLESHADINGARBPROC dolMinSampleShading;

#define glMinSampleShading dolMinSampleShading
