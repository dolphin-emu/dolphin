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

#define GL_BLEND_DST_RGB                  0x80C8
#define GL_BLEND_SRC_RGB                  0x80C9
#define GL_BLEND_DST_ALPHA                0x80CA
#define GL_BLEND_SRC_ALPHA                0x80CB
#define GL_POINT_FADE_THRESHOLD_SIZE      0x8128
#define GL_DEPTH_COMPONENT16              0x81A5
#define GL_DEPTH_COMPONENT24              0x81A6
#define GL_DEPTH_COMPONENT32              0x81A7
#define GL_MIRRORED_REPEAT                0x8370
#define GL_MAX_TEXTURE_LOD_BIAS           0x84FD
#define GL_TEXTURE_LOD_BIAS               0x8501
#define GL_INCR_WRAP                      0x8507
#define GL_DECR_WRAP                      0x8508
#define GL_TEXTURE_DEPTH_SIZE             0x884A
#define GL_TEXTURE_COMPARE_MODE           0x884C
#define GL_TEXTURE_COMPARE_FUNC           0x884D
#define GL_POINT_SIZE_MIN                 0x8126
#define GL_POINT_SIZE_MAX                 0x8127
#define GL_POINT_DISTANCE_ATTENUATION     0x8129
#define GL_GENERATE_MIPMAP                0x8191
#define GL_GENERATE_MIPMAP_HINT           0x8192
#define GL_FOG_COORDINATE_SOURCE          0x8450
#define GL_FOG_COORDINATE                 0x8451
#define GL_FRAGMENT_DEPTH                 0x8452
#define GL_CURRENT_FOG_COORDINATE         0x8453
#define GL_FOG_COORDINATE_ARRAY_TYPE      0x8454
#define GL_FOG_COORDINATE_ARRAY_STRIDE    0x8455
#define GL_FOG_COORDINATE_ARRAY_POINTER   0x8456
#define GL_FOG_COORDINATE_ARRAY           0x8457
#define GL_COLOR_SUM                      0x8458
#define GL_CURRENT_SECONDARY_COLOR        0x8459
#define GL_SECONDARY_COLOR_ARRAY_SIZE     0x845A
#define GL_SECONDARY_COLOR_ARRAY_TYPE     0x845B
#define GL_SECONDARY_COLOR_ARRAY_STRIDE   0x845C
#define GL_SECONDARY_COLOR_ARRAY_POINTER  0x845D
#define GL_SECONDARY_COLOR_ARRAY          0x845E
#define GL_TEXTURE_FILTER_CONTROL         0x8500
#define GL_DEPTH_TEXTURE_MODE             0x884B
#define GL_COMPARE_R_TO_TEXTURE           0x884E
#define GL_FUNC_ADD                       0x8006
#define GL_FUNC_SUBTRACT                  0x800A
#define GL_FUNC_REVERSE_SUBTRACT          0x800B
#define GL_MIN                            0x8007
#define GL_MAX                            0x8008
#define GL_CONSTANT_COLOR                 0x8001
#define GL_ONE_MINUS_CONSTANT_COLOR       0x8002
#define GL_CONSTANT_ALPHA                 0x8003
#define GL_ONE_MINUS_CONSTANT_ALPHA       0x8004

typedef void (APIENTRYP PFNDOLBLENDFUNCSEPARATEPROC) (GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha);
typedef void (APIENTRYP PFNDOLMULTIDRAWARRAYSPROC) (GLenum mode, const GLint *first, const GLsizei *count, GLsizei drawcount);
typedef void (APIENTRYP PFNDOLMULTIDRAWELEMENTSPROC) (GLenum mode, const GLsizei *count, GLenum type, const void *const*indices, GLsizei drawcount);
typedef void (APIENTRYP PFNDOLPOINTPARAMETERFPROC) (GLenum pname, GLfloat param);
typedef void (APIENTRYP PFNDOLPOINTPARAMETERFVPROC) (GLenum pname, const GLfloat *params);
typedef void (APIENTRYP PFNDOLPOINTPARAMETERIPROC) (GLenum pname, GLint param);
typedef void (APIENTRYP PFNDOLPOINTPARAMETERIVPROC) (GLenum pname, const GLint *params);
typedef void (APIENTRYP PFNDOLFOGCOORDFPROC) (GLfloat coord);
typedef void (APIENTRYP PFNDOLFOGCOORDFVPROC) (const GLfloat *coord);
typedef void (APIENTRYP PFNDOLFOGCOORDDPROC) (GLdouble coord);
typedef void (APIENTRYP PFNDOLFOGCOORDDVPROC) (const GLdouble *coord);
typedef void (APIENTRYP PFNDOLFOGCOORDPOINTERPROC) (GLenum type, GLsizei stride, const void *pointer);
typedef void (APIENTRYP PFNDOLSECONDARYCOLOR3BPROC) (GLbyte red, GLbyte green, GLbyte blue);
typedef void (APIENTRYP PFNDOLSECONDARYCOLOR3BVPROC) (const GLbyte *v);
typedef void (APIENTRYP PFNDOLSECONDARYCOLOR3DPROC) (GLdouble red, GLdouble green, GLdouble blue);
typedef void (APIENTRYP PFNDOLSECONDARYCOLOR3DVPROC) (const GLdouble *v);
typedef void (APIENTRYP PFNDOLSECONDARYCOLOR3FPROC) (GLfloat red, GLfloat green, GLfloat blue);
typedef void (APIENTRYP PFNDOLSECONDARYCOLOR3FVPROC) (const GLfloat *v);
typedef void (APIENTRYP PFNDOLSECONDARYCOLOR3IPROC) (GLint red, GLint green, GLint blue);
typedef void (APIENTRYP PFNDOLSECONDARYCOLOR3IVPROC) (const GLint *v);
typedef void (APIENTRYP PFNDOLSECONDARYCOLOR3SPROC) (GLshort red, GLshort green, GLshort blue);
typedef void (APIENTRYP PFNDOLSECONDARYCOLOR3SVPROC) (const GLshort *v);
typedef void (APIENTRYP PFNDOLSECONDARYCOLOR3UBPROC) (GLubyte red, GLubyte green, GLubyte blue);
typedef void (APIENTRYP PFNDOLSECONDARYCOLOR3UBVPROC) (const GLubyte *v);
typedef void (APIENTRYP PFNDOLSECONDARYCOLOR3UIPROC) (GLuint red, GLuint green, GLuint blue);
typedef void (APIENTRYP PFNDOLSECONDARYCOLOR3UIVPROC) (const GLuint *v);
typedef void (APIENTRYP PFNDOLSECONDARYCOLOR3USPROC) (GLushort red, GLushort green, GLushort blue);
typedef void (APIENTRYP PFNDOLSECONDARYCOLOR3USVPROC) (const GLushort *v);
typedef void (APIENTRYP PFNDOLSECONDARYCOLORPOINTERPROC) (GLint size, GLenum type, GLsizei stride, const void *pointer);
typedef void (APIENTRYP PFNDOLWINDOWPOS2DPROC) (GLdouble x, GLdouble y);
typedef void (APIENTRYP PFNDOLWINDOWPOS2DVPROC) (const GLdouble *v);
typedef void (APIENTRYP PFNDOLWINDOWPOS2FPROC) (GLfloat x, GLfloat y);
typedef void (APIENTRYP PFNDOLWINDOWPOS2FVPROC) (const GLfloat *v);
typedef void (APIENTRYP PFNDOLWINDOWPOS2IPROC) (GLint x, GLint y);
typedef void (APIENTRYP PFNDOLWINDOWPOS2IVPROC) (const GLint *v);
typedef void (APIENTRYP PFNDOLWINDOWPOS2SPROC) (GLshort x, GLshort y);
typedef void (APIENTRYP PFNDOLWINDOWPOS2SVPROC) (const GLshort *v);
typedef void (APIENTRYP PFNDOLWINDOWPOS3DPROC) (GLdouble x, GLdouble y, GLdouble z);
typedef void (APIENTRYP PFNDOLWINDOWPOS3DVPROC) (const GLdouble *v);
typedef void (APIENTRYP PFNDOLWINDOWPOS3FPROC) (GLfloat x, GLfloat y, GLfloat z);
typedef void (APIENTRYP PFNDOLWINDOWPOS3FVPROC) (const GLfloat *v);
typedef void (APIENTRYP PFNDOLWINDOWPOS3IPROC) (GLint x, GLint y, GLint z);
typedef void (APIENTRYP PFNDOLWINDOWPOS3IVPROC) (const GLint *v);
typedef void (APIENTRYP PFNDOLWINDOWPOS3SPROC) (GLshort x, GLshort y, GLshort z);
typedef void (APIENTRYP PFNDOLWINDOWPOS3SVPROC) (const GLshort *v);
typedef void (APIENTRYP PFNDOLBLENDCOLORPROC) (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
typedef void (APIENTRYP PFNDOLBLENDEQUATIONPROC) (GLenum mode);

// These two are provided by ARB_imaging as well
extern PFNDOLBLENDCOLORPROC dolBlendColor;
extern PFNDOLBLENDEQUATIONPROC dolBlendEquation;
extern PFNDOLBLENDFUNCSEPARATEPROC dolBlendFuncSeparate;
extern PFNDOLFOGCOORDPOINTERPROC dolFogCoordPointer;
extern PFNDOLFOGCOORDDPROC dolFogCoordd;
extern PFNDOLFOGCOORDDVPROC dolFogCoorddv;
extern PFNDOLFOGCOORDFPROC dolFogCoordf;
extern PFNDOLFOGCOORDFVPROC dolFogCoordfv;
extern PFNDOLMULTIDRAWARRAYSPROC dolMultiDrawArrays;
extern PFNDOLMULTIDRAWELEMENTSPROC dolMultiDrawElements;
extern PFNDOLPOINTPARAMETERFPROC dolPointParameterf;
extern PFNDOLPOINTPARAMETERFVPROC dolPointParameterfv;
extern PFNDOLPOINTPARAMETERIPROC dolPointParameteri;
extern PFNDOLPOINTPARAMETERIVPROC dolPointParameteriv;
extern PFNDOLSECONDARYCOLOR3BPROC dolSecondaryColor3b;
extern PFNDOLSECONDARYCOLOR3BVPROC dolSecondaryColor3bv;
extern PFNDOLSECONDARYCOLOR3DPROC dolSecondaryColor3d;
extern PFNDOLSECONDARYCOLOR3DVPROC dolSecondaryColor3dv;
extern PFNDOLSECONDARYCOLOR3FPROC dolSecondaryColor3f;
extern PFNDOLSECONDARYCOLOR3FVPROC dolSecondaryColor3fv;
extern PFNDOLSECONDARYCOLOR3IPROC dolSecondaryColor3i;
extern PFNDOLSECONDARYCOLOR3IVPROC dolSecondaryColor3iv;
extern PFNDOLSECONDARYCOLOR3SPROC dolSecondaryColor3s;
extern PFNDOLSECONDARYCOLOR3SVPROC dolSecondaryColor3sv;
extern PFNDOLSECONDARYCOLOR3UBPROC dolSecondaryColor3ub;
extern PFNDOLSECONDARYCOLOR3UBVPROC dolSecondaryColor3ubv;
extern PFNDOLSECONDARYCOLOR3UIPROC dolSecondaryColor3ui;
extern PFNDOLSECONDARYCOLOR3UIVPROC dolSecondaryColor3uiv;
extern PFNDOLSECONDARYCOLOR3USPROC dolSecondaryColor3us;
extern PFNDOLSECONDARYCOLOR3USVPROC dolSecondaryColor3usv;
extern PFNDOLSECONDARYCOLORPOINTERPROC dolSecondaryColorPointer;
extern PFNDOLWINDOWPOS2DPROC dolWindowPos2d;
extern PFNDOLWINDOWPOS2DVPROC dolWindowPos2dv;
extern PFNDOLWINDOWPOS2FPROC dolWindowPos2f;
extern PFNDOLWINDOWPOS2FVPROC dolWindowPos2fv;
extern PFNDOLWINDOWPOS2IPROC dolWindowPos2i;
extern PFNDOLWINDOWPOS2IVPROC dolWindowPos2iv;
extern PFNDOLWINDOWPOS2SPROC dolWindowPos2s;
extern PFNDOLWINDOWPOS2SVPROC dolWindowPos2sv;
extern PFNDOLWINDOWPOS3DPROC dolWindowPos3d;
extern PFNDOLWINDOWPOS3DVPROC dolWindowPos3dv;
extern PFNDOLWINDOWPOS3FPROC dolWindowPos3f;
extern PFNDOLWINDOWPOS3FVPROC dolWindowPos3fv;
extern PFNDOLWINDOWPOS3IPROC dolWindowPos3i;
extern PFNDOLWINDOWPOS3IVPROC dolWindowPos3iv;
extern PFNDOLWINDOWPOS3SPROC dolWindowPos3s;
extern PFNDOLWINDOWPOS3SVPROC dolWindowPos3sv;

#define glBlendColor dolBlendColor
#define glBlendEquation dolBlendEquation
#define glBlendFuncSeparate dolBlendFuncSeparate
#define glFogCoordPointer dolFogCoordPointer
#define glFogCoordd dolFogCoordd
#define glFogCoorddv dolFogCoorddv
#define glFogCoordf dolFogCoordf
#define glFogCoordfv dolFogCoordfv
#define glMultiDrawArrays dolMultiDrawArrays
#define glMultiDrawElements dolMultiDrawElements
#define glPointParameterf dolPointParameterf
#define glPointParameterfv dolPointParameterfv
#define glPointParameteri dolPointParameteri
#define glPointParameteriv dolPointParameteriv
#define glSecondaryColor3b dolSecondaryColor3b
#define glSecondaryColor3bv dolSecondaryColor3bv
#define glSecondaryColor3d dolSecondaryColor3d
#define glSecondaryColor3dv dolSecondaryColor3dv
#define glSecondaryColor3f dolSecondaryColor3f
#define glSecondaryColor3fv dolSecondaryColor3fv
#define glSecondaryColor3i dolSecondaryColor3i
#define glSecondaryColor3iv dolSecondaryColor3iv
#define glSecondaryColor3s dolSecondaryColor3s
#define glSecondaryColor3sv dolSecondaryColor3sv
#define glSecondaryColor3ub dolSecondaryColor3ub
#define glSecondaryColor3ubv dolSecondaryColor3ubv
#define glSecondaryColor3ui dolSecondaryColor3ui
#define glSecondaryColor3uiv dolSecondaryColor3uiv
#define glSecondaryColor3us dolSecondaryColor3us
#define glSecondaryColor3usv dolSecondaryColor3usv
#define glSecondaryColorPointer dolSecondaryColorPointer
#define glWindowPos2d dolWindowPos2d
#define glWindowPos2dv dolWindowPos2dv
#define glWindowPos2f dolWindowPos2f
#define glWindowPos2fv dolWindowPos2fv
#define glWindowPos2i dolWindowPos2i
#define glWindowPos2iv dolWindowPos2iv
#define glWindowPos2s dolWindowPos2s
#define glWindowPos2sv dolWindowPos2sv
#define glWindowPos3d dolWindowPos3d
#define glWindowPos3dv dolWindowPos3dv
#define glWindowPos3f dolWindowPos3f
#define glWindowPos3fv dolWindowPos3fv
#define glWindowPos3i dolWindowPos3i
#define glWindowPos3iv dolWindowPos3iv
#define glWindowPos3s dolWindowPos3s
#define glWindowPos3sv dolWindowPos3sv
