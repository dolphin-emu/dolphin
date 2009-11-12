/* list of OpenGL ES functions sorted alphabetically
   If you need to use a GLES function from the SDL video subsystem,
   change it's entry from SDL_PROC_UNUSED to SDL_PROC and rebuild.
*/
#define SDL_PROC_UNUSED(ret,func,params)

SDL_PROC_UNUSED(void, glAlphaFunc, (GLenum func, GLclampf ref))
SDL_PROC(void, glClearColor,
         (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha))
SDL_PROC_UNUSED(void, glClearDepthf, (GLclampf depth))
SDL_PROC_UNUSED(void, glClipPlanef, (GLenum plane, const GLfloat * equation))
SDL_PROC(void, glColor4f,
         (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha))
SDL_PROC_UNUSED(void, glDepthRangef, (GLclampf zNear, GLclampf zFar))
SDL_PROC_UNUSED(void, glFogf, (GLenum pname, GLfloat param))
SDL_PROC_UNUSED(void, glFogfv, (GLenum pname, const GLfloat * params))
SDL_PROC_UNUSED(void, glFrustumf,
                (GLfloat left, GLfloat right, GLfloat bottom, GLfloat top,
                 GLfloat zNear, GLfloat zFar))
SDL_PROC_UNUSED(void, glGetClipPlanef, (GLenum pname, GLfloat eqn[4]))
SDL_PROC(void, glGetFloatv, (GLenum pname, GLfloat * params))
SDL_PROC_UNUSED(void, glGetLightfv,
                (GLenum light, GLenum pname, GLfloat * params))
SDL_PROC_UNUSED(void, glGetMaterialfv,
                (GLenum face, GLenum pname, GLfloat * params))
SDL_PROC_UNUSED(void, glGetTexEnvfv,
                (GLenum env, GLenum pname, GLfloat * params))
SDL_PROC_UNUSED(void, glGetTexParameterfv,
                (GLenum target, GLenum pname, GLfloat * params))
SDL_PROC_UNUSED(void, glLightModelf, (GLenum pname, GLfloat param))
SDL_PROC_UNUSED(void, glLightModelfv, (GLenum pname, const GLfloat * params))
SDL_PROC_UNUSED(void, glLightf, (GLenum light, GLenum pname, GLfloat param))
SDL_PROC_UNUSED(void, glLightfv,
                (GLenum light, GLenum pname, const GLfloat * params))
SDL_PROC_UNUSED(void, glLineWidth, (GLfloat width))
SDL_PROC_UNUSED(void, glLoadMatrixf, (const GLfloat * m))
SDL_PROC_UNUSED(void, glMaterialf, (GLenum face, GLenum pname, GLfloat param))
SDL_PROC_UNUSED(void, glMaterialfv,
                (GLenum face, GLenum pname, const GLfloat * params))
SDL_PROC_UNUSED(void, glMultMatrixf, (const GLfloat * m))
SDL_PROC_UNUSED(void, glMultiTexCoord4f,
                (GLenum target, GLfloat s, GLfloat t, GLfloat r, GLfloat q))
SDL_PROC_UNUSED(void, glNormal3f, (GLfloat nx, GLfloat ny, GLfloat nz))
SDL_PROC(void, glOrthof,
         (GLfloat left, GLfloat right, GLfloat bottom, GLfloat top,
          GLfloat zNear, GLfloat zFar))
SDL_PROC_UNUSED(void, glPointParameterf, (GLenum pname, GLfloat param))
SDL_PROC_UNUSED(void, glPointParameterfv,
                (GLenum pname, const GLfloat * params))
SDL_PROC_UNUSED(void, glPointSize, (GLfloat size))
SDL_PROC_UNUSED(void, glPolygonOffset, (GLfloat factor, GLfloat units))
SDL_PROC_UNUSED(void, glRotatef,
                (GLfloat angle, GLfloat x, GLfloat y, GLfloat z))
SDL_PROC_UNUSED(void, glScalef, (GLfloat x, GLfloat y, GLfloat z))
SDL_PROC(void, glTexEnvf, (GLenum target, GLenum pname, GLfloat param))
SDL_PROC_UNUSED(void, glTexEnvfv,
                (GLenum target, GLenum pname, const GLfloat * params))
SDL_PROC(void, glTexParameterf, (GLenum target, GLenum pname, GLfloat param))
SDL_PROC_UNUSED(void, glTexParameterfv,
                (GLenum target, GLenum pname, const GLfloat * params))
SDL_PROC_UNUSED(void, glTranslatef, (GLfloat x, GLfloat y, GLfloat z))

/* Available in both Common and Common-Lite profiles */
SDL_PROC_UNUSED(void, glActiveTexture, (GLenum texture))
SDL_PROC_UNUSED(void, glAlphaFuncx, (GLenum func, GLclampx ref))
SDL_PROC_UNUSED(void, glBindBuffer, (GLenum target, GLuint buffer))
SDL_PROC(void, glBindTexture, (GLenum target, GLuint texture))
SDL_PROC(void, glBlendFunc, (GLenum sfactor, GLenum dfactor))
SDL_PROC_UNUSED(void, glBufferData,
                (GLenum target, GLsizeiptr size, const GLvoid * data,
                 GLenum usage))
SDL_PROC_UNUSED(void, glBufferSubData,
                (GLenum target, GLintptr offset, GLsizeiptr size,
                 const GLvoid * data))
SDL_PROC(void, glClear, (GLbitfield mask))
SDL_PROC_UNUSED(void, glClearColorx,
                (GLclampx red, GLclampx green, GLclampx blue, GLclampx alpha))
SDL_PROC_UNUSED(void, glClearDepthx, (GLclampx depth))
SDL_PROC_UNUSED(void, glClearStencil, (GLint s))
SDL_PROC_UNUSED(void, glClientActiveTexture, (GLenum texture))
SDL_PROC_UNUSED(void, glClipPlanex, (GLenum plane, const GLfixed * equation))
SDL_PROC_UNUSED(void, glColor4ub,
                (GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha))
SDL_PROC_UNUSED(void, glColor4x,
                (GLfixed red, GLfixed green, GLfixed blue, GLfixed alpha))
SDL_PROC_UNUSED(void, glColorMask,
                (GLboolean red, GLboolean green, GLboolean blue,
                 GLboolean alpha))
SDL_PROC(void, glColorPointer,
         (GLint size, GLenum type, GLsizei stride, const GLvoid * pointer))
SDL_PROC_UNUSED(void, glCompressedTexImage2D,
                (GLenum target, GLint level, GLenum internalformat,
                 GLsizei width, GLsizei height, GLint border,
                 GLsizei imageSize, const GLvoid * data))
SDL_PROC_UNUSED(void, glCompressedTexSubImage2D,
                (GLenum target, GLint level, GLint xoffset, GLint yoffset,
                 GLsizei width, GLsizei height, GLenum format,
                 GLsizei imageSize, const GLvoid * data))
SDL_PROC_UNUSED(void, glCopyTexImage2D,
                (GLenum target, GLint level, GLenum internalformat, GLint x,
                 GLint y, GLsizei width, GLsizei height, GLint border))
SDL_PROC_UNUSED(void, glCopyTexSubImage2D,
                (GLenum target, GLint level, GLint xoffset, GLint yoffset,
                 GLint x, GLint y, GLsizei width, GLsizei height))
SDL_PROC_UNUSED(void, glCullFace, (GLenum mode))
SDL_PROC_UNUSED(void, glDeleteBuffers, (GLsizei n, const GLuint * buffers))
SDL_PROC_UNUSED(void, glDeleteTextures, (GLsizei n, const GLuint * textures))
SDL_PROC_UNUSED(void, glDepthFunc, (GLenum func))
SDL_PROC_UNUSED(void, glDepthMask, (GLboolean flag))
SDL_PROC_UNUSED(void, glDepthRangex, (GLclampx zNear, GLclampx zFar))
SDL_PROC(void, glDisable, (GLenum cap))
SDL_PROC(void, glDisableClientState, (GLenum array))
SDL_PROC(void, glDrawArrays, (GLenum mode, GLint first, GLsizei count))
SDL_PROC_UNUSED(void, glDrawElements,
                (GLenum mode, GLsizei count, GLenum type,
                 const GLvoid * indices))
SDL_PROC(void, glEnable, (GLenum cap))
SDL_PROC(void, glEnableClientState, (GLenum array))
SDL_PROC_UNUSED(void, glFinish, (void))
SDL_PROC_UNUSED(void, glFlush, (void))
SDL_PROC_UNUSED(void, glFogx, (GLenum pname, GLfixed param))
SDL_PROC_UNUSED(void, glFogxv, (GLenum pname, const GLfixed * params))
SDL_PROC_UNUSED(void, glFrontFace, (GLenum mode))
SDL_PROC_UNUSED(void, glFrustumx,
                (GLfixed left, GLfixed right, GLfixed bottom, GLfixed top,
                 GLfixed zNear, GLfixed zFar))
SDL_PROC_UNUSED(void, glGetBooleanv, (GLenum pname, GLboolean * params))
SDL_PROC_UNUSED(void, glGetBufferParameteriv,
                (GLenum target, GLenum pname, GLint * params))
SDL_PROC_UNUSED(void, glGetClipPlanex, (GLenum pname, GLfixed eqn[4]))
SDL_PROC_UNUSED(void, glGenBuffers, (GLsizei n, GLuint * buffers))
SDL_PROC(void, glGenTextures, (GLsizei n, GLuint * textures))
SDL_PROC(GLenum, glGetError, (void))
SDL_PROC_UNUSED(void, glGetFixedv, (GLenum pname, GLfixed * params))
SDL_PROC(void, glGetIntegerv, (GLenum pname, GLint * params))
SDL_PROC_UNUSED(void, glGetLightxv,
                (GLenum light, GLenum pname, GLfixed * params))
SDL_PROC_UNUSED(void, glGetMaterialxv,
                (GLenum face, GLenum pname, GLfixed * params))
SDL_PROC_UNUSED(void, glGetPointerv, (GLenum pname, void **params))
SDL_PROC_UNUSED(const GLubyte *, glGetString, (GLenum name))
SDL_PROC_UNUSED(void, glGetTexEnviv,
                (GLenum env, GLenum pname, GLint * params))
SDL_PROC_UNUSED(void, glGetTexEnvxv,
                (GLenum env, GLenum pname, GLfixed * params))
SDL_PROC_UNUSED(void, glGetTexParameteriv,
                (GLenum target, GLenum pname, GLint * params))
SDL_PROC_UNUSED(void, glGetTexParameterxv,
                (GLenum target, GLenum pname, GLfixed * params))
SDL_PROC_UNUSED(void, glHint, (GLenum target, GLenum mode))
SDL_PROC_UNUSED(GLboolean, glIsBuffer, (GLuint buffer))
SDL_PROC_UNUSED(GLboolean, glIsEnabled, (GLenum cap))
SDL_PROC_UNUSED(GLboolean, glIsTexture, (GLuint texture))
SDL_PROC_UNUSED(void, glLightModelx, (GLenum pname, GLfixed param))
SDL_PROC_UNUSED(void, glLightModelxv, (GLenum pname, const GLfixed * params))
SDL_PROC_UNUSED(void, glLightx, (GLenum light, GLenum pname, GLfixed param))
SDL_PROC_UNUSED(void, glLightxv,
                (GLenum light, GLenum pname, const GLfixed * params))
SDL_PROC_UNUSED(void, glLineWidthx, (GLfixed width))
SDL_PROC(void, glLoadIdentity, (void))
SDL_PROC_UNUSED(void, glLoadMatrixx, (const GLfixed * m))
SDL_PROC_UNUSED(void, glLogicOp, (GLenum opcode))
SDL_PROC_UNUSED(void, glMaterialx, (GLenum face, GLenum pname, GLfixed param))
SDL_PROC_UNUSED(void, glMaterialxv,
                (GLenum face, GLenum pname, const GLfixed * params))
SDL_PROC(void, glMatrixMode, (GLenum mode))
SDL_PROC_UNUSED(void, glMultMatrixx, (const GLfixed * m))
SDL_PROC_UNUSED(void, glMultiTexCoord4x,
                (GLenum target, GLfixed s, GLfixed t, GLfixed r, GLfixed q))
SDL_PROC_UNUSED(void, glNormal3x, (GLfixed nx, GLfixed ny, GLfixed nz))
SDL_PROC_UNUSED(void, glNormalPointer,
                (GLenum type, GLsizei stride, const GLvoid * pointer))
SDL_PROC_UNUSED(void, glOrthox,
                (GLfixed left, GLfixed right, GLfixed bottom, GLfixed top,
                 GLfixed zNear, GLfixed zFar))
SDL_PROC(void, glPixelStorei, (GLenum pname, GLint param))
SDL_PROC_UNUSED(void, glPointParameterx, (GLenum pname, GLfixed param))
SDL_PROC_UNUSED(void, glPointParameterxv,
                (GLenum pname, const GLfixed * params))
SDL_PROC_UNUSED(void, glPointSizex, (GLfixed size))
SDL_PROC_UNUSED(void, glPolygonOffsetx, (GLfixed factor, GLfixed units))
SDL_PROC_UNUSED(void, glPopMatrix, (void))
SDL_PROC_UNUSED(void, glPushMatrix, (void))
SDL_PROC_UNUSED(void, glReadPixels,
                (GLint x, GLint y, GLsizei width, GLsizei height,
                 GLenum format, GLenum type, GLvoid * pixels))
SDL_PROC_UNUSED(void, glRotatex,
                (GLfixed angle, GLfixed x, GLfixed y, GLfixed z))
SDL_PROC_UNUSED(void, glSampleCoverage, (GLclampf value, GLboolean invert))
SDL_PROC_UNUSED(void, glSampleCoveragex, (GLclampx value, GLboolean invert))
SDL_PROC_UNUSED(void, glScalex, (GLfixed x, GLfixed y, GLfixed z))
SDL_PROC(void, glScissor, (GLint x, GLint y, GLsizei width, GLsizei height))
SDL_PROC_UNUSED(void, glShadeModel, (GLenum mode))
SDL_PROC_UNUSED(void, glStencilFunc, (GLenum func, GLint ref, GLuint mask))
SDL_PROC_UNUSED(void, glStencilMask, (GLuint mask))
SDL_PROC_UNUSED(void, glStencilOp, (GLenum fail, GLenum zfail, GLenum zpass))
SDL_PROC(void, glTexCoordPointer,
         (GLint size, GLenum type, GLsizei stride, const GLvoid * pointer))
SDL_PROC_UNUSED(void, glTexEnvi, (GLenum target, GLenum pname, GLint param))
SDL_PROC_UNUSED(void, glTexEnvx, (GLenum target, GLenum pname, GLfixed param))
SDL_PROC_UNUSED(void, glTexEnviv,
                (GLenum target, GLenum pname, const GLint * params))
SDL_PROC_UNUSED(void, glTexEnvxv,
                (GLenum target, GLenum pname, const GLfixed * params))
SDL_PROC(void, glTexImage2D,
         (GLenum target, GLint level, GLint internalformat, GLsizei width,
          GLsizei height, GLint border, GLenum format, GLenum type,
          const GLvoid * pixels))
SDL_PROC(void, glTexParameteri, (GLenum target, GLenum pname, GLint param))
SDL_PROC_UNUSED(void, glTexParameterx,
                (GLenum target, GLenum pname, GLfixed param))
SDL_PROC(void, glTexParameteriv,
         (GLenum target, GLenum pname, const GLint * params))
SDL_PROC_UNUSED(void, glTexParameterxv,
                (GLenum target, GLenum pname, const GLfixed * params))
SDL_PROC(void, glTexSubImage2D,
         (GLenum target, GLint level, GLint xoffset, GLint yoffset,
          GLsizei width, GLsizei height, GLenum format, GLenum type,
          const GLvoid * pixels))
SDL_PROC_UNUSED(void, glTranslatex, (GLfixed x, GLfixed y, GLfixed z))
SDL_PROC(void, glVertexPointer,
         (GLint size, GLenum type, GLsizei stride, const GLvoid * pointer))
SDL_PROC(void, glViewport, (GLint x, GLint y, GLsizei width, GLsizei height))

/* extension functions used */
SDL_PROC(void, glDrawTexiOES,
         (GLint x, GLint y, GLint z, GLint width, GLint height))

/* vi: set ts=4 sw=4 expandtab: */
