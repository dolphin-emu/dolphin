/*
** Copyright (c) 2013-2015 The Khronos Group Inc.
** SPDX-License-Identifier: MIT
*/

#include "Common/GL/GLExtensions/gl_common.h"

#define GL_CURRENT_RASTER_SECONDARY_COLOR 0x845F
#define GL_PIXEL_PACK_BUFFER 0x88EB
#define GL_PIXEL_UNPACK_BUFFER 0x88EC
#define GL_PIXEL_PACK_BUFFER_BINDING 0x88ED
#define GL_PIXEL_UNPACK_BUFFER_BINDING 0x88EF
#define GL_FLOAT_MAT2x3 0x8B65
#define GL_FLOAT_MAT2x4 0x8B66
#define GL_FLOAT_MAT3x2 0x8B67
#define GL_FLOAT_MAT3x4 0x8B68
#define GL_FLOAT_MAT4x2 0x8B69
#define GL_FLOAT_MAT4x3 0x8B6A
#define GL_SRGB 0x8C40
#define GL_SRGB8 0x8C41
#define GL_SRGB_ALPHA 0x8C42
#define GL_SRGB8_ALPHA8 0x8C43
#define GL_SLUMINANCE_ALPHA 0x8C44
#define GL_SLUMINANCE8_ALPHA8 0x8C45
#define GL_SLUMINANCE 0x8C46
#define GL_SLUMINANCE8 0x8C47
#define GL_COMPRESSED_SRGB 0x8C48
#define GL_COMPRESSED_SRGB_ALPHA 0x8C49
#define GL_COMPRESSED_SLUMINANCE 0x8C4A
#define GL_COMPRESSED_SLUMINANCE_ALPHA 0x8C4B

typedef void(APIENTRYP PFNDOLUNIFORMMATRIX2X3FVPROC)(
    GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
typedef void(APIENTRYP PFNDOLUNIFORMMATRIX2X4FVPROC)(
    GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
typedef void(APIENTRYP PFNDOLUNIFORMMATRIX3X2FVPROC)(
    GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
typedef void(APIENTRYP PFNDOLUNIFORMMATRIX3X4FVPROC)(
    GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
typedef void(APIENTRYP PFNDOLUNIFORMMATRIX4X2FVPROC)(
    GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
typedef void(APIENTRYP PFNDOLUNIFORMMATRIX4X3FVPROC)(
    GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);

extern PFNDOLUNIFORMMATRIX2X3FVPROC dolUniformMatrix2x3fv;
extern PFNDOLUNIFORMMATRIX2X4FVPROC dolUniformMatrix2x4fv;
extern PFNDOLUNIFORMMATRIX3X2FVPROC dolUniformMatrix3x2fv;
extern PFNDOLUNIFORMMATRIX3X4FVPROC dolUniformMatrix3x4fv;
extern PFNDOLUNIFORMMATRIX4X2FVPROC dolUniformMatrix4x2fv;
extern PFNDOLUNIFORMMATRIX4X3FVPROC dolUniformMatrix4x3fv;

#define glUniformMatrix2x3fv dolUniformMatrix2x3fv
#define glUniformMatrix2x4fv dolUniformMatrix2x4fv
#define glUniformMatrix3x2fv dolUniformMatrix3x2fv
#define glUniformMatrix3x4fv dolUniformMatrix3x4fv
#define glUniformMatrix4x2fv dolUniformMatrix4x2fv
#define glUniformMatrix4x3fv dolUniformMatrix4x3fv
