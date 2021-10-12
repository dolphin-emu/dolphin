/*
** Copyright (c) 2013-2015 The Khronos Group Inc.
** SPDX-License-Identifier: MIT
*/

#include "Common/GL/GLExtensions/gl_common.h"

#define GL_MAP_READ_BIT 0x0001
#define GL_MAP_WRITE_BIT 0x0002
#define GL_MAP_PERSISTENT_BIT 0x00000040
#define GL_MAP_COHERENT_BIT 0x00000080
#define GL_DYNAMIC_STORAGE_BIT 0x0100
#define GL_CLIENT_STORAGE_BIT 0x0200
#define GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT 0x00004000
#define GL_BUFFER_IMMUTABLE_STORAGE 0x821F
#define GL_BUFFER_STORAGE_FLAGS 0x8220

typedef void(APIENTRYP PFNDOLBUFFERSTORAGEPROC)(GLenum target, GLsizeiptr size, const void* data,
                                                GLbitfield flags);
typedef void(APIENTRYP PFNDOLNAMEDBUFFERSTORAGEEXTPROC)(GLuint buffer, GLsizeiptr size,
                                                        const void* data, GLbitfield flags);

extern PFNDOLBUFFERSTORAGEPROC dolBufferStorage;

#define glBufferStorage dolBufferStorage
