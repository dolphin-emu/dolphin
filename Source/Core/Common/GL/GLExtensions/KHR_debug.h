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

#define GL_CONTEXT_FLAG_DEBUG_BIT 0x00000002
#define GL_STACK_OVERFLOW 0x0503
#define GL_STACK_UNDERFLOW 0x0504
#define GL_DEBUG_OUTPUT_SYNCHRONOUS 0x8242
#define GL_DEBUG_NEXT_LOGGED_MESSAGE_LENGTH 0x8243
#define GL_DEBUG_CALLBACK_FUNCTION 0x8244
#define GL_DEBUG_CALLBACK_USER_PARAM 0x8245
#define GL_DEBUG_SOURCE_API 0x8246
#define GL_DEBUG_SOURCE_WINDOW_SYSTEM 0x8247
#define GL_DEBUG_SOURCE_SHADER_COMPILER 0x8248
#define GL_DEBUG_SOURCE_THIRD_PARTY 0x8249
#define GL_DEBUG_SOURCE_APPLICATION 0x824A
#define GL_DEBUG_SOURCE_OTHER 0x824B
#define GL_DEBUG_TYPE_ERROR 0x824C
#define GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR 0x824D
#define GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR 0x824E
#define GL_DEBUG_TYPE_PORTABILITY 0x824F
#define GL_DEBUG_TYPE_PERFORMANCE 0x8250
#define GL_DEBUG_TYPE_OTHER 0x8251
#define GL_DEBUG_TYPE_MARKER 0x8268
#define GL_DEBUG_TYPE_PUSH_GROUP 0x8269
#define GL_DEBUG_TYPE_POP_GROUP 0x826A
#define GL_DEBUG_SEVERITY_NOTIFICATION 0x826B
#define GL_MAX_DEBUG_GROUP_STACK_DEPTH 0x826C
#define GL_DEBUG_GROUP_STACK_DEPTH 0x826D
#define GL_BUFFER 0x82E0
#define GL_SHADER 0x82E1
#define GL_PROGRAM 0x82E2
#define GL_QUERY 0x82E3
#define GL_PROGRAM_PIPELINE 0x82E4
#define GL_SAMPLER 0x82E6
#define GL_DISPLAY_LIST 0x82E7
#define GL_MAX_LABEL_LENGTH 0x82E8
#define GL_MAX_DEBUG_MESSAGE_LENGTH 0x9143
#define GL_MAX_DEBUG_LOGGED_MESSAGES 0x9144
#define GL_DEBUG_LOGGED_MESSAGES 0x9145
#define GL_DEBUG_SEVERITY_HIGH 0x9146
#define GL_DEBUG_SEVERITY_MEDIUM 0x9147
#define GL_DEBUG_SEVERITY_LOW 0x9148
#define GL_DEBUG_OUTPUT 0x92E0

typedef void (APIENTRYP GLDEBUGPROC)(GLenum source,GLenum type,GLuint id,GLenum severity,GLsizei length,const GLchar *message,const void *userParam);

typedef void (APIENTRYP PFNDOLDEBUGMESSAGECALLBACKPROC) (GLDEBUGPROC callback, const GLvoid *userParam);
typedef void (APIENTRYP PFNDOLDEBUGMESSAGECONTROLPROC) (GLenum source, GLenum type, GLenum severity, GLsizei count, const GLuint* ids, GLboolean enabled);
typedef void (APIENTRYP PFNDOLDEBUGMESSAGEINSERTPROC) (GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* buf);
typedef GLuint (APIENTRYP PFNDOLGETDEBUGMESSAGELOGPROC) (GLuint count, GLsizei bufsize, GLenum* sources, GLenum* types, GLuint* ids, GLenum* severities, GLsizei* lengths, GLchar* messageLog);
typedef void (APIENTRYP PFNDOLGETOBJECTLABELPROC) (GLenum identifier, GLuint name, GLsizei bufSize, GLsizei* length, GLchar *label);
typedef void (APIENTRYP PFNDOLGETOBJECTPTRLABELPROC) (void* ptr, GLsizei bufSize, GLsizei* length, GLchar *label);
typedef void (APIENTRYP PFNDOLOBJECTLABELPROC) (GLenum identifier, GLuint name, GLsizei length, const GLchar* label);
typedef void (APIENTRYP PFNDOLOBJECTPTRLABELPROC) (void* ptr, GLsizei length, const GLchar* label);
typedef void (APIENTRYP PFNDOLPOPDEBUGGROUPPROC) (void);
typedef void (APIENTRYP PFNDOLPUSHDEBUGGROUPPROC) (GLenum source, GLuint id, GLsizei length, const GLchar * message);

extern PFNDOLDEBUGMESSAGECALLBACKPROC dolDebugMessageCallback;
extern PFNDOLDEBUGMESSAGECONTROLPROC dolDebugMessageControl;
extern PFNDOLDEBUGMESSAGEINSERTPROC dolDebugMessageInsert;
extern PFNDOLGETDEBUGMESSAGELOGPROC dolGetDebugMessageLog;
extern PFNDOLGETOBJECTLABELPROC dolGetObjectLabel;
extern PFNDOLGETOBJECTPTRLABELPROC dolGetObjectPtrLabel;
extern PFNDOLOBJECTLABELPROC dolObjectLabel;
extern PFNDOLOBJECTPTRLABELPROC dolObjectPtrLabel;
extern PFNDOLPOPDEBUGGROUPPROC dolPopDebugGroup;
extern PFNDOLPUSHDEBUGGROUPPROC dolPushDebugGroup;

#define glDebugMessageCallback dolDebugMessageCallback
#define glDebugMessageControl dolDebugMessageControl
#define glDebugMessageInsert dolDebugMessageInsert
#define glGetDebugMessageLog dolGetDebugMessageLog
#define glGetObjectLabel dolGetObjectLabel
#define glGetObjectPtrLabel dolGetObjectPtrLabel
#define glObjectLabel dolObjectLabel
#define glObjectPtrLabel dolObjectPtrLabel
#define glPopDebugGroup dolPopDebugGroup
#define glPushDebugGroup dolPushDebugGroup
