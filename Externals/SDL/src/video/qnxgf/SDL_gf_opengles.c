/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997-2009 Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    Sam Lantinga
    slouken@libsdl.org

    QNX Graphics Framework SDL driver
    Copyright (C) 2009 Mike Gorchak
    (mike@malva.ua, lestat@i.com.ua)
*/

#include <GLES/gl.h>
#include <GLES/glext.h>

/* This is OpenGL ES 1.0 helper functions from OpenGL ES 1.1 specification,  */
/* which could be implemented independently from hardware, just wrappers     */

GLAPI void APIENTRY
glTexParameteri(GLenum target, GLenum pname, GLint param)
{
    glTexParameterx(target, pname, (GLfixed) param);
    return;
}

GLAPI void APIENTRY
glTexParameteriv(GLenum target, GLenum pname, const GLint * params)
{
    /* Retrieve one parameter only */
    glTexParameterx(target, pname, (GLfixed) * params);
    return;
}

GLAPI void APIENTRY
glColor4ub(GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha)
{
    glColor4f(((GLfloat) red) / 255.f, ((GLfloat) green) / 255.f,
              ((GLfloat) blue) / 255.f, ((GLfloat) alpha) / 255.f);
    return;
}
