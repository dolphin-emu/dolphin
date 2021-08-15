/*
** Copyright (c) 2013-2015 The Khronos Group Inc.
** SPDX-License-Identifier: MIT
*/

#include "Common/GL/GLExtensions/gl_common.h"

#define GL_NEGATIVE_ONE_TO_ONE 0x935E
#define GL_ZERO_TO_ONE 0x935F
#define GL_CLIP_ORIGIN 0x935C
#define GL_CLIP_DEPTH_MODE 0x935D

typedef void(APIENTRYP PFNDOLCLIPCONTROLPROC)(GLenum origin, GLenum depth);

extern PFNDOLCLIPCONTROLPROC dolClipControl;

#define glClipControl dolClipControl
