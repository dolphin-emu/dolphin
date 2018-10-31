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

#define GL_BLEND_EQUATION_RGB 0x8009
#define GL_VERTEX_ATTRIB_ARRAY_ENABLED 0x8622
#define GL_VERTEX_ATTRIB_ARRAY_SIZE 0x8623
#define GL_VERTEX_ATTRIB_ARRAY_STRIDE 0x8624
#define GL_VERTEX_ATTRIB_ARRAY_TYPE 0x8625
#define GL_CURRENT_VERTEX_ATTRIB 0x8626
#define GL_VERTEX_PROGRAM_POINT_SIZE 0x8642
#define GL_VERTEX_ATTRIB_ARRAY_POINTER 0x8645
#define GL_STENCIL_BACK_FUNC 0x8800
#define GL_STENCIL_BACK_FAIL 0x8801
#define GL_STENCIL_BACK_PASS_DEPTH_FAIL 0x8802
#define GL_STENCIL_BACK_PASS_DEPTH_PASS 0x8803
#define GL_MAX_DRAW_BUFFERS 0x8824
#define GL_DRAW_BUFFER0 0x8825
#define GL_DRAW_BUFFER1 0x8826
#define GL_DRAW_BUFFER2 0x8827
#define GL_DRAW_BUFFER3 0x8828
#define GL_DRAW_BUFFER4 0x8829
#define GL_DRAW_BUFFER5 0x882A
#define GL_DRAW_BUFFER6 0x882B
#define GL_DRAW_BUFFER7 0x882C
#define GL_DRAW_BUFFER8 0x882D
#define GL_DRAW_BUFFER9 0x882E
#define GL_DRAW_BUFFER10 0x882F
#define GL_DRAW_BUFFER11 0x8830
#define GL_DRAW_BUFFER12 0x8831
#define GL_DRAW_BUFFER13 0x8832
#define GL_DRAW_BUFFER14 0x8833
#define GL_DRAW_BUFFER15 0x8834
#define GL_BLEND_EQUATION_ALPHA 0x883D
#define GL_MAX_VERTEX_ATTRIBS 0x8869
#define GL_VERTEX_ATTRIB_ARRAY_NORMALIZED 0x886A
#define GL_MAX_TEXTURE_IMAGE_UNITS 0x8872
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_MAX_FRAGMENT_UNIFORM_COMPONENTS 0x8B49
#define GL_MAX_VERTEX_UNIFORM_COMPONENTS 0x8B4A
#define GL_MAX_VARYING_FLOATS 0x8B4B
#define GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS 0x8B4C
#define GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS 0x8B4D
#define GL_SHADER_TYPE 0x8B4F
#define GL_FLOAT_VEC2 0x8B50
#define GL_FLOAT_VEC3 0x8B51
#define GL_FLOAT_VEC4 0x8B52
#define GL_INT_VEC2 0x8B53
#define GL_INT_VEC3 0x8B54
#define GL_INT_VEC4 0x8B55
#define GL_BOOL 0x8B56
#define GL_BOOL_VEC2 0x8B57
#define GL_BOOL_VEC3 0x8B58
#define GL_BOOL_VEC4 0x8B59
#define GL_FLOAT_MAT2 0x8B5A
#define GL_FLOAT_MAT3 0x8B5B
#define GL_FLOAT_MAT4 0x8B5C
#define GL_SAMPLER_1D 0x8B5D
#define GL_SAMPLER_2D 0x8B5E
#define GL_SAMPLER_3D 0x8B5F
#define GL_SAMPLER_CUBE 0x8B60
#define GL_SAMPLER_1D_SHADOW 0x8B61
#define GL_SAMPLER_2D_SHADOW 0x8B62
#define GL_DELETE_STATUS 0x8B80
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_VALIDATE_STATUS 0x8B83
#define GL_INFO_LOG_LENGTH 0x8B84
#define GL_ATTACHED_SHADERS 0x8B85
#define GL_ACTIVE_UNIFORMS 0x8B86
#define GL_ACTIVE_UNIFORM_MAX_LENGTH 0x8B87
#define GL_SHADER_SOURCE_LENGTH 0x8B88
#define GL_ACTIVE_ATTRIBUTES 0x8B89
#define GL_ACTIVE_ATTRIBUTE_MAX_LENGTH 0x8B8A
#define GL_FRAGMENT_SHADER_DERIVATIVE_HINT 0x8B8B
#define GL_SHADING_LANGUAGE_VERSION 0x8B8C
#define GL_CURRENT_PROGRAM 0x8B8D
#define GL_POINT_SPRITE_COORD_ORIGIN 0x8CA0
#define GL_LOWER_LEFT 0x8CA1
#define GL_UPPER_LEFT 0x8CA2
#define GL_STENCIL_BACK_REF 0x8CA3
#define GL_STENCIL_BACK_VALUE_MASK 0x8CA4
#define GL_STENCIL_BACK_WRITEMASK 0x8CA5
#define GL_VERTEX_PROGRAM_TWO_SIDE 0x8643
#define GL_POINT_SPRITE 0x8861
#define GL_COORD_REPLACE 0x8862
#define GL_MAX_TEXTURE_COORDS 0x8871

typedef void(APIENTRYP PFNDOLBLENDEQUATIONSEPARATEPROC)(GLenum modeRGB, GLenum modeAlpha);
typedef void(APIENTRYP PFNDOLDRAWBUFFERSPROC)(GLsizei n, const GLenum* bufs);
typedef void(APIENTRYP PFNDOLSTENCILOPSEPARATEPROC)(GLenum face, GLenum sfail, GLenum dpfail,
                                                    GLenum dppass);
typedef void(APIENTRYP PFNDOLSTENCILFUNCSEPARATEPROC)(GLenum face, GLenum func, GLint ref,
                                                      GLuint mask);
typedef void(APIENTRYP PFNDOLSTENCILMASKSEPARATEPROC)(GLenum face, GLuint mask);
typedef void(APIENTRYP PFNDOLATTACHSHADERPROC)(GLuint program, GLuint shader);
typedef void(APIENTRYP PFNDOLBINDATTRIBLOCATIONPROC)(GLuint program, GLuint index,
                                                     const GLchar* name);
typedef void(APIENTRYP PFNDOLCOMPILESHADERPROC)(GLuint shader);
typedef GLuint(APIENTRYP PFNDOLCREATEPROGRAMPROC)(void);
typedef GLuint(APIENTRYP PFNDOLCREATESHADERPROC)(GLenum type);
typedef void(APIENTRYP PFNDOLDELETEPROGRAMPROC)(GLuint program);
typedef void(APIENTRYP PFNDOLDELETESHADERPROC)(GLuint shader);
typedef void(APIENTRYP PFNDOLDETACHSHADERPROC)(GLuint program, GLuint shader);
typedef void(APIENTRYP PFNDOLDISABLEVERTEXATTRIBARRAYPROC)(GLuint index);
typedef void(APIENTRYP PFNDOLENABLEVERTEXATTRIBARRAYPROC)(GLuint index);
typedef void(APIENTRYP PFNDOLGETACTIVEATTRIBPROC)(GLuint program, GLuint index, GLsizei bufSize,
                                                  GLsizei* length, GLint* size, GLenum* type,
                                                  GLchar* name);
typedef void(APIENTRYP PFNDOLGETACTIVEUNIFORMPROC)(GLuint program, GLuint index, GLsizei bufSize,
                                                   GLsizei* length, GLint* size, GLenum* type,
                                                   GLchar* name);
typedef void(APIENTRYP PFNDOLGETATTACHEDSHADERSPROC)(GLuint program, GLsizei maxCount,
                                                     GLsizei* count, GLuint* shaders);
typedef GLint(APIENTRYP PFNDOLGETATTRIBLOCATIONPROC)(GLuint program, const GLchar* name);
typedef void(APIENTRYP PFNDOLGETPROGRAMIVPROC)(GLuint program, GLenum pname, GLint* params);
typedef void(APIENTRYP PFNDOLGETPROGRAMINFOLOGPROC)(GLuint program, GLsizei bufSize,
                                                    GLsizei* length, GLchar* infoLog);
typedef void(APIENTRYP PFNDOLGETSHADERIVPROC)(GLuint shader, GLenum pname, GLint* params);
typedef void(APIENTRYP PFNDOLGETSHADERINFOLOGPROC)(GLuint shader, GLsizei bufSize, GLsizei* length,
                                                   GLchar* infoLog);
typedef void(APIENTRYP PFNDOLGETSHADERSOURCEPROC)(GLuint shader, GLsizei bufSize, GLsizei* length,
                                                  GLchar* source);
typedef GLint(APIENTRYP PFNDOLGETUNIFORMLOCATIONPROC)(GLuint program, const GLchar* name);
typedef void(APIENTRYP PFNDOLGETUNIFORMFVPROC)(GLuint program, GLint location, GLfloat* params);
typedef void(APIENTRYP PFNDOLGETUNIFORMIVPROC)(GLuint program, GLint location, GLint* params);
typedef void(APIENTRYP PFNDOLGETVERTEXATTRIBDVPROC)(GLuint index, GLenum pname, GLdouble* params);
typedef void(APIENTRYP PFNDOLGETVERTEXATTRIBFVPROC)(GLuint index, GLenum pname, GLfloat* params);
typedef void(APIENTRYP PFNDOLGETVERTEXATTRIBIVPROC)(GLuint index, GLenum pname, GLint* params);
typedef void(APIENTRYP PFNDOLGETVERTEXATTRIBPOINTERVPROC)(GLuint index, GLenum pname,
                                                          void** pointer);
typedef GLboolean(APIENTRYP PFNDOLISPROGRAMPROC)(GLuint program);
typedef GLboolean(APIENTRYP PFNDOLISSHADERPROC)(GLuint shader);
typedef void(APIENTRYP PFNDOLLINKPROGRAMPROC)(GLuint program);
typedef void(APIENTRYP PFNDOLSHADERSOURCEPROC)(GLuint shader, GLsizei count,
                                               const GLchar* const* string, const GLint* length);
typedef void(APIENTRYP PFNDOLUSEPROGRAMPROC)(GLuint program);
typedef void(APIENTRYP PFNDOLUNIFORM1FPROC)(GLint location, GLfloat v0);
typedef void(APIENTRYP PFNDOLUNIFORM2FPROC)(GLint location, GLfloat v0, GLfloat v1);
typedef void(APIENTRYP PFNDOLUNIFORM3FPROC)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
typedef void(APIENTRYP PFNDOLUNIFORM4FPROC)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2,
                                            GLfloat v3);
typedef void(APIENTRYP PFNDOLUNIFORM1IPROC)(GLint location, GLint v0);
typedef void(APIENTRYP PFNDOLUNIFORM2IPROC)(GLint location, GLint v0, GLint v1);
typedef void(APIENTRYP PFNDOLUNIFORM3IPROC)(GLint location, GLint v0, GLint v1, GLint v2);
typedef void(APIENTRYP PFNDOLUNIFORM4IPROC)(GLint location, GLint v0, GLint v1, GLint v2, GLint v3);
typedef void(APIENTRYP PFNDOLUNIFORM1FVPROC)(GLint location, GLsizei count, const GLfloat* value);
typedef void(APIENTRYP PFNDOLUNIFORM2FVPROC)(GLint location, GLsizei count, const GLfloat* value);
typedef void(APIENTRYP PFNDOLUNIFORM3FVPROC)(GLint location, GLsizei count, const GLfloat* value);
typedef void(APIENTRYP PFNDOLUNIFORM4FVPROC)(GLint location, GLsizei count, const GLfloat* value);
typedef void(APIENTRYP PFNDOLUNIFORM1IVPROC)(GLint location, GLsizei count, const GLint* value);
typedef void(APIENTRYP PFNDOLUNIFORM2IVPROC)(GLint location, GLsizei count, const GLint* value);
typedef void(APIENTRYP PFNDOLUNIFORM3IVPROC)(GLint location, GLsizei count, const GLint* value);
typedef void(APIENTRYP PFNDOLUNIFORM4IVPROC)(GLint location, GLsizei count, const GLint* value);
typedef void(APIENTRYP PFNDOLUNIFORMMATRIX2FVPROC)(GLint location, GLsizei count,
                                                   GLboolean transpose, const GLfloat* value);
typedef void(APIENTRYP PFNDOLUNIFORMMATRIX3FVPROC)(GLint location, GLsizei count,
                                                   GLboolean transpose, const GLfloat* value);
typedef void(APIENTRYP PFNDOLUNIFORMMATRIX4FVPROC)(GLint location, GLsizei count,
                                                   GLboolean transpose, const GLfloat* value);
typedef void(APIENTRYP PFNDOLVALIDATEPROGRAMPROC)(GLuint program);
typedef void(APIENTRYP PFNDOLVERTEXATTRIB1DPROC)(GLuint index, GLdouble x);
typedef void(APIENTRYP PFNDOLVERTEXATTRIB1DVPROC)(GLuint index, const GLdouble* v);
typedef void(APIENTRYP PFNDOLVERTEXATTRIB1FPROC)(GLuint index, GLfloat x);
typedef void(APIENTRYP PFNDOLVERTEXATTRIB1FVPROC)(GLuint index, const GLfloat* v);
typedef void(APIENTRYP PFNDOLVERTEXATTRIB1SPROC)(GLuint index, GLshort x);
typedef void(APIENTRYP PFNDOLVERTEXATTRIB1SVPROC)(GLuint index, const GLshort* v);
typedef void(APIENTRYP PFNDOLVERTEXATTRIB2DPROC)(GLuint index, GLdouble x, GLdouble y);
typedef void(APIENTRYP PFNDOLVERTEXATTRIB2DVPROC)(GLuint index, const GLdouble* v);
typedef void(APIENTRYP PFNDOLVERTEXATTRIB2FPROC)(GLuint index, GLfloat x, GLfloat y);
typedef void(APIENTRYP PFNDOLVERTEXATTRIB2FVPROC)(GLuint index, const GLfloat* v);
typedef void(APIENTRYP PFNDOLVERTEXATTRIB2SPROC)(GLuint index, GLshort x, GLshort y);
typedef void(APIENTRYP PFNDOLVERTEXATTRIB2SVPROC)(GLuint index, const GLshort* v);
typedef void(APIENTRYP PFNDOLVERTEXATTRIB3DPROC)(GLuint index, GLdouble x, GLdouble y, GLdouble z);
typedef void(APIENTRYP PFNDOLVERTEXATTRIB3DVPROC)(GLuint index, const GLdouble* v);
typedef void(APIENTRYP PFNDOLVERTEXATTRIB3FPROC)(GLuint index, GLfloat x, GLfloat y, GLfloat z);
typedef void(APIENTRYP PFNDOLVERTEXATTRIB3FVPROC)(GLuint index, const GLfloat* v);
typedef void(APIENTRYP PFNDOLVERTEXATTRIB3SPROC)(GLuint index, GLshort x, GLshort y, GLshort z);
typedef void(APIENTRYP PFNDOLVERTEXATTRIB3SVPROC)(GLuint index, const GLshort* v);
typedef void(APIENTRYP PFNDOLVERTEXATTRIB4NBVPROC)(GLuint index, const GLbyte* v);
typedef void(APIENTRYP PFNDOLVERTEXATTRIB4NIVPROC)(GLuint index, const GLint* v);
typedef void(APIENTRYP PFNDOLVERTEXATTRIB4NSVPROC)(GLuint index, const GLshort* v);
typedef void(APIENTRYP PFNDOLVERTEXATTRIB4NUBPROC)(GLuint index, GLubyte x, GLubyte y, GLubyte z,
                                                   GLubyte w);
typedef void(APIENTRYP PFNDOLVERTEXATTRIB4NUBVPROC)(GLuint index, const GLubyte* v);
typedef void(APIENTRYP PFNDOLVERTEXATTRIB4NUIVPROC)(GLuint index, const GLuint* v);
typedef void(APIENTRYP PFNDOLVERTEXATTRIB4NUSVPROC)(GLuint index, const GLushort* v);
typedef void(APIENTRYP PFNDOLVERTEXATTRIB4BVPROC)(GLuint index, const GLbyte* v);
typedef void(APIENTRYP PFNDOLVERTEXATTRIB4DPROC)(GLuint index, GLdouble x, GLdouble y, GLdouble z,
                                                 GLdouble w);
typedef void(APIENTRYP PFNDOLVERTEXATTRIB4DVPROC)(GLuint index, const GLdouble* v);
typedef void(APIENTRYP PFNDOLVERTEXATTRIB4FPROC)(GLuint index, GLfloat x, GLfloat y, GLfloat z,
                                                 GLfloat w);
typedef void(APIENTRYP PFNDOLVERTEXATTRIB4FVPROC)(GLuint index, const GLfloat* v);
typedef void(APIENTRYP PFNDOLVERTEXATTRIB4IVPROC)(GLuint index, const GLint* v);
typedef void(APIENTRYP PFNDOLVERTEXATTRIB4SPROC)(GLuint index, GLshort x, GLshort y, GLshort z,
                                                 GLshort w);
typedef void(APIENTRYP PFNDOLVERTEXATTRIB4SVPROC)(GLuint index, const GLshort* v);
typedef void(APIENTRYP PFNDOLVERTEXATTRIB4UBVPROC)(GLuint index, const GLubyte* v);
typedef void(APIENTRYP PFNDOLVERTEXATTRIB4UIVPROC)(GLuint index, const GLuint* v);
typedef void(APIENTRYP PFNDOLVERTEXATTRIB4USVPROC)(GLuint index, const GLushort* v);
typedef void(APIENTRYP PFNDOLVERTEXATTRIBPOINTERPROC)(GLuint index, GLint size, GLenum type,
                                                      GLboolean normalized, GLsizei stride,
                                                      const void* pointer);

extern PFNDOLATTACHSHADERPROC dolAttachShader;
extern PFNDOLBINDATTRIBLOCATIONPROC dolBindAttribLocation;
extern PFNDOLBLENDEQUATIONSEPARATEPROC dolBlendEquationSeparate;
extern PFNDOLCOMPILESHADERPROC dolCompileShader;
extern PFNDOLCREATEPROGRAMPROC dolCreateProgram;
extern PFNDOLCREATESHADERPROC dolCreateShader;
extern PFNDOLDELETEPROGRAMPROC dolDeleteProgram;
extern PFNDOLDELETESHADERPROC dolDeleteShader;
extern PFNDOLDETACHSHADERPROC dolDetachShader;
extern PFNDOLDISABLEVERTEXATTRIBARRAYPROC dolDisableVertexAttribArray;
extern PFNDOLDRAWBUFFERSPROC dolDrawBuffers;
extern PFNDOLENABLEVERTEXATTRIBARRAYPROC dolEnableVertexAttribArray;
extern PFNDOLGETACTIVEATTRIBPROC dolGetActiveAttrib;
extern PFNDOLGETACTIVEUNIFORMPROC dolGetActiveUniform;
extern PFNDOLGETATTACHEDSHADERSPROC dolGetAttachedShaders;
extern PFNDOLGETATTRIBLOCATIONPROC dolGetAttribLocation;
extern PFNDOLGETPROGRAMINFOLOGPROC dolGetProgramInfoLog;
extern PFNDOLGETPROGRAMIVPROC dolGetProgramiv;
extern PFNDOLGETSHADERINFOLOGPROC dolGetShaderInfoLog;
extern PFNDOLGETSHADERSOURCEPROC dolGetShaderSource;
extern PFNDOLGETSHADERIVPROC dolGetShaderiv;
extern PFNDOLGETUNIFORMLOCATIONPROC dolGetUniformLocation;
extern PFNDOLGETUNIFORMFVPROC dolGetUniformfv;
extern PFNDOLGETUNIFORMIVPROC dolGetUniformiv;
extern PFNDOLGETVERTEXATTRIBPOINTERVPROC dolGetVertexAttribPointerv;
extern PFNDOLGETVERTEXATTRIBDVPROC dolGetVertexAttribdv;
extern PFNDOLGETVERTEXATTRIBFVPROC dolGetVertexAttribfv;
extern PFNDOLGETVERTEXATTRIBIVPROC dolGetVertexAttribiv;
extern PFNDOLISPROGRAMPROC dolIsProgram;
extern PFNDOLISSHADERPROC dolIsShader;
extern PFNDOLLINKPROGRAMPROC dolLinkProgram;
extern PFNDOLSHADERSOURCEPROC dolShaderSource;
extern PFNDOLSTENCILFUNCSEPARATEPROC dolStencilFuncSeparate;
extern PFNDOLSTENCILMASKSEPARATEPROC dolStencilMaskSeparate;
extern PFNDOLSTENCILOPSEPARATEPROC dolStencilOpSeparate;
extern PFNDOLUNIFORM1FPROC dolUniform1f;
extern PFNDOLUNIFORM1FVPROC dolUniform1fv;
extern PFNDOLUNIFORM1IPROC dolUniform1i;
extern PFNDOLUNIFORM1IVPROC dolUniform1iv;
extern PFNDOLUNIFORM2FPROC dolUniform2f;
extern PFNDOLUNIFORM2FVPROC dolUniform2fv;
extern PFNDOLUNIFORM2IPROC dolUniform2i;
extern PFNDOLUNIFORM2IVPROC dolUniform2iv;
extern PFNDOLUNIFORM3FPROC dolUniform3f;
extern PFNDOLUNIFORM3FVPROC dolUniform3fv;
extern PFNDOLUNIFORM3IPROC dolUniform3i;
extern PFNDOLUNIFORM3IVPROC dolUniform3iv;
extern PFNDOLUNIFORM4FPROC dolUniform4f;
extern PFNDOLUNIFORM4FVPROC dolUniform4fv;
extern PFNDOLUNIFORM4IPROC dolUniform4i;
extern PFNDOLUNIFORM4IVPROC dolUniform4iv;
extern PFNDOLUNIFORMMATRIX2FVPROC dolUniformMatrix2fv;
extern PFNDOLUNIFORMMATRIX3FVPROC dolUniformMatrix3fv;
extern PFNDOLUNIFORMMATRIX4FVPROC dolUniformMatrix4fv;
extern PFNDOLUSEPROGRAMPROC dolUseProgram;
extern PFNDOLVALIDATEPROGRAMPROC dolValidateProgram;
extern PFNDOLVERTEXATTRIB1DPROC dolVertexAttrib1d;
extern PFNDOLVERTEXATTRIB1DVPROC dolVertexAttrib1dv;
extern PFNDOLVERTEXATTRIB1FPROC dolVertexAttrib1f;
extern PFNDOLVERTEXATTRIB1FVPROC dolVertexAttrib1fv;
extern PFNDOLVERTEXATTRIB1SPROC dolVertexAttrib1s;
extern PFNDOLVERTEXATTRIB1SVPROC dolVertexAttrib1sv;
extern PFNDOLVERTEXATTRIB2DPROC dolVertexAttrib2d;
extern PFNDOLVERTEXATTRIB2DVPROC dolVertexAttrib2dv;
extern PFNDOLVERTEXATTRIB2FPROC dolVertexAttrib2f;
extern PFNDOLVERTEXATTRIB2FVPROC dolVertexAttrib2fv;
extern PFNDOLVERTEXATTRIB2SPROC dolVertexAttrib2s;
extern PFNDOLVERTEXATTRIB2SVPROC dolVertexAttrib2sv;
extern PFNDOLVERTEXATTRIB3DPROC dolVertexAttrib3d;
extern PFNDOLVERTEXATTRIB3DVPROC dolVertexAttrib3dv;
extern PFNDOLVERTEXATTRIB3FPROC dolVertexAttrib3f;
extern PFNDOLVERTEXATTRIB3FVPROC dolVertexAttrib3fv;
extern PFNDOLVERTEXATTRIB3SPROC dolVertexAttrib3s;
extern PFNDOLVERTEXATTRIB3SVPROC dolVertexAttrib3sv;
extern PFNDOLVERTEXATTRIB4NBVPROC dolVertexAttrib4Nbv;
extern PFNDOLVERTEXATTRIB4NIVPROC dolVertexAttrib4Niv;
extern PFNDOLVERTEXATTRIB4NSVPROC dolVertexAttrib4Nsv;
extern PFNDOLVERTEXATTRIB4NUBPROC dolVertexAttrib4Nub;
extern PFNDOLVERTEXATTRIB4NUBVPROC dolVertexAttrib4Nubv;
extern PFNDOLVERTEXATTRIB4NUIVPROC dolVertexAttrib4Nuiv;
extern PFNDOLVERTEXATTRIB4NUSVPROC dolVertexAttrib4Nusv;
extern PFNDOLVERTEXATTRIB4BVPROC dolVertexAttrib4bv;
extern PFNDOLVERTEXATTRIB4DPROC dolVertexAttrib4d;
extern PFNDOLVERTEXATTRIB4DVPROC dolVertexAttrib4dv;
extern PFNDOLVERTEXATTRIB4FPROC dolVertexAttrib4f;
extern PFNDOLVERTEXATTRIB4FVPROC dolVertexAttrib4fv;
extern PFNDOLVERTEXATTRIB4IVPROC dolVertexAttrib4iv;
extern PFNDOLVERTEXATTRIB4SPROC dolVertexAttrib4s;
extern PFNDOLVERTEXATTRIB4SVPROC dolVertexAttrib4sv;
extern PFNDOLVERTEXATTRIB4UBVPROC dolVertexAttrib4ubv;
extern PFNDOLVERTEXATTRIB4UIVPROC dolVertexAttrib4uiv;
extern PFNDOLVERTEXATTRIB4USVPROC dolVertexAttrib4usv;
extern PFNDOLVERTEXATTRIBPOINTERPROC dolVertexAttribPointer;

#define glAttachShader dolAttachShader
#define glBindAttribLocation dolBindAttribLocation
#define glBlendEquationSeparate dolBlendEquationSeparate
#define glCompileShader dolCompileShader
#define glCreateProgram dolCreateProgram
#define glCreateShader dolCreateShader
#define glDeleteProgram dolDeleteProgram
#define glDeleteShader dolDeleteShader
#define glDetachShader dolDetachShader
#define glDisableVertexAttribArray dolDisableVertexAttribArray
#define glDrawBuffers dolDrawBuffers
#define glEnableVertexAttribArray dolEnableVertexAttribArray
#define glGetActiveAttrib dolGetActiveAttrib
#define glGetActiveUniform dolGetActiveUniform
#define glGetAttachedShaders dolGetAttachedShaders
#define glGetAttribLocation dolGetAttribLocation
#define glGetProgramInfoLog dolGetProgramInfoLog
#define glGetProgramiv dolGetProgramiv
#define glGetShaderInfoLog dolGetShaderInfoLog
#define glGetShaderSource dolGetShaderSource
#define glGetShaderiv dolGetShaderiv
#define glGetUniformLocation dolGetUniformLocation
#define glGetUniformfv dolGetUniformfv
#define glGetUniformiv dolGetUniformiv
#define glGetVertexAttribPointerv dolGetVertexAttribPointerv
#define glGetVertexAttribdv dolGetVertexAttribdv
#define glGetVertexAttribfv dolGetVertexAttribfv
#define glGetVertexAttribiv dolGetVertexAttribiv
#define glIsProgram dolIsProgram
#define glIsShader dolIsShader
#define glLinkProgram dolLinkProgram
#define glShaderSource dolShaderSource
#define glStencilFuncSeparate dolStencilFuncSeparate
#define glStencilMaskSeparate dolStencilMaskSeparate
#define glStencilOpSeparate dolStencilOpSeparate
#define glUniform1f dolUniform1f
#define glUniform1fv dolUniform1fv
#define glUniform1i dolUniform1i
#define glUniform1iv dolUniform1iv
#define glUniform2f dolUniform2f
#define glUniform2fv dolUniform2fv
#define glUniform2i dolUniform2i
#define glUniform2iv dolUniform2iv
#define glUniform3f dolUniform3f
#define glUniform3fv dolUniform3fv
#define glUniform3i dolUniform3i
#define glUniform3iv dolUniform3iv
#define glUniform4f dolUniform4f
#define glUniform4fv dolUniform4fv
#define glUniform4i dolUniform4i
#define glUniform4iv dolUniform4iv
#define glUniformMatrix2fv dolUniformMatrix2fv
#define glUniformMatrix3fv dolUniformMatrix3fv
#define glUniformMatrix4fv dolUniformMatrix4fv
#define glUseProgram dolUseProgram
#define glValidateProgram dolValidateProgram
#define glVertexAttrib1d dolVertexAttrib1d
#define glVertexAttrib1dv dolVertexAttrib1dv
#define glVertexAttrib1f dolVertexAttrib1f
#define glVertexAttrib1fv dolVertexAttrib1fv
#define glVertexAttrib1s dolVertexAttrib1s
#define glVertexAttrib1sv dolVertexAttrib1sv
#define glVertexAttrib2d dolVertexAttrib2d
#define glVertexAttrib2dv dolVertexAttrib2dv
#define glVertexAttrib2f dolVertexAttrib2f
#define glVertexAttrib2fv dolVertexAttrib2fv
#define glVertexAttrib2s dolVertexAttrib2s
#define glVertexAttrib2sv dolVertexAttrib2sv
#define glVertexAttrib3d dolVertexAttrib3d
#define glVertexAttrib3dv dolVertexAttrib3dv
#define glVertexAttrib3f dolVertexAttrib3f
#define glVertexAttrib3fv dolVertexAttrib3fv
#define glVertexAttrib3s dolVertexAttrib3s
#define glVertexAttrib3sv dolVertexAttrib3sv
#define glVertexAttrib4Nbv dolVertexAttrib4Nbv
#define glVertexAttrib4Niv dolVertexAttrib4Niv
#define glVertexAttrib4Nsv dolVertexAttrib4Nsv
#define glVertexAttrib4Nub dolVertexAttrib4Nub
#define glVertexAttrib4Nubv dolVertexAttrib4Nubv
#define glVertexAttrib4Nuiv dolVertexAttrib4Nuiv
#define glVertexAttrib4Nusv dolVertexAttrib4Nusv
#define glVertexAttrib4bv dolVertexAttrib4bv
#define glVertexAttrib4d dolVertexAttrib4d
#define glVertexAttrib4dv dolVertexAttrib4dv
#define glVertexAttrib4f dolVertexAttrib4f
#define glVertexAttrib4fv dolVertexAttrib4fv
#define glVertexAttrib4iv dolVertexAttrib4iv
#define glVertexAttrib4s dolVertexAttrib4s
#define glVertexAttrib4sv dolVertexAttrib4sv
#define glVertexAttrib4ubv dolVertexAttrib4ubv
#define glVertexAttrib4uiv dolVertexAttrib4uiv
#define glVertexAttrib4usv dolVertexAttrib4usv
#define glVertexAttribPointer dolVertexAttribPointer
