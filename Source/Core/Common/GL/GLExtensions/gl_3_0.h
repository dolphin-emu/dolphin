/*
** Copyright (c) 2013-2015 The Khronos Group Inc.
** SPDX-License-Identifier: MIT
*/

#include "Common/GL/GLExtensions/gl_common.h"

#define GL_COMPARE_REF_TO_TEXTURE 0x884E
#define GL_CLIP_DISTANCE0 0x3000
#define GL_CLIP_DISTANCE1 0x3001
#define GL_CLIP_DISTANCE2 0x3002
#define GL_CLIP_DISTANCE3 0x3003
#define GL_CLIP_DISTANCE4 0x3004
#define GL_CLIP_DISTANCE5 0x3005
#define GL_CLIP_DISTANCE6 0x3006
#define GL_CLIP_DISTANCE7 0x3007
#define GL_MAX_CLIP_DISTANCES 0x0D32
#define GL_MAJOR_VERSION 0x821B
#define GL_MINOR_VERSION 0x821C
#define GL_NUM_EXTENSIONS 0x821D
#define GL_CONTEXT_FLAGS 0x821E
#define GL_COMPRESSED_RED 0x8225
#define GL_COMPRESSED_RG 0x8226
#define GL_CONTEXT_FLAG_FORWARD_COMPATIBLE_BIT 0x00000001
#define GL_RGBA32F 0x8814
#define GL_RGB32F 0x8815
#define GL_RGBA16F 0x881A
#define GL_RGB16F 0x881B
#define GL_VERTEX_ATTRIB_ARRAY_INTEGER 0x88FD
#define GL_MAX_ARRAY_TEXTURE_LAYERS 0x88FF
#define GL_MIN_PROGRAM_TEXEL_OFFSET 0x8904
#define GL_MAX_PROGRAM_TEXEL_OFFSET 0x8905
#define GL_CLAMP_READ_COLOR 0x891C
#define GL_FIXED_ONLY 0x891D
#define GL_MAX_VARYING_COMPONENTS 0x8B4B
#define GL_TEXTURE_1D_ARRAY 0x8C18
#define GL_PROXY_TEXTURE_1D_ARRAY 0x8C19
#define GL_TEXTURE_2D_ARRAY 0x8C1A
#define GL_PROXY_TEXTURE_2D_ARRAY 0x8C1B
#define GL_TEXTURE_BINDING_1D_ARRAY 0x8C1C
#define GL_TEXTURE_BINDING_2D_ARRAY 0x8C1D
#define GL_R11F_G11F_B10F 0x8C3A
#define GL_UNSIGNED_INT_10F_11F_11F_REV 0x8C3B
#define GL_RGB9_E5 0x8C3D
#define GL_UNSIGNED_INT_5_9_9_9_REV 0x8C3E
#define GL_TEXTURE_SHARED_SIZE 0x8C3F
#define GL_TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH 0x8C76
#define GL_TRANSFORM_FEEDBACK_BUFFER_MODE 0x8C7F
#define GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS 0x8C80
#define GL_TRANSFORM_FEEDBACK_VARYINGS 0x8C83
#define GL_TRANSFORM_FEEDBACK_BUFFER_START 0x8C84
#define GL_TRANSFORM_FEEDBACK_BUFFER_SIZE 0x8C85
#define GL_PRIMITIVES_GENERATED 0x8C87
#define GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN 0x8C88
#define GL_RASTERIZER_DISCARD 0x8C89
#define GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS 0x8C8A
#define GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS 0x8C8B
#define GL_INTERLEAVED_ATTRIBS 0x8C8C
#define GL_SEPARATE_ATTRIBS 0x8C8D
#define GL_TRANSFORM_FEEDBACK_BUFFER 0x8C8E
#define GL_TRANSFORM_FEEDBACK_BUFFER_BINDING 0x8C8F
#define GL_RGBA32UI 0x8D70
#define GL_RGB32UI 0x8D71
#define GL_RGBA16UI 0x8D76
#define GL_RGB16UI 0x8D77
#define GL_RGBA8UI 0x8D7C
#define GL_RGB8UI 0x8D7D
#define GL_RGBA32I 0x8D82
#define GL_RGB32I 0x8D83
#define GL_RGBA16I 0x8D88
#define GL_RGB16I 0x8D89
#define GL_RGBA8I 0x8D8E
#define GL_RGB8I 0x8D8F
#define GL_RED_INTEGER 0x8D94
#define GL_GREEN_INTEGER 0x8D95
#define GL_BLUE_INTEGER 0x8D96
#define GL_RGB_INTEGER 0x8D98
#define GL_RGBA_INTEGER 0x8D99
#define GL_BGR_INTEGER 0x8D9A
#define GL_BGRA_INTEGER 0x8D9B
#define GL_SAMPLER_1D_ARRAY 0x8DC0
#define GL_SAMPLER_2D_ARRAY 0x8DC1
#define GL_SAMPLER_1D_ARRAY_SHADOW 0x8DC3
#define GL_SAMPLER_2D_ARRAY_SHADOW 0x8DC4
#define GL_SAMPLER_CUBE_SHADOW 0x8DC5
#define GL_UNSIGNED_INT_VEC2 0x8DC6
#define GL_UNSIGNED_INT_VEC3 0x8DC7
#define GL_UNSIGNED_INT_VEC4 0x8DC8
#define GL_INT_SAMPLER_1D 0x8DC9
#define GL_INT_SAMPLER_2D 0x8DCA
#define GL_INT_SAMPLER_3D 0x8DCB
#define GL_INT_SAMPLER_CUBE 0x8DCC
#define GL_INT_SAMPLER_1D_ARRAY 0x8DCE
#define GL_INT_SAMPLER_2D_ARRAY 0x8DCF
#define GL_UNSIGNED_INT_SAMPLER_1D 0x8DD1
#define GL_UNSIGNED_INT_SAMPLER_2D 0x8DD2
#define GL_UNSIGNED_INT_SAMPLER_3D 0x8DD3
#define GL_UNSIGNED_INT_SAMPLER_CUBE 0x8DD4
#define GL_UNSIGNED_INT_SAMPLER_1D_ARRAY 0x8DD6
#define GL_UNSIGNED_INT_SAMPLER_2D_ARRAY 0x8DD7
#define GL_QUERY_WAIT 0x8E13
#define GL_QUERY_NO_WAIT 0x8E14
#define GL_QUERY_BY_REGION_WAIT 0x8E15
#define GL_QUERY_BY_REGION_NO_WAIT 0x8E16
#define GL_BUFFER_ACCESS_FLAGS 0x911F
#define GL_BUFFER_MAP_LENGTH 0x9120
#define GL_BUFFER_MAP_OFFSET 0x9121
#define GL_DEPTH_COMPONENT32F 0x8CAC
#define GL_DEPTH32F_STENCIL8 0x8CAD
#define GL_FLOAT_32_UNSIGNED_INT_24_8_REV 0x8DAD
#define GL_INVALID_FRAMEBUFFER_OPERATION 0x0506
#define GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING 0x8210
#define GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE 0x8211
#define GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE 0x8212
#define GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE 0x8213
#define GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE 0x8214
#define GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE 0x8215
#define GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE 0x8216
#define GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE 0x8217
#define GL_FRAMEBUFFER_DEFAULT 0x8218
#define GL_FRAMEBUFFER_UNDEFINED 0x8219
#define GL_DEPTH_STENCIL_ATTACHMENT 0x821A
#define GL_MAX_RENDERBUFFER_SIZE 0x84E8
#define GL_DEPTH_STENCIL 0x84F9
#define GL_UNSIGNED_INT_24_8 0x84FA
#define GL_DEPTH24_STENCIL8 0x88F0
#define GL_TEXTURE_STENCIL_SIZE 0x88F1
#define GL_TEXTURE_RED_TYPE 0x8C10
#define GL_TEXTURE_GREEN_TYPE 0x8C11
#define GL_TEXTURE_BLUE_TYPE 0x8C12
#define GL_TEXTURE_ALPHA_TYPE 0x8C13
#define GL_TEXTURE_DEPTH_TYPE 0x8C16
#define GL_UNSIGNED_NORMALIZED 0x8C17
#define GL_FRAMEBUFFER_BINDING 0x8CA6
#define GL_DRAW_FRAMEBUFFER_BINDING 0x8CA6
#define GL_RENDERBUFFER_BINDING 0x8CA7
#define GL_READ_FRAMEBUFFER 0x8CA8
#define GL_DRAW_FRAMEBUFFER 0x8CA9
#define GL_READ_FRAMEBUFFER_BINDING 0x8CAA
#define GL_RENDERBUFFER_SAMPLES 0x8CAB
#define GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE 0x8CD0
#define GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME 0x8CD1
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL 0x8CD2
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE 0x8CD3
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER 0x8CD4
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT 0x8CD6
#define GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT 0x8CD7
#define GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER 0x8CDB
#define GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER 0x8CDC
#define GL_FRAMEBUFFER_UNSUPPORTED 0x8CDD
#define GL_MAX_COLOR_ATTACHMENTS 0x8CDF
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_COLOR_ATTACHMENT1 0x8CE1
#define GL_COLOR_ATTACHMENT2 0x8CE2
#define GL_COLOR_ATTACHMENT3 0x8CE3
#define GL_COLOR_ATTACHMENT4 0x8CE4
#define GL_COLOR_ATTACHMENT5 0x8CE5
#define GL_COLOR_ATTACHMENT6 0x8CE6
#define GL_COLOR_ATTACHMENT7 0x8CE7
#define GL_COLOR_ATTACHMENT8 0x8CE8
#define GL_COLOR_ATTACHMENT9 0x8CE9
#define GL_COLOR_ATTACHMENT10 0x8CEA
#define GL_COLOR_ATTACHMENT11 0x8CEB
#define GL_COLOR_ATTACHMENT12 0x8CEC
#define GL_COLOR_ATTACHMENT13 0x8CED
#define GL_COLOR_ATTACHMENT14 0x8CEE
#define GL_COLOR_ATTACHMENT15 0x8CEF
#define GL_COLOR_ATTACHMENT16 0x8CF0
#define GL_COLOR_ATTACHMENT17 0x8CF1
#define GL_COLOR_ATTACHMENT18 0x8CF2
#define GL_COLOR_ATTACHMENT19 0x8CF3
#define GL_COLOR_ATTACHMENT20 0x8CF4
#define GL_COLOR_ATTACHMENT21 0x8CF5
#define GL_COLOR_ATTACHMENT22 0x8CF6
#define GL_COLOR_ATTACHMENT23 0x8CF7
#define GL_COLOR_ATTACHMENT24 0x8CF8
#define GL_COLOR_ATTACHMENT25 0x8CF9
#define GL_COLOR_ATTACHMENT26 0x8CFA
#define GL_COLOR_ATTACHMENT27 0x8CFB
#define GL_COLOR_ATTACHMENT28 0x8CFC
#define GL_COLOR_ATTACHMENT29 0x8CFD
#define GL_COLOR_ATTACHMENT30 0x8CFE
#define GL_COLOR_ATTACHMENT31 0x8CFF
#define GL_DEPTH_ATTACHMENT 0x8D00
#define GL_STENCIL_ATTACHMENT 0x8D20
#define GL_FRAMEBUFFER 0x8D40
#define GL_RENDERBUFFER 0x8D41
#define GL_RENDERBUFFER_WIDTH 0x8D42
#define GL_RENDERBUFFER_HEIGHT 0x8D43
#define GL_RENDERBUFFER_INTERNAL_FORMAT 0x8D44
#define GL_STENCIL_INDEX1 0x8D46
#define GL_STENCIL_INDEX4 0x8D47
#define GL_STENCIL_INDEX8 0x8D48
#define GL_STENCIL_INDEX16 0x8D49
#define GL_RENDERBUFFER_RED_SIZE 0x8D50
#define GL_RENDERBUFFER_GREEN_SIZE 0x8D51
#define GL_RENDERBUFFER_BLUE_SIZE 0x8D52
#define GL_RENDERBUFFER_ALPHA_SIZE 0x8D53
#define GL_RENDERBUFFER_DEPTH_SIZE 0x8D54
#define GL_RENDERBUFFER_STENCIL_SIZE 0x8D55
#define GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE 0x8D56
#define GL_MAX_SAMPLES 0x8D57
#define GL_INDEX 0x8222
#define GL_TEXTURE_LUMINANCE_TYPE 0x8C14
#define GL_TEXTURE_INTENSITY_TYPE 0x8C15
#define GL_FRAMEBUFFER_SRGB 0x8DB9
#define GL_HALF_FLOAT 0x140B
#define GL_MAP_READ_BIT 0x0001
#define GL_MAP_WRITE_BIT 0x0002
#define GL_MAP_INVALIDATE_RANGE_BIT 0x0004
#define GL_MAP_INVALIDATE_BUFFER_BIT 0x0008
#define GL_MAP_FLUSH_EXPLICIT_BIT 0x0010
#define GL_MAP_UNSYNCHRONIZED_BIT 0x0020
#define GL_COMPRESSED_RED_RGTC1 0x8DBB
#define GL_COMPRESSED_SIGNED_RED_RGTC1 0x8DBC
#define GL_COMPRESSED_RG_RGTC2 0x8DBD
#define GL_COMPRESSED_SIGNED_RG_RGTC2 0x8DBE
#define GL_RG 0x8227
#define GL_RG_INTEGER 0x8228
#define GL_R8 0x8229
#define GL_R16 0x822A
#define GL_RG8 0x822B
#define GL_RG16 0x822C
#define GL_R16F 0x822D
#define GL_R32F 0x822E
#define GL_RG16F 0x822F
#define GL_RG32F 0x8230
#define GL_R8I 0x8231
#define GL_R8UI 0x8232
#define GL_R16I 0x8233
#define GL_R16UI 0x8234
#define GL_R32I 0x8235
#define GL_R32UI 0x8236
#define GL_RG8I 0x8237
#define GL_RG8UI 0x8238
#define GL_RG16I 0x8239
#define GL_RG16UI 0x823A
#define GL_RG32I 0x823B
#define GL_RG32UI 0x823C
#define GL_VERTEX_ARRAY_BINDING 0x85B5
#define GL_CLAMP_VERTEX_COLOR 0x891A
#define GL_CLAMP_FRAGMENT_COLOR 0x891B
#define GL_ALPHA_INTEGER 0x8D97

typedef void(APIENTRYP PFNDOLCOLORMASKIPROC)(GLuint index, GLboolean r, GLboolean g, GLboolean b,
                                             GLboolean a);
typedef void(APIENTRYP PFNDOLGETBOOLEANI_VPROC)(GLenum target, GLuint index, GLboolean* data);
typedef void(APIENTRYP PFNDOLGETINTEGERI_VPROC)(GLenum target, GLuint index, GLint* data);
typedef void(APIENTRYP PFNDOLENABLEIPROC)(GLenum target, GLuint index);
typedef void(APIENTRYP PFNDOLDISABLEIPROC)(GLenum target, GLuint index);
typedef GLboolean(APIENTRYP PFNDOLISENABLEDIPROC)(GLenum target, GLuint index);
typedef void(APIENTRYP PFNDOLBEGINTRANSFORMFEEDBACKPROC)(GLenum primitiveMode);
typedef void(APIENTRYP PFNDOLENDTRANSFORMFEEDBACKPROC)(void);
typedef void(APIENTRYP PFNDOLBINDBUFFERRANGEPROC)(GLenum target, GLuint index, GLuint buffer,
                                                  GLintptr offset, GLsizeiptr size);
typedef void(APIENTRYP PFNDOLBINDBUFFERBASEPROC)(GLenum target, GLuint index, GLuint buffer);
typedef void(APIENTRYP PFNDOLTRANSFORMFEEDBACKVARYINGSPROC)(GLuint program, GLsizei count,
                                                            const GLchar* const* varyings,
                                                            GLenum bufferMode);
typedef void(APIENTRYP PFNDOLGETTRANSFORMFEEDBACKVARYINGPROC)(GLuint program, GLuint index,
                                                              GLsizei bufSize, GLsizei* length,
                                                              GLsizei* size, GLenum* type,
                                                              GLchar* name);
typedef void(APIENTRYP PFNDOLCLAMPCOLORPROC)(GLenum target, GLenum clamp);
typedef void(APIENTRYP PFNDOLBEGINCONDITIONALRENDERPROC)(GLuint id, GLenum mode);
typedef void(APIENTRYP PFNDOLENDCONDITIONALRENDERPROC)(void);
typedef void(APIENTRYP PFNDOLVERTEXATTRIBIPOINTERPROC)(GLuint index, GLint size, GLenum type,
                                                       GLsizei stride, const void* pointer);
typedef void(APIENTRYP PFNDOLGETVERTEXATTRIBIIVPROC)(GLuint index, GLenum pname, GLint* params);
typedef void(APIENTRYP PFNDOLGETVERTEXATTRIBIUIVPROC)(GLuint index, GLenum pname, GLuint* params);
typedef void(APIENTRYP PFNDOLVERTEXATTRIBI1IPROC)(GLuint index, GLint x);
typedef void(APIENTRYP PFNDOLVERTEXATTRIBI2IPROC)(GLuint index, GLint x, GLint y);
typedef void(APIENTRYP PFNDOLVERTEXATTRIBI3IPROC)(GLuint index, GLint x, GLint y, GLint z);
typedef void(APIENTRYP PFNDOLVERTEXATTRIBI4IPROC)(GLuint index, GLint x, GLint y, GLint z, GLint w);
typedef void(APIENTRYP PFNDOLVERTEXATTRIBI1UIPROC)(GLuint index, GLuint x);
typedef void(APIENTRYP PFNDOLVERTEXATTRIBI2UIPROC)(GLuint index, GLuint x, GLuint y);
typedef void(APIENTRYP PFNDOLVERTEXATTRIBI3UIPROC)(GLuint index, GLuint x, GLuint y, GLuint z);
typedef void(APIENTRYP PFNDOLVERTEXATTRIBI4UIPROC)(GLuint index, GLuint x, GLuint y, GLuint z,
                                                   GLuint w);
typedef void(APIENTRYP PFNDOLVERTEXATTRIBI1IVPROC)(GLuint index, const GLint* v);
typedef void(APIENTRYP PFNDOLVERTEXATTRIBI2IVPROC)(GLuint index, const GLint* v);
typedef void(APIENTRYP PFNDOLVERTEXATTRIBI3IVPROC)(GLuint index, const GLint* v);
typedef void(APIENTRYP PFNDOLVERTEXATTRIBI4IVPROC)(GLuint index, const GLint* v);
typedef void(APIENTRYP PFNDOLVERTEXATTRIBI1UIVPROC)(GLuint index, const GLuint* v);
typedef void(APIENTRYP PFNDOLVERTEXATTRIBI2UIVPROC)(GLuint index, const GLuint* v);
typedef void(APIENTRYP PFNDOLVERTEXATTRIBI3UIVPROC)(GLuint index, const GLuint* v);
typedef void(APIENTRYP PFNDOLVERTEXATTRIBI4UIVPROC)(GLuint index, const GLuint* v);
typedef void(APIENTRYP PFNDOLVERTEXATTRIBI4BVPROC)(GLuint index, const GLbyte* v);
typedef void(APIENTRYP PFNDOLVERTEXATTRIBI4SVPROC)(GLuint index, const GLshort* v);
typedef void(APIENTRYP PFNDOLVERTEXATTRIBI4UBVPROC)(GLuint index, const GLubyte* v);
typedef void(APIENTRYP PFNDOLVERTEXATTRIBI4USVPROC)(GLuint index, const GLushort* v);
typedef void(APIENTRYP PFNDOLGETUNIFORMUIVPROC)(GLuint program, GLint location, GLuint* params);
typedef void(APIENTRYP PFNDOLBINDFRAGDATALOCATIONPROC)(GLuint program, GLuint color,
                                                       const GLchar* name);
typedef GLint(APIENTRYP PFNDOLGETFRAGDATALOCATIONPROC)(GLuint program, const GLchar* name);
typedef void(APIENTRYP PFNDOLUNIFORM1UIPROC)(GLint location, GLuint v0);
typedef void(APIENTRYP PFNDOLUNIFORM2UIPROC)(GLint location, GLuint v0, GLuint v1);
typedef void(APIENTRYP PFNDOLUNIFORM3UIPROC)(GLint location, GLuint v0, GLuint v1, GLuint v2);
typedef void(APIENTRYP PFNDOLUNIFORM4UIPROC)(GLint location, GLuint v0, GLuint v1, GLuint v2,
                                             GLuint v3);
typedef void(APIENTRYP PFNDOLUNIFORM1UIVPROC)(GLint location, GLsizei count, const GLuint* value);
typedef void(APIENTRYP PFNDOLUNIFORM2UIVPROC)(GLint location, GLsizei count, const GLuint* value);
typedef void(APIENTRYP PFNDOLUNIFORM3UIVPROC)(GLint location, GLsizei count, const GLuint* value);
typedef void(APIENTRYP PFNDOLUNIFORM4UIVPROC)(GLint location, GLsizei count, const GLuint* value);
typedef void(APIENTRYP PFNDOLTEXPARAMETERIIVPROC)(GLenum target, GLenum pname, const GLint* params);
typedef void(APIENTRYP PFNDOLTEXPARAMETERIUIVPROC)(GLenum target, GLenum pname,
                                                   const GLuint* params);
typedef void(APIENTRYP PFNDOLGETTEXPARAMETERIIVPROC)(GLenum target, GLenum pname, GLint* params);
typedef void(APIENTRYP PFNDOLGETTEXPARAMETERIUIVPROC)(GLenum target, GLenum pname, GLuint* params);
typedef void(APIENTRYP PFNDOLCLEARBUFFERIVPROC)(GLenum buffer, GLint drawbuffer,
                                                const GLint* value);
typedef void(APIENTRYP PFNDOLCLEARBUFFERUIVPROC)(GLenum buffer, GLint drawbuffer,
                                                 const GLuint* value);
typedef void(APIENTRYP PFNDOLCLEARBUFFERFVPROC)(GLenum buffer, GLint drawbuffer,
                                                const GLfloat* value);
typedef void(APIENTRYP PFNDOLCLEARBUFFERFIPROC)(GLenum buffer, GLint drawbuffer, GLfloat depth,
                                                GLint stencil);
typedef const GLubyte*(APIENTRYP PFNDOLGETSTRINGIPROC)(GLenum name, GLuint index);
typedef GLboolean(APIENTRYP PFNDOLISRENDERBUFFERPROC)(GLuint renderbuffer);
typedef void(APIENTRYP PFNDOLBINDRENDERBUFFERPROC)(GLenum target, GLuint renderbuffer);
typedef void(APIENTRYP PFNDOLDELETERENDERBUFFERSPROC)(GLsizei n, const GLuint* renderbuffers);
typedef void(APIENTRYP PFNDOLGENRENDERBUFFERSPROC)(GLsizei n, GLuint* renderbuffers);
typedef void(APIENTRYP PFNDOLRENDERBUFFERSTORAGEPROC)(GLenum target, GLenum internalformat,
                                                      GLsizei width, GLsizei height);
typedef void(APIENTRYP PFNDOLGETRENDERBUFFERPARAMETERIVPROC)(GLenum target, GLenum pname,
                                                             GLint* params);
typedef GLboolean(APIENTRYP PFNDOLISFRAMEBUFFERPROC)(GLuint framebuffer);
typedef void(APIENTRYP PFNDOLBINDFRAMEBUFFERPROC)(GLenum target, GLuint framebuffer);
typedef void(APIENTRYP PFNDOLDELETEFRAMEBUFFERSPROC)(GLsizei n, const GLuint* framebuffers);
typedef void(APIENTRYP PFNDOLGENFRAMEBUFFERSPROC)(GLsizei n, GLuint* framebuffers);
typedef GLenum(APIENTRYP PFNDOLCHECKFRAMEBUFFERSTATUSPROC)(GLenum target);
typedef void(APIENTRYP PFNDOLFRAMEBUFFERTEXTURE1DPROC)(GLenum target, GLenum attachment,
                                                       GLenum textarget, GLuint texture,
                                                       GLint level);
typedef void(APIENTRYP PFNDOLFRAMEBUFFERTEXTURE2DPROC)(GLenum target, GLenum attachment,
                                                       GLenum textarget, GLuint texture,
                                                       GLint level);
typedef void(APIENTRYP PFNDOLFRAMEBUFFERTEXTURE3DPROC)(GLenum target, GLenum attachment,
                                                       GLenum textarget, GLuint texture,
                                                       GLint level, GLint zoffset);
typedef void(APIENTRYP PFNDOLFRAMEBUFFERRENDERBUFFERPROC)(GLenum target, GLenum attachment,
                                                          GLenum renderbuffertarget,
                                                          GLuint renderbuffer);
typedef void(APIENTRYP PFNDOLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC)(GLenum target,
                                                                      GLenum attachment,
                                                                      GLenum pname, GLint* params);
typedef void(APIENTRYP PFNDOLGENERATEMIPMAPPROC)(GLenum target);
typedef void(APIENTRYP PFNDOLBLITFRAMEBUFFERPROC)(GLint srcX0, GLint srcY0, GLint srcX1,
                                                  GLint srcY1, GLint dstX0, GLint dstY0,
                                                  GLint dstX1, GLint dstY1, GLbitfield mask,
                                                  GLenum filter);
typedef void(APIENTRYP PFNDOLRENDERBUFFERSTORAGEMULTISAMPLEPROC)(GLenum target, GLsizei samples,
                                                                 GLenum internalformat,
                                                                 GLsizei width, GLsizei height);
typedef void(APIENTRYP PFNDOLFRAMEBUFFERTEXTURELAYERPROC)(GLenum target, GLenum attachment,
                                                          GLuint texture, GLint level, GLint layer);
typedef void*(APIENTRYP PFNDOLMAPBUFFERRANGEPROC)(GLenum target, GLintptr offset, GLsizeiptr length,
                                                  GLbitfield access);
typedef void(APIENTRYP PFNDOLFLUSHMAPPEDBUFFERRANGEPROC)(GLenum target, GLintptr offset,
                                                         GLsizeiptr length);
typedef void(APIENTRYP PFNDOLBINDVERTEXARRAYPROC)(GLuint array);
typedef void(APIENTRYP PFNDOLDELETEVERTEXARRAYSPROC)(GLsizei n, const GLuint* arrays);
typedef void(APIENTRYP PFNDOLGENVERTEXARRAYSPROC)(GLsizei n, GLuint* arrays);
typedef GLboolean(APIENTRYP PFNDOLISVERTEXARRAYPROC)(GLuint array);

extern PFNDOLBEGINCONDITIONALRENDERPROC dolBeginConditionalRender;
extern PFNDOLBEGINTRANSFORMFEEDBACKPROC dolBeginTransformFeedback;
extern PFNDOLBINDFRAGDATALOCATIONPROC dolBindFragDataLocation;
extern PFNDOLCLAMPCOLORPROC dolClampColor;
extern PFNDOLCLEARBUFFERFIPROC dolClearBufferfi;
extern PFNDOLCLEARBUFFERFVPROC dolClearBufferfv;
extern PFNDOLCLEARBUFFERIVPROC dolClearBufferiv;
extern PFNDOLCLEARBUFFERUIVPROC dolClearBufferuiv;
extern PFNDOLCOLORMASKIPROC dolColorMaski;
extern PFNDOLDISABLEIPROC dolDisablei;
extern PFNDOLENABLEIPROC dolEnablei;
extern PFNDOLENDCONDITIONALRENDERPROC dolEndConditionalRender;
extern PFNDOLENDTRANSFORMFEEDBACKPROC dolEndTransformFeedback;
extern PFNDOLGETBOOLEANI_VPROC dolGetBooleani_v;
extern PFNDOLGETFRAGDATALOCATIONPROC dolGetFragDataLocation;
extern PFNDOLGETSTRINGIPROC dolGetStringi;
extern PFNDOLGETTEXPARAMETERIIVPROC dolGetTexParameterIiv;
extern PFNDOLGETTEXPARAMETERIUIVPROC dolGetTexParameterIuiv;
extern PFNDOLGETTRANSFORMFEEDBACKVARYINGPROC dolGetTransformFeedbackVarying;
extern PFNDOLGETUNIFORMUIVPROC dolGetUniformuiv;
extern PFNDOLGETVERTEXATTRIBIIVPROC dolGetVertexAttribIiv;
extern PFNDOLGETVERTEXATTRIBIUIVPROC dolGetVertexAttribIuiv;
extern PFNDOLISENABLEDIPROC dolIsEnabledi;
extern PFNDOLTEXPARAMETERIIVPROC dolTexParameterIiv;
extern PFNDOLTEXPARAMETERIUIVPROC dolTexParameterIuiv;
extern PFNDOLTRANSFORMFEEDBACKVARYINGSPROC dolTransformFeedbackVaryings;
extern PFNDOLUNIFORM1UIPROC dolUniform1ui;
extern PFNDOLUNIFORM1UIVPROC dolUniform1uiv;
extern PFNDOLUNIFORM2UIPROC dolUniform2ui;
extern PFNDOLUNIFORM2UIVPROC dolUniform2uiv;
extern PFNDOLUNIFORM3UIPROC dolUniform3ui;
extern PFNDOLUNIFORM3UIVPROC dolUniform3uiv;
extern PFNDOLUNIFORM4UIPROC dolUniform4ui;
extern PFNDOLUNIFORM4UIVPROC dolUniform4uiv;
extern PFNDOLVERTEXATTRIBI1IPROC dolVertexAttribI1i;
extern PFNDOLVERTEXATTRIBI1IVPROC dolVertexAttribI1iv;
extern PFNDOLVERTEXATTRIBI1UIPROC dolVertexAttribI1ui;
extern PFNDOLVERTEXATTRIBI1UIVPROC dolVertexAttribI1uiv;
extern PFNDOLVERTEXATTRIBI2IPROC dolVertexAttribI2i;
extern PFNDOLVERTEXATTRIBI2IVPROC dolVertexAttribI2iv;
extern PFNDOLVERTEXATTRIBI2UIPROC dolVertexAttribI2ui;
extern PFNDOLVERTEXATTRIBI2UIVPROC dolVertexAttribI2uiv;
extern PFNDOLVERTEXATTRIBI3IPROC dolVertexAttribI3i;
extern PFNDOLVERTEXATTRIBI3IVPROC dolVertexAttribI3iv;
extern PFNDOLVERTEXATTRIBI3UIPROC dolVertexAttribI3ui;
extern PFNDOLVERTEXATTRIBI3UIVPROC dolVertexAttribI3uiv;
extern PFNDOLVERTEXATTRIBI4BVPROC dolVertexAttribI4bv;
extern PFNDOLVERTEXATTRIBI4IPROC dolVertexAttribI4i;
extern PFNDOLVERTEXATTRIBI4IVPROC dolVertexAttribI4iv;
extern PFNDOLVERTEXATTRIBI4SVPROC dolVertexAttribI4sv;
extern PFNDOLVERTEXATTRIBI4UBVPROC dolVertexAttribI4ubv;
extern PFNDOLVERTEXATTRIBI4UIPROC dolVertexAttribI4ui;
extern PFNDOLVERTEXATTRIBI4UIVPROC dolVertexAttribI4uiv;
extern PFNDOLVERTEXATTRIBI4USVPROC dolVertexAttribI4usv;
extern PFNDOLVERTEXATTRIBIPOINTERPROC dolVertexAttribIPointer;

#define glBeginConditionalRender dolBeginConditionalRender
#define glBeginTransformFeedback dolBeginTransformFeedback
#define glBindFragDataLocation dolBindFragDataLocation
#define glClampColor dolClampColor
#define glClearBufferfi dolClearBufferfi
#define glClearBufferfv dolClearBufferfv
#define glClearBufferiv dolClearBufferiv
#define glClearBufferuiv dolClearBufferuiv
#define glColorMaski dolColorMaski
#define glDisablei dolDisablei
#define glEnablei dolEnablei
#define glEndConditionalRender dolEndConditionalRender
#define glEndTransformFeedback dolEndTransformFeedback
#define glGetBooleani_v dolGetBooleani_v
#define glGetFragDataLocation dolGetFragDataLocation
#define glGetStringi dolGetStringi
#define glGetTexParameterIiv dolGetTexParameterIiv
#define glGetTexParameterIuiv dolGetTexParameterIuiv
#define glGetTransformFeedbackVarying dolGetTransformFeedbackVarying
#define glGetUniformuiv dolGetUniformuiv
#define glGetVertexAttribIiv dolGetVertexAttribIiv
#define glGetVertexAttribIuiv dolGetVertexAttribIuiv
#define glIsEnabledi dolIsEnabledi
#define glTexParameterIiv dolTexParameterIiv
#define glTexParameterIuiv dolTexParameterIuiv
#define glTransformFeedbackVaryings dolTransformFeedbackVaryings
#define glUniform1ui dolUniform1ui
#define glUniform1uiv dolUniform1uiv
#define glUniform2ui dolUniform2ui
#define glUniform2uiv dolUniform2uiv
#define glUniform3ui dolUniform3ui
#define glUniform3uiv dolUniform3uiv
#define glUniform4ui dolUniform4ui
#define glUniform4uiv dolUniform4uiv
#define glVertexAttribI1i dolVertexAttribI1i
#define glVertexAttribI1iv dolVertexAttribI1iv
#define glVertexAttribI1ui dolVertexAttribI1ui
#define glVertexAttribI1uiv dolVertexAttribI1uiv
#define glVertexAttribI2i dolVertexAttribI2i
#define glVertexAttribI2iv dolVertexAttribI2iv
#define glVertexAttribI2ui dolVertexAttribI2ui
#define glVertexAttribI2uiv dolVertexAttribI2uiv
#define glVertexAttribI3i dolVertexAttribI3i
#define glVertexAttribI3iv dolVertexAttribI3iv
#define glVertexAttribI3ui dolVertexAttribI3ui
#define glVertexAttribI3uiv dolVertexAttribI3uiv
#define glVertexAttribI4bv dolVertexAttribI4bv
#define glVertexAttribI4i dolVertexAttribI4i
#define glVertexAttribI4iv dolVertexAttribI4iv
#define glVertexAttribI4sv dolVertexAttribI4sv
#define glVertexAttribI4ubv dolVertexAttribI4ubv
#define glVertexAttribI4ui dolVertexAttribI4ui
#define glVertexAttribI4uiv dolVertexAttribI4uiv
#define glVertexAttribI4usv dolVertexAttribI4usv
#define glVertexAttribIPointer dolVertexAttribIPointer
