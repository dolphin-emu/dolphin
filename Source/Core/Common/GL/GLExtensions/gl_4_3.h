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

#define GL_NUM_SHADING_LANGUAGE_VERSIONS 0x82E9
#define GL_VERTEX_ATTRIB_ARRAY_LONG 0x874E
#define GL_COMPRESSED_RGB8_ETC2 0x9274
#define GL_COMPRESSED_SRGB8_ETC2 0x9275
#define GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2 0x9276
#define GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2 0x9277
#define GL_COMPRESSED_RGBA8_ETC2_EAC 0x9278
#define GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC 0x9279
#define GL_COMPRESSED_R11_EAC 0x9270
#define GL_COMPRESSED_SIGNED_R11_EAC 0x9271
#define GL_COMPRESSED_RG11_EAC 0x9272
#define GL_COMPRESSED_SIGNED_RG11_EAC 0x9273
#define GL_PRIMITIVE_RESTART_FIXED_INDEX 0x8D69
#define GL_ANY_SAMPLES_PASSED_CONSERVATIVE 0x8D6A
#define GL_MAX_ELEMENT_INDEX 0x8D6B
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
#define GL_MAX_DEBUG_MESSAGE_LENGTH 0x9143
#define GL_MAX_DEBUG_LOGGED_MESSAGES 0x9144
#define GL_DEBUG_LOGGED_MESSAGES 0x9145
#define GL_DEBUG_SEVERITY_HIGH 0x9146
#define GL_DEBUG_SEVERITY_MEDIUM 0x9147
#define GL_DEBUG_SEVERITY_LOW 0x9148
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
#define GL_MAX_LABEL_LENGTH 0x82E8
#define GL_DEBUG_OUTPUT 0x92E0
#define GL_CONTEXT_FLAG_DEBUG_BIT 0x00000002
#define GL_MAX_UNIFORM_LOCATIONS 0x826E
#define GL_FRAMEBUFFER_DEFAULT_WIDTH 0x9310
#define GL_FRAMEBUFFER_DEFAULT_HEIGHT 0x9311
#define GL_FRAMEBUFFER_DEFAULT_LAYERS 0x9312
#define GL_FRAMEBUFFER_DEFAULT_SAMPLES 0x9313
#define GL_FRAMEBUFFER_DEFAULT_FIXED_SAMPLE_LOCATIONS 0x9314
#define GL_MAX_FRAMEBUFFER_WIDTH 0x9315
#define GL_MAX_FRAMEBUFFER_HEIGHT 0x9316
#define GL_MAX_FRAMEBUFFER_LAYERS 0x9317
#define GL_MAX_FRAMEBUFFER_SAMPLES 0x9318
#define GL_INTERNALFORMAT_SUPPORTED 0x826F
#define GL_INTERNALFORMAT_PREFERRED 0x8270
#define GL_INTERNALFORMAT_RED_SIZE 0x8271
#define GL_INTERNALFORMAT_GREEN_SIZE 0x8272
#define GL_INTERNALFORMAT_BLUE_SIZE 0x8273
#define GL_INTERNALFORMAT_ALPHA_SIZE 0x8274
#define GL_INTERNALFORMAT_DEPTH_SIZE 0x8275
#define GL_INTERNALFORMAT_STENCIL_SIZE 0x8276
#define GL_INTERNALFORMAT_SHARED_SIZE 0x8277
#define GL_INTERNALFORMAT_RED_TYPE 0x8278
#define GL_INTERNALFORMAT_GREEN_TYPE 0x8279
#define GL_INTERNALFORMAT_BLUE_TYPE 0x827A
#define GL_INTERNALFORMAT_ALPHA_TYPE 0x827B
#define GL_INTERNALFORMAT_DEPTH_TYPE 0x827C
#define GL_INTERNALFORMAT_STENCIL_TYPE 0x827D
#define GL_MAX_WIDTH 0x827E
#define GL_MAX_HEIGHT 0x827F
#define GL_MAX_DEPTH 0x8280
#define GL_MAX_LAYERS 0x8281
#define GL_MAX_COMBINED_DIMENSIONS 0x8282
#define GL_COLOR_COMPONENTS 0x8283
#define GL_DEPTH_COMPONENTS 0x8284
#define GL_STENCIL_COMPONENTS 0x8285
#define GL_COLOR_RENDERABLE 0x8286
#define GL_DEPTH_RENDERABLE 0x8287
#define GL_STENCIL_RENDERABLE 0x8288
#define GL_FRAMEBUFFER_RENDERABLE 0x8289
#define GL_FRAMEBUFFER_RENDERABLE_LAYERED 0x828A
#define GL_FRAMEBUFFER_BLEND 0x828B
#define GL_READ_PIXELS 0x828C
#define GL_READ_PIXELS_FORMAT 0x828D
#define GL_READ_PIXELS_TYPE 0x828E
#define GL_TEXTURE_IMAGE_FORMAT 0x828F
#define GL_TEXTURE_IMAGE_TYPE 0x8290
#define GL_GET_TEXTURE_IMAGE_FORMAT 0x8291
#define GL_GET_TEXTURE_IMAGE_TYPE 0x8292
#define GL_MIPMAP 0x8293
#define GL_MANUAL_GENERATE_MIPMAP 0x8294
#define GL_AUTO_GENERATE_MIPMAP 0x8295
#define GL_COLOR_ENCODING 0x8296
#define GL_SRGB_READ 0x8297
#define GL_SRGB_WRITE 0x8298
#define GL_FILTER 0x829A
#define GL_VERTEX_TEXTURE 0x829B
#define GL_TESS_CONTROL_TEXTURE 0x829C
#define GL_TESS_EVALUATION_TEXTURE 0x829D
#define GL_GEOMETRY_TEXTURE 0x829E
#define GL_FRAGMENT_TEXTURE 0x829F
#define GL_COMPUTE_TEXTURE 0x82A0
#define GL_TEXTURE_SHADOW 0x82A1
#define GL_TEXTURE_GATHER 0x82A2
#define GL_TEXTURE_GATHER_SHADOW 0x82A3
#define GL_SHADER_IMAGE_LOAD 0x82A4
#define GL_SHADER_IMAGE_STORE 0x82A5
#define GL_SHADER_IMAGE_ATOMIC 0x82A6
#define GL_IMAGE_TEXEL_SIZE 0x82A7
#define GL_IMAGE_COMPATIBILITY_CLASS 0x82A8
#define GL_IMAGE_PIXEL_FORMAT 0x82A9
#define GL_IMAGE_PIXEL_TYPE 0x82AA
#define GL_SIMULTANEOUS_TEXTURE_AND_DEPTH_TEST 0x82AC
#define GL_SIMULTANEOUS_TEXTURE_AND_STENCIL_TEST 0x82AD
#define GL_SIMULTANEOUS_TEXTURE_AND_DEPTH_WRITE 0x82AE
#define GL_SIMULTANEOUS_TEXTURE_AND_STENCIL_WRITE 0x82AF
#define GL_TEXTURE_COMPRESSED_BLOCK_WIDTH 0x82B1
#define GL_TEXTURE_COMPRESSED_BLOCK_HEIGHT 0x82B2
#define GL_TEXTURE_COMPRESSED_BLOCK_SIZE 0x82B3
#define GL_CLEAR_BUFFER 0x82B4
#define GL_TEXTURE_VIEW 0x82B5
#define GL_VIEW_COMPATIBILITY_CLASS 0x82B6
#define GL_FULL_SUPPORT 0x82B7
#define GL_CAVEAT_SUPPORT 0x82B8
#define GL_IMAGE_CLASS_4_X_32 0x82B9
#define GL_IMAGE_CLASS_2_X_32 0x82BA
#define GL_IMAGE_CLASS_1_X_32 0x82BB
#define GL_IMAGE_CLASS_4_X_16 0x82BC
#define GL_IMAGE_CLASS_2_X_16 0x82BD
#define GL_IMAGE_CLASS_1_X_16 0x82BE
#define GL_IMAGE_CLASS_4_X_8 0x82BF
#define GL_IMAGE_CLASS_2_X_8 0x82C0
#define GL_IMAGE_CLASS_1_X_8 0x82C1
#define GL_IMAGE_CLASS_11_11_10 0x82C2
#define GL_IMAGE_CLASS_10_10_10_2 0x82C3
#define GL_VIEW_CLASS_128_BITS 0x82C4
#define GL_VIEW_CLASS_96_BITS 0x82C5
#define GL_VIEW_CLASS_64_BITS 0x82C6
#define GL_VIEW_CLASS_48_BITS 0x82C7
#define GL_VIEW_CLASS_32_BITS 0x82C8
#define GL_VIEW_CLASS_24_BITS 0x82C9
#define GL_VIEW_CLASS_16_BITS 0x82CA
#define GL_VIEW_CLASS_8_BITS 0x82CB
#define GL_VIEW_CLASS_S3TC_DXT1_RGB 0x82CC
#define GL_VIEW_CLASS_S3TC_DXT1_RGBA 0x82CD
#define GL_VIEW_CLASS_S3TC_DXT3_RGBA 0x82CE
#define GL_VIEW_CLASS_S3TC_DXT5_RGBA 0x82CF
#define GL_VIEW_CLASS_RGTC1_RED 0x82D0
#define GL_VIEW_CLASS_RGTC2_RG 0x82D1
#define GL_VIEW_CLASS_BPTC_UNORM 0x82D2
#define GL_VIEW_CLASS_BPTC_FLOAT 0x82D3
#define GL_UNIFORM 0x92E1
#define GL_UNIFORM_BLOCK 0x92E2
#define GL_PROGRAM_INPUT 0x92E3
#define GL_PROGRAM_OUTPUT 0x92E4
#define GL_BUFFER_VARIABLE 0x92E5
#define GL_SHADER_STORAGE_BLOCK 0x92E6
#define GL_VERTEX_SUBROUTINE 0x92E8
#define GL_TESS_CONTROL_SUBROUTINE 0x92E9
#define GL_TESS_EVALUATION_SUBROUTINE 0x92EA
#define GL_GEOMETRY_SUBROUTINE 0x92EB
#define GL_FRAGMENT_SUBROUTINE 0x92EC
#define GL_COMPUTE_SUBROUTINE 0x92ED
#define GL_VERTEX_SUBROUTINE_UNIFORM 0x92EE
#define GL_TESS_CONTROL_SUBROUTINE_UNIFORM 0x92EF
#define GL_TESS_EVALUATION_SUBROUTINE_UNIFORM 0x92F0
#define GL_GEOMETRY_SUBROUTINE_UNIFORM 0x92F1
#define GL_FRAGMENT_SUBROUTINE_UNIFORM 0x92F2
#define GL_COMPUTE_SUBROUTINE_UNIFORM 0x92F3
#define GL_TRANSFORM_FEEDBACK_VARYING 0x92F4
#define GL_ACTIVE_RESOURCES 0x92F5
#define GL_MAX_NAME_LENGTH 0x92F6
#define GL_MAX_NUM_ACTIVE_VARIABLES 0x92F7
#define GL_MAX_NUM_COMPATIBLE_SUBROUTINES 0x92F8
#define GL_NAME_LENGTH 0x92F9
#define GL_TYPE 0x92FA
#define GL_ARRAY_SIZE 0x92FB
#define GL_OFFSET 0x92FC
#define GL_BLOCK_INDEX 0x92FD
#define GL_ARRAY_STRIDE 0x92FE
#define GL_MATRIX_STRIDE 0x92FF
#define GL_IS_ROW_MAJOR 0x9300
#define GL_ATOMIC_COUNTER_BUFFER_INDEX 0x9301
#define GL_BUFFER_BINDING 0x9302
#define GL_BUFFER_DATA_SIZE 0x9303
#define GL_NUM_ACTIVE_VARIABLES 0x9304
#define GL_ACTIVE_VARIABLES 0x9305
#define GL_REFERENCED_BY_VERTEX_SHADER 0x9306
#define GL_REFERENCED_BY_TESS_CONTROL_SHADER 0x9307
#define GL_REFERENCED_BY_TESS_EVALUATION_SHADER 0x9308
#define GL_REFERENCED_BY_GEOMETRY_SHADER 0x9309
#define GL_REFERENCED_BY_FRAGMENT_SHADER 0x930A
#define GL_REFERENCED_BY_COMPUTE_SHADER 0x930B
#define GL_TOP_LEVEL_ARRAY_SIZE 0x930C
#define GL_TOP_LEVEL_ARRAY_STRIDE 0x930D
#define GL_LOCATION 0x930E
#define GL_LOCATION_INDEX 0x930F
#define GL_IS_PER_PATCH 0x92E7
#define GL_SHADER_STORAGE_BUFFER 0x90D2
#define GL_SHADER_STORAGE_BUFFER_BINDING 0x90D3
#define GL_SHADER_STORAGE_BUFFER_START 0x90D4
#define GL_SHADER_STORAGE_BUFFER_SIZE 0x90D5
#define GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS 0x90D6
#define GL_MAX_GEOMETRY_SHADER_STORAGE_BLOCKS 0x90D7
#define GL_MAX_TESS_CONTROL_SHADER_STORAGE_BLOCKS 0x90D8
#define GL_MAX_TESS_EVALUATION_SHADER_STORAGE_BLOCKS 0x90D9
#define GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS 0x90DA
#define GL_MAX_COMPUTE_SHADER_STORAGE_BLOCKS 0x90DB
#define GL_MAX_COMBINED_SHADER_STORAGE_BLOCKS 0x90DC
#define GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS 0x90DD
#define GL_MAX_SHADER_STORAGE_BLOCK_SIZE 0x90DE
#define GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT 0x90DF
#define GL_MAX_COMBINED_SHADER_OUTPUT_RESOURCES 0x8F39
#define GL_DEPTH_STENCIL_TEXTURE_MODE 0x90EA
#define GL_TEXTURE_BUFFER_OFFSET 0x919D
#define GL_TEXTURE_BUFFER_SIZE 0x919E
#define GL_TEXTURE_BUFFER_OFFSET_ALIGNMENT 0x919F
#define GL_TEXTURE_VIEW_MIN_LEVEL 0x82DB
#define GL_TEXTURE_VIEW_NUM_LEVELS 0x82DC
#define GL_TEXTURE_VIEW_MIN_LAYER 0x82DD
#define GL_TEXTURE_VIEW_NUM_LAYERS 0x82DE
#define GL_TEXTURE_IMMUTABLE_LEVELS 0x82DF
#define GL_VERTEX_ATTRIB_BINDING 0x82D4
#define GL_VERTEX_ATTRIB_RELATIVE_OFFSET 0x82D5
#define GL_VERTEX_BINDING_DIVISOR 0x82D6
#define GL_VERTEX_BINDING_OFFSET 0x82D7
#define GL_VERTEX_BINDING_STRIDE 0x82D8
#define GL_MAX_VERTEX_ATTRIB_RELATIVE_OFFSET 0x82D9
#define GL_MAX_VERTEX_ATTRIB_BINDINGS 0x82DA
#define GL_VERTEX_BINDING_BUFFER 0x8F4F
#define GL_DISPLAY_LIST 0x82E7

typedef void(APIENTRYP PFNDOLCLEARBUFFERDATAPROC)(GLenum target, GLenum internalformat,
                                                  GLenum format, GLenum type, const void* data);
typedef void(APIENTRYP PFNDOLCLEARBUFFERSUBDATAPROC)(GLenum target, GLenum internalformat,
                                                     GLintptr offset, GLsizeiptr size,
                                                     GLenum format, GLenum type, const void* data);
typedef void(APIENTRYP PFNDOLFRAMEBUFFERPARAMETERIPROC)(GLenum target, GLenum pname, GLint param);
typedef void(APIENTRYP PFNDOLGETFRAMEBUFFERPARAMETERIVPROC)(GLenum target, GLenum pname,
                                                            GLint* params);
typedef void(APIENTRYP PFNDOLGETINTERNALFORMATI64VPROC)(GLenum target, GLenum internalformat,
                                                        GLenum pname, GLsizei bufSize,
                                                        GLint64* params);
typedef void(APIENTRYP PFNDOLINVALIDATETEXSUBIMAGEPROC)(GLuint texture, GLint level, GLint xoffset,
                                                        GLint yoffset, GLint zoffset, GLsizei width,
                                                        GLsizei height, GLsizei depth);
typedef void(APIENTRYP PFNDOLINVALIDATETEXIMAGEPROC)(GLuint texture, GLint level);
typedef void(APIENTRYP PFNDOLINVALIDATEBUFFERSUBDATAPROC)(GLuint buffer, GLintptr offset,
                                                          GLsizeiptr length);
typedef void(APIENTRYP PFNDOLINVALIDATEBUFFERDATAPROC)(GLuint buffer);
typedef void(APIENTRYP PFNDOLINVALIDATEFRAMEBUFFERPROC)(GLenum target, GLsizei numAttachments,
                                                        const GLenum* attachments);
typedef void(APIENTRYP PFNDOLINVALIDATESUBFRAMEBUFFERPROC)(GLenum target, GLsizei numAttachments,
                                                           const GLenum* attachments, GLint x,
                                                           GLint y, GLsizei width, GLsizei height);
typedef void(APIENTRYP PFNDOLMULTIDRAWARRAYSINDIRECTPROC)(GLenum mode, const void* indirect,
                                                          GLsizei drawcount, GLsizei stride);
typedef void(APIENTRYP PFNDOLMULTIDRAWELEMENTSINDIRECTPROC)(GLenum mode, GLenum type,
                                                            const void* indirect, GLsizei drawcount,
                                                            GLsizei stride);
typedef void(APIENTRYP PFNDOLGETPROGRAMINTERFACEIVPROC)(GLuint program, GLenum programInterface,
                                                        GLenum pname, GLint* params);
typedef GLuint(APIENTRYP PFNDOLGETPROGRAMRESOURCEINDEXPROC)(GLuint program, GLenum programInterface,
                                                            const GLchar* name);
typedef void(APIENTRYP PFNDOLGETPROGRAMRESOURCENAMEPROC)(GLuint program, GLenum programInterface,
                                                         GLuint index, GLsizei bufSize,
                                                         GLsizei* length, GLchar* name);
typedef void(APIENTRYP PFNDOLGETPROGRAMRESOURCEIVPROC)(GLuint program, GLenum programInterface,
                                                       GLuint index, GLsizei propCount,
                                                       const GLenum* props, GLsizei bufSize,
                                                       GLsizei* length, GLint* params);
typedef GLint(APIENTRYP PFNDOLGETPROGRAMRESOURCELOCATIONPROC)(GLuint program,
                                                              GLenum programInterface,
                                                              const GLchar* name);
typedef GLint(APIENTRYP PFNDOLGETPROGRAMRESOURCELOCATIONINDEXPROC)(GLuint program,
                                                                   GLenum programInterface,
                                                                   const GLchar* name);
typedef void(APIENTRYP PFNDOLTEXBUFFERRANGEPROC)(GLenum target, GLenum internalformat,
                                                 GLuint buffer, GLintptr offset, GLsizeiptr size);
typedef void(APIENTRYP PFNDOLTEXTUREVIEWPROC)(GLuint texture, GLenum target, GLuint origtexture,
                                              GLenum internalformat, GLuint minlevel,
                                              GLuint numlevels, GLuint minlayer, GLuint numlayers);
typedef void(APIENTRYP PFNDOLBINDVERTEXBUFFERPROC)(GLuint bindingindex, GLuint buffer,
                                                   GLintptr offset, GLsizei stride);
typedef void(APIENTRYP PFNDOLVERTEXATTRIBFORMATPROC)(GLuint attribindex, GLint size, GLenum type,
                                                     GLboolean normalized, GLuint relativeoffset);
typedef void(APIENTRYP PFNDOLVERTEXATTRIBIFORMATPROC)(GLuint attribindex, GLint size, GLenum type,
                                                      GLuint relativeoffset);
typedef void(APIENTRYP PFNDOLVERTEXATTRIBLFORMATPROC)(GLuint attribindex, GLint size, GLenum type,
                                                      GLuint relativeoffset);
typedef void(APIENTRYP PFNDOLVERTEXATTRIBBINDINGPROC)(GLuint attribindex, GLuint bindingindex);
typedef void(APIENTRYP PFNDOLVERTEXBINDINGDIVISORPROC)(GLuint bindingindex, GLuint divisor);

extern PFNDOLCLEARBUFFERDATAPROC dolClearBufferData;
extern PFNDOLCLEARBUFFERSUBDATAPROC dolClearBufferSubData;
extern PFNDOLFRAMEBUFFERPARAMETERIPROC dolFramebufferParameteri;
extern PFNDOLGETFRAMEBUFFERPARAMETERIVPROC dolGetFramebufferParameteriv;
extern PFNDOLGETINTERNALFORMATI64VPROC dolGetInternalformati64v;
extern PFNDOLINVALIDATETEXSUBIMAGEPROC dolInvalidateTexSubImage;
extern PFNDOLINVALIDATETEXIMAGEPROC dolInvalidateTexImage;
extern PFNDOLINVALIDATEBUFFERSUBDATAPROC dolInvalidateBufferSubData;
extern PFNDOLINVALIDATEBUFFERDATAPROC dolInvalidateBufferData;
extern PFNDOLINVALIDATEFRAMEBUFFERPROC dolInvalidateFramebuffer;
extern PFNDOLINVALIDATESUBFRAMEBUFFERPROC dolInvalidateSubFramebuffer;
extern PFNDOLMULTIDRAWARRAYSINDIRECTPROC dolMultiDrawArraysIndirect;
extern PFNDOLMULTIDRAWELEMENTSINDIRECTPROC dolMultiDrawElementsIndirect;
extern PFNDOLGETPROGRAMINTERFACEIVPROC dolGetProgramInterfaceiv;
extern PFNDOLGETPROGRAMRESOURCEINDEXPROC dolGetProgramResourceIndex;
extern PFNDOLGETPROGRAMRESOURCENAMEPROC dolGetProgramResourceName;
extern PFNDOLGETPROGRAMRESOURCEIVPROC dolGetProgramResourceiv;
extern PFNDOLGETPROGRAMRESOURCELOCATIONPROC dolGetProgramResourceLocation;
extern PFNDOLGETPROGRAMRESOURCELOCATIONINDEXPROC dolGetProgramResourceLocationIndex;
extern PFNDOLTEXBUFFERRANGEPROC dolTexBufferRange;
extern PFNDOLTEXTUREVIEWPROC dolTextureView;
extern PFNDOLBINDVERTEXBUFFERPROC dolBindVertexBuffer;
extern PFNDOLVERTEXATTRIBFORMATPROC dolVertexAttribFormat;
extern PFNDOLVERTEXATTRIBIFORMATPROC dolVertexAttribIFormat;
extern PFNDOLVERTEXATTRIBLFORMATPROC dolVertexAttribLFormat;
extern PFNDOLVERTEXATTRIBBINDINGPROC dolVertexAttribBinding;
extern PFNDOLVERTEXBINDINGDIVISORPROC dolVertexBindingDivisor;

#define glClearBufferData dolClearBufferData
#define glClearBufferSubData dolClearBufferSubData
#define glFramebufferParameteri dolFramebufferParameteri
#define glGetFramebufferParameteriv dolGetFramebufferParameteriv
#define glGetInternalformati64v dolGetInternalformati64v
#define glInvalidateTexSubImage dolInvalidateTexSubImage
#define glInvalidateTexImage dolInvalidateTexImage
#define glInvalidateBufferSubData dolInvalidateBufferSubData
#define glInvalidateBufferData dolInvalidateBufferData
#define glInvalidateFramebuffer dolInvalidateFramebuffer
#define glInvalidateSubFramebuffer dolInvalidateSubFramebuffer
#define glMultiDrawArraysIndirect dolMultiDrawArraysIndirect
#define glMultiDrawElementsIndirect dolMultiDrawElementsIndirect
#define glGetProgramInterfaceiv dolGetProgramInterfaceiv
#define glGetProgramResourceIndex dolGetProgramResourceIndex
#define glGetProgramResourceName dolGetProgramResourceName
#define glGetProgramResourceiv dolGetProgramResourceiv
#define glGetProgramResourceLocation dolGetProgramResourceLocation
#define glGetProgramResourceLocationIndex dolGetProgramResourceLocationIndex
#define glTexBufferRange dolTexBufferRange
#define glTextureView dolTextureView
#define glBindVertexBuffer dolBindVertexBuffer
#define glVertexAttribFormat dolVertexAttribFormat
#define glVertexAttribIFormat dolVertexAttribIFormat
#define glVertexAttribLFormat dolVertexAttribLFormat
#define glVertexAttribBinding dolVertexAttribBinding
#define glVertexBindingDivisor dolVertexBindingDivisor
