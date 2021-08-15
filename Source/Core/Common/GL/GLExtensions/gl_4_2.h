/*
** Copyright (c) 2013-2015 The Khronos Group Inc.
** SPDX-License-Identifier: MIT
*/

#include "Common/GL/GLExtensions/gl_common.h"

#define GL_COPY_READ_BUFFER_BINDING 0x8F36
#define GL_COPY_WRITE_BUFFER_BINDING 0x8F37
#define GL_TRANSFORM_FEEDBACK_ACTIVE 0x8E24
#define GL_TRANSFORM_FEEDBACK_PAUSED 0x8E23
#define GL_UNPACK_COMPRESSED_BLOCK_WIDTH 0x9127
#define GL_UNPACK_COMPRESSED_BLOCK_HEIGHT 0x9128
#define GL_UNPACK_COMPRESSED_BLOCK_DEPTH 0x9129
#define GL_UNPACK_COMPRESSED_BLOCK_SIZE 0x912A
#define GL_PACK_COMPRESSED_BLOCK_WIDTH 0x912B
#define GL_PACK_COMPRESSED_BLOCK_HEIGHT 0x912C
#define GL_PACK_COMPRESSED_BLOCK_DEPTH 0x912D
#define GL_PACK_COMPRESSED_BLOCK_SIZE 0x912E
#define GL_NUM_SAMPLE_COUNTS 0x9380
#define GL_MIN_MAP_BUFFER_ALIGNMENT 0x90BC
#define GL_ATOMIC_COUNTER_BUFFER 0x92C0
#define GL_ATOMIC_COUNTER_BUFFER_BINDING 0x92C1
#define GL_ATOMIC_COUNTER_BUFFER_START 0x92C2
#define GL_ATOMIC_COUNTER_BUFFER_SIZE 0x92C3
#define GL_ATOMIC_COUNTER_BUFFER_DATA_SIZE 0x92C4
#define GL_ATOMIC_COUNTER_BUFFER_ACTIVE_ATOMIC_COUNTERS 0x92C5
#define GL_ATOMIC_COUNTER_BUFFER_ACTIVE_ATOMIC_COUNTER_INDICES 0x92C6
#define GL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_VERTEX_SHADER 0x92C7
#define GL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_TESS_CONTROL_SHADER 0x92C8
#define GL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_TESS_EVALUATION_SHADER 0x92C9
#define GL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_GEOMETRY_SHADER 0x92CA
#define GL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_FRAGMENT_SHADER 0x92CB
#define GL_MAX_VERTEX_ATOMIC_COUNTER_BUFFERS 0x92CC
#define GL_MAX_TESS_CONTROL_ATOMIC_COUNTER_BUFFERS 0x92CD
#define GL_MAX_TESS_EVALUATION_ATOMIC_COUNTER_BUFFERS 0x92CE
#define GL_MAX_GEOMETRY_ATOMIC_COUNTER_BUFFERS 0x92CF
#define GL_MAX_FRAGMENT_ATOMIC_COUNTER_BUFFERS 0x92D0
#define GL_MAX_COMBINED_ATOMIC_COUNTER_BUFFERS 0x92D1
#define GL_MAX_VERTEX_ATOMIC_COUNTERS 0x92D2
#define GL_MAX_TESS_CONTROL_ATOMIC_COUNTERS 0x92D3
#define GL_MAX_TESS_EVALUATION_ATOMIC_COUNTERS 0x92D4
#define GL_MAX_GEOMETRY_ATOMIC_COUNTERS 0x92D5
#define GL_MAX_FRAGMENT_ATOMIC_COUNTERS 0x92D6
#define GL_MAX_COMBINED_ATOMIC_COUNTERS 0x92D7
#define GL_MAX_ATOMIC_COUNTER_BUFFER_SIZE 0x92D8
#define GL_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS 0x92DC
#define GL_ACTIVE_ATOMIC_COUNTER_BUFFERS 0x92D9
#define GL_UNIFORM_ATOMIC_COUNTER_BUFFER_INDEX 0x92DA
#define GL_UNSIGNED_INT_ATOMIC_COUNTER 0x92DB
#define GL_COMPRESSED_RGBA_BPTC_UNORM 0x8E8C
#define GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM 0x8E8D
#define GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT 0x8E8E
#define GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT 0x8E8F

typedef void(APIENTRYP PFNDOLDRAWARRAYSINSTANCEDBASEINSTANCEPROC)(GLenum mode, GLint first,
                                                                  GLsizei count,
                                                                  GLsizei instancecount,
                                                                  GLuint baseinstance);
typedef void(APIENTRYP PFNDOLDRAWELEMENTSINSTANCEDBASEINSTANCEPROC)(GLenum mode, GLsizei count,
                                                                    GLenum type,
                                                                    const void* indices,
                                                                    GLsizei instancecount,
                                                                    GLuint baseinstance);
typedef void(APIENTRYP PFNDOLDRAWELEMENTSINSTANCEDBASEVERTEXBASEINSTANCEPROC)(
    GLenum mode, GLsizei count, GLenum type, const void* indices, GLsizei instancecount,
    GLint basevertex, GLuint baseinstance);
typedef void(APIENTRYP PFNDOLGETINTERNALFORMATIVPROC)(GLenum target, GLenum internalformat,
                                                      GLenum pname, GLsizei bufSize, GLint* params);
typedef void(APIENTRYP PFNDOLGETACTIVEATOMICCOUNTERBUFFERIVPROC)(GLuint program, GLuint bufferIndex,
                                                                 GLenum pname, GLint* params);
typedef void(APIENTRYP PFNDOLDRAWTRANSFORMFEEDBACKINSTANCEDPROC)(GLenum mode, GLuint id,
                                                                 GLsizei instancecount);
typedef void(APIENTRYP PFNDOLDRAWTRANSFORMFEEDBACKSTREAMINSTANCEDPROC)(GLenum mode, GLuint id,
                                                                       GLuint stream,
                                                                       GLsizei instancecount);

extern PFNDOLDRAWARRAYSINSTANCEDBASEINSTANCEPROC dolDrawArraysInstancedBaseInstance;
extern PFNDOLDRAWELEMENTSINSTANCEDBASEINSTANCEPROC dolDrawElementsInstancedBaseInstance;
extern PFNDOLDRAWELEMENTSINSTANCEDBASEVERTEXBASEINSTANCEPROC
    dolDrawElementsInstancedBaseVertexBaseInstance;
extern PFNDOLGETINTERNALFORMATIVPROC dolGetInternalformativ;
extern PFNDOLGETACTIVEATOMICCOUNTERBUFFERIVPROC dolGetActiveAtomicCounterBufferiv;
extern PFNDOLDRAWTRANSFORMFEEDBACKINSTANCEDPROC dolDrawTransformFeedbackInstanced;
extern PFNDOLDRAWTRANSFORMFEEDBACKSTREAMINSTANCEDPROC dolDrawTransformFeedbackStreamInstanced;

#define glDrawArraysInstancedBaseInstance dolDrawArraysInstancedBaseInstance
#define glDrawElementsInstancedBaseInstance dolDrawElementsInstancedBaseInstance
#define glDrawElementsInstancedBaseVertexBaseInstance dolDrawElementsInstancedBaseVertexBaseInstance
#define glGetInternalformativ dolGetInternalformativ
#define glGetActiveAtomicCounterBufferiv dolGetActiveAtomicCounterBufferiv
#define glDrawTransformFeedbackInstanced dolDrawTransformFeedbackInstanced
#define glDrawTransformFeedbackStreamInstanced dolDrawTransformFeedbackStreamInstanced
