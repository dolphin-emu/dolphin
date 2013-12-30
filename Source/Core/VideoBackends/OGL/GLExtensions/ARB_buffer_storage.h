// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "gl_common.h"

#ifndef GL_ARB_buffer_storage
#define GL_ARB_buffer_storage 1

#define GL_MAP_READ_BIT 0x0001
#define GL_MAP_WRITE_BIT 0x0002
#define GL_MAP_PERSISTENT_BIT 0x00000040
#define GL_MAP_COHERENT_BIT 0x00000080
#define GL_DYNAMIC_STORAGE_BIT 0x0100
#define GL_CLIENT_STORAGE_BIT 0x0200
#define GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT 0x00004000
#define GL_BUFFER_IMMUTABLE_STORAGE 0x821F
#define GL_BUFFER_STORAGE_FLAGS 0x8220

typedef void (GLAPIENTRY * PFNGLBUFFERSTORAGEPROC) (GLenum target, GLsizeiptr size, const GLvoid* data, GLbitfield flags);
typedef void (GLAPIENTRY * PFNGLNAMEDBUFFERSTORAGEEXTPROC) (GLuint buffer, GLsizeiptr size, const GLvoid* data, GLbitfield flags);

#endif

extern PFNGLBUFFERSTORAGEPROC glBufferStorage;
extern PFNGLNAMEDBUFFERSTORAGEEXTPROC glNamedBufferStorageEXT;

