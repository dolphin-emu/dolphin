/*
** Copyright (c) 2013-2015 The Khronos Group Inc.
** SPDX-License-Identifier: MIT
*/

#include "Common/GL/GLExtensions/gl_common.h"

/* multitexture */
#define GL_TEXTURE0 0x84C0
#define GL_TEXTURE1 0x84C1
#define GL_TEXTURE2 0x84C2
#define GL_TEXTURE3 0x84C3
#define GL_TEXTURE4 0x84C4
#define GL_TEXTURE5 0x84C5
#define GL_TEXTURE6 0x84C6
#define GL_TEXTURE7 0x84C7
#define GL_TEXTURE8 0x84C8
#define GL_TEXTURE9 0x84C9
#define GL_TEXTURE10 0x84CA
#define GL_TEXTURE11 0x84CB
#define GL_TEXTURE12 0x84CC
#define GL_TEXTURE13 0x84CD
#define GL_TEXTURE14 0x84CE
#define GL_TEXTURE15 0x84CF
#define GL_TEXTURE16 0x84D0
#define GL_TEXTURE17 0x84D1
#define GL_TEXTURE18 0x84D2
#define GL_TEXTURE19 0x84D3
#define GL_TEXTURE20 0x84D4
#define GL_TEXTURE21 0x84D5
#define GL_TEXTURE22 0x84D6
#define GL_TEXTURE23 0x84D7
#define GL_TEXTURE24 0x84D8
#define GL_TEXTURE25 0x84D9
#define GL_TEXTURE26 0x84DA
#define GL_TEXTURE27 0x84DB
#define GL_TEXTURE28 0x84DC
#define GL_TEXTURE29 0x84DD
#define GL_TEXTURE30 0x84DE
#define GL_TEXTURE31 0x84DF
#define GL_ACTIVE_TEXTURE 0x84E0
#define GL_CLIENT_ACTIVE_TEXTURE 0x84E1
#define GL_MAX_TEXTURE_UNITS 0x84E2
/* texture_cube_map */
#define GL_NORMAL_MAP 0x8511
#define GL_REFLECTION_MAP 0x8512
#define GL_TEXTURE_CUBE_MAP 0x8513
#define GL_TEXTURE_BINDING_CUBE_MAP 0x8514
#define GL_TEXTURE_CUBE_MAP_POSITIVE_X 0x8515
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_X 0x8516
#define GL_TEXTURE_CUBE_MAP_POSITIVE_Y 0x8517
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_Y 0x8518
#define GL_TEXTURE_CUBE_MAP_POSITIVE_Z 0x8519
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_Z 0x851A
#define GL_PROXY_TEXTURE_CUBE_MAP 0x851B
#define GL_MAX_CUBE_MAP_TEXTURE_SIZE 0x851C
/* texture_compression */
#define GL_COMPRESSED_ALPHA 0x84E9
#define GL_COMPRESSED_LUMINANCE 0x84EA
#define GL_COMPRESSED_LUMINANCE_ALPHA 0x84EB
#define GL_COMPRESSED_INTENSITY 0x84EC
#define GL_COMPRESSED_RGB 0x84ED
#define GL_COMPRESSED_RGBA 0x84EE
#define GL_TEXTURE_COMPRESSION_HINT 0x84EF
#define GL_TEXTURE_COMPRESSED_IMAGE_SIZE 0x86A0
#define GL_TEXTURE_COMPRESSED 0x86A1
#define GL_NUM_COMPRESSED_TEXTURE_FORMATS 0x86A2
#define GL_COMPRESSED_TEXTURE_FORMATS 0x86A3
/* multisample */
#define GL_MULTISAMPLE 0x809D
#define GL_SAMPLE_ALPHA_TO_COVERAGE 0x809E
#define GL_SAMPLE_ALPHA_TO_ONE 0x809F
#define GL_SAMPLE_COVERAGE 0x80A0
#define GL_SAMPLE_BUFFERS 0x80A8
#define GL_SAMPLES 0x80A9
#define GL_SAMPLE_COVERAGE_VALUE 0x80AA
#define GL_SAMPLE_COVERAGE_INVERT 0x80AB
#define GL_MULTISAMPLE_BIT 0x20000000
/* transpose_matrix */
#define GL_TRANSPOSE_MODELVIEW_MATRIX 0x84E3
#define GL_TRANSPOSE_PROJECTION_MATRIX 0x84E4
#define GL_TRANSPOSE_TEXTURE_MATRIX 0x84E5
#define GL_TRANSPOSE_COLOR_MATRIX 0x84E6
/* texture_env_combine */
#define GL_COMBINE 0x8570
#define GL_COMBINE_RGB 0x8571
#define GL_COMBINE_ALPHA 0x8572
#define GL_SOURCE0_RGB 0x8580
#define GL_SOURCE1_RGB 0x8581
#define GL_SOURCE2_RGB 0x8582
#define GL_SOURCE0_ALPHA 0x8588
#define GL_SOURCE1_ALPHA 0x8589
#define GL_SOURCE2_ALPHA 0x858A
#define GL_OPERAND0_RGB 0x8590
#define GL_OPERAND1_RGB 0x8591
#define GL_OPERAND2_RGB 0x8592
#define GL_OPERAND0_ALPHA 0x8598
#define GL_OPERAND1_ALPHA 0x8599
#define GL_OPERAND2_ALPHA 0x859A
#define GL_RGB_SCALE 0x8573
#define GL_ADD_SIGNED 0x8574
#define GL_INTERPOLATE 0x8575
#define GL_SUBTRACT 0x84E7
#define GL_CONSTANT 0x8576
#define GL_PRIMARY_COLOR 0x8577
#define GL_PREVIOUS 0x8578
/* texture_env_dot3 */
#define GL_DOT3_RGB 0x86AE
#define GL_DOT3_RGBA 0x86AF
/* texture_border_clamp */
#define GL_CLAMP_TO_BORDER 0x812D

typedef void(APIENTRYP PFNDOLACTIVETEXTUREPROC)(GLenum texture);
typedef void(APIENTRYP PFNDOLSAMPLECOVERAGEPROC)(GLclampf value, GLboolean invert);
typedef void(APIENTRYP PFNDOLCOMPRESSEDTEXIMAGE3DPROC)(GLenum target, GLint level,
                                                       GLenum internalformat, GLsizei width,
                                                       GLsizei height, GLsizei depth, GLint border,
                                                       GLsizei imageSize, const GLvoid* data);
typedef void(APIENTRYP PFNDOLCOMPRESSEDTEXIMAGE2DPROC)(GLenum target, GLint level,
                                                       GLenum internalformat, GLsizei width,
                                                       GLsizei height, GLint border,
                                                       GLsizei imageSize, const GLvoid* data);
typedef void(APIENTRYP PFNDOLCOMPRESSEDTEXIMAGE1DPROC)(GLenum target, GLint level,
                                                       GLenum internalformat, GLsizei width,
                                                       GLint border, GLsizei imageSize,
                                                       const GLvoid* data);
typedef void(APIENTRYP PFNDOLCOMPRESSEDTEXSUBIMAGE3DPROC)(GLenum target, GLint level, GLint xoffset,
                                                          GLint yoffset, GLint zoffset,
                                                          GLsizei width, GLsizei height,
                                                          GLsizei depth, GLenum format,
                                                          GLsizei imageSize, const GLvoid* data);
typedef void(APIENTRYP PFNDOLCOMPRESSEDTEXSUBIMAGE2DPROC)(GLenum target, GLint level, GLint xoffset,
                                                          GLint yoffset, GLsizei width,
                                                          GLsizei height, GLenum format,
                                                          GLsizei imageSize, const GLvoid* data);
typedef void(APIENTRYP PFNDOLCOMPRESSEDTEXSUBIMAGE1DPROC)(GLenum target, GLint level, GLint xoffset,
                                                          GLsizei width, GLenum format,
                                                          GLsizei imageSize, const GLvoid* data);
typedef void(APIENTRYP PFNDOLGETCOMPRESSEDTEXIMAGEPROC)(GLenum target, GLint level, GLvoid* img);

typedef void(APIENTRYP PFNDOLACTIVETEXTUREARBPROC)(GLenum texture);
typedef void(APIENTRYP PFNDOLCLIENTACTIVETEXTUREARBPROC)(GLenum texture);
typedef void(APIENTRYP PFNDOLMULTITEXCOORD1DARBPROC)(GLenum target, GLdouble s);
typedef void(APIENTRYP PFNDOLMULTITEXCOORD1DVARBPROC)(GLenum target, const GLdouble* v);
typedef void(APIENTRYP PFNDOLMULTITEXCOORD1FARBPROC)(GLenum target, GLfloat s);
typedef void(APIENTRYP PFNDOLMULTITEXCOORD1FVARBPROC)(GLenum target, const GLfloat* v);
typedef void(APIENTRYP PFNDOLMULTITEXCOORD1IARBPROC)(GLenum target, GLint s);
typedef void(APIENTRYP PFNDOLMULTITEXCOORD1IVARBPROC)(GLenum target, const GLint* v);
typedef void(APIENTRYP PFNDOLMULTITEXCOORD1SARBPROC)(GLenum target, GLshort s);
typedef void(APIENTRYP PFNDOLMULTITEXCOORD1SVARBPROC)(GLenum target, const GLshort* v);
typedef void(APIENTRYP PFNDOLMULTITEXCOORD2DARBPROC)(GLenum target, GLdouble s, GLdouble t);
typedef void(APIENTRYP PFNDOLMULTITEXCOORD2DVARBPROC)(GLenum target, const GLdouble* v);
typedef void(APIENTRYP PFNDOLMULTITEXCOORD2FARBPROC)(GLenum target, GLfloat s, GLfloat t);
typedef void(APIENTRYP PFNDOLMULTITEXCOORD2FVARBPROC)(GLenum target, const GLfloat* v);
typedef void(APIENTRYP PFNDOLMULTITEXCOORD2IARBPROC)(GLenum target, GLint s, GLint t);
typedef void(APIENTRYP PFNDOLMULTITEXCOORD2IVARBPROC)(GLenum target, const GLint* v);
typedef void(APIENTRYP PFNDOLMULTITEXCOORD2SARBPROC)(GLenum target, GLshort s, GLshort t);
typedef void(APIENTRYP PFNDOLMULTITEXCOORD2SVARBPROC)(GLenum target, const GLshort* v);
typedef void(APIENTRYP PFNDOLMULTITEXCOORD3DARBPROC)(GLenum target, GLdouble s, GLdouble t,
                                                     GLdouble r);
typedef void(APIENTRYP PFNDOLMULTITEXCOORD3DVARBPROC)(GLenum target, const GLdouble* v);
typedef void(APIENTRYP PFNDOLMULTITEXCOORD3FARBPROC)(GLenum target, GLfloat s, GLfloat t,
                                                     GLfloat r);
typedef void(APIENTRYP PFNDOLMULTITEXCOORD3FVARBPROC)(GLenum target, const GLfloat* v);
typedef void(APIENTRYP PFNDOLMULTITEXCOORD3IARBPROC)(GLenum target, GLint s, GLint t, GLint r);
typedef void(APIENTRYP PFNDOLMULTITEXCOORD3IVARBPROC)(GLenum target, const GLint* v);
typedef void(APIENTRYP PFNDOLMULTITEXCOORD3SARBPROC)(GLenum target, GLshort s, GLshort t,
                                                     GLshort r);
typedef void(APIENTRYP PFNDOLMULTITEXCOORD3SVARBPROC)(GLenum target, const GLshort* v);
typedef void(APIENTRYP PFNDOLMULTITEXCOORD4DARBPROC)(GLenum target, GLdouble s, GLdouble t,
                                                     GLdouble r, GLdouble q);
typedef void(APIENTRYP PFNDOLMULTITEXCOORD4DVARBPROC)(GLenum target, const GLdouble* v);
typedef void(APIENTRYP PFNDOLMULTITEXCOORD4FARBPROC)(GLenum target, GLfloat s, GLfloat t, GLfloat r,
                                                     GLfloat q);
typedef void(APIENTRYP PFNDOLMULTITEXCOORD4FVARBPROC)(GLenum target, const GLfloat* v);
typedef void(APIENTRYP PFNDOLMULTITEXCOORD4IARBPROC)(GLenum target, GLint s, GLint t, GLint r,
                                                     GLint q);
typedef void(APIENTRYP PFNDOLMULTITEXCOORD4IVARBPROC)(GLenum target, const GLint* v);
typedef void(APIENTRYP PFNDOLMULTITEXCOORD4SARBPROC)(GLenum target, GLshort s, GLshort t, GLshort r,
                                                     GLshort q);
typedef void(APIENTRYP PFNDOLMULTITEXCOORD4SVARBPROC)(GLenum target, const GLshort* v);
typedef void(APIENTRYP PFNDOLLOADTRANSPOSEMATRIXFARBPROC)(const GLfloat* m);
typedef void(APIENTRYP PFNDOLLOADTRANSPOSEMATRIXDARBPROC)(const GLdouble* m);
typedef void(APIENTRYP PFNDOLMULTTRANSPOSEMATRIXFARBPROC)(const GLfloat* m);
typedef void(APIENTRYP PFNDOLMULTTRANSPOSEMATRIXDARBPROC)(const GLdouble* m);
typedef void(APIENTRYP PFNDOLSAMPLECOVERAGEARBPROC)(GLfloat value, GLboolean invert);

extern PFNDOLACTIVETEXTUREARBPROC dolActiveTexture;
extern PFNDOLCLIENTACTIVETEXTUREARBPROC dolClientActiveTexture;
extern PFNDOLCOMPRESSEDTEXIMAGE1DPROC dolCompressedTexImage1D;
extern PFNDOLCOMPRESSEDTEXIMAGE2DPROC dolCompressedTexImage2D;
extern PFNDOLCOMPRESSEDTEXIMAGE3DPROC dolCompressedTexImage3D;
extern PFNDOLCOMPRESSEDTEXSUBIMAGE1DPROC dolCompressedTexSubImage1D;
extern PFNDOLCOMPRESSEDTEXSUBIMAGE2DPROC dolCompressedTexSubImage2D;
extern PFNDOLCOMPRESSEDTEXSUBIMAGE3DPROC dolCompressedTexSubImage3D;
extern PFNDOLGETCOMPRESSEDTEXIMAGEPROC dolGetCompressedTexImage;
extern PFNDOLLOADTRANSPOSEMATRIXDARBPROC dolLoadTransposeMatrixd;
extern PFNDOLLOADTRANSPOSEMATRIXFARBPROC dolLoadTransposeMatrixf;
extern PFNDOLMULTTRANSPOSEMATRIXDARBPROC dolMultTransposeMatrixd;
extern PFNDOLMULTTRANSPOSEMATRIXFARBPROC dolMultTransposeMatrixf;
extern PFNDOLMULTITEXCOORD1DARBPROC dolMultiTexCoord1d;
extern PFNDOLMULTITEXCOORD1DVARBPROC dolMultiTexCoord1dv;
extern PFNDOLMULTITEXCOORD1FARBPROC dolMultiTexCoord1f;
extern PFNDOLMULTITEXCOORD1FVARBPROC dolMultiTexCoord1fv;
extern PFNDOLMULTITEXCOORD1IARBPROC dolMultiTexCoord1i;
extern PFNDOLMULTITEXCOORD1IVARBPROC dolMultiTexCoord1iv;
extern PFNDOLMULTITEXCOORD1SARBPROC dolMultiTexCoord1s;
extern PFNDOLMULTITEXCOORD1SVARBPROC dolMultiTexCoord1sv;
extern PFNDOLMULTITEXCOORD2DARBPROC dolMultiTexCoord2d;
extern PFNDOLMULTITEXCOORD2DVARBPROC dolMultiTexCoord2dv;
extern PFNDOLMULTITEXCOORD2FARBPROC dolMultiTexCoord2f;
extern PFNDOLMULTITEXCOORD2FVARBPROC dolMultiTexCoord2fv;
extern PFNDOLMULTITEXCOORD2IARBPROC dolMultiTexCoord2i;
extern PFNDOLMULTITEXCOORD2IVARBPROC dolMultiTexCoord2iv;
extern PFNDOLMULTITEXCOORD2SARBPROC dolMultiTexCoord2s;
extern PFNDOLMULTITEXCOORD2SVARBPROC dolMultiTexCoord2sv;
extern PFNDOLMULTITEXCOORD3DARBPROC dolMultiTexCoord3d;
extern PFNDOLMULTITEXCOORD3DVARBPROC dolMultiTexCoord3dv;
extern PFNDOLMULTITEXCOORD3FARBPROC dolMultiTexCoord3f;
extern PFNDOLMULTITEXCOORD3FVARBPROC dolMultiTexCoord3fv;
extern PFNDOLMULTITEXCOORD3IARBPROC dolMultiTexCoord3i;
extern PFNDOLMULTITEXCOORD3IVARBPROC dolMultiTexCoord3iv;
extern PFNDOLMULTITEXCOORD3SARBPROC dolMultiTexCoord3s;
extern PFNDOLMULTITEXCOORD3SVARBPROC dolMultiTexCoord3sv;
extern PFNDOLMULTITEXCOORD4DARBPROC dolMultiTexCoord4d;
extern PFNDOLMULTITEXCOORD4DVARBPROC dolMultiTexCoord4dv;
extern PFNDOLMULTITEXCOORD4FARBPROC dolMultiTexCoord4f;
extern PFNDOLMULTITEXCOORD4FVARBPROC dolMultiTexCoord4fv;
extern PFNDOLMULTITEXCOORD4IARBPROC dolMultiTexCoord4i;
extern PFNDOLMULTITEXCOORD4IVARBPROC dolMultiTexCoord4iv;
extern PFNDOLMULTITEXCOORD4SARBPROC dolMultiTexCoord4s;
extern PFNDOLMULTITEXCOORD4SVARBPROC dolMultiTexCoord4sv;
extern PFNDOLSAMPLECOVERAGEARBPROC dolSampleCoverage;

#define glActiveTexture dolActiveTexture
#define glClientActiveTexture dolClientActiveTexture
#define glCompressedTexImage1D dolCompressedTexImage1D
#define glCompressedTexImage2D dolCompressedTexImage2D
#define glCompressedTexImage3D dolCompressedTexImage3D
#define glCompressedTexSubImage1D dolCompressedTexSubImage1D
#define glCompressedTexSubImage2D dolCompressedTexSubImage2D
#define glCompressedTexSubImage3D dolCompressedTexSubImage3D
#define glGetCompressedTexImage dolGetCompressedTexImage
#define glLoadTransposeMatrixd dolLoadTransposeMatrixd
#define glLoadTransposeMatrixf dolLoadTransposeMatrixf
#define glMultTransposeMatrixd dolMultTransposeMatrixd
#define glMultTransposeMatrixf dolMultTransposeMatrixf
#define glMultiTexCoord1d dolMultiTexCoord1d
#define glMultiTexCoord1dv dolMultiTexCoord1dv
#define glMultiTexCoord1f dolMultiTexCoord1f
#define glMultiTexCoord1fv dolMultiTexCoord1fv
#define glMultiTexCoord1i dolMultiTexCoord1i
#define glMultiTexCoord1iv dolMultiTexCoord1iv
#define glMultiTexCoord1s dolMultiTexCoord1s
#define glMultiTexCoord1sv dolMultiTexCoord1sv
#define glMultiTexCoord2d dolMultiTexCoord2d
#define glMultiTexCoord2dv dolMultiTexCoord2dv
#define glMultiTexCoord2f dolMultiTexCoord2f
#define glMultiTexCoord2fv dolMultiTexCoord2fv
#define glMultiTexCoord2i dolMultiTexCoord2i
#define glMultiTexCoord2iv dolMultiTexCoord2iv
#define glMultiTexCoord2s dolMultiTexCoord2s
#define glMultiTexCoord2sv dolMultiTexCoord2sv
#define glMultiTexCoord3d dolMultiTexCoord3d
#define glMultiTexCoord3dv dolMultiTexCoord3dv
#define glMultiTexCoord3f dolMultiTexCoord3f
#define glMultiTexCoord3fv dolMultiTexCoord3fv
#define glMultiTexCoord3i dolMultiTexCoord3i
#define glMultiTexCoord3iv dolMultiTexCoord3iv
#define glMultiTexCoord3s dolMultiTexCoord3s
#define glMultiTexCoord3sv dolMultiTexCoord3sv
#define glMultiTexCoord4d dolMultiTexCoord4d
#define glMultiTexCoord4dv dolMultiTexCoord4dv
#define glMultiTexCoord4f dolMultiTexCoord4f
#define glMultiTexCoord4fv dolMultiTexCoord4fv
#define glMultiTexCoord4i dolMultiTexCoord4i
#define glMultiTexCoord4iv dolMultiTexCoord4iv
#define glMultiTexCoord4s dolMultiTexCoord4s
#define glMultiTexCoord4sv dolMultiTexCoord4sv
#define glSampleCoverage dolSampleCoverage
