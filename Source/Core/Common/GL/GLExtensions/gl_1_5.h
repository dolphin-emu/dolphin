/*
** Copyright (c) 2013-2015 The Khronos Group Inc.
** SPDX-License-Identifier: MIT
*/

#include "Common/GL/GLExtensions/gl_common.h"

#define GL_BUFFER_SIZE 0x8764
#define GL_BUFFER_USAGE 0x8765
#define GL_QUERY_COUNTER_BITS 0x8864
#define GL_CURRENT_QUERY 0x8865
#define GL_QUERY_RESULT 0x8866
#define GL_QUERY_RESULT_AVAILABLE 0x8867
#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_ARRAY_BUFFER_BINDING 0x8894
#define GL_ELEMENT_ARRAY_BUFFER_BINDING 0x8895
#define GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING 0x889F
#define GL_READ_ONLY 0x88B8
#define GL_WRITE_ONLY 0x88B9
#define GL_READ_WRITE 0x88BA
#define GL_BUFFER_ACCESS 0x88BB
#define GL_BUFFER_MAPPED 0x88BC
#define GL_BUFFER_MAP_POINTER 0x88BD
#define GL_STREAM_DRAW 0x88E0
#define GL_STREAM_READ 0x88E1
#define GL_STREAM_COPY 0x88E2
#define GL_STATIC_DRAW 0x88E4
#define GL_STATIC_READ 0x88E5
#define GL_STATIC_COPY 0x88E6
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_DYNAMIC_READ 0x88E9
#define GL_DYNAMIC_COPY 0x88EA
#define GL_SAMPLES_PASSED 0x8914
#define GL_SRC1_ALPHA 0x8589
#define GL_VERTEX_ARRAY_BUFFER_BINDING 0x8896
#define GL_NORMAL_ARRAY_BUFFER_BINDING 0x8897
#define GL_COLOR_ARRAY_BUFFER_BINDING 0x8898
#define GL_INDEX_ARRAY_BUFFER_BINDING 0x8899
#define GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING 0x889A
#define GL_EDGE_FLAG_ARRAY_BUFFER_BINDING 0x889B
#define GL_SECONDARY_COLOR_ARRAY_BUFFER_BINDING 0x889C
#define GL_FOG_COORDINATE_ARRAY_BUFFER_BINDING 0x889D
#define GL_WEIGHT_ARRAY_BUFFER_BINDING 0x889E
#define GL_FOG_COORD_SRC 0x8450
#define GL_FOG_COORD 0x8451
#define GL_CURRENT_FOG_COORD 0x8453
#define GL_FOG_COORD_ARRAY_TYPE 0x8454
#define GL_FOG_COORD_ARRAY_STRIDE 0x8455
#define GL_FOG_COORD_ARRAY_POINTER 0x8456
#define GL_FOG_COORD_ARRAY 0x8457
#define GL_FOG_COORD_ARRAY_BUFFER_BINDING 0x889D
#define GL_SRC0_RGB 0x8580
#define GL_SRC1_RGB 0x8581
#define GL_SRC2_RGB 0x8582
#define GL_SRC0_ALPHA 0x8588
#define GL_SRC2_ALPHA 0x858A

typedef void(APIENTRYP PFNDOLGENQUERIESPROC)(GLsizei n, GLuint* ids);
typedef void(APIENTRYP PFNDOLDELETEQUERIESPROC)(GLsizei n, const GLuint* ids);
typedef GLboolean(APIENTRYP PFNDOLISQUERYPROC)(GLuint id);
typedef void(APIENTRYP PFNDOLBEGINQUERYPROC)(GLenum target, GLuint id);
typedef void(APIENTRYP PFNDOLENDQUERYPROC)(GLenum target);
typedef void(APIENTRYP PFNDOLGETQUERYIVPROC)(GLenum target, GLenum pname, GLint* params);
typedef void(APIENTRYP PFNDOLGETQUERYOBJECTIVPROC)(GLuint id, GLenum pname, GLint* params);
typedef void(APIENTRYP PFNDOLGETQUERYOBJECTUIVPROC)(GLuint id, GLenum pname, GLuint* params);
typedef void(APIENTRYP PFNDOLBINDBUFFERPROC)(GLenum target, GLuint buffer);
typedef void(APIENTRYP PFNDOLDELETEBUFFERSPROC)(GLsizei n, const GLuint* buffers);
typedef void(APIENTRYP PFNDOLGENBUFFERSPROC)(GLsizei n, GLuint* buffers);
typedef GLboolean(APIENTRYP PFNDOLISBUFFERPROC)(GLuint buffer);
typedef void(APIENTRYP PFNDOLBUFFERDATAPROC)(GLenum target, GLsizeiptr size, const void* data,
                                             GLenum usage);
typedef void(APIENTRYP PFNDOLBUFFERSUBDATAPROC)(GLenum target, GLintptr offset, GLsizeiptr size,
                                                const void* data);
typedef void(APIENTRYP PFNDOLGETBUFFERSUBDATAPROC)(GLenum target, GLintptr offset, GLsizeiptr size,
                                                   void* data);
typedef void*(APIENTRYP PFNDOLMAPBUFFERPROC)(GLenum target, GLenum access);
typedef GLboolean(APIENTRYP PFNDOLUNMAPBUFFERPROC)(GLenum target);
typedef void(APIENTRYP PFNDOLGETBUFFERPARAMETERIVPROC)(GLenum target, GLenum pname, GLint* params);
typedef void(APIENTRYP PFNDOLGETBUFFERPOINTERVPROC)(GLenum target, GLenum pname, void** params);

extern PFNDOLBEGINQUERYPROC dolBeginQuery;
extern PFNDOLBINDBUFFERPROC dolBindBuffer;
extern PFNDOLBUFFERDATAPROC dolBufferData;
extern PFNDOLBUFFERSUBDATAPROC dolBufferSubData;
extern PFNDOLDELETEBUFFERSPROC dolDeleteBuffers;
extern PFNDOLDELETEQUERIESPROC dolDeleteQueries;
extern PFNDOLENDQUERYPROC dolEndQuery;
extern PFNDOLGENBUFFERSPROC dolGenBuffers;
extern PFNDOLGENQUERIESPROC dolGenQueries;
extern PFNDOLGETBUFFERPARAMETERIVPROC dolGetBufferParameteriv;
extern PFNDOLGETBUFFERPOINTERVPROC dolGetBufferPointerv;
extern PFNDOLGETBUFFERSUBDATAPROC dolGetBufferSubData;
extern PFNDOLGETQUERYOBJECTIVPROC dolGetQueryObjectiv;
extern PFNDOLGETQUERYOBJECTUIVPROC dolGetQueryObjectuiv;
extern PFNDOLGETQUERYIVPROC dolGetQueryiv;
extern PFNDOLISBUFFERPROC dolIsBuffer;
extern PFNDOLISQUERYPROC dolIsQuery;
extern PFNDOLMAPBUFFERPROC dolMapBuffer;
extern PFNDOLUNMAPBUFFERPROC dolUnmapBuffer;

#define glBeginQuery dolBeginQuery
#define glBindBuffer dolBindBuffer
#define glBufferData dolBufferData
#define glBufferSubData dolBufferSubData
#define glDeleteBuffers dolDeleteBuffers
#define glDeleteQueries dolDeleteQueries
#define glEndQuery dolEndQuery
#define glGenBuffers dolGenBuffers
#define glGenQueries dolGenQueries
#define glGetBufferParameteriv dolGetBufferParameteriv
#define glGetBufferPointerv dolGetBufferPointerv
#define glGetBufferSubData dolGetBufferSubData
#define glGetQueryObjectiv dolGetQueryObjectiv
#define glGetQueryObjectuiv dolGetQueryObjectuiv
#define glGetQueryiv dolGetQueryiv
#define glIsBuffer dolIsBuffer
#define glIsQuery dolIsQuery
#define glMapBuffer dolMapBuffer
#define glUnmapBuffer dolUnmapBuffer
