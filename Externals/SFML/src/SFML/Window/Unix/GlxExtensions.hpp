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

#ifndef SF_POINTER_C_GENERATED_HEADER_GLXWIN_HPP
#define SF_POINTER_C_GENERATED_HEADER_GLXWIN_HPP

#ifdef __glxext_h_
#error Attempt to include glx_exts after including glxext.h
#endif

#define __glxext_h_

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <GL/glx.h>
#ifdef CODEGEN_FUNCPTR
#undef CODEGEN_FUNCPTR
#endif // CODEGEN_FUNCPTR
#define CODEGEN_FUNCPTR

#ifndef GL_LOAD_GEN_BASIC_OPENGL_TYPEDEFS
#define GL_LOAD_GEN_BASIC_OPENGL_TYPEDEFS

typedef unsigned int GLenum;
typedef unsigned char GLboolean;
typedef unsigned int GLbitfield;
typedef signed char GLbyte;
typedef short GLshort;
typedef int GLint;
typedef int GLsizei;
typedef unsigned char GLubyte;
typedef unsigned short GLushort;
typedef unsigned int GLuint;
typedef float GLfloat;
typedef float GLclampf;
typedef double GLdouble;
typedef double GLclampd;
#define GLvoid void

#endif // GL_LOAD_GEN_BASIC_OPENGL_TYPEDEFS


#ifndef GL_LOAD_GEN_BASIC_OPENGL_TYPEDEFS
#define GL_LOAD_GEN_BASIC_OPENGL_TYPEDEFS


#endif // GL_LOAD_GEN_BASIC_OPENGL_TYPEDEFS


#ifndef GLEXT_64_TYPES_DEFINED
// This code block is duplicated in glext.h, so must be protected
#define GLEXT_64_TYPES_DEFINED
// Define int32_t, int64_t, and uint64_t types for UST/MSC
// (as used in the GLX_OML_sync_control extension).
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#include <inttypes.h>
#elif defined(__sun__) || defined(__digital__)
#include <inttypes.h>
#if defined(__STDC__)
#if defined(__arch64__) || defined(_LP64)
typedef long int int64_t;
typedef unsigned long int uint64_t;
#else
typedef long long int int64_t;
typedef unsigned long long int uint64_t;
#endif // __arch64__
#endif // __STDC__
#elif defined( __VMS ) || defined(__sgi)
#include <inttypes.h>
#elif defined(__SCO__) || defined(__USLC__)
#include <stdint.h>
#elif defined(__UNIXOS2__) || defined(__SOL64__)
typedef long int int32_t;
typedef long long int int64_t;
typedef unsigned long long int uint64_t;
#elif defined(_WIN32) && defined(__GNUC__)
#include <stdint.h>
#elif defined(_WIN32)
typedef __int32 int32_t;
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;
#else
// Fallback if nothing above works
#include <inttypes.h>
#endif
#endif
    typedef struct __GLXFBConfigRec *GLXFBConfig;
    typedef XID GLXContextID;
    typedef struct __GLXcontextRec *GLXContext;
    typedef XID GLXPixmap;
    typedef XID GLXDrawable;
    typedef XID GLXPbuffer;
    typedef void (APIENTRY *__GLXextFuncPtr)(void);
    typedef XID GLXVideoCaptureDeviceNV;
    typedef unsigned int GLXVideoDeviceNV;
    typedef XID GLXVideoSourceSGIX;
    typedef struct __GLXFBConfigRec *GLXFBConfigSGIX;
    typedef XID GLXPbufferSGIX;
    typedef struct {
        char    pipeName[80]; // Should be [GLX_HYPERPIPE_PIPE_NAME_LENGTH_SGIX]
        int     networkId;
} GLXHyperpipeNetworkSGIX;
    typedef struct {
        char    pipeName[80]; // Should be [GLX_HYPERPIPE_PIPE_NAME_LENGTH_SGIX]
        int     channel;
        unsigned int participationType;
        int     timeSlice;
} GLXHyperpipeConfigSGIX;
    typedef struct {
        char pipeName[80]; // Should be [GLX_HYPERPIPE_PIPE_NAME_LENGTH_SGIX]
        int srcXOrigin, srcYOrigin, srcWidth, srcHeight;
        int destXOrigin, destYOrigin, destWidth, destHeight;
} GLXPipeRect;
    typedef struct {
        char pipeName[80]; // Should be [GLX_HYPERPIPE_PIPE_NAME_LENGTH_SGIX]
        int XOrigin, YOrigin, maxHeight, maxWidth;
} GLXPipeRectLimits;

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

extern int sfglx_ext_EXT_swap_control;
extern int sfglx_ext_MESA_swap_control;
extern int sfglx_ext_SGI_swap_control;
extern int sfglx_ext_EXT_framebuffer_sRGB;
extern int sfglx_ext_ARB_framebuffer_sRGB;
extern int sfglx_ext_ARB_multisample;
extern int sfglx_ext_SGIX_pbuffer;
extern int sfglx_ext_ARB_create_context;
extern int sfglx_ext_ARB_create_context_profile;

#define GLX_MAX_SWAP_INTERVAL_EXT 0x20F2
#define GLX_SWAP_INTERVAL_EXT 0x20F1

#define GLX_FRAMEBUFFER_SRGB_CAPABLE_EXT 0x20B2

#define GLX_FRAMEBUFFER_SRGB_CAPABLE_ARB 0x20B2

#define GLX_SAMPLES_ARB 100001
#define GLX_SAMPLE_BUFFERS_ARB 100000

#define GLX_ACCUM_BUFFER_BIT_SGIX 0x00000080
#define GLX_AUX_BUFFERS_BIT_SGIX 0x00000010
#define GLX_BACK_LEFT_BUFFER_BIT_SGIX 0x00000004
#define GLX_BACK_RIGHT_BUFFER_BIT_SGIX 0x00000008
#define GLX_BUFFER_CLOBBER_MASK_SGIX 0x08000000
#define GLX_DAMAGED_SGIX 0x8020
#define GLX_DEPTH_BUFFER_BIT_SGIX 0x00000020
#define GLX_EVENT_MASK_SGIX 0x801F
#define GLX_FRONT_LEFT_BUFFER_BIT_SGIX 0x00000001
#define GLX_FRONT_RIGHT_BUFFER_BIT_SGIX 0x00000002
#define GLX_HEIGHT_SGIX 0x801E
#define GLX_LARGEST_PBUFFER_SGIX 0x801C
#define GLX_MAX_PBUFFER_HEIGHT_SGIX 0x8017
#define GLX_MAX_PBUFFER_PIXELS_SGIX 0x8018
#define GLX_MAX_PBUFFER_WIDTH_SGIX 0x8016
#define GLX_OPTIMAL_PBUFFER_HEIGHT_SGIX 0x801A
#define GLX_OPTIMAL_PBUFFER_WIDTH_SGIX 0x8019
#define GLX_PBUFFER_BIT_SGIX 0x00000004
#define GLX_PBUFFER_SGIX 0x8023
#define GLX_PRESERVED_CONTENTS_SGIX 0x801B
#define GLX_SAMPLE_BUFFERS_BIT_SGIX 0x00000100
#define GLX_SAVED_SGIX 0x8021
#define GLX_STENCIL_BUFFER_BIT_SGIX 0x00000040
#define GLX_WIDTH_SGIX 0x801D
#define GLX_WINDOW_SGIX 0x8022

#define GLX_CONTEXT_DEBUG_BIT_ARB 0x00000001
#define GLX_CONTEXT_FLAGS_ARB 0x2094
#define GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB 0x00000002
#define GLX_CONTEXT_MAJOR_VERSION_ARB 0x2091
#define GLX_CONTEXT_MINOR_VERSION_ARB 0x2092

#define GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB 0x00000002
#define GLX_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001
#define GLX_CONTEXT_PROFILE_MASK_ARB 0x9126

#ifndef GLX_EXT_swap_control
#define GLX_EXT_swap_control 1
extern void (CODEGEN_FUNCPTR *sf_ptrc_glXSwapIntervalEXT)(Display *, GLXDrawable, int);
#define glXSwapIntervalEXT sf_ptrc_glXSwapIntervalEXT
#endif // GLX_EXT_swap_control

// Declare entry point even if GLX header already provides glXSwapIntervalMESA
// We won't make use of an alias here
extern int (CODEGEN_FUNCPTR *sf_ptrc_glXSwapIntervalMESA)(int);

#ifndef GLX_SGI_swap_control
#define GLX_SGI_swap_control 1
extern int (CODEGEN_FUNCPTR *sf_ptrc_glXSwapIntervalSGI)(int);
#define glXSwapIntervalSGI sf_ptrc_glXSwapIntervalSGI
#endif // GLX_SGI_swap_control

#ifndef GLX_SGIX_pbuffer
#define GLX_SGIX_pbuffer 1
extern GLXPbufferSGIX (CODEGEN_FUNCPTR *sf_ptrc_glXCreateGLXPbufferSGIX)(Display*, GLXFBConfigSGIX, unsigned int, unsigned int, int*);
#define glXCreateGLXPbufferSGIX sf_ptrc_glXCreateGLXPbufferSGIX
extern void (CODEGEN_FUNCPTR *sf_ptrc_glXDestroyGLXPbufferSGIX)(Display*, GLXPbufferSGIX);
#define glXDestroyGLXPbufferSGIX sf_ptrc_glXDestroyGLXPbufferSGIX
extern void (CODEGEN_FUNCPTR *sf_ptrc_glXGetSelectedEventSGIX)(Display*, GLXDrawable, unsigned long*);
#define glXGetSelectedEventSGIX sf_ptrc_glXGetSelectedEventSGIX
extern int (CODEGEN_FUNCPTR *sf_ptrc_glXQueryGLXPbufferSGIX)(Display*, GLXPbufferSGIX, int, unsigned int*);
#define glXQueryGLXPbufferSGIX sf_ptrc_glXQueryGLXPbufferSGIX
extern void (CODEGEN_FUNCPTR *sf_ptrc_glXSelectEventSGIX)(Display*, GLXDrawable, unsigned long);
#define glXSelectEventSGIX sf_ptrc_glXSelectEventSGIX
#endif // GLX_SGIX_pbuffer

#ifndef GLX_ARB_create_context
#define GLX_ARB_create_context 1
extern GLXContext (CODEGEN_FUNCPTR *sf_ptrc_glXCreateContextAttribsARB)(Display*, GLXFBConfig, GLXContext, Bool, const int*);
#define glXCreateContextAttribsARB sf_ptrc_glXCreateContextAttribsARB
#endif // GLX_ARB_create_context


enum sfglx_LoadStatus
{
    sfglx_LOAD_FAILED = 0,
    sfglx_LOAD_SUCCEEDED = 1
};

int sfglx_LoadFunctions(Display *display, int screen);


#ifdef __cplusplus
}
#endif // __cplusplus

#endif // SF_POINTER_C_GENERATED_HEADER_GLXWIN_HPP
