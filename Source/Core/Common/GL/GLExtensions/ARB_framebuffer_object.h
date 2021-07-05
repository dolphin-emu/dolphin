/*
** Copyright (c) 2013-2015 The Khronos Group Inc.
** SPDX-License-Identifier: MIT
*/

#include "Common/GL/GLExtensions/gl_common.h"

typedef void(APIENTRYP PFNDOLBINDFRAMEBUFFERPROC)(GLenum target, GLuint framebuffer);
typedef void(APIENTRYP PFNDOLBINDRENDERBUFFERPROC)(GLenum target, GLuint renderbuffer);
typedef void(APIENTRYP PFNDOLBLITFRAMEBUFFERPROC)(GLint srcX0, GLint srcY0, GLint srcX1,
                                                  GLint srcY1, GLint dstX0, GLint dstY0,
                                                  GLint dstX1, GLint dstY1, GLbitfield mask,
                                                  GLenum filter);
typedef GLenum(APIENTRYP PFNDOLCHECKFRAMEBUFFERSTATUSPROC)(GLenum target);
typedef void(APIENTRYP PFNDOLDELETEFRAMEBUFFERSPROC)(GLsizei n, const GLuint* framebuffers);
typedef void(APIENTRYP PFNDOLDELETERENDERBUFFERSPROC)(GLsizei n, const GLuint* renderbuffers);
typedef void(APIENTRYP PFNDOLFRAMEBUFFERRENDERBUFFERPROC)(GLenum target, GLenum attachment,
                                                          GLenum renderbuffertarget,
                                                          GLuint renderbuffer);
typedef void(APIENTRYP PFNDOLFRAMEBUFFERTEXTURE1DPROC)(GLenum target, GLenum attachment,
                                                       GLenum textarget, GLuint texture,
                                                       GLint level);
typedef void(APIENTRYP PFNDOLFRAMEBUFFERTEXTURE2DPROC)(GLenum target, GLenum attachment,
                                                       GLenum textarget, GLuint texture,
                                                       GLint level);
typedef void(APIENTRYP PFNDOLFRAMEBUFFERTEXTURE3DPROC)(GLenum target, GLenum attachment,
                                                       GLenum textarget, GLuint texture,
                                                       GLint level, GLint zoffset);
typedef void(APIENTRYP PFNDOLFRAMEBUFFERTEXTURELAYERPROC)(GLenum target, GLenum attachment,
                                                          GLuint texture, GLint level, GLint layer);
typedef void(APIENTRYP PFNDOLGENFRAMEBUFFERSPROC)(GLsizei n, GLuint* framebuffers);
typedef void(APIENTRYP PFNDOLGENRENDERBUFFERSPROC)(GLsizei n, GLuint* renderbuffers);
typedef void(APIENTRYP PFNDOLGENERATEMIPMAPPROC)(GLenum target);
typedef void(APIENTRYP PFNDOLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC)(GLenum target,
                                                                      GLenum attachment,
                                                                      GLenum pname, GLint* params);
typedef void(APIENTRYP PFNDOLGETRENDERBUFFERPARAMETERIVPROC)(GLenum target, GLenum pname,
                                                             GLint* params);
typedef GLboolean(APIENTRYP PFNDOLISFRAMEBUFFERPROC)(GLuint framebuffer);
typedef GLboolean(APIENTRYP PFNDOLISRENDERBUFFERPROC)(GLuint renderbuffer);
typedef void(APIENTRYP PFNDOLRENDERBUFFERSTORAGEPROC)(GLenum target, GLenum internalformat,
                                                      GLsizei width, GLsizei height);
typedef void(APIENTRYP PFNDOLRENDERBUFFERSTORAGEMULTISAMPLEPROC)(GLenum target, GLsizei samples,
                                                                 GLenum internalformat,
                                                                 GLsizei width, GLsizei height);

extern PFNDOLBINDFRAMEBUFFERPROC dolBindFramebuffer;
extern PFNDOLBINDRENDERBUFFERPROC dolBindRenderbuffer;
extern PFNDOLBLITFRAMEBUFFERPROC dolBlitFramebuffer;
extern PFNDOLCHECKFRAMEBUFFERSTATUSPROC dolCheckFramebufferStatus;
extern PFNDOLDELETEFRAMEBUFFERSPROC dolDeleteFramebuffers;
extern PFNDOLDELETERENDERBUFFERSPROC dolDeleteRenderbuffers;
extern PFNDOLFRAMEBUFFERRENDERBUFFERPROC dolFramebufferRenderbuffer;
extern PFNDOLFRAMEBUFFERTEXTURE1DPROC dolFramebufferTexture1D;
extern PFNDOLFRAMEBUFFERTEXTURE2DPROC dolFramebufferTexture2D;
extern PFNDOLFRAMEBUFFERTEXTURE3DPROC dolFramebufferTexture3D;
extern PFNDOLFRAMEBUFFERTEXTURELAYERPROC dolFramebufferTextureLayer;
extern PFNDOLGENFRAMEBUFFERSPROC dolGenFramebuffers;
extern PFNDOLGENRENDERBUFFERSPROC dolGenRenderbuffers;
extern PFNDOLGENERATEMIPMAPPROC dolGenerateMipmap;
extern PFNDOLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC dolGetFramebufferAttachmentParameteriv;
extern PFNDOLGETRENDERBUFFERPARAMETERIVPROC dolGetRenderbufferParameteriv;
extern PFNDOLISFRAMEBUFFERPROC dolIsFramebuffer;
extern PFNDOLISRENDERBUFFERPROC dolIsRenderbuffer;
extern PFNDOLRENDERBUFFERSTORAGEPROC dolRenderbufferStorage;
extern PFNDOLRENDERBUFFERSTORAGEMULTISAMPLEPROC dolRenderbufferStorageMultisample;

#define glBindFramebuffer dolBindFramebuffer
#define glBindRenderbuffer dolBindRenderbuffer
#define glBlitFramebuffer dolBlitFramebuffer
#define glCheckFramebufferStatus dolCheckFramebufferStatus
#define glDeleteFramebuffers dolDeleteFramebuffers
#define glDeleteRenderbuffers dolDeleteRenderbuffers
#define glFramebufferRenderbuffer dolFramebufferRenderbuffer
#define glFramebufferTexture1D dolFramebufferTexture1D
#define glFramebufferTexture2D dolFramebufferTexture2D
#define glFramebufferTexture3D dolFramebufferTexture3D
#define glFramebufferTextureLayer dolFramebufferTextureLayer
#define glGenFramebuffers dolGenFramebuffers
#define glGenRenderbuffers dolGenRenderbuffers
#define glGenerateMipmap dolGenerateMipmap
#define glGetFramebufferAttachmentParameteriv dolGetFramebufferAttachmentParameteriv
#define glGetRenderbufferParameteriv dolGetRenderbufferParameteriv
#define glIsFramebuffer dolIsFramebuffer
#define glIsRenderbuffer dolIsRenderbuffer
#define glRenderbufferStorage dolRenderbufferStorage
#define glRenderbufferStorageMultisample dolRenderbufferStorageMultisample
