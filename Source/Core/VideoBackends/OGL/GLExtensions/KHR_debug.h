// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "gl_common.h"

typedef void (GLAPIENTRY  *GLDEBUGPROCKHR)(GLenum source,GLenum type,GLuint id,GLenum severity,GLsizei length,const GLchar *message,const void
*userParam);
#define GL_SAMPLER                        0x82E6
#define GL_DEBUG_OUTPUT_SYNCHRONOUS_KHR   0x8242
#define GL_DEBUG_NEXT_LOGGED_MESSAGE_LENGTH_KHR 0x8243
#define GL_DEBUG_CALLBACK_FUNCTION_KHR    0x8244
#define GL_DEBUG_CALLBACK_USER_PARAM_KHR  0x8245
#define GL_DEBUG_SOURCE_API_KHR           0x8246
#define GL_DEBUG_SOURCE_WINDOW_SYSTEM_KHR 0x8247
#define GL_DEBUG_SOURCE_SHADER_COMPILER_KHR 0x8248
#define GL_DEBUG_SOURCE_THIRD_PARTY_KHR   0x8249
#define GL_DEBUG_SOURCE_APPLICATION_KHR   0x824A
#define GL_DEBUG_SOURCE_OTHER_KHR         0x824B
#define GL_DEBUG_TYPE_ERROR_KHR           0x824C
#define GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_KHR 0x824D
#define GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_KHR 0x824E
#define GL_DEBUG_TYPE_PORTABILITY_KHR     0x824F
#define GL_DEBUG_TYPE_PERFORMANCE_KHR     0x8250
#define GL_DEBUG_TYPE_OTHER_KHR           0x8251
#define GL_DEBUG_TYPE_MARKER_KHR          0x8268
#define GL_DEBUG_TYPE_PUSH_GROUP_KHR      0x8269
#define GL_DEBUG_TYPE_POP_GROUP_KHR       0x826A
#define GL_DEBUG_SEVERITY_NOTIFICATION_KHR 0x826B
#define GL_MAX_DEBUG_GROUP_STACK_DEPTH_KHR 0x826C
#define GL_DEBUG_GROUP_STACK_DEPTH_KHR    0x826D
#define GL_BUFFER_KHR                     0x82E0
#define GL_SHADER_KHR                     0x82E1
#define GL_PROGRAM_KHR                    0x82E2
#define GL_VERTEX_ARRAY_KHR               0x8074
#define GL_QUERY_KHR                      0x82E3
#define GL_SAMPLER_KHR                    0x82E6
#define GL_MAX_LABEL_LENGTH_KHR           0x82E8
#define GL_MAX_DEBUG_MESSAGE_LENGTH_KHR   0x9143
#define GL_MAX_DEBUG_LOGGED_MESSAGES_KHR  0x9144
#define GL_DEBUG_LOGGED_MESSAGES_KHR      0x9145
#define GL_DEBUG_SEVERITY_HIGH_KHR        0x9146
#define GL_DEBUG_SEVERITY_MEDIUM_KHR      0x9147
#define GL_DEBUG_SEVERITY_LOW_KHR         0x9148
#define GL_DEBUG_OUTPUT_KHR               0x92E0
#define GL_CONTEXT_FLAG_DEBUG_BIT_KHR     0x00000002
#define GL_STACK_OVERFLOW_KHR             0x0503
#define GL_STACK_UNDERFLOW_KHR            0x0504

typedef void (GLAPIENTRY * PFNGLDEBUGMESSAGECONTROLKHRPROC) (GLenum source, GLenum type, GLenum severity, GLsizei count, const GLuint *ids, GLboolean enabled);
typedef void (GLAPIENTRY * PFNGLDEBUGMESSAGEINSERTKHRPROC) (GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *buf);
typedef void (GLAPIENTRY * PFNGLDEBUGMESSAGECALLBACKKHRPROC) (GLDEBUGPROCKHR callback, const void *userParam);
typedef GLuint (GLAPIENTRY * PFNGLGETDEBUGMESSAGELOGKHRPROC) (GLuint count, GLsizei bufSize, GLenum *sources, GLenum *types, GLuint *ids, GLenum *severities, GLsizei *lengths, GLchar *messageLog);
typedef void (GLAPIENTRY * PFNGLPUSHDEBUGGROUPKHRPROC) (GLenum source, GLuint id, GLsizei length, const GLchar *message);
typedef void (GLAPIENTRY * PFNGLPOPDEBUGGROUPKHRPROC) (void);
typedef void (GLAPIENTRY * PFNGLOBJECTLABELKHRPROC) (GLenum identifier, GLuint name, GLsizei length, const GLchar *label);
typedef void (GLAPIENTRY * PFNGLGETOBJECTLABELKHRPROC) (GLenum identifier, GLuint name, GLsizei bufSize, GLsizei *length, GLchar *label);
typedef void (GLAPIENTRY * PFNGLOBJECTPTRLABELKHRPROC) (const void *ptr, GLsizei length, const GLchar *label);
typedef void (GLAPIENTRY * PFNGLGETOBJECTPTRLABELKHRPROC) (const void *ptr, GLsizei bufSize, GLsizei *length, GLchar *label);
typedef void (GLAPIENTRY * PFNGLGETPOINTERVKHRPROC) (GLenum pname, void **params);

extern PFNGLDEBUGMESSAGECONTROLKHRPROC glDebugMessageControlKHR;
extern PFNGLDEBUGMESSAGEINSERTKHRPROC glDebugMessageInsertKHR;
extern PFNGLDEBUGMESSAGECALLBACKKHRPROC glDebugMessageCallbackKHR;
extern PFNGLGETDEBUGMESSAGELOGKHRPROC glGetDebugMessageLogKHR;
extern PFNGLPUSHDEBUGGROUPKHRPROC glPushDebugGroupKHR;
extern PFNGLPOPDEBUGGROUPKHRPROC glPopDebugGroupKHR;
extern PFNGLOBJECTLABELKHRPROC glObjectLabelKHR;
extern PFNGLGETOBJECTLABELKHRPROC glGetObjectLabelKHR;
extern PFNGLOBJECTPTRLABELKHRPROC glObjectPtrLabelKHR;
extern PFNGLGETOBJECTPTRLABELKHRPROC glGetObjectPtrLabelKHR;
extern PFNGLGETPOINTERVKHRPROC glGetPointervKHR;

