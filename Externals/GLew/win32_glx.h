#ifndef __win32_glx_h__
#define __win32_glx_h__

/* Copyright (c) Nate Robins, 1997. */

/* This program is freely distributable without licensing fees 
   and is provided without guarantee or warrantee expressed or 
   implied. This program is -not- in the public domain. */

#include "win32_x11.h"

/* Type definitions (conversions). */
typedef HGLRC GLXContext;

#define GLX_USE_GL              1       /* support GLX rendering */
#define GLX_BUFFER_SIZE         2       /* depth of the color buffer */
#define GLX_LEVEL               3       /* level in plane stacking */
#define GLX_RGBA                4       /* true if RGBA mode */
#define GLX_DOUBLEBUFFER        5       /* double buffering supported */
#define GLX_STEREO              6       /* stereo buffering supported */
#define GLX_AUX_BUFFERS         7       /* number of aux buffers */
#define GLX_RED_SIZE            8       /* number of red component bits */
#define GLX_GREEN_SIZE          9       /* number of green component bits */
#define GLX_BLUE_SIZE           10      /* number of blue component bits */
#define GLX_ALPHA_SIZE          11      /* number of alpha component bits */
#define GLX_DEPTH_SIZE          12      /* number of depth bits */
#define GLX_STENCIL_SIZE        13      /* number of stencil bits */
#define GLX_ACCUM_RED_SIZE      14      /* number of red accum bits */
#define GLX_ACCUM_GREEN_SIZE    15      /* number of green accum bits */
#define GLX_ACCUM_BLUE_SIZE     16      /* number of blue accum bits */
#define GLX_ACCUM_ALPHA_SIZE    17      /* number of alpha accum bits */

#define GLX_BAD_ATTRIB  2
#define GLX_BAD_VISUAL  4

/* Functions emulated by macros. */

#define glXDestroyContext(display, context) \
  wglDeleteContext(context)

/* Function prototypes. */

extern GLXContext glXCreateContext(
  Display* display,
  XVisualInfo* visinfo,
  GLXContext share,
  Bool direct);
extern int glXGetConfig(
  Display* display,
  XVisualInfo* visual,
  int attrib,
  int* value);
extern XVisualInfo* glXChooseVisual(
  Display* display,
  int screen,
  int* attribList);

#endif /* __win32_glx_h__ */
