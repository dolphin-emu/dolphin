////////////////////////////////////////////////////////////
//
// SFML - Simple and Fast Multimedia Library
// Copyright (C) 2007-2016 Laurent Gomila (laurent@sfml-dev.org)
//
// This software is provided 'as-is', without any express or implied warranty.
// In no event will the authors be held liable for any damages arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it freely,
// subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented;
//    you must not claim that you wrote the original software.
//    If you use this software in a product, an acknowledgment
//    in the product documentation would be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such,
//    and must not be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source distribution.
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Graphics/GLLoader.hpp>
#include <SFML/Window/Context.hpp>

static sf::GlFunctionPointer glLoaderGetProcAddress(const char* name)
{
    return sf::Context::getFunction(name);
}

int sfogl_ext_SGIS_texture_edge_clamp = sfogl_LOAD_FAILED;
int sfogl_ext_EXT_texture_edge_clamp = sfogl_LOAD_FAILED;
int sfogl_ext_EXT_blend_minmax = sfogl_LOAD_FAILED;
int sfogl_ext_EXT_blend_subtract = sfogl_LOAD_FAILED;
int sfogl_ext_ARB_multitexture = sfogl_LOAD_FAILED;
int sfogl_ext_EXT_blend_func_separate = sfogl_LOAD_FAILED;
int sfogl_ext_ARB_shading_language_100 = sfogl_LOAD_FAILED;
int sfogl_ext_ARB_shader_objects = sfogl_LOAD_FAILED;
int sfogl_ext_ARB_vertex_shader = sfogl_LOAD_FAILED;
int sfogl_ext_ARB_fragment_shader = sfogl_LOAD_FAILED;
int sfogl_ext_ARB_texture_non_power_of_two = sfogl_LOAD_FAILED;
int sfogl_ext_EXT_blend_equation_separate = sfogl_LOAD_FAILED;
int sfogl_ext_EXT_texture_sRGB = sfogl_LOAD_FAILED;
int sfogl_ext_EXT_framebuffer_object = sfogl_LOAD_FAILED;
int sfogl_ext_ARB_geometry_shader4 = sfogl_LOAD_FAILED;

void (GL_FUNCPTR *sf_ptrc_glBlendEquationEXT)(GLenum) = NULL;

static int Load_EXT_blend_minmax()
{
    int numFailed = 0;

    sf_ptrc_glBlendEquationEXT = reinterpret_cast<void (GL_FUNCPTR *)(GLenum)>(glLoaderGetProcAddress("glBlendEquationEXT"));
    if (!sf_ptrc_glBlendEquationEXT)
        numFailed++;

    return numFailed;
}

void (GL_FUNCPTR *sf_ptrc_glActiveTextureARB)(GLenum) = NULL;
void (GL_FUNCPTR *sf_ptrc_glClientActiveTextureARB)(GLenum) = NULL;
void (GL_FUNCPTR *sf_ptrc_glMultiTexCoord1dARB)(GLenum, GLdouble) = NULL;
void (GL_FUNCPTR *sf_ptrc_glMultiTexCoord1dvARB)(GLenum, const GLdouble*) = NULL;
void (GL_FUNCPTR *sf_ptrc_glMultiTexCoord1fARB)(GLenum, GLfloat) = NULL;
void (GL_FUNCPTR *sf_ptrc_glMultiTexCoord1fvARB)(GLenum, const GLfloat*) = NULL;
void (GL_FUNCPTR *sf_ptrc_glMultiTexCoord1iARB)(GLenum, GLint) = NULL;
void (GL_FUNCPTR *sf_ptrc_glMultiTexCoord1ivARB)(GLenum, const GLint*) = NULL;
void (GL_FUNCPTR *sf_ptrc_glMultiTexCoord1sARB)(GLenum, GLshort) = NULL;
void (GL_FUNCPTR *sf_ptrc_glMultiTexCoord1svARB)(GLenum, const GLshort*) = NULL;
void (GL_FUNCPTR *sf_ptrc_glMultiTexCoord2dARB)(GLenum, GLdouble, GLdouble) = NULL;
void (GL_FUNCPTR *sf_ptrc_glMultiTexCoord2dvARB)(GLenum, const GLdouble*) = NULL;
void (GL_FUNCPTR *sf_ptrc_glMultiTexCoord2fARB)(GLenum, GLfloat, GLfloat) = NULL;
void (GL_FUNCPTR *sf_ptrc_glMultiTexCoord2fvARB)(GLenum, const GLfloat*) = NULL;
void (GL_FUNCPTR *sf_ptrc_glMultiTexCoord2iARB)(GLenum, GLint, GLint) = NULL;
void (GL_FUNCPTR *sf_ptrc_glMultiTexCoord2ivARB)(GLenum, const GLint*) = NULL;
void (GL_FUNCPTR *sf_ptrc_glMultiTexCoord2sARB)(GLenum, GLshort, GLshort) = NULL;
void (GL_FUNCPTR *sf_ptrc_glMultiTexCoord2svARB)(GLenum, const GLshort*) = NULL;
void (GL_FUNCPTR *sf_ptrc_glMultiTexCoord3dARB)(GLenum, GLdouble, GLdouble, GLdouble) = NULL;
void (GL_FUNCPTR *sf_ptrc_glMultiTexCoord3dvARB)(GLenum, const GLdouble*) = NULL;
void (GL_FUNCPTR *sf_ptrc_glMultiTexCoord3fARB)(GLenum, GLfloat, GLfloat, GLfloat) = NULL;
void (GL_FUNCPTR *sf_ptrc_glMultiTexCoord3fvARB)(GLenum, const GLfloat*) = NULL;
void (GL_FUNCPTR *sf_ptrc_glMultiTexCoord3iARB)(GLenum, GLint, GLint, GLint) = NULL;
void (GL_FUNCPTR *sf_ptrc_glMultiTexCoord3ivARB)(GLenum, const GLint*) = NULL;
void (GL_FUNCPTR *sf_ptrc_glMultiTexCoord3sARB)(GLenum, GLshort, GLshort, GLshort) = NULL;
void (GL_FUNCPTR *sf_ptrc_glMultiTexCoord3svARB)(GLenum, const GLshort*) = NULL;
void (GL_FUNCPTR *sf_ptrc_glMultiTexCoord4dARB)(GLenum, GLdouble, GLdouble, GLdouble, GLdouble) = NULL;
void (GL_FUNCPTR *sf_ptrc_glMultiTexCoord4dvARB)(GLenum, const GLdouble*) = NULL;
void (GL_FUNCPTR *sf_ptrc_glMultiTexCoord4fARB)(GLenum, GLfloat, GLfloat, GLfloat, GLfloat) = NULL;
void (GL_FUNCPTR *sf_ptrc_glMultiTexCoord4fvARB)(GLenum, const GLfloat*) = NULL;
void (GL_FUNCPTR *sf_ptrc_glMultiTexCoord4iARB)(GLenum, GLint, GLint, GLint, GLint) = NULL;
void (GL_FUNCPTR *sf_ptrc_glMultiTexCoord4ivARB)(GLenum, const GLint*) = NULL;
void (GL_FUNCPTR *sf_ptrc_glMultiTexCoord4sARB)(GLenum, GLshort, GLshort, GLshort, GLshort) = NULL;
void (GL_FUNCPTR *sf_ptrc_glMultiTexCoord4svARB)(GLenum, const GLshort*) = NULL;

static int Load_ARB_multitexture()
{
    int numFailed = 0;

    sf_ptrc_glActiveTextureARB = reinterpret_cast<void (GL_FUNCPTR *)(GLenum)>(glLoaderGetProcAddress("glActiveTextureARB"));
    if (!sf_ptrc_glActiveTextureARB)
        numFailed++;

    sf_ptrc_glClientActiveTextureARB = reinterpret_cast<void (GL_FUNCPTR *)(GLenum)>(glLoaderGetProcAddress("glClientActiveTextureARB"));
    if (!sf_ptrc_glClientActiveTextureARB)
        numFailed++;

    sf_ptrc_glMultiTexCoord1dARB = reinterpret_cast<void (GL_FUNCPTR *)(GLenum, GLdouble)>(glLoaderGetProcAddress("glMultiTexCoord1dARB"));
    if (!sf_ptrc_glMultiTexCoord1dARB)
        numFailed++;

    sf_ptrc_glMultiTexCoord1dvARB = reinterpret_cast<void (GL_FUNCPTR *)(GLenum, const GLdouble*)>(glLoaderGetProcAddress("glMultiTexCoord1dvARB"));
    if (!sf_ptrc_glMultiTexCoord1dvARB)
        numFailed++;

    sf_ptrc_glMultiTexCoord1fARB = reinterpret_cast<void (GL_FUNCPTR *)(GLenum, GLfloat)>(glLoaderGetProcAddress("glMultiTexCoord1fARB"));
    if (!sf_ptrc_glMultiTexCoord1fARB)
        numFailed++;

    sf_ptrc_glMultiTexCoord1fvARB = reinterpret_cast<void (GL_FUNCPTR *)(GLenum, const GLfloat*)>(glLoaderGetProcAddress("glMultiTexCoord1fvARB"));
    if (!sf_ptrc_glMultiTexCoord1fvARB)
        numFailed++;

    sf_ptrc_glMultiTexCoord1iARB = reinterpret_cast<void (GL_FUNCPTR *)(GLenum, GLint)>(glLoaderGetProcAddress("glMultiTexCoord1iARB"));
    if (!sf_ptrc_glMultiTexCoord1iARB)
        numFailed++;

    sf_ptrc_glMultiTexCoord1ivARB = reinterpret_cast<void (GL_FUNCPTR *)(GLenum, const GLint*)>(glLoaderGetProcAddress("glMultiTexCoord1ivARB"));
    if (!sf_ptrc_glMultiTexCoord1ivARB)
        numFailed++;

    sf_ptrc_glMultiTexCoord1sARB = reinterpret_cast<void (GL_FUNCPTR *)(GLenum, GLshort)>(glLoaderGetProcAddress("glMultiTexCoord1sARB"));
    if (!sf_ptrc_glMultiTexCoord1sARB)
        numFailed++;

    sf_ptrc_glMultiTexCoord1svARB = reinterpret_cast<void (GL_FUNCPTR *)(GLenum, const GLshort*)>(glLoaderGetProcAddress("glMultiTexCoord1svARB"));
    if (!sf_ptrc_glMultiTexCoord1svARB)
        numFailed++;

    sf_ptrc_glMultiTexCoord2dARB = reinterpret_cast<void (GL_FUNCPTR *)(GLenum, GLdouble, GLdouble)>(glLoaderGetProcAddress("glMultiTexCoord2dARB"));
    if (!sf_ptrc_glMultiTexCoord2dARB)
        numFailed++;

    sf_ptrc_glMultiTexCoord2dvARB = reinterpret_cast<void (GL_FUNCPTR *)(GLenum, const GLdouble*)>(glLoaderGetProcAddress("glMultiTexCoord2dvARB"));
    if (!sf_ptrc_glMultiTexCoord2dvARB)
        numFailed++;

    sf_ptrc_glMultiTexCoord2fARB = reinterpret_cast<void (GL_FUNCPTR *)(GLenum, GLfloat, GLfloat)>(glLoaderGetProcAddress("glMultiTexCoord2fARB"));
    if (!sf_ptrc_glMultiTexCoord2fARB)
        numFailed++;

    sf_ptrc_glMultiTexCoord2fvARB = reinterpret_cast<void (GL_FUNCPTR *)(GLenum, const GLfloat*)>(glLoaderGetProcAddress("glMultiTexCoord2fvARB"));
    if (!sf_ptrc_glMultiTexCoord2fvARB)
        numFailed++;

    sf_ptrc_glMultiTexCoord2iARB = reinterpret_cast<void (GL_FUNCPTR *)(GLenum, GLint, GLint)>(glLoaderGetProcAddress("glMultiTexCoord2iARB"));
    if (!sf_ptrc_glMultiTexCoord2iARB)
        numFailed++;

    sf_ptrc_glMultiTexCoord2ivARB = reinterpret_cast<void (GL_FUNCPTR *)(GLenum, const GLint*)>(glLoaderGetProcAddress("glMultiTexCoord2ivARB"));
    if (!sf_ptrc_glMultiTexCoord2ivARB)
        numFailed++;

    sf_ptrc_glMultiTexCoord2sARB = reinterpret_cast<void (GL_FUNCPTR *)(GLenum, GLshort, GLshort)>(glLoaderGetProcAddress("glMultiTexCoord2sARB"));
    if (!sf_ptrc_glMultiTexCoord2sARB)
        numFailed++;

    sf_ptrc_glMultiTexCoord2svARB = reinterpret_cast<void (GL_FUNCPTR *)(GLenum, const GLshort*)>(glLoaderGetProcAddress("glMultiTexCoord2svARB"));
    if (!sf_ptrc_glMultiTexCoord2svARB)
        numFailed++;

    sf_ptrc_glMultiTexCoord3dARB = reinterpret_cast<void (GL_FUNCPTR *)(GLenum, GLdouble, GLdouble, GLdouble)>(glLoaderGetProcAddress("glMultiTexCoord3dARB"));
    if (!sf_ptrc_glMultiTexCoord3dARB)
        numFailed++;

    sf_ptrc_glMultiTexCoord3dvARB = reinterpret_cast<void (GL_FUNCPTR *)(GLenum, const GLdouble*)>(glLoaderGetProcAddress("glMultiTexCoord3dvARB"));
    if (!sf_ptrc_glMultiTexCoord3dvARB)
        numFailed++;

    sf_ptrc_glMultiTexCoord3fARB = reinterpret_cast<void (GL_FUNCPTR *)(GLenum, GLfloat, GLfloat, GLfloat)>(glLoaderGetProcAddress("glMultiTexCoord3fARB"));
    if (!sf_ptrc_glMultiTexCoord3fARB)
        numFailed++;

    sf_ptrc_glMultiTexCoord3fvARB = reinterpret_cast<void (GL_FUNCPTR *)(GLenum, const GLfloat*)>(glLoaderGetProcAddress("glMultiTexCoord3fvARB"));
    if (!sf_ptrc_glMultiTexCoord3fvARB)
        numFailed++;

    sf_ptrc_glMultiTexCoord3iARB = reinterpret_cast<void (GL_FUNCPTR *)(GLenum, GLint, GLint, GLint)>(glLoaderGetProcAddress("glMultiTexCoord3iARB"));
    if (!sf_ptrc_glMultiTexCoord3iARB)
        numFailed++;

    sf_ptrc_glMultiTexCoord3ivARB = reinterpret_cast<void (GL_FUNCPTR *)(GLenum, const GLint*)>(glLoaderGetProcAddress("glMultiTexCoord3ivARB"));
    if (!sf_ptrc_glMultiTexCoord3ivARB)
        numFailed++;

    sf_ptrc_glMultiTexCoord3sARB = reinterpret_cast<void (GL_FUNCPTR *)(GLenum, GLshort, GLshort, GLshort)>(glLoaderGetProcAddress("glMultiTexCoord3sARB"));
    if (!sf_ptrc_glMultiTexCoord3sARB)
        numFailed++;

    sf_ptrc_glMultiTexCoord3svARB = reinterpret_cast<void (GL_FUNCPTR *)(GLenum, const GLshort*)>(glLoaderGetProcAddress("glMultiTexCoord3svARB"));
    if (!sf_ptrc_glMultiTexCoord3svARB)
        numFailed++;

    sf_ptrc_glMultiTexCoord4dARB = reinterpret_cast<void (GL_FUNCPTR *)(GLenum, GLdouble, GLdouble, GLdouble, GLdouble)>(glLoaderGetProcAddress("glMultiTexCoord4dARB"));
    if (!sf_ptrc_glMultiTexCoord4dARB)
        numFailed++;

    sf_ptrc_glMultiTexCoord4dvARB = reinterpret_cast<void (GL_FUNCPTR *)(GLenum, const GLdouble*)>(glLoaderGetProcAddress("glMultiTexCoord4dvARB"));
    if (!sf_ptrc_glMultiTexCoord4dvARB)
        numFailed++;

    sf_ptrc_glMultiTexCoord4fARB = reinterpret_cast<void (GL_FUNCPTR *)(GLenum, GLfloat, GLfloat, GLfloat, GLfloat)>(glLoaderGetProcAddress("glMultiTexCoord4fARB"));
    if (!sf_ptrc_glMultiTexCoord4fARB)
        numFailed++;

    sf_ptrc_glMultiTexCoord4fvARB = reinterpret_cast<void (GL_FUNCPTR *)(GLenum, const GLfloat*)>(glLoaderGetProcAddress("glMultiTexCoord4fvARB"));
    if (!sf_ptrc_glMultiTexCoord4fvARB)
        numFailed++;

    sf_ptrc_glMultiTexCoord4iARB = reinterpret_cast<void (GL_FUNCPTR *)(GLenum, GLint, GLint, GLint, GLint)>(glLoaderGetProcAddress("glMultiTexCoord4iARB"));
    if (!sf_ptrc_glMultiTexCoord4iARB)
        numFailed++;

    sf_ptrc_glMultiTexCoord4ivARB = reinterpret_cast<void (GL_FUNCPTR *)(GLenum, const GLint*)>(glLoaderGetProcAddress("glMultiTexCoord4ivARB"));
    if (!sf_ptrc_glMultiTexCoord4ivARB)
        numFailed++;

    sf_ptrc_glMultiTexCoord4sARB = reinterpret_cast<void (GL_FUNCPTR *)(GLenum, GLshort, GLshort, GLshort, GLshort)>(glLoaderGetProcAddress("glMultiTexCoord4sARB"));
    if (!sf_ptrc_glMultiTexCoord4sARB)
        numFailed++;

    sf_ptrc_glMultiTexCoord4svARB = reinterpret_cast<void (GL_FUNCPTR *)(GLenum, const GLshort*)>(glLoaderGetProcAddress("glMultiTexCoord4svARB"));
    if (!sf_ptrc_glMultiTexCoord4svARB)
        numFailed++;

    return numFailed;
}

void (GL_FUNCPTR *sf_ptrc_glBlendFuncSeparateEXT)(GLenum, GLenum, GLenum, GLenum) = NULL;

static int Load_EXT_blend_func_separate()
{
    int numFailed = 0;

    sf_ptrc_glBlendFuncSeparateEXT = reinterpret_cast<void (GL_FUNCPTR *)(GLenum, GLenum, GLenum, GLenum)>(glLoaderGetProcAddress("glBlendFuncSeparateEXT"));
    if (!sf_ptrc_glBlendFuncSeparateEXT)
        numFailed++;

    return numFailed;
}

void (GL_FUNCPTR *sf_ptrc_glAttachObjectARB)(GLhandleARB, GLhandleARB) = NULL;
void (GL_FUNCPTR *sf_ptrc_glCompileShaderARB)(GLhandleARB) = NULL;
GLhandleARB (GL_FUNCPTR *sf_ptrc_glCreateProgramObjectARB)() = NULL;
GLhandleARB (GL_FUNCPTR *sf_ptrc_glCreateShaderObjectARB)(GLenum) = NULL;
void (GL_FUNCPTR *sf_ptrc_glDeleteObjectARB)(GLhandleARB) = NULL;
void (GL_FUNCPTR *sf_ptrc_glDetachObjectARB)(GLhandleARB, GLhandleARB) = NULL;
void (GL_FUNCPTR *sf_ptrc_glGetActiveUniformARB)(GLhandleARB, GLuint, GLsizei, GLsizei*, GLint*, GLenum*, GLcharARB*) = NULL;
void (GL_FUNCPTR *sf_ptrc_glGetAttachedObjectsARB)(GLhandleARB, GLsizei, GLsizei*, GLhandleARB*) = NULL;
GLhandleARB (GL_FUNCPTR *sf_ptrc_glGetHandleARB)(GLenum) = NULL;
void (GL_FUNCPTR *sf_ptrc_glGetInfoLogARB)(GLhandleARB, GLsizei, GLsizei*, GLcharARB*) = NULL;
void (GL_FUNCPTR *sf_ptrc_glGetObjectParameterfvARB)(GLhandleARB, GLenum, GLfloat*) = NULL;
void (GL_FUNCPTR *sf_ptrc_glGetObjectParameterivARB)(GLhandleARB, GLenum, GLint*) = NULL;
void (GL_FUNCPTR *sf_ptrc_glGetShaderSourceARB)(GLhandleARB, GLsizei, GLsizei*, GLcharARB*) = NULL;
GLint (GL_FUNCPTR *sf_ptrc_glGetUniformLocationARB)(GLhandleARB, const GLcharARB*) = NULL;
void (GL_FUNCPTR *sf_ptrc_glGetUniformfvARB)(GLhandleARB, GLint, GLfloat*) = NULL;
void (GL_FUNCPTR *sf_ptrc_glGetUniformivARB)(GLhandleARB, GLint, GLint*) = NULL;
void (GL_FUNCPTR *sf_ptrc_glLinkProgramARB)(GLhandleARB) = NULL;
void (GL_FUNCPTR *sf_ptrc_glShaderSourceARB)(GLhandleARB, GLsizei, const GLcharARB**, const GLint*) = NULL;
void (GL_FUNCPTR *sf_ptrc_glUniform1fARB)(GLint, GLfloat) = NULL;
void (GL_FUNCPTR *sf_ptrc_glUniform1fvARB)(GLint, GLsizei, const GLfloat*) = NULL;
void (GL_FUNCPTR *sf_ptrc_glUniform1iARB)(GLint, GLint) = NULL;
void (GL_FUNCPTR *sf_ptrc_glUniform1ivARB)(GLint, GLsizei, const GLint*) = NULL;
void (GL_FUNCPTR *sf_ptrc_glUniform2fARB)(GLint, GLfloat, GLfloat) = NULL;
void (GL_FUNCPTR *sf_ptrc_glUniform2fvARB)(GLint, GLsizei, const GLfloat*) = NULL;
void (GL_FUNCPTR *sf_ptrc_glUniform2iARB)(GLint, GLint, GLint) = NULL;
void (GL_FUNCPTR *sf_ptrc_glUniform2ivARB)(GLint, GLsizei, const GLint*) = NULL;
void (GL_FUNCPTR *sf_ptrc_glUniform3fARB)(GLint, GLfloat, GLfloat, GLfloat) = NULL;
void (GL_FUNCPTR *sf_ptrc_glUniform3fvARB)(GLint, GLsizei, const GLfloat*) = NULL;
void (GL_FUNCPTR *sf_ptrc_glUniform3iARB)(GLint, GLint, GLint, GLint) = NULL;
void (GL_FUNCPTR *sf_ptrc_glUniform3ivARB)(GLint, GLsizei, const GLint*) = NULL;
void (GL_FUNCPTR *sf_ptrc_glUniform4fARB)(GLint, GLfloat, GLfloat, GLfloat, GLfloat) = NULL;
void (GL_FUNCPTR *sf_ptrc_glUniform4fvARB)(GLint, GLsizei, const GLfloat*) = NULL;
void (GL_FUNCPTR *sf_ptrc_glUniform4iARB)(GLint, GLint, GLint, GLint, GLint) = NULL;
void (GL_FUNCPTR *sf_ptrc_glUniform4ivARB)(GLint, GLsizei, const GLint*) = NULL;
void (GL_FUNCPTR *sf_ptrc_glUniformMatrix2fvARB)(GLint, GLsizei, GLboolean, const GLfloat*) = NULL;
void (GL_FUNCPTR *sf_ptrc_glUniformMatrix3fvARB)(GLint, GLsizei, GLboolean, const GLfloat*) = NULL;
void (GL_FUNCPTR *sf_ptrc_glUniformMatrix4fvARB)(GLint, GLsizei, GLboolean, const GLfloat*) = NULL;
void (GL_FUNCPTR *sf_ptrc_glUseProgramObjectARB)(GLhandleARB) = NULL;
void (GL_FUNCPTR *sf_ptrc_glValidateProgramARB)(GLhandleARB) = NULL;

static int Load_ARB_shader_objects()
{
    int numFailed = 0;

    sf_ptrc_glAttachObjectARB = reinterpret_cast<void (GL_FUNCPTR *)(GLhandleARB, GLhandleARB)>(glLoaderGetProcAddress("glAttachObjectARB"));
    if (!sf_ptrc_glAttachObjectARB)
        numFailed++;

    sf_ptrc_glCompileShaderARB = reinterpret_cast<void (GL_FUNCPTR *)(GLhandleARB)>(glLoaderGetProcAddress("glCompileShaderARB"));
    if (!sf_ptrc_glCompileShaderARB)
        numFailed++;

    sf_ptrc_glCreateProgramObjectARB = reinterpret_cast<GLhandleARB (GL_FUNCPTR *)()>(glLoaderGetProcAddress("glCreateProgramObjectARB"));
    if (!sf_ptrc_glCreateProgramObjectARB)
        numFailed++;

    sf_ptrc_glCreateShaderObjectARB = reinterpret_cast<GLhandleARB (GL_FUNCPTR *)(GLenum)>(glLoaderGetProcAddress("glCreateShaderObjectARB"));
    if (!sf_ptrc_glCreateShaderObjectARB)
        numFailed++;

    sf_ptrc_glDeleteObjectARB = reinterpret_cast<void (GL_FUNCPTR *)(GLhandleARB)>(glLoaderGetProcAddress("glDeleteObjectARB"));
    if (!sf_ptrc_glDeleteObjectARB)
        numFailed++;

    sf_ptrc_glDetachObjectARB = reinterpret_cast<void (GL_FUNCPTR *)(GLhandleARB, GLhandleARB)>(glLoaderGetProcAddress("glDetachObjectARB"));
    if (!sf_ptrc_glDetachObjectARB)
        numFailed++;

    sf_ptrc_glGetActiveUniformARB = reinterpret_cast<void (GL_FUNCPTR *)(GLhandleARB, GLuint, GLsizei, GLsizei*, GLint*, GLenum*, GLcharARB*)>(glLoaderGetProcAddress("glGetActiveUniformARB"));
    if (!sf_ptrc_glGetActiveUniformARB)
        numFailed++;

    sf_ptrc_glGetAttachedObjectsARB = reinterpret_cast<void (GL_FUNCPTR *)(GLhandleARB, GLsizei, GLsizei*, GLhandleARB*)>(glLoaderGetProcAddress("glGetAttachedObjectsARB"));
    if (!sf_ptrc_glGetAttachedObjectsARB)
        numFailed++;

    sf_ptrc_glGetHandleARB = reinterpret_cast<GLhandleARB (GL_FUNCPTR *)(GLenum)>(glLoaderGetProcAddress("glGetHandleARB"));
    if (!sf_ptrc_glGetHandleARB)
        numFailed++;

    sf_ptrc_glGetInfoLogARB = reinterpret_cast<void (GL_FUNCPTR *)(GLhandleARB, GLsizei, GLsizei*, GLcharARB*)>(glLoaderGetProcAddress("glGetInfoLogARB"));
    if (!sf_ptrc_glGetInfoLogARB)
        numFailed++;

    sf_ptrc_glGetObjectParameterfvARB = reinterpret_cast<void (GL_FUNCPTR *)(GLhandleARB, GLenum, GLfloat*)>(glLoaderGetProcAddress("glGetObjectParameterfvARB"));
    if (!sf_ptrc_glGetObjectParameterfvARB)
        numFailed++;

    sf_ptrc_glGetObjectParameterivARB = reinterpret_cast<void (GL_FUNCPTR *)(GLhandleARB, GLenum, GLint*)>(glLoaderGetProcAddress("glGetObjectParameterivARB"));
    if (!sf_ptrc_glGetObjectParameterivARB)
        numFailed++;

    sf_ptrc_glGetShaderSourceARB = reinterpret_cast<void (GL_FUNCPTR *)(GLhandleARB, GLsizei, GLsizei*, GLcharARB*)>(glLoaderGetProcAddress("glGetShaderSourceARB"));
    if (!sf_ptrc_glGetShaderSourceARB)
        numFailed++;

    sf_ptrc_glGetUniformLocationARB = reinterpret_cast<GLint (GL_FUNCPTR *)(GLhandleARB, const GLcharARB*)>(glLoaderGetProcAddress("glGetUniformLocationARB"));
    if (!sf_ptrc_glGetUniformLocationARB)
        numFailed++;

    sf_ptrc_glGetUniformfvARB = reinterpret_cast<void (GL_FUNCPTR *)(GLhandleARB, GLint, GLfloat*)>(glLoaderGetProcAddress("glGetUniformfvARB"));
    if (!sf_ptrc_glGetUniformfvARB)
        numFailed++;

    sf_ptrc_glGetUniformivARB = reinterpret_cast<void (GL_FUNCPTR *)(GLhandleARB, GLint, GLint*)>(glLoaderGetProcAddress("glGetUniformivARB"));
    if (!sf_ptrc_glGetUniformivARB)
        numFailed++;

    sf_ptrc_glLinkProgramARB = reinterpret_cast<void (GL_FUNCPTR *)(GLhandleARB)>(glLoaderGetProcAddress("glLinkProgramARB"));
    if (!sf_ptrc_glLinkProgramARB)
        numFailed++;

    sf_ptrc_glShaderSourceARB = reinterpret_cast<void (GL_FUNCPTR *)(GLhandleARB, GLsizei, const GLcharARB**, const GLint*)>(glLoaderGetProcAddress("glShaderSourceARB"));
    if (!sf_ptrc_glShaderSourceARB)
        numFailed++;

    sf_ptrc_glUniform1fARB = reinterpret_cast<void (GL_FUNCPTR *)(GLint, GLfloat)>(glLoaderGetProcAddress("glUniform1fARB"));
    if (!sf_ptrc_glUniform1fARB)
        numFailed++;

    sf_ptrc_glUniform1fvARB = reinterpret_cast<void (GL_FUNCPTR *)(GLint, GLsizei, const GLfloat*)>(glLoaderGetProcAddress("glUniform1fvARB"));
    if (!sf_ptrc_glUniform1fvARB)
        numFailed++;

    sf_ptrc_glUniform1iARB = reinterpret_cast<void (GL_FUNCPTR *)(GLint, GLint)>(glLoaderGetProcAddress("glUniform1iARB"));
    if (!sf_ptrc_glUniform1iARB)
        numFailed++;

    sf_ptrc_glUniform1ivARB = reinterpret_cast<void (GL_FUNCPTR *)(GLint, GLsizei, const GLint*)>(glLoaderGetProcAddress("glUniform1ivARB"));
    if (!sf_ptrc_glUniform1ivARB)
        numFailed++;

    sf_ptrc_glUniform2fARB = reinterpret_cast<void (GL_FUNCPTR *)(GLint, GLfloat, GLfloat)>(glLoaderGetProcAddress("glUniform2fARB"));
    if (!sf_ptrc_glUniform2fARB)
        numFailed++;

    sf_ptrc_glUniform2fvARB = reinterpret_cast<void (GL_FUNCPTR *)(GLint, GLsizei, const GLfloat*)>(glLoaderGetProcAddress("glUniform2fvARB"));
    if (!sf_ptrc_glUniform2fvARB)
        numFailed++;

    sf_ptrc_glUniform2iARB = reinterpret_cast<void (GL_FUNCPTR *)(GLint, GLint, GLint)>(glLoaderGetProcAddress("glUniform2iARB"));
    if (!sf_ptrc_glUniform2iARB)
        numFailed++;

    sf_ptrc_glUniform2ivARB = reinterpret_cast<void (GL_FUNCPTR *)(GLint, GLsizei, const GLint*)>(glLoaderGetProcAddress("glUniform2ivARB"));
    if (!sf_ptrc_glUniform2ivARB)
        numFailed++;

    sf_ptrc_glUniform3fARB = reinterpret_cast<void (GL_FUNCPTR *)(GLint, GLfloat, GLfloat, GLfloat)>(glLoaderGetProcAddress("glUniform3fARB"));
    if (!sf_ptrc_glUniform3fARB)
        numFailed++;

    sf_ptrc_glUniform3fvARB = reinterpret_cast<void (GL_FUNCPTR *)(GLint, GLsizei, const GLfloat*)>(glLoaderGetProcAddress("glUniform3fvARB"));
    if (!sf_ptrc_glUniform3fvARB)
        numFailed++;

    sf_ptrc_glUniform3iARB = reinterpret_cast<void (GL_FUNCPTR *)(GLint, GLint, GLint, GLint)>(glLoaderGetProcAddress("glUniform3iARB"));
    if (!sf_ptrc_glUniform3iARB)
        numFailed++;

    sf_ptrc_glUniform3ivARB = reinterpret_cast<void (GL_FUNCPTR *)(GLint, GLsizei, const GLint*)>(glLoaderGetProcAddress("glUniform3ivARB"));
    if (!sf_ptrc_glUniform3ivARB)
        numFailed++;

    sf_ptrc_glUniform4fARB = reinterpret_cast<void (GL_FUNCPTR *)(GLint, GLfloat, GLfloat, GLfloat, GLfloat)>(glLoaderGetProcAddress("glUniform4fARB"));
    if (!sf_ptrc_glUniform4fARB)
        numFailed++;

    sf_ptrc_glUniform4fvARB = reinterpret_cast<void (GL_FUNCPTR *)(GLint, GLsizei, const GLfloat*)>(glLoaderGetProcAddress("glUniform4fvARB"));
    if (!sf_ptrc_glUniform4fvARB)
        numFailed++;

    sf_ptrc_glUniform4iARB = reinterpret_cast<void (GL_FUNCPTR *)(GLint, GLint, GLint, GLint, GLint)>(glLoaderGetProcAddress("glUniform4iARB"));
    if (!sf_ptrc_glUniform4iARB)
        numFailed++;

    sf_ptrc_glUniform4ivARB = reinterpret_cast<void (GL_FUNCPTR *)(GLint, GLsizei, const GLint*)>(glLoaderGetProcAddress("glUniform4ivARB"));
    if (!sf_ptrc_glUniform4ivARB)
        numFailed++;

    sf_ptrc_glUniformMatrix2fvARB = reinterpret_cast<void (GL_FUNCPTR *)(GLint, GLsizei, GLboolean, const GLfloat*)>(glLoaderGetProcAddress("glUniformMatrix2fvARB"));
    if (!sf_ptrc_glUniformMatrix2fvARB)
        numFailed++;

    sf_ptrc_glUniformMatrix3fvARB = reinterpret_cast<void (GL_FUNCPTR *)(GLint, GLsizei, GLboolean, const GLfloat*)>(glLoaderGetProcAddress("glUniformMatrix3fvARB"));
    if (!sf_ptrc_glUniformMatrix3fvARB)
        numFailed++;

    sf_ptrc_glUniformMatrix4fvARB = reinterpret_cast<void (GL_FUNCPTR *)(GLint, GLsizei, GLboolean, const GLfloat*)>(glLoaderGetProcAddress("glUniformMatrix4fvARB"));
    if (!sf_ptrc_glUniformMatrix4fvARB)
        numFailed++;

    sf_ptrc_glUseProgramObjectARB = reinterpret_cast<void (GL_FUNCPTR *)(GLhandleARB)>(glLoaderGetProcAddress("glUseProgramObjectARB"));
    if (!sf_ptrc_glUseProgramObjectARB)
        numFailed++;

    sf_ptrc_glValidateProgramARB = reinterpret_cast<void (GL_FUNCPTR *)(GLhandleARB)>(glLoaderGetProcAddress("glValidateProgramARB"));
    if (!sf_ptrc_glValidateProgramARB)
        numFailed++;

    return numFailed;
}

void (GL_FUNCPTR *sf_ptrc_glBindAttribLocationARB)(GLhandleARB, GLuint, const GLcharARB*) = NULL;
void (GL_FUNCPTR *sf_ptrc_glDisableVertexAttribArrayARB)(GLuint) = NULL;
void (GL_FUNCPTR *sf_ptrc_glEnableVertexAttribArrayARB)(GLuint) = NULL;
void (GL_FUNCPTR *sf_ptrc_glGetActiveAttribARB)(GLhandleARB, GLuint, GLsizei, GLsizei*, GLint*, GLenum*, GLcharARB*) = NULL;
GLint (GL_FUNCPTR *sf_ptrc_glGetAttribLocationARB)(GLhandleARB, const GLcharARB*) = NULL;
void (GL_FUNCPTR *sf_ptrc_glGetVertexAttribPointervARB)(GLuint, GLenum, void**) = NULL;
void (GL_FUNCPTR *sf_ptrc_glGetVertexAttribdvARB)(GLuint, GLenum, GLdouble*) = NULL;
void (GL_FUNCPTR *sf_ptrc_glGetVertexAttribfvARB)(GLuint, GLenum, GLfloat*) = NULL;
void (GL_FUNCPTR *sf_ptrc_glGetVertexAttribivARB)(GLuint, GLenum, GLint*) = NULL;
void (GL_FUNCPTR *sf_ptrc_glVertexAttrib1dARB)(GLuint, GLdouble) = NULL;
void (GL_FUNCPTR *sf_ptrc_glVertexAttrib1dvARB)(GLuint, const GLdouble*) = NULL;
void (GL_FUNCPTR *sf_ptrc_glVertexAttrib1fARB)(GLuint, GLfloat) = NULL;
void (GL_FUNCPTR *sf_ptrc_glVertexAttrib1fvARB)(GLuint, const GLfloat*) = NULL;
void (GL_FUNCPTR *sf_ptrc_glVertexAttrib1sARB)(GLuint, GLshort) = NULL;
void (GL_FUNCPTR *sf_ptrc_glVertexAttrib1svARB)(GLuint, const GLshort*) = NULL;
void (GL_FUNCPTR *sf_ptrc_glVertexAttrib2dARB)(GLuint, GLdouble, GLdouble) = NULL;
void (GL_FUNCPTR *sf_ptrc_glVertexAttrib2dvARB)(GLuint, const GLdouble*) = NULL;
void (GL_FUNCPTR *sf_ptrc_glVertexAttrib2fARB)(GLuint, GLfloat, GLfloat) = NULL;
void (GL_FUNCPTR *sf_ptrc_glVertexAttrib2fvARB)(GLuint, const GLfloat*) = NULL;
void (GL_FUNCPTR *sf_ptrc_glVertexAttrib2sARB)(GLuint, GLshort, GLshort) = NULL;
void (GL_FUNCPTR *sf_ptrc_glVertexAttrib2svARB)(GLuint, const GLshort*) = NULL;
void (GL_FUNCPTR *sf_ptrc_glVertexAttrib3dARB)(GLuint, GLdouble, GLdouble, GLdouble) = NULL;
void (GL_FUNCPTR *sf_ptrc_glVertexAttrib3dvARB)(GLuint, const GLdouble*) = NULL;
void (GL_FUNCPTR *sf_ptrc_glVertexAttrib3fARB)(GLuint, GLfloat, GLfloat, GLfloat) = NULL;
void (GL_FUNCPTR *sf_ptrc_glVertexAttrib3fvARB)(GLuint, const GLfloat*) = NULL;
void (GL_FUNCPTR *sf_ptrc_glVertexAttrib3sARB)(GLuint, GLshort, GLshort, GLshort) = NULL;
void (GL_FUNCPTR *sf_ptrc_glVertexAttrib3svARB)(GLuint, const GLshort*) = NULL;
void (GL_FUNCPTR *sf_ptrc_glVertexAttrib4NbvARB)(GLuint, const GLbyte*) = NULL;
void (GL_FUNCPTR *sf_ptrc_glVertexAttrib4NivARB)(GLuint, const GLint*) = NULL;
void (GL_FUNCPTR *sf_ptrc_glVertexAttrib4NsvARB)(GLuint, const GLshort*) = NULL;
void (GL_FUNCPTR *sf_ptrc_glVertexAttrib4NubARB)(GLuint, GLubyte, GLubyte, GLubyte, GLubyte) = NULL;
void (GL_FUNCPTR *sf_ptrc_glVertexAttrib4NubvARB)(GLuint, const GLubyte*) = NULL;
void (GL_FUNCPTR *sf_ptrc_glVertexAttrib4NuivARB)(GLuint, const GLuint*) = NULL;
void (GL_FUNCPTR *sf_ptrc_glVertexAttrib4NusvARB)(GLuint, const GLushort*) = NULL;
void (GL_FUNCPTR *sf_ptrc_glVertexAttrib4bvARB)(GLuint, const GLbyte*) = NULL;
void (GL_FUNCPTR *sf_ptrc_glVertexAttrib4dARB)(GLuint, GLdouble, GLdouble, GLdouble, GLdouble) = NULL;
void (GL_FUNCPTR *sf_ptrc_glVertexAttrib4dvARB)(GLuint, const GLdouble*) = NULL;
void (GL_FUNCPTR *sf_ptrc_glVertexAttrib4fARB)(GLuint, GLfloat, GLfloat, GLfloat, GLfloat) = NULL;
void (GL_FUNCPTR *sf_ptrc_glVertexAttrib4fvARB)(GLuint, const GLfloat*) = NULL;
void (GL_FUNCPTR *sf_ptrc_glVertexAttrib4ivARB)(GLuint, const GLint*) = NULL;
void (GL_FUNCPTR *sf_ptrc_glVertexAttrib4sARB)(GLuint, GLshort, GLshort, GLshort, GLshort) = NULL;
void (GL_FUNCPTR *sf_ptrc_glVertexAttrib4svARB)(GLuint, const GLshort*) = NULL;
void (GL_FUNCPTR *sf_ptrc_glVertexAttrib4ubvARB)(GLuint, const GLubyte*) = NULL;
void (GL_FUNCPTR *sf_ptrc_glVertexAttrib4uivARB)(GLuint, const GLuint*) = NULL;
void (GL_FUNCPTR *sf_ptrc_glVertexAttrib4usvARB)(GLuint, const GLushort*) = NULL;
void (GL_FUNCPTR *sf_ptrc_glVertexAttribPointerARB)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*) = NULL;

static int Load_ARB_vertex_shader()
{
    int numFailed = 0;

    sf_ptrc_glBindAttribLocationARB = reinterpret_cast<void (GL_FUNCPTR *)(GLhandleARB, GLuint, const GLcharARB*)>(glLoaderGetProcAddress("glBindAttribLocationARB"));
    if (!sf_ptrc_glBindAttribLocationARB)
        numFailed++;

    sf_ptrc_glDisableVertexAttribArrayARB = reinterpret_cast<void (GL_FUNCPTR *)(GLuint)>(glLoaderGetProcAddress("glDisableVertexAttribArrayARB"));
    if (!sf_ptrc_glDisableVertexAttribArrayARB)
        numFailed++;

    sf_ptrc_glEnableVertexAttribArrayARB = reinterpret_cast<void (GL_FUNCPTR *)(GLuint)>(glLoaderGetProcAddress("glEnableVertexAttribArrayARB"));
    if (!sf_ptrc_glEnableVertexAttribArrayARB)
        numFailed++;

    sf_ptrc_glGetActiveAttribARB = reinterpret_cast<void (GL_FUNCPTR *)(GLhandleARB, GLuint, GLsizei, GLsizei*, GLint*, GLenum*, GLcharARB*)>(glLoaderGetProcAddress("glGetActiveAttribARB"));
    if (!sf_ptrc_glGetActiveAttribARB)
        numFailed++;

    sf_ptrc_glGetAttribLocationARB = reinterpret_cast<GLint (GL_FUNCPTR *)(GLhandleARB, const GLcharARB*)>(glLoaderGetProcAddress("glGetAttribLocationARB"));
    if (!sf_ptrc_glGetAttribLocationARB)
        numFailed++;

    sf_ptrc_glGetVertexAttribPointervARB = reinterpret_cast<void (GL_FUNCPTR *)(GLuint, GLenum, void**)>(glLoaderGetProcAddress("glGetVertexAttribPointervARB"));
    if (!sf_ptrc_glGetVertexAttribPointervARB)
        numFailed++;

    sf_ptrc_glGetVertexAttribdvARB = reinterpret_cast<void (GL_FUNCPTR *)(GLuint, GLenum, GLdouble*)>(glLoaderGetProcAddress("glGetVertexAttribdvARB"));
    if (!sf_ptrc_glGetVertexAttribdvARB)
        numFailed++;

    sf_ptrc_glGetVertexAttribfvARB = reinterpret_cast<void (GL_FUNCPTR *)(GLuint, GLenum, GLfloat*)>(glLoaderGetProcAddress("glGetVertexAttribfvARB"));
    if (!sf_ptrc_glGetVertexAttribfvARB)
        numFailed++;

    sf_ptrc_glGetVertexAttribivARB = reinterpret_cast<void (GL_FUNCPTR *)(GLuint, GLenum, GLint*)>(glLoaderGetProcAddress("glGetVertexAttribivARB"));
    if (!sf_ptrc_glGetVertexAttribivARB)
        numFailed++;

    sf_ptrc_glVertexAttrib1dARB = reinterpret_cast<void (GL_FUNCPTR *)(GLuint, GLdouble)>(glLoaderGetProcAddress("glVertexAttrib1dARB"));
    if (!sf_ptrc_glVertexAttrib1dARB)
        numFailed++;

    sf_ptrc_glVertexAttrib1dvARB = reinterpret_cast<void (GL_FUNCPTR *)(GLuint, const GLdouble*)>(glLoaderGetProcAddress("glVertexAttrib1dvARB"));
    if (!sf_ptrc_glVertexAttrib1dvARB)
        numFailed++;

    sf_ptrc_glVertexAttrib1fARB = reinterpret_cast<void (GL_FUNCPTR *)(GLuint, GLfloat)>(glLoaderGetProcAddress("glVertexAttrib1fARB"));
    if (!sf_ptrc_glVertexAttrib1fARB)
        numFailed++;

    sf_ptrc_glVertexAttrib1fvARB = reinterpret_cast<void (GL_FUNCPTR *)(GLuint, const GLfloat*)>(glLoaderGetProcAddress("glVertexAttrib1fvARB"));
    if (!sf_ptrc_glVertexAttrib1fvARB)
        numFailed++;

    sf_ptrc_glVertexAttrib1sARB = reinterpret_cast<void (GL_FUNCPTR *)(GLuint, GLshort)>(glLoaderGetProcAddress("glVertexAttrib1sARB"));
    if (!sf_ptrc_glVertexAttrib1sARB)
        numFailed++;

    sf_ptrc_glVertexAttrib1svARB = reinterpret_cast<void (GL_FUNCPTR *)(GLuint, const GLshort*)>(glLoaderGetProcAddress("glVertexAttrib1svARB"));
    if (!sf_ptrc_glVertexAttrib1svARB)
        numFailed++;

    sf_ptrc_glVertexAttrib2dARB = reinterpret_cast<void (GL_FUNCPTR *)(GLuint, GLdouble, GLdouble)>(glLoaderGetProcAddress("glVertexAttrib2dARB"));
    if (!sf_ptrc_glVertexAttrib2dARB)
        numFailed++;

    sf_ptrc_glVertexAttrib2dvARB = reinterpret_cast<void (GL_FUNCPTR *)(GLuint, const GLdouble*)>(glLoaderGetProcAddress("glVertexAttrib2dvARB"));
    if (!sf_ptrc_glVertexAttrib2dvARB)
        numFailed++;

    sf_ptrc_glVertexAttrib2fARB = reinterpret_cast<void (GL_FUNCPTR *)(GLuint, GLfloat, GLfloat)>(glLoaderGetProcAddress("glVertexAttrib2fARB"));
    if (!sf_ptrc_glVertexAttrib2fARB)
        numFailed++;

    sf_ptrc_glVertexAttrib2fvARB = reinterpret_cast<void (GL_FUNCPTR *)(GLuint, const GLfloat*)>(glLoaderGetProcAddress("glVertexAttrib2fvARB"));
    if (!sf_ptrc_glVertexAttrib2fvARB)
        numFailed++;

    sf_ptrc_glVertexAttrib2sARB = reinterpret_cast<void (GL_FUNCPTR *)(GLuint, GLshort, GLshort)>(glLoaderGetProcAddress("glVertexAttrib2sARB"));
    if (!sf_ptrc_glVertexAttrib2sARB)
        numFailed++;

    sf_ptrc_glVertexAttrib2svARB = reinterpret_cast<void (GL_FUNCPTR *)(GLuint, const GLshort*)>(glLoaderGetProcAddress("glVertexAttrib2svARB"));
    if (!sf_ptrc_glVertexAttrib2svARB)
        numFailed++;

    sf_ptrc_glVertexAttrib3dARB = reinterpret_cast<void (GL_FUNCPTR *)(GLuint, GLdouble, GLdouble, GLdouble)>(glLoaderGetProcAddress("glVertexAttrib3dARB"));
    if (!sf_ptrc_glVertexAttrib3dARB)
        numFailed++;

    sf_ptrc_glVertexAttrib3dvARB = reinterpret_cast<void (GL_FUNCPTR *)(GLuint, const GLdouble*)>(glLoaderGetProcAddress("glVertexAttrib3dvARB"));
    if (!sf_ptrc_glVertexAttrib3dvARB)
        numFailed++;

    sf_ptrc_glVertexAttrib3fARB = reinterpret_cast<void (GL_FUNCPTR *)(GLuint, GLfloat, GLfloat, GLfloat)>(glLoaderGetProcAddress("glVertexAttrib3fARB"));
    if (!sf_ptrc_glVertexAttrib3fARB)
        numFailed++;

    sf_ptrc_glVertexAttrib3fvARB = reinterpret_cast<void (GL_FUNCPTR *)(GLuint, const GLfloat*)>(glLoaderGetProcAddress("glVertexAttrib3fvARB"));
    if (!sf_ptrc_glVertexAttrib3fvARB)
        numFailed++;

    sf_ptrc_glVertexAttrib3sARB = reinterpret_cast<void (GL_FUNCPTR *)(GLuint, GLshort, GLshort, GLshort)>(glLoaderGetProcAddress("glVertexAttrib3sARB"));
    if (!sf_ptrc_glVertexAttrib3sARB)
        numFailed++;

    sf_ptrc_glVertexAttrib3svARB = reinterpret_cast<void (GL_FUNCPTR *)(GLuint, const GLshort*)>(glLoaderGetProcAddress("glVertexAttrib3svARB"));
    if (!sf_ptrc_glVertexAttrib3svARB)
        numFailed++;

    sf_ptrc_glVertexAttrib4NbvARB = reinterpret_cast<void (GL_FUNCPTR *)(GLuint, const GLbyte*)>(glLoaderGetProcAddress("glVertexAttrib4NbvARB"));
    if (!sf_ptrc_glVertexAttrib4NbvARB)
        numFailed++;

    sf_ptrc_glVertexAttrib4NivARB = reinterpret_cast<void (GL_FUNCPTR *)(GLuint, const GLint*)>(glLoaderGetProcAddress("glVertexAttrib4NivARB"));
    if (!sf_ptrc_glVertexAttrib4NivARB)
        numFailed++;

    sf_ptrc_glVertexAttrib4NsvARB = reinterpret_cast<void (GL_FUNCPTR *)(GLuint, const GLshort*)>(glLoaderGetProcAddress("glVertexAttrib4NsvARB"));
    if (!sf_ptrc_glVertexAttrib4NsvARB)
        numFailed++;

    sf_ptrc_glVertexAttrib4NubARB = reinterpret_cast<void (GL_FUNCPTR *)(GLuint, GLubyte, GLubyte, GLubyte, GLubyte)>(glLoaderGetProcAddress("glVertexAttrib4NubARB"));
    if (!sf_ptrc_glVertexAttrib4NubARB)
        numFailed++;

    sf_ptrc_glVertexAttrib4NubvARB = reinterpret_cast<void (GL_FUNCPTR *)(GLuint, const GLubyte*)>(glLoaderGetProcAddress("glVertexAttrib4NubvARB"));
    if (!sf_ptrc_glVertexAttrib4NubvARB)
        numFailed++;

    sf_ptrc_glVertexAttrib4NuivARB = reinterpret_cast<void (GL_FUNCPTR *)(GLuint, const GLuint*)>(glLoaderGetProcAddress("glVertexAttrib4NuivARB"));
    if (!sf_ptrc_glVertexAttrib4NuivARB)
        numFailed++;

    sf_ptrc_glVertexAttrib4NusvARB = reinterpret_cast<void (GL_FUNCPTR *)(GLuint, const GLushort*)>(glLoaderGetProcAddress("glVertexAttrib4NusvARB"));
    if (!sf_ptrc_glVertexAttrib4NusvARB)
        numFailed++;

    sf_ptrc_glVertexAttrib4bvARB = reinterpret_cast<void (GL_FUNCPTR *)(GLuint, const GLbyte*)>(glLoaderGetProcAddress("glVertexAttrib4bvARB"));
    if (!sf_ptrc_glVertexAttrib4bvARB)
        numFailed++;

    sf_ptrc_glVertexAttrib4dARB = reinterpret_cast<void (GL_FUNCPTR *)(GLuint, GLdouble, GLdouble, GLdouble, GLdouble)>(glLoaderGetProcAddress("glVertexAttrib4dARB"));
    if (!sf_ptrc_glVertexAttrib4dARB)
        numFailed++;

    sf_ptrc_glVertexAttrib4dvARB = reinterpret_cast<void (GL_FUNCPTR *)(GLuint, const GLdouble*)>(glLoaderGetProcAddress("glVertexAttrib4dvARB"));
    if (!sf_ptrc_glVertexAttrib4dvARB)
        numFailed++;

    sf_ptrc_glVertexAttrib4fARB = reinterpret_cast<void (GL_FUNCPTR *)(GLuint, GLfloat, GLfloat, GLfloat, GLfloat)>(glLoaderGetProcAddress("glVertexAttrib4fARB"));
    if (!sf_ptrc_glVertexAttrib4fARB)
        numFailed++;

    sf_ptrc_glVertexAttrib4fvARB = reinterpret_cast<void (GL_FUNCPTR *)(GLuint, const GLfloat*)>(glLoaderGetProcAddress("glVertexAttrib4fvARB"));
    if (!sf_ptrc_glVertexAttrib4fvARB)
        numFailed++;

    sf_ptrc_glVertexAttrib4ivARB = reinterpret_cast<void (GL_FUNCPTR *)(GLuint, const GLint*)>(glLoaderGetProcAddress("glVertexAttrib4ivARB"));
    if (!sf_ptrc_glVertexAttrib4ivARB)
        numFailed++;

    sf_ptrc_glVertexAttrib4sARB = reinterpret_cast<void (GL_FUNCPTR *)(GLuint, GLshort, GLshort, GLshort, GLshort)>(glLoaderGetProcAddress("glVertexAttrib4sARB"));
    if (!sf_ptrc_glVertexAttrib4sARB)
        numFailed++;

    sf_ptrc_glVertexAttrib4svARB = reinterpret_cast<void (GL_FUNCPTR *)(GLuint, const GLshort*)>(glLoaderGetProcAddress("glVertexAttrib4svARB"));
    if (!sf_ptrc_glVertexAttrib4svARB)
        numFailed++;

    sf_ptrc_glVertexAttrib4ubvARB = reinterpret_cast<void (GL_FUNCPTR *)(GLuint, const GLubyte*)>(glLoaderGetProcAddress("glVertexAttrib4ubvARB"));
    if (!sf_ptrc_glVertexAttrib4ubvARB)
        numFailed++;

    sf_ptrc_glVertexAttrib4uivARB = reinterpret_cast<void (GL_FUNCPTR *)(GLuint, const GLuint*)>(glLoaderGetProcAddress("glVertexAttrib4uivARB"));
    if (!sf_ptrc_glVertexAttrib4uivARB)
        numFailed++;

    sf_ptrc_glVertexAttrib4usvARB = reinterpret_cast<void (GL_FUNCPTR *)(GLuint, const GLushort*)>(glLoaderGetProcAddress("glVertexAttrib4usvARB"));
    if (!sf_ptrc_glVertexAttrib4usvARB)
        numFailed++;

    sf_ptrc_glVertexAttribPointerARB = reinterpret_cast<void (GL_FUNCPTR *)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*)>(glLoaderGetProcAddress("glVertexAttribPointerARB"));
    if (!sf_ptrc_glVertexAttribPointerARB)
        numFailed++;

    return numFailed;
}

void (GL_FUNCPTR *sf_ptrc_glBlendEquationSeparateEXT)(GLenum, GLenum) = NULL;

static int Load_EXT_blend_equation_separate()
{
    int numFailed = 0;

    sf_ptrc_glBlendEquationSeparateEXT = reinterpret_cast<void (GL_FUNCPTR *)(GLenum, GLenum)>(glLoaderGetProcAddress("glBlendEquationSeparateEXT"));
    if (!sf_ptrc_glBlendEquationSeparateEXT)
        numFailed++;

    return numFailed;
}

void (GL_FUNCPTR *sf_ptrc_glBindFramebufferEXT)(GLenum, GLuint) = NULL;
void (GL_FUNCPTR *sf_ptrc_glBindRenderbufferEXT)(GLenum, GLuint) = NULL;
GLenum (GL_FUNCPTR *sf_ptrc_glCheckFramebufferStatusEXT)(GLenum) = NULL;
void (GL_FUNCPTR *sf_ptrc_glDeleteFramebuffersEXT)(GLsizei, const GLuint*) = NULL;
void (GL_FUNCPTR *sf_ptrc_glDeleteRenderbuffersEXT)(GLsizei, const GLuint*) = NULL;
void (GL_FUNCPTR *sf_ptrc_glFramebufferRenderbufferEXT)(GLenum, GLenum, GLenum, GLuint) = NULL;
void (GL_FUNCPTR *sf_ptrc_glFramebufferTexture1DEXT)(GLenum, GLenum, GLenum, GLuint, GLint) = NULL;
void (GL_FUNCPTR *sf_ptrc_glFramebufferTexture2DEXT)(GLenum, GLenum, GLenum, GLuint, GLint) = NULL;
void (GL_FUNCPTR *sf_ptrc_glFramebufferTexture3DEXT)(GLenum, GLenum, GLenum, GLuint, GLint, GLint) = NULL;
void (GL_FUNCPTR *sf_ptrc_glGenFramebuffersEXT)(GLsizei, GLuint*) = NULL;
void (GL_FUNCPTR *sf_ptrc_glGenRenderbuffersEXT)(GLsizei, GLuint*) = NULL;
void (GL_FUNCPTR *sf_ptrc_glGenerateMipmapEXT)(GLenum) = NULL;
void (GL_FUNCPTR *sf_ptrc_glGetFramebufferAttachmentParameterivEXT)(GLenum, GLenum, GLenum, GLint*) = NULL;
void (GL_FUNCPTR *sf_ptrc_glGetRenderbufferParameterivEXT)(GLenum, GLenum, GLint*) = NULL;
GLboolean (GL_FUNCPTR *sf_ptrc_glIsFramebufferEXT)(GLuint) = NULL;
GLboolean (GL_FUNCPTR *sf_ptrc_glIsRenderbufferEXT)(GLuint) = NULL;
void (GL_FUNCPTR *sf_ptrc_glRenderbufferStorageEXT)(GLenum, GLenum, GLsizei, GLsizei) = NULL;

static int Load_EXT_framebuffer_object()
{
    int numFailed = 0;

    sf_ptrc_glBindFramebufferEXT = reinterpret_cast<void (GL_FUNCPTR *)(GLenum, GLuint)>(glLoaderGetProcAddress("glBindFramebufferEXT"));
    if (!sf_ptrc_glBindFramebufferEXT)
        numFailed++;

    sf_ptrc_glBindRenderbufferEXT = reinterpret_cast<void (GL_FUNCPTR *)(GLenum, GLuint)>(glLoaderGetProcAddress("glBindRenderbufferEXT"));
    if (!sf_ptrc_glBindRenderbufferEXT)
        numFailed++;

    sf_ptrc_glCheckFramebufferStatusEXT = reinterpret_cast<GLenum (GL_FUNCPTR *)(GLenum)>(glLoaderGetProcAddress("glCheckFramebufferStatusEXT"));
    if (!sf_ptrc_glCheckFramebufferStatusEXT)
        numFailed++;

    sf_ptrc_glDeleteFramebuffersEXT = reinterpret_cast<void (GL_FUNCPTR *)(GLsizei, const GLuint*)>(glLoaderGetProcAddress("glDeleteFramebuffersEXT"));
    if (!sf_ptrc_glDeleteFramebuffersEXT)
        numFailed++;

    sf_ptrc_glDeleteRenderbuffersEXT = reinterpret_cast<void (GL_FUNCPTR *)(GLsizei, const GLuint*)>(glLoaderGetProcAddress("glDeleteRenderbuffersEXT"));
    if (!sf_ptrc_glDeleteRenderbuffersEXT)
        numFailed++;

    sf_ptrc_glFramebufferRenderbufferEXT = reinterpret_cast<void (GL_FUNCPTR *)(GLenum, GLenum, GLenum, GLuint)>(glLoaderGetProcAddress("glFramebufferRenderbufferEXT"));
    if (!sf_ptrc_glFramebufferRenderbufferEXT)
        numFailed++;

    sf_ptrc_glFramebufferTexture1DEXT = reinterpret_cast<void (GL_FUNCPTR *)(GLenum, GLenum, GLenum, GLuint, GLint)>(glLoaderGetProcAddress("glFramebufferTexture1DEXT"));
    if (!sf_ptrc_glFramebufferTexture1DEXT)
        numFailed++;

    sf_ptrc_glFramebufferTexture2DEXT = reinterpret_cast<void (GL_FUNCPTR *)(GLenum, GLenum, GLenum, GLuint, GLint)>(glLoaderGetProcAddress("glFramebufferTexture2DEXT"));
    if (!sf_ptrc_glFramebufferTexture2DEXT)
        numFailed++;

    sf_ptrc_glFramebufferTexture3DEXT = reinterpret_cast<void (GL_FUNCPTR *)(GLenum, GLenum, GLenum, GLuint, GLint, GLint)>(glLoaderGetProcAddress("glFramebufferTexture3DEXT"));
    if (!sf_ptrc_glFramebufferTexture3DEXT)
        numFailed++;

    sf_ptrc_glGenFramebuffersEXT = reinterpret_cast<void (GL_FUNCPTR *)(GLsizei, GLuint*)>(glLoaderGetProcAddress("glGenFramebuffersEXT"));
    if (!sf_ptrc_glGenFramebuffersEXT)
        numFailed++;

    sf_ptrc_glGenRenderbuffersEXT = reinterpret_cast<void (GL_FUNCPTR *)(GLsizei, GLuint*)>(glLoaderGetProcAddress("glGenRenderbuffersEXT"));
    if (!sf_ptrc_glGenRenderbuffersEXT)
        numFailed++;

    sf_ptrc_glGenerateMipmapEXT = reinterpret_cast<void (GL_FUNCPTR *)(GLenum)>(glLoaderGetProcAddress("glGenerateMipmapEXT"));
    if (!sf_ptrc_glGenerateMipmapEXT)
        numFailed++;

    sf_ptrc_glGetFramebufferAttachmentParameterivEXT = reinterpret_cast<void (GL_FUNCPTR *)(GLenum, GLenum, GLenum, GLint*)>(glLoaderGetProcAddress("glGetFramebufferAttachmentParameterivEXT"));
    if (!sf_ptrc_glGetFramebufferAttachmentParameterivEXT)
        numFailed++;

    sf_ptrc_glGetRenderbufferParameterivEXT = reinterpret_cast<void (GL_FUNCPTR *)(GLenum, GLenum, GLint*)>(glLoaderGetProcAddress("glGetRenderbufferParameterivEXT"));
    if (!sf_ptrc_glGetRenderbufferParameterivEXT)
        numFailed++;

    sf_ptrc_glIsFramebufferEXT = reinterpret_cast<GLboolean (GL_FUNCPTR *)(GLuint)>(glLoaderGetProcAddress("glIsFramebufferEXT"));
    if (!sf_ptrc_glIsFramebufferEXT)
        numFailed++;

    sf_ptrc_glIsRenderbufferEXT = reinterpret_cast<GLboolean (GL_FUNCPTR *)(GLuint)>(glLoaderGetProcAddress("glIsRenderbufferEXT"));
    if (!sf_ptrc_glIsRenderbufferEXT)
        numFailed++;

    sf_ptrc_glRenderbufferStorageEXT = reinterpret_cast<void (GL_FUNCPTR *)(GLenum, GLenum, GLsizei, GLsizei)>(glLoaderGetProcAddress("glRenderbufferStorageEXT"));
    if (!sf_ptrc_glRenderbufferStorageEXT)
        numFailed++;

    return numFailed;
}

void (GL_FUNCPTR *sf_ptrc_glFramebufferTextureARB)(GLenum, GLenum, GLuint, GLint) = NULL;
void (GL_FUNCPTR *sf_ptrc_glFramebufferTextureFaceARB)(GLenum, GLenum, GLuint, GLint, GLenum) = NULL;
void (GL_FUNCPTR *sf_ptrc_glFramebufferTextureLayerARB)(GLenum, GLenum, GLuint, GLint, GLint) = NULL;
void (GL_FUNCPTR *sf_ptrc_glProgramParameteriARB)(GLuint, GLenum, GLint) = NULL;

static int Load_ARB_geometry_shader4()
{
    int numFailed = 0;

    sf_ptrc_glFramebufferTextureARB = reinterpret_cast<void (GL_FUNCPTR *)(GLenum, GLenum, GLuint, GLint)>(glLoaderGetProcAddress("glFramebufferTextureARB"));
    if (!sf_ptrc_glFramebufferTextureARB)
        numFailed++;

    sf_ptrc_glFramebufferTextureFaceARB = reinterpret_cast<void (GL_FUNCPTR *)(GLenum, GLenum, GLuint, GLint, GLenum)>(glLoaderGetProcAddress("glFramebufferTextureFaceARB"));
    if (!sf_ptrc_glFramebufferTextureFaceARB)
        numFailed++;

    sf_ptrc_glFramebufferTextureLayerARB = reinterpret_cast<void (GL_FUNCPTR *)(GLenum, GLenum, GLuint, GLint, GLint)>(glLoaderGetProcAddress("glFramebufferTextureLayerARB"));
    if (!sf_ptrc_glFramebufferTextureLayerARB)
        numFailed++;

    sf_ptrc_glProgramParameteriARB = reinterpret_cast<void (GL_FUNCPTR *)(GLuint, GLenum, GLint)>(glLoaderGetProcAddress("glProgramParameteriARB"));
    if (!sf_ptrc_glProgramParameteriARB)
        numFailed++;

    return numFailed;
}

typedef int (*PFN_LOADFUNCPOINTERS)();
typedef struct sfogl_StrToExtMap_s
{
    const char* extensionName;
    int* extensionVariable;
    PFN_LOADFUNCPOINTERS LoadExtension;
} sfogl_StrToExtMap;

static sfogl_StrToExtMap ExtensionMap[15] = {
    {"GL_SGIS_texture_edge_clamp", &sfogl_ext_SGIS_texture_edge_clamp, NULL},
    {"GL_EXT_texture_edge_clamp", &sfogl_ext_EXT_texture_edge_clamp, NULL},
    {"GL_EXT_blend_minmax", &sfogl_ext_EXT_blend_minmax, Load_EXT_blend_minmax},
    {"GL_EXT_blend_subtract", &sfogl_ext_EXT_blend_subtract, NULL},
    {"GL_ARB_multitexture", &sfogl_ext_ARB_multitexture, Load_ARB_multitexture},
    {"GL_EXT_blend_func_separate", &sfogl_ext_EXT_blend_func_separate, Load_EXT_blend_func_separate},
    {"GL_ARB_shading_language_100", &sfogl_ext_ARB_shading_language_100, NULL},
    {"GL_ARB_shader_objects", &sfogl_ext_ARB_shader_objects, Load_ARB_shader_objects},
    {"GL_ARB_vertex_shader", &sfogl_ext_ARB_vertex_shader, Load_ARB_vertex_shader},
    {"GL_ARB_fragment_shader", &sfogl_ext_ARB_fragment_shader, NULL},
    {"GL_ARB_texture_non_power_of_two", &sfogl_ext_ARB_texture_non_power_of_two, NULL},
    {"GL_EXT_blend_equation_separate", &sfogl_ext_EXT_blend_equation_separate, Load_EXT_blend_equation_separate},
    {"GL_EXT_texture_sRGB", &sfogl_ext_EXT_texture_sRGB, NULL},
    {"GL_EXT_framebuffer_object", &sfogl_ext_EXT_framebuffer_object, Load_EXT_framebuffer_object},
    {"GL_ARB_geometry_shader4", &sfogl_ext_ARB_geometry_shader4, Load_ARB_geometry_shader4}
};

static int g_extensionMapSize = 15;


static void ClearExtensionVars()
{
    sfogl_ext_SGIS_texture_edge_clamp = sfogl_LOAD_FAILED;
    sfogl_ext_EXT_texture_edge_clamp = sfogl_LOAD_FAILED;
    sfogl_ext_EXT_blend_minmax = sfogl_LOAD_FAILED;
    sfogl_ext_EXT_blend_subtract = sfogl_LOAD_FAILED;
    sfogl_ext_ARB_multitexture = sfogl_LOAD_FAILED;
    sfogl_ext_EXT_blend_func_separate = sfogl_LOAD_FAILED;
    sfogl_ext_ARB_shading_language_100 = sfogl_LOAD_FAILED;
    sfogl_ext_ARB_shader_objects = sfogl_LOAD_FAILED;
    sfogl_ext_ARB_vertex_shader = sfogl_LOAD_FAILED;
    sfogl_ext_ARB_fragment_shader = sfogl_LOAD_FAILED;
    sfogl_ext_ARB_texture_non_power_of_two = sfogl_LOAD_FAILED;
    sfogl_ext_EXT_blend_equation_separate = sfogl_LOAD_FAILED;
    sfogl_ext_EXT_texture_sRGB = sfogl_LOAD_FAILED;
    sfogl_ext_EXT_framebuffer_object = sfogl_LOAD_FAILED;
    sfogl_ext_ARB_geometry_shader4 = sfogl_LOAD_FAILED;
}


static void LoadExtension(sfogl_StrToExtMap& extension)
{
    if (extension.LoadExtension)
    {
        *(extension.extensionVariable) = sfogl_LOAD_SUCCEEDED + extension.LoadExtension();
    }
    else
    {
        *(extension.extensionVariable) = sfogl_LOAD_SUCCEEDED;
    }
}


void sfogl_LoadFunctions()
{
    ClearExtensionVars();

    for (int i = 0; i < g_extensionMapSize; ++i)
    {
        if (sf::Context::isExtensionAvailable(ExtensionMap[i].extensionName))
            LoadExtension(ExtensionMap[i]);
    }
}
