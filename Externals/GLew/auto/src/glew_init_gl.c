/* ------------------------------------------------------------------------- */

/* 
 * Search for name in the extensions string. Use of strstr()
 * is not sufficient because extension names can be prefixes of
 * other extension names. Could use strtok() but the constant
 * string returned by glGetString might be in read-only memory.
 */
GLboolean glewGetExtension (const char* name)
{    
  GLubyte* p;
  GLubyte* end;
  GLuint len = _glewStrLen((const GLubyte*)name);
  p = (GLubyte*)glGetString(GL_EXTENSIONS);
  if (0 == p) return GL_FALSE;
  end = p + _glewStrLen(p);
  while (p < end)
  {
    GLuint n = _glewStrCLen(p, ' ');
    if (len == n && _glewStrSame((const GLubyte*)name, p, n)) return GL_TRUE;
    p += n+1;
  }
  return GL_FALSE;
}

/* ------------------------------------------------------------------------- */

#ifndef GLEW_MX
static
#endif
GLenum glewContextInit (GLEW_CONTEXT_ARG_DEF_LIST)
{
  const GLubyte* s;
  GLuint dot;
  GLint major, minor;
  /* query opengl version */
  s = glGetString(GL_VERSION);
  dot = _glewStrCLen(s, '.');
  if (dot == 0)
    return GLEW_ERROR_NO_GL_VERSION;
  
  major = s[dot-1]-'0';
  minor = s[dot+1]-'0';

  if (minor < 0 || minor > 9)
    minor = 0;
  if (major<0 || major>9)
    return GLEW_ERROR_NO_GL_VERSION;
  

  if (major == 1 && minor == 0)
  {
    return GLEW_ERROR_GL_VERSION_10_ONLY;
  }
  else
  {
    CONST_CAST(GLEW_VERSION_4_0) = ( major > 4 )               || ( major == 4               ) ? GL_TRUE : GL_FALSE;
    CONST_CAST(GLEW_VERSION_3_3) = GLEW_VERSION_4_0 == GL_TRUE || ( major == 3 && minor >= 3 ) ? GL_TRUE : GL_FALSE;
    CONST_CAST(GLEW_VERSION_3_2) = GLEW_VERSION_3_3 == GL_TRUE || ( major == 3 && minor >= 2 ) ? GL_TRUE : GL_FALSE;
    CONST_CAST(GLEW_VERSION_3_1) = GLEW_VERSION_3_2 == GL_TRUE || ( major == 3 && minor >= 1 ) ? GL_TRUE : GL_FALSE;
    CONST_CAST(GLEW_VERSION_3_0) = GLEW_VERSION_3_1 == GL_TRUE || ( major == 3               ) ? GL_TRUE : GL_FALSE;
    CONST_CAST(GLEW_VERSION_2_1) = GLEW_VERSION_3_0 == GL_TRUE || ( major == 2 && minor >= 1 ) ? GL_TRUE : GL_FALSE;    
    CONST_CAST(GLEW_VERSION_2_0) = GLEW_VERSION_2_1 == GL_TRUE || ( major == 2               ) ? GL_TRUE : GL_FALSE;
    CONST_CAST(GLEW_VERSION_1_5) = GLEW_VERSION_2_0 == GL_TRUE || ( major == 1 && minor >= 5 ) ? GL_TRUE : GL_FALSE;
    CONST_CAST(GLEW_VERSION_1_4) = GLEW_VERSION_1_5 == GL_TRUE || ( major == 1 && minor >= 4 ) ? GL_TRUE : GL_FALSE;
    CONST_CAST(GLEW_VERSION_1_3) = GLEW_VERSION_1_4 == GL_TRUE || ( major == 1 && minor >= 3 ) ? GL_TRUE : GL_FALSE;
    CONST_CAST(GLEW_VERSION_1_2) = GLEW_VERSION_1_3 == GL_TRUE || ( major == 1 && minor >= 2 ) ? GL_TRUE : GL_FALSE;
    CONST_CAST(GLEW_VERSION_1_1) = GLEW_VERSION_1_2 == GL_TRUE || ( major == 1 && minor >= 1 ) ? GL_TRUE : GL_FALSE;
  }
  /* initialize extensions */
