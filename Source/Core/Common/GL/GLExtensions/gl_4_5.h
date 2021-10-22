/*
** Copyright (c) 2013-2015 The Khronos Group Inc.
** SPDX-License-Identifier: MIT
*/

#include "Common/GL/GLExtensions/gl_common.h"

#define GL_CONTEXT_LOST 0x0507
#define GL_NEGATIVE_ONE_TO_ONE 0x935E
#define GL_ZERO_TO_ONE 0x935F
#define GL_CLIP_ORIGIN 0x935C
#define GL_CLIP_DEPTH_MODE 0x935D
#define GL_QUERY_WAIT_INVERTED 0x8E17
#define GL_QUERY_NO_WAIT_INVERTED 0x8E18
#define GL_QUERY_BY_REGION_WAIT_INVERTED 0x8E19
#define GL_QUERY_BY_REGION_NO_WAIT_INVERTED 0x8E1A
#define GL_MAX_CULL_DISTANCES 0x82F9
#define GL_MAX_COMBINED_CLIP_AND_CULL_DISTANCES 0x82FA
#define GL_TEXTURE_TARGET 0x1006
#define GL_QUERY_TARGET 0x82EA
#define GL_GUILTY_CONTEXT_RESET 0x8253
#define GL_INNOCENT_CONTEXT_RESET 0x8254
#define GL_UNKNOWN_CONTEXT_RESET 0x8255
#define GL_RESET_NOTIFICATION_STRATEGY 0x8256
#define GL_LOSE_CONTEXT_ON_RESET 0x8252
#define GL_NO_RESET_NOTIFICATION 0x8261
#define GL_CONTEXT_FLAG_ROBUST_ACCESS_BIT 0x00000004
#define GL_CONTEXT_RELEASE_BEHAVIOR 0x82FB
#define GL_CONTEXT_RELEASE_BEHAVIOR_FLUSH 0x82FC

typedef void(APIENTRYP PFNDOLCLIPCONTROLPROC)(GLenum origin, GLenum depth);
typedef void(APIENTRYP PFNDOLCREATETRANSFORMFEEDBACKSPROC)(GLsizei n, GLuint* ids);
typedef void(APIENTRYP PFNDOLTRANSFORMFEEDBACKBUFFERBASEPROC)(GLuint xfb, GLuint index,
                                                              GLuint buffer);
typedef void(APIENTRYP PFNDOLTRANSFORMFEEDBACKBUFFERRANGEPROC)(GLuint xfb, GLuint index,
                                                               GLuint buffer, GLintptr offset,
                                                               GLsizeiptr size);
typedef void(APIENTRYP PFNDOLGETTRANSFORMFEEDBACKIVPROC)(GLuint xfb, GLenum pname, GLint* param);
typedef void(APIENTRYP PFNDOLGETTRANSFORMFEEDBACKI_VPROC)(GLuint xfb, GLenum pname, GLuint index,
                                                          GLint* param);
typedef void(APIENTRYP PFNDOLGETTRANSFORMFEEDBACKI64_VPROC)(GLuint xfb, GLenum pname, GLuint index,
                                                            GLint64* param);
typedef void(APIENTRYP PFNDOLCREATEBUFFERSPROC)(GLsizei n, GLuint* buffers);
typedef void(APIENTRYP PFNDOLNAMEDBUFFERSTORAGEPROC)(GLuint buffer, GLsizeiptr size,
                                                     const void* data, GLbitfield flags);
typedef void(APIENTRYP PFNDOLNAMEDBUFFERDATAPROC)(GLuint buffer, GLsizeiptr size, const void* data,
                                                  GLenum usage);
typedef void(APIENTRYP PFNDOLNAMEDBUFFERSUBDATAPROC)(GLuint buffer, GLintptr offset,
                                                     GLsizeiptr size, const void* data);
typedef void(APIENTRYP PFNDOLCOPYNAMEDBUFFERSUBDATAPROC)(GLuint readBuffer, GLuint writeBuffer,
                                                         GLintptr readOffset, GLintptr writeOffset,
                                                         GLsizeiptr size);
typedef void(APIENTRYP PFNDOLCLEARNAMEDBUFFERDATAPROC)(GLuint buffer, GLenum internalformat,
                                                       GLenum format, GLenum type,
                                                       const void* data);
typedef void(APIENTRYP PFNDOLCLEARNAMEDBUFFERSUBDATAPROC)(GLuint buffer, GLenum internalformat,
                                                          GLintptr offset, GLsizeiptr size,
                                                          GLenum format, GLenum type,
                                                          const void* data);
typedef void*(APIENTRYP PFNDOLMAPNAMEDBUFFERPROC)(GLuint buffer, GLenum access);
typedef void*(APIENTRYP PFNDOLMAPNAMEDBUFFERRANGEPROC)(GLuint buffer, GLintptr offset,
                                                       GLsizeiptr length, GLbitfield access);
typedef GLboolean(APIENTRYP PFNDOLUNMAPNAMEDBUFFERPROC)(GLuint buffer);
typedef void(APIENTRYP PFNDOLFLUSHMAPPEDNAMEDBUFFERRANGEPROC)(GLuint buffer, GLintptr offset,
                                                              GLsizeiptr length);
typedef void(APIENTRYP PFNDOLGETNAMEDBUFFERPARAMETERIVPROC)(GLuint buffer, GLenum pname,
                                                            GLint* params);
typedef void(APIENTRYP PFNDOLGETNAMEDBUFFERPARAMETERI64VPROC)(GLuint buffer, GLenum pname,
                                                              GLint64* params);
typedef void(APIENTRYP PFNDOLGETNAMEDBUFFERPOINTERVPROC)(GLuint buffer, GLenum pname,
                                                         void** params);
typedef void(APIENTRYP PFNDOLGETNAMEDBUFFERSUBDATAPROC)(GLuint buffer, GLintptr offset,
                                                        GLsizeiptr size, void* data);
typedef void(APIENTRYP PFNDOLCREATEFRAMEBUFFERSPROC)(GLsizei n, GLuint* framebuffers);
typedef void(APIENTRYP PFNDOLNAMEDFRAMEBUFFERRENDERBUFFERPROC)(GLuint framebuffer,
                                                               GLenum attachment,
                                                               GLenum renderbuffertarget,
                                                               GLuint renderbuffer);
typedef void(APIENTRYP PFNDOLNAMEDFRAMEBUFFERPARAMETERIPROC)(GLuint framebuffer, GLenum pname,
                                                             GLint param);
typedef void(APIENTRYP PFNDOLNAMEDFRAMEBUFFERTEXTUREPROC)(GLuint framebuffer, GLenum attachment,
                                                          GLuint texture, GLint level);
typedef void(APIENTRYP PFNDOLNAMEDFRAMEBUFFERTEXTURELAYERPROC)(GLuint framebuffer,
                                                               GLenum attachment, GLuint texture,
                                                               GLint level, GLint layer);
typedef void(APIENTRYP PFNDOLNAMEDFRAMEBUFFERDRAWBUFFERPROC)(GLuint framebuffer, GLenum buf);
typedef void(APIENTRYP PFNDOLNAMEDFRAMEBUFFERDRAWBUFFERSPROC)(GLuint framebuffer, GLsizei n,
                                                              const GLenum* bufs);
typedef void(APIENTRYP PFNDOLNAMEDFRAMEBUFFERREADBUFFERPROC)(GLuint framebuffer, GLenum src);
typedef void(APIENTRYP PFNDOLINVALIDATENAMEDFRAMEBUFFERDATAPROC)(GLuint framebuffer,
                                                                 GLsizei numAttachments,
                                                                 const GLenum* attachments);
typedef void(APIENTRYP PFNDOLINVALIDATENAMEDFRAMEBUFFERSUBDATAPROC)(GLuint framebuffer,
                                                                    GLsizei numAttachments,
                                                                    const GLenum* attachments,
                                                                    GLint x, GLint y, GLsizei width,
                                                                    GLsizei height);
typedef void(APIENTRYP PFNDOLCLEARNAMEDFRAMEBUFFERIVPROC)(GLuint framebuffer, GLenum buffer,
                                                          GLint drawbuffer, const GLint* value);
typedef void(APIENTRYP PFNDOLCLEARNAMEDFRAMEBUFFERUIVPROC)(GLuint framebuffer, GLenum buffer,
                                                           GLint drawbuffer, const GLuint* value);
typedef void(APIENTRYP PFNDOLCLEARNAMEDFRAMEBUFFERFVPROC)(GLuint framebuffer, GLenum buffer,
                                                          GLint drawbuffer, const GLfloat* value);
typedef void(APIENTRYP PFNDOLCLEARNAMEDFRAMEBUFFERFIPROC)(GLuint framebuffer, GLenum buffer,
                                                          const GLfloat depth, GLint stencil);
typedef void(APIENTRYP PFNDOLBLITNAMEDFRAMEBUFFERPROC)(GLuint readFramebuffer,
                                                       GLuint drawFramebuffer, GLint srcX0,
                                                       GLint srcY0, GLint srcX1, GLint srcY1,
                                                       GLint dstX0, GLint dstY0, GLint dstX1,
                                                       GLint dstY1, GLbitfield mask, GLenum filter);
typedef GLenum(APIENTRYP PFNDOLCHECKNAMEDFRAMEBUFFERSTATUSPROC)(GLuint framebuffer, GLenum target);
typedef void(APIENTRYP PFNDOLGETNAMEDFRAMEBUFFERPARAMETERIVPROC)(GLuint framebuffer, GLenum pname,
                                                                 GLint* param);
typedef void(APIENTRYP PFNDOLGETNAMEDFRAMEBUFFERATTACHMENTPARAMETERIVPROC)(GLuint framebuffer,
                                                                           GLenum attachment,
                                                                           GLenum pname,
                                                                           GLint* params);
typedef void(APIENTRYP PFNDOLCREATERENDERBUFFERSPROC)(GLsizei n, GLuint* renderbuffers);
typedef void(APIENTRYP PFNDOLNAMEDRENDERBUFFERSTORAGEPROC)(GLuint renderbuffer,
                                                           GLenum internalformat, GLsizei width,
                                                           GLsizei height);
typedef void(APIENTRYP PFNDOLNAMEDRENDERBUFFERSTORAGEMULTISAMPLEPROC)(
    GLuint renderbuffer, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height);
typedef void(APIENTRYP PFNDOLGETNAMEDRENDERBUFFERPARAMETERIVPROC)(GLuint renderbuffer, GLenum pname,
                                                                  GLint* params);
typedef void(APIENTRYP PFNDOLCREATETEXTURESPROC)(GLenum target, GLsizei n, GLuint* textures);
typedef void(APIENTRYP PFNDOLTEXTUREBUFFERPROC)(GLuint texture, GLenum internalformat,
                                                GLuint buffer);
typedef void(APIENTRYP PFNDOLTEXTUREBUFFERRANGEPROC)(GLuint texture, GLenum internalformat,
                                                     GLuint buffer, GLintptr offset,
                                                     GLsizeiptr size);
typedef void(APIENTRYP PFNDOLTEXTURESTORAGE1DPROC)(GLuint texture, GLsizei levels,
                                                   GLenum internalformat, GLsizei width);
typedef void(APIENTRYP PFNDOLTEXTURESTORAGE2DPROC)(GLuint texture, GLsizei levels,
                                                   GLenum internalformat, GLsizei width,
                                                   GLsizei height);
typedef void(APIENTRYP PFNDOLTEXTURESTORAGE3DPROC)(GLuint texture, GLsizei levels,
                                                   GLenum internalformat, GLsizei width,
                                                   GLsizei height, GLsizei depth);
typedef void(APIENTRYP PFNDOLTEXTURESTORAGE2DMULTISAMPLEPROC)(GLuint texture, GLsizei samples,
                                                              GLenum internalformat, GLsizei width,
                                                              GLsizei height,
                                                              GLboolean fixedsamplelocations);
typedef void(APIENTRYP PFNDOLTEXTURESTORAGE3DMULTISAMPLEPROC)(GLuint texture, GLsizei samples,
                                                              GLenum internalformat, GLsizei width,
                                                              GLsizei height, GLsizei depth,
                                                              GLboolean fixedsamplelocations);
typedef void(APIENTRYP PFNDOLTEXTURESUBIMAGE1DPROC)(GLuint texture, GLint level, GLint xoffset,
                                                    GLsizei width, GLenum format, GLenum type,
                                                    const void* pixels);
typedef void(APIENTRYP PFNDOLTEXTURESUBIMAGE2DPROC)(GLuint texture, GLint level, GLint xoffset,
                                                    GLint yoffset, GLsizei width, GLsizei height,
                                                    GLenum format, GLenum type, const void* pixels);
typedef void(APIENTRYP PFNDOLTEXTURESUBIMAGE3DPROC)(GLuint texture, GLint level, GLint xoffset,
                                                    GLint yoffset, GLint zoffset, GLsizei width,
                                                    GLsizei height, GLsizei depth, GLenum format,
                                                    GLenum type, const void* pixels);
typedef void(APIENTRYP PFNDOLCOMPRESSEDTEXTURESUBIMAGE1DPROC)(GLuint texture, GLint level,
                                                              GLint xoffset, GLsizei width,
                                                              GLenum format, GLsizei imageSize,
                                                              const void* data);
typedef void(APIENTRYP PFNDOLCOMPRESSEDTEXTURESUBIMAGE2DPROC)(GLuint texture, GLint level,
                                                              GLint xoffset, GLint yoffset,
                                                              GLsizei width, GLsizei height,
                                                              GLenum format, GLsizei imageSize,
                                                              const void* data);
typedef void(APIENTRYP PFNDOLCOMPRESSEDTEXTURESUBIMAGE3DPROC)(
    GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width,
    GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void* data);
typedef void(APIENTRYP PFNDOLCOPYTEXTURESUBIMAGE1DPROC)(GLuint texture, GLint level, GLint xoffset,
                                                        GLint x, GLint y, GLsizei width);
typedef void(APIENTRYP PFNDOLCOPYTEXTURESUBIMAGE2DPROC)(GLuint texture, GLint level, GLint xoffset,
                                                        GLint yoffset, GLint x, GLint y,
                                                        GLsizei width, GLsizei height);
typedef void(APIENTRYP PFNDOLCOPYTEXTURESUBIMAGE3DPROC)(GLuint texture, GLint level, GLint xoffset,
                                                        GLint yoffset, GLint zoffset, GLint x,
                                                        GLint y, GLsizei width, GLsizei height);
typedef void(APIENTRYP PFNDOLTEXTUREPARAMETERFPROC)(GLuint texture, GLenum pname, GLfloat param);
typedef void(APIENTRYP PFNDOLTEXTUREPARAMETERFVPROC)(GLuint texture, GLenum pname,
                                                     const GLfloat* param);
typedef void(APIENTRYP PFNDOLTEXTUREPARAMETERIPROC)(GLuint texture, GLenum pname, GLint param);
typedef void(APIENTRYP PFNDOLTEXTUREPARAMETERIIVPROC)(GLuint texture, GLenum pname,
                                                      const GLint* params);
typedef void(APIENTRYP PFNDOLTEXTUREPARAMETERIUIVPROC)(GLuint texture, GLenum pname,
                                                       const GLuint* params);
typedef void(APIENTRYP PFNDOLTEXTUREPARAMETERIVPROC)(GLuint texture, GLenum pname,
                                                     const GLint* param);
typedef void(APIENTRYP PFNDOLGENERATETEXTUREMIPMAPPROC)(GLuint texture);
typedef void(APIENTRYP PFNDOLBINDTEXTUREUNITPROC)(GLuint unit, GLuint texture);
typedef void(APIENTRYP PFNDOLGETTEXTUREIMAGEPROC)(GLuint texture, GLint level, GLenum format,
                                                  GLenum type, GLsizei bufSize, void* pixels);
typedef void(APIENTRYP PFNDOLGETCOMPRESSEDTEXTUREIMAGEPROC)(GLuint texture, GLint level,
                                                            GLsizei bufSize, void* pixels);
typedef void(APIENTRYP PFNDOLGETTEXTURELEVELPARAMETERFVPROC)(GLuint texture, GLint level,
                                                             GLenum pname, GLfloat* params);
typedef void(APIENTRYP PFNDOLGETTEXTURELEVELPARAMETERIVPROC)(GLuint texture, GLint level,
                                                             GLenum pname, GLint* params);
typedef void(APIENTRYP PFNDOLGETTEXTUREPARAMETERFVPROC)(GLuint texture, GLenum pname,
                                                        GLfloat* params);
typedef void(APIENTRYP PFNDOLGETTEXTUREPARAMETERIIVPROC)(GLuint texture, GLenum pname,
                                                         GLint* params);
typedef void(APIENTRYP PFNDOLGETTEXTUREPARAMETERIUIVPROC)(GLuint texture, GLenum pname,
                                                          GLuint* params);
typedef void(APIENTRYP PFNDOLGETTEXTUREPARAMETERIVPROC)(GLuint texture, GLenum pname,
                                                        GLint* params);
typedef void(APIENTRYP PFNDOLCREATEVERTEXARRAYSPROC)(GLsizei n, GLuint* arrays);
typedef void(APIENTRYP PFNDOLDISABLEVERTEXARRAYATTRIBPROC)(GLuint vaobj, GLuint index);
typedef void(APIENTRYP PFNDOLENABLEVERTEXARRAYATTRIBPROC)(GLuint vaobj, GLuint index);
typedef void(APIENTRYP PFNDOLVERTEXARRAYELEMENTBUFFERPROC)(GLuint vaobj, GLuint buffer);
typedef void(APIENTRYP PFNDOLVERTEXARRAYVERTEXBUFFERPROC)(GLuint vaobj, GLuint bindingindex,
                                                          GLuint buffer, GLintptr offset,
                                                          GLsizei stride);
typedef void(APIENTRYP PFNDOLVERTEXARRAYVERTEXBUFFERSPROC)(GLuint vaobj, GLuint first,
                                                           GLsizei count, const GLuint* buffers,
                                                           const GLintptr* offsets,
                                                           const GLsizei* strides);
typedef void(APIENTRYP PFNDOLVERTEXARRAYATTRIBBINDINGPROC)(GLuint vaobj, GLuint attribindex,
                                                           GLuint bindingindex);
typedef void(APIENTRYP PFNDOLVERTEXARRAYATTRIBFORMATPROC)(GLuint vaobj, GLuint attribindex,
                                                          GLint size, GLenum type,
                                                          GLboolean normalized,
                                                          GLuint relativeoffset);
typedef void(APIENTRYP PFNDOLVERTEXARRAYATTRIBIFORMATPROC)(GLuint vaobj, GLuint attribindex,
                                                           GLint size, GLenum type,
                                                           GLuint relativeoffset);
typedef void(APIENTRYP PFNDOLVERTEXARRAYATTRIBLFORMATPROC)(GLuint vaobj, GLuint attribindex,
                                                           GLint size, GLenum type,
                                                           GLuint relativeoffset);
typedef void(APIENTRYP PFNDOLVERTEXARRAYBINDINGDIVISORPROC)(GLuint vaobj, GLuint bindingindex,
                                                            GLuint divisor);
typedef void(APIENTRYP PFNDOLGETVERTEXARRAYIVPROC)(GLuint vaobj, GLenum pname, GLint* param);
typedef void(APIENTRYP PFNDOLGETVERTEXARRAYINDEXEDIVPROC)(GLuint vaobj, GLuint index, GLenum pname,
                                                          GLint* param);
typedef void(APIENTRYP PFNDOLGETVERTEXARRAYINDEXED64IVPROC)(GLuint vaobj, GLuint index,
                                                            GLenum pname, GLint64* param);
typedef void(APIENTRYP PFNDOLCREATESAMPLERSPROC)(GLsizei n, GLuint* samplers);
typedef void(APIENTRYP PFNDOLCREATEPROGRAMPIPELINESPROC)(GLsizei n, GLuint* pipelines);
typedef void(APIENTRYP PFNDOLCREATEQUERIESPROC)(GLenum target, GLsizei n, GLuint* ids);
typedef void(APIENTRYP PFNDOLGETQUERYBUFFEROBJECTI64VPROC)(GLuint id, GLuint buffer, GLenum pname,
                                                           GLintptr offset);
typedef void(APIENTRYP PFNDOLGETQUERYBUFFEROBJECTIVPROC)(GLuint id, GLuint buffer, GLenum pname,
                                                         GLintptr offset);
typedef void(APIENTRYP PFNDOLGETQUERYBUFFEROBJECTUI64VPROC)(GLuint id, GLuint buffer, GLenum pname,
                                                            GLintptr offset);
typedef void(APIENTRYP PFNDOLGETQUERYBUFFEROBJECTUIVPROC)(GLuint id, GLuint buffer, GLenum pname,
                                                          GLintptr offset);
typedef void(APIENTRYP PFNDOLMEMORYBARRIERBYREGIONPROC)(GLbitfield barriers);
typedef void(APIENTRYP PFNDOLGETTEXTURESUBIMAGEPROC)(GLuint texture, GLint level, GLint xoffset,
                                                     GLint yoffset, GLint zoffset, GLsizei width,
                                                     GLsizei height, GLsizei depth, GLenum format,
                                                     GLenum type, GLsizei bufSize, void* pixels);
typedef void(APIENTRYP PFNDOLGETCOMPRESSEDTEXTURESUBIMAGEPROC)(GLuint texture, GLint level,
                                                               GLint xoffset, GLint yoffset,
                                                               GLint zoffset, GLsizei width,
                                                               GLsizei height, GLsizei depth,
                                                               GLsizei bufSize, void* pixels);
typedef GLenum(APIENTRYP PFNDOLGETGRAPHICSRESETSTATUSPROC)(void);
typedef void(APIENTRYP PFNDOLGETNCOMPRESSEDTEXIMAGEPROC)(GLenum target, GLint lod, GLsizei bufSize,
                                                         void* pixels);
typedef void(APIENTRYP PFNDOLGETNTEXIMAGEPROC)(GLenum target, GLint level, GLenum format,
                                               GLenum type, GLsizei bufSize, void* pixels);
typedef void(APIENTRYP PFNDOLGETNUNIFORMDVPROC)(GLuint program, GLint location, GLsizei bufSize,
                                                GLdouble* params);
typedef void(APIENTRYP PFNDOLGETNUNIFORMFVPROC)(GLuint program, GLint location, GLsizei bufSize,
                                                GLfloat* params);
typedef void(APIENTRYP PFNDOLGETNUNIFORMIVPROC)(GLuint program, GLint location, GLsizei bufSize,
                                                GLint* params);
typedef void(APIENTRYP PFNDOLGETNUNIFORMUIVPROC)(GLuint program, GLint location, GLsizei bufSize,
                                                 GLuint* params);
typedef void(APIENTRYP PFNDOLREADNPIXELSPROC)(GLint x, GLint y, GLsizei width, GLsizei height,
                                              GLenum format, GLenum type, GLsizei bufSize,
                                              void* data);
typedef void(APIENTRYP PFNDOLGETNMAPDVPROC)(GLenum target, GLenum query, GLsizei bufSize,
                                            GLdouble* v);
typedef void(APIENTRYP PFNDOLGETNMAPFVPROC)(GLenum target, GLenum query, GLsizei bufSize,
                                            GLfloat* v);
typedef void(APIENTRYP PFNDOLGETNMAPIVPROC)(GLenum target, GLenum query, GLsizei bufSize, GLint* v);
typedef void(APIENTRYP PFNDOLGETNPIXELMAPFVPROC)(GLenum map, GLsizei bufSize, GLfloat* values);
typedef void(APIENTRYP PFNDOLGETNPIXELMAPUIVPROC)(GLenum map, GLsizei bufSize, GLuint* values);
typedef void(APIENTRYP PFNDOLGETNPIXELMAPUSVPROC)(GLenum map, GLsizei bufSize, GLushort* values);
typedef void(APIENTRYP PFNDOLGETNPOLYGONSTIPPLEPROC)(GLsizei bufSize, GLubyte* pattern);
typedef void(APIENTRYP PFNDOLGETNCOLORTABLEPROC)(GLenum target, GLenum format, GLenum type,
                                                 GLsizei bufSize, void* table);
typedef void(APIENTRYP PFNDOLGETNCONVOLUTIONFILTERPROC)(GLenum target, GLenum format, GLenum type,
                                                        GLsizei bufSize, void* image);
typedef void(APIENTRYP PFNDOLGETNSEPARABLEFILTERPROC)(GLenum target, GLenum format, GLenum type,
                                                      GLsizei rowBufSize, void* row,
                                                      GLsizei columnBufSize, void* column,
                                                      void* span);
typedef void(APIENTRYP PFNDOLGETNHISTOGRAMPROC)(GLenum target, GLboolean reset, GLenum format,
                                                GLenum type, GLsizei bufSize, void* values);
typedef void(APIENTRYP PFNDOLGETNMINMAXPROC)(GLenum target, GLboolean reset, GLenum format,
                                             GLenum type, GLsizei bufSize, void* values);
typedef void(APIENTRYP PFNDOLTEXTUREBARRIERPROC)(void);

extern PFNDOLCREATETRANSFORMFEEDBACKSPROC dolCreateTransformFeedbacks;
extern PFNDOLTRANSFORMFEEDBACKBUFFERBASEPROC dolTransformFeedbackBufferBase;
extern PFNDOLTRANSFORMFEEDBACKBUFFERRANGEPROC dolTransformFeedbackBufferRange;
extern PFNDOLGETTRANSFORMFEEDBACKIVPROC dolGetTransformFeedbackiv;
extern PFNDOLGETTRANSFORMFEEDBACKI_VPROC dolGetTransformFeedbacki_v;
extern PFNDOLGETTRANSFORMFEEDBACKI64_VPROC dolGetTransformFeedbacki64_v;
extern PFNDOLCREATEBUFFERSPROC dolCreateBuffers;
extern PFNDOLNAMEDBUFFERSTORAGEPROC dolNamedBufferStorage;
extern PFNDOLNAMEDBUFFERDATAPROC dolNamedBufferData;
extern PFNDOLNAMEDBUFFERSUBDATAPROC dolNamedBufferSubData;
extern PFNDOLCOPYNAMEDBUFFERSUBDATAPROC dolCopyNamedBufferSubData;
extern PFNDOLCLEARNAMEDBUFFERDATAPROC dolClearNamedBufferData;
extern PFNDOLCLEARNAMEDBUFFERSUBDATAPROC dolClearNamedBufferSubData;
extern PFNDOLMAPNAMEDBUFFERPROC dolMapNamedBuffer;
extern PFNDOLMAPNAMEDBUFFERRANGEPROC dolMapNamedBufferRange;
extern PFNDOLUNMAPNAMEDBUFFERPROC dolUnmapNamedBuffer;
extern PFNDOLFLUSHMAPPEDNAMEDBUFFERRANGEPROC dolFlushMappedNamedBufferRange;
extern PFNDOLGETNAMEDBUFFERPARAMETERIVPROC dolGetNamedBufferParameteriv;
extern PFNDOLGETNAMEDBUFFERPARAMETERI64VPROC dolGetNamedBufferParameteri64v;
extern PFNDOLGETNAMEDBUFFERPOINTERVPROC dolGetNamedBufferPointerv;
extern PFNDOLGETNAMEDBUFFERSUBDATAPROC dolGetNamedBufferSubData;
extern PFNDOLCREATEFRAMEBUFFERSPROC dolCreateFramebuffers;
extern PFNDOLNAMEDFRAMEBUFFERRENDERBUFFERPROC dolNamedFramebufferRenderbuffer;
extern PFNDOLNAMEDFRAMEBUFFERPARAMETERIPROC dolNamedFramebufferParameteri;
extern PFNDOLNAMEDFRAMEBUFFERTEXTUREPROC dolNamedFramebufferTexture;
extern PFNDOLNAMEDFRAMEBUFFERTEXTURELAYERPROC dolNamedFramebufferTextureLayer;
extern PFNDOLNAMEDFRAMEBUFFERDRAWBUFFERPROC dolNamedFramebufferDrawBuffer;
extern PFNDOLNAMEDFRAMEBUFFERDRAWBUFFERSPROC dolNamedFramebufferDrawBuffers;
extern PFNDOLNAMEDFRAMEBUFFERREADBUFFERPROC dolNamedFramebufferReadBuffer;
extern PFNDOLINVALIDATENAMEDFRAMEBUFFERDATAPROC dolInvalidateNamedFramebufferData;
extern PFNDOLINVALIDATENAMEDFRAMEBUFFERSUBDATAPROC dolInvalidateNamedFramebufferSubData;
extern PFNDOLCLEARNAMEDFRAMEBUFFERIVPROC dolClearNamedFramebufferiv;
extern PFNDOLCLEARNAMEDFRAMEBUFFERUIVPROC dolClearNamedFramebufferuiv;
extern PFNDOLCLEARNAMEDFRAMEBUFFERFVPROC dolClearNamedFramebufferfv;
extern PFNDOLCLEARNAMEDFRAMEBUFFERFIPROC dolClearNamedFramebufferfi;
extern PFNDOLBLITNAMEDFRAMEBUFFERPROC dolBlitNamedFramebuffer;
extern PFNDOLCHECKNAMEDFRAMEBUFFERSTATUSPROC dolCheckNamedFramebufferStatus;
extern PFNDOLGETNAMEDFRAMEBUFFERPARAMETERIVPROC dolGetNamedFramebufferParameteriv;
extern PFNDOLGETNAMEDFRAMEBUFFERATTACHMENTPARAMETERIVPROC
    dolGetNamedFramebufferAttachmentParameteriv;
extern PFNDOLCREATERENDERBUFFERSPROC dolCreateRenderbuffers;
extern PFNDOLNAMEDRENDERBUFFERSTORAGEPROC dolNamedRenderbufferStorage;
extern PFNDOLNAMEDRENDERBUFFERSTORAGEMULTISAMPLEPROC dolNamedRenderbufferStorageMultisample;
extern PFNDOLGETNAMEDRENDERBUFFERPARAMETERIVPROC dolGetNamedRenderbufferParameteriv;
extern PFNDOLCREATETEXTURESPROC dolCreateTextures;
extern PFNDOLTEXTUREBUFFERPROC dolTextureBuffer;
extern PFNDOLTEXTUREBUFFERRANGEPROC dolTextureBufferRange;
extern PFNDOLTEXTURESTORAGE1DPROC dolTextureStorage1D;
extern PFNDOLTEXTURESTORAGE2DPROC dolTextureStorage2D;
extern PFNDOLTEXTURESTORAGE3DPROC dolTextureStorage3D;
extern PFNDOLTEXTURESTORAGE2DMULTISAMPLEPROC dolTextureStorage2DMultisample;
extern PFNDOLTEXTURESTORAGE3DMULTISAMPLEPROC dolTextureStorage3DMultisample;
extern PFNDOLTEXTURESUBIMAGE1DPROC dolTextureSubImage1D;
extern PFNDOLTEXTURESUBIMAGE2DPROC dolTextureSubImage2D;
extern PFNDOLTEXTURESUBIMAGE3DPROC dolTextureSubImage3D;
extern PFNDOLCOMPRESSEDTEXTURESUBIMAGE1DPROC dolCompressedTextureSubImage1D;
extern PFNDOLCOMPRESSEDTEXTURESUBIMAGE2DPROC dolCompressedTextureSubImage2D;
extern PFNDOLCOMPRESSEDTEXTURESUBIMAGE3DPROC dolCompressedTextureSubImage3D;
extern PFNDOLCOPYTEXTURESUBIMAGE1DPROC dolCopyTextureSubImage1D;
extern PFNDOLCOPYTEXTURESUBIMAGE2DPROC dolCopyTextureSubImage2D;
extern PFNDOLCOPYTEXTURESUBIMAGE3DPROC dolCopyTextureSubImage3D;
extern PFNDOLTEXTUREPARAMETERFPROC dolTextureParameterf;
extern PFNDOLTEXTUREPARAMETERFVPROC dolTextureParameterfv;
extern PFNDOLTEXTUREPARAMETERIPROC dolTextureParameteri;
extern PFNDOLTEXTUREPARAMETERIIVPROC dolTextureParameterIiv;
extern PFNDOLTEXTUREPARAMETERIUIVPROC dolTextureParameterIuiv;
extern PFNDOLTEXTUREPARAMETERIVPROC dolTextureParameteriv;
extern PFNDOLGENERATETEXTUREMIPMAPPROC dolGenerateTextureMipmap;
extern PFNDOLBINDTEXTUREUNITPROC dolBindTextureUnit;
extern PFNDOLGETTEXTUREIMAGEPROC dolGetTextureImage;
extern PFNDOLGETCOMPRESSEDTEXTUREIMAGEPROC dolGetCompressedTextureImage;
extern PFNDOLGETTEXTURELEVELPARAMETERFVPROC dolGetTextureLevelParameterfv;
extern PFNDOLGETTEXTURELEVELPARAMETERIVPROC dolGetTextureLevelParameteriv;
extern PFNDOLGETTEXTUREPARAMETERFVPROC dolGetTextureParameterfv;
extern PFNDOLGETTEXTUREPARAMETERIIVPROC dolGetTextureParameterIiv;
extern PFNDOLGETTEXTUREPARAMETERIUIVPROC dolGetTextureParameterIuiv;
extern PFNDOLGETTEXTUREPARAMETERIVPROC dolGetTextureParameteriv;
extern PFNDOLCREATEVERTEXARRAYSPROC dolCreateVertexArrays;
extern PFNDOLDISABLEVERTEXARRAYATTRIBPROC dolDisableVertexArrayAttrib;
extern PFNDOLENABLEVERTEXARRAYATTRIBPROC dolEnableVertexArrayAttrib;
extern PFNDOLVERTEXARRAYELEMENTBUFFERPROC dolVertexArrayElementBuffer;
extern PFNDOLVERTEXARRAYVERTEXBUFFERPROC dolVertexArrayVertexBuffer;
extern PFNDOLVERTEXARRAYVERTEXBUFFERSPROC dolVertexArrayVertexBuffers;
extern PFNDOLVERTEXARRAYATTRIBBINDINGPROC dolVertexArrayAttribBinding;
extern PFNDOLVERTEXARRAYATTRIBFORMATPROC dolVertexArrayAttribFormat;
extern PFNDOLVERTEXARRAYATTRIBIFORMATPROC dolVertexArrayAttribIFormat;
extern PFNDOLVERTEXARRAYATTRIBLFORMATPROC dolVertexArrayAttribLFormat;
extern PFNDOLVERTEXARRAYBINDINGDIVISORPROC dolVertexArrayBindingDivisor;
extern PFNDOLGETVERTEXARRAYIVPROC dolGetVertexArrayiv;
extern PFNDOLGETVERTEXARRAYINDEXEDIVPROC dolGetVertexArrayIndexediv;
extern PFNDOLGETVERTEXARRAYINDEXED64IVPROC dolGetVertexArrayIndexed64iv;
extern PFNDOLCREATESAMPLERSPROC dolCreateSamplers;
extern PFNDOLCREATEPROGRAMPIPELINESPROC dolCreateProgramPipelines;
extern PFNDOLCREATEQUERIESPROC dolCreateQueries;
extern PFNDOLGETQUERYBUFFEROBJECTI64VPROC dolGetQueryBufferObjecti64v;
extern PFNDOLGETQUERYBUFFEROBJECTIVPROC dolGetQueryBufferObjectiv;
extern PFNDOLGETQUERYBUFFEROBJECTUI64VPROC dolGetQueryBufferObjectui64v;
extern PFNDOLGETQUERYBUFFEROBJECTUIVPROC dolGetQueryBufferObjectuiv;
extern PFNDOLMEMORYBARRIERBYREGIONPROC dolMemoryBarrierByRegion;
extern PFNDOLGETTEXTURESUBIMAGEPROC dolGetTextureSubImage;
extern PFNDOLGETCOMPRESSEDTEXTURESUBIMAGEPROC dolGetCompressedTextureSubImage;
extern PFNDOLGETGRAPHICSRESETSTATUSPROC dolGetGraphicsResetStatus;
extern PFNDOLGETNCOMPRESSEDTEXIMAGEPROC dolGetnCompressedTexImage;
extern PFNDOLGETNTEXIMAGEPROC dolGetnTexImage;
extern PFNDOLGETNUNIFORMDVPROC dolGetnUniformdv;
extern PFNDOLGETNUNIFORMFVPROC dolGetnUniformfv;
extern PFNDOLGETNUNIFORMIVPROC dolGetnUniformiv;
extern PFNDOLGETNUNIFORMUIVPROC dolGetnUniformuiv;
extern PFNDOLREADNPIXELSPROC dolReadnPixels;
extern PFNDOLGETNMAPDVPROC dolGetnMapdv;
extern PFNDOLGETNMAPFVPROC dolGetnMapfv;
extern PFNDOLGETNMAPIVPROC dolGetnMapiv;
extern PFNDOLGETNPIXELMAPFVPROC dolGetnPixelMapfv;
extern PFNDOLGETNPIXELMAPUIVPROC dolGetnPixelMapuiv;
extern PFNDOLGETNPIXELMAPUSVPROC dolGetnPixelMapusv;
extern PFNDOLGETNPOLYGONSTIPPLEPROC dolGetnPolygonStipple;
extern PFNDOLGETNCOLORTABLEPROC dolGetnColorTable;
extern PFNDOLGETNCONVOLUTIONFILTERPROC dolGetnConvolutionFilter;
extern PFNDOLGETNSEPARABLEFILTERPROC dolGetnSeparableFilter;
extern PFNDOLGETNHISTOGRAMPROC dolGetnHistogram;
extern PFNDOLGETNMINMAXPROC dolGetnMinmax;
extern PFNDOLTEXTUREBARRIERPROC dolTextureBarrier;

#define glCreateTransformFeedbacks dolCreateTransformFeedbacks
#define glTransformFeedbackBufferBase dolTransformFeedbackBufferBase
#define glTransformFeedbackBufferRange dolTransformFeedbackBufferRange
#define glGetTransformFeedbackiv dolGetTransformFeedbackiv
#define glGetTransformFeedbacki_v dolGetTransformFeedbacki_v
#define glGetTransformFeedbacki64_v dolGetTransformFeedbacki64_v
#define glCreateBuffers dolCreateBuffers
#define glNamedBufferStorage dolNamedBufferStorage
#define glNamedBufferData dolNamedBufferData
#define glNamedBufferSubData dolNamedBufferSubData
#define glCopyNamedBufferSubData dolCopyNamedBufferSubData
#define glClearNamedBufferData dolClearNamedBufferData
#define glClearNamedBufferSubData dolClearNamedBufferSubData
#define glMapNamedBuffer dolMapNamedBuffer
#define glMapNamedBufferRange dolMapNamedBufferRange
#define glUnmapNamedBuffer dolUnmapNamedBuffer
#define glFlushMappedNamedBufferRange dolFlushMappedNamedBufferRange
#define glGetNamedBufferParameteriv dolGetNamedBufferParameteriv
#define glGetNamedBufferParameteri64v dolGetNamedBufferParameteri64v
#define glGetNamedBufferPointerv dolGetNamedBufferPointerv
#define glGetNamedBufferSubData dolGetNamedBufferSubData
#define glCreateFramebuffers dolCreateFramebuffers
#define glNamedFramebufferRenderbuffer dolNamedFramebufferRenderbuffer
#define glNamedFramebufferParameteri dolNamedFramebufferParameteri
#define glNamedFramebufferTexture dolNamedFramebufferTexture
#define glNamedFramebufferTextureLayer dolNamedFramebufferTextureLayer
#define glNamedFramebufferDrawBuffer dolNamedFramebufferDrawBuffer
#define glNamedFramebufferDrawBuffers dolNamedFramebufferDrawBuffers
#define glNamedFramebufferReadBuffer dolNamedFramebufferReadBuffer
#define glInvalidateNamedFramebufferData dolInvalidateNamedFramebufferData
#define glInvalidateNamedFramebufferSubData dolInvalidateNamedFramebufferSubData
#define glClearNamedFramebufferiv dolClearNamedFramebufferiv
#define glClearNamedFramebufferuiv dolClearNamedFramebufferuiv
#define glClearNamedFramebufferfv dolClearNamedFramebufferfv
#define glClearNamedFramebufferfi dolClearNamedFramebufferfi
#define glBlitNamedFramebuffer dolBlitNamedFramebuffer
#define glCheckNamedFramebufferStatus dolCheckNamedFramebufferStatus
#define glGetNamedFramebufferParameteriv dolGetNamedFramebufferParameteriv
#define glGetNamedFramebufferAttachmentParameteriv dolGetNamedFramebufferAttachmentParameteriv
#define glCreateRenderbuffers dolCreateRenderbuffers
#define glNamedRenderbufferStorage dolNamedRenderbufferStorage
#define glNamedRenderbufferStorageMultisample dolNamedRenderbufferStorageMultisample
#define glGetNamedRenderbufferParameteriv dolGetNamedRenderbufferParameteriv
#define glCreateTextures dolCreateTextures
#define glTextureBuffer dolTextureBuffer
#define glTextureBufferRange dolTextureBufferRange
#define glTextureStorage1D dolTextureStorage1D
#define glTextureStorage2D dolTextureStorage2D
#define glTextureStorage3D dolTextureStorage3D
#define glTextureStorage2DMultisample dolTextureStorage2DMultisample
#define glTextureStorage3DMultisample dolTextureStorage3DMultisample
#define glTextureSubImage1D dolTextureSubImage1D
#define glTextureSubImage2D dolTextureSubImage2D
#define glTextureSubImage3D dolTextureSubImage3D
#define glCompressedTextureSubImage1D dolCompressedTextureSubImage1D
#define glCompressedTextureSubImage2D dolCompressedTextureSubImage2D
#define glCompressedTextureSubImage3D dolCompressedTextureSubImage3D
#define glCopyTextureSubImage1D dolCopyTextureSubImage1D
#define glCopyTextureSubImage2D dolCopyTextureSubImage2D
#define glCopyTextureSubImage3D dolCopyTextureSubImage3D
#define glTextureParameterf dolTextureParameterf
#define glTextureParameterfv dolTextureParameterfv
#define glTextureParameteri dolTextureParameteri
#define glTextureParameterIiv dolTextureParameterIiv
#define glTextureParameterIuiv dolTextureParameterIuiv
#define glTextureParameteriv dolTextureParameteriv
#define glGenerateTextureMipmap dolGenerateTextureMipmap
#define glBindTextureUnit dolBindTextureUnit
#define glGetTextureImage dolGetTextureImage
#define glGetCompressedTextureImage dolGetCompressedTextureImage
#define glGetTextureLevelParameterfv dolGetTextureLevelParameterfv
#define glGetTextureLevelParameteriv dolGetTextureLevelParameteriv
#define glGetTextureParameterfv dolGetTextureParameterfv
#define glGetTextureParameterIiv dolGetTextureParameterIiv
#define glGetTextureParameterIuiv dolGetTextureParameterIuiv
#define glGetTextureParameteriv dolGetTextureParameteriv
#define glCreateVertexArrays dolCreateVertexArrays
#define glDisableVertexArrayAttrib dolDisableVertexArrayAttrib
#define glEnableVertexArrayAttrib dolEnableVertexArrayAttrib
#define glVertexArrayElementBuffer dolVertexArrayElementBuffer
#define glVertexArrayVertexBuffer dolVertexArrayVertexBuffer
#define glVertexArrayVertexBuffers dolVertexArrayVertexBuffers
#define glVertexArrayAttribBinding dolVertexArrayAttribBinding
#define glVertexArrayAttribFormat dolVertexArrayAttribFormat
#define glVertexArrayAttribIFormat dolVertexArrayAttribIFormat
#define glVertexArrayAttribLFormat dolVertexArrayAttribLFormat
#define glVertexArrayBindingDivisor dolVertexArrayBindingDivisor
#define glGetVertexArrayiv dolGetVertexArrayiv
#define glGetVertexArrayIndexediv dolGetVertexArrayIndexediv
#define glGetVertexArrayIndexed64iv dolGetVertexArrayIndexed64iv
#define glCreateSamplers dolCreateSamplers
#define glCreateProgramPipelines dolCreateProgramPipelines
#define glCreateQueries dolCreateQueries
#define glGetQueryBufferObjecti64v dolGetQueryBufferObjecti64v
#define glGetQueryBufferObjectiv dolGetQueryBufferObjectiv
#define glGetQueryBufferObjectui64v dolGetQueryBufferObjectui64v
#define glGetQueryBufferObjectuiv dolGetQueryBufferObjectuiv
#define glMemoryBarrierByRegion dolMemoryBarrierByRegion
#define glGetTextureSubImage dolGetTextureSubImage
#define glGetCompressedTextureSubImage dolGetCompressedTextureSubImage
#define glGetGraphicsResetStatus dolGetGraphicsResetStatus
#define glGetnCompressedTexImage dolGetnCompressedTexImage
#define glGetnTexImage dolGetnTexImage
#define glGetnUniformdv dolGetnUniformdv
#define glGetnUniformfv dolGetnUniformfv
#define glGetnUniformiv dolGetnUniformiv
#define glGetnUniformuiv dolGetnUniformuiv
#define glReadnPixels dolReadnPixels
#define glGetnMapdv dolGetnMapdv
#define glGetnMapfv dolGetnMapfv
#define glGetnMapiv dolGetnMapiv
#define glGetnPixelMapfv dolGetnPixelMapfv
#define glGetnPixelMapuiv dolGetnPixelMapuiv
#define glGetnPixelMapusv dolGetnPixelMapusv
#define glGetnPolygonStipple dolGetnPolygonStipple
#define glGetnColorTable dolGetnColorTable
#define glGetnConvolutionFilter dolGetnConvolutionFilter
#define glGetnSeparableFilter dolGetnSeparableFilter
#define glGetnHistogram dolGetnHistogram
#define glGetnMinmax dolGetnMinmax
#define glTextureBarrier dolTextureBarrier
