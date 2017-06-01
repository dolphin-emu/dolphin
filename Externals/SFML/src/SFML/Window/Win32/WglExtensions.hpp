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

#ifndef SF_POINTER_C_GENERATED_HEADER_WINDOWSGL_HPP
#define SF_POINTER_C_GENERATED_HEADER_WINDOWSGL_HPP

#ifdef __wglext_h_
#error Attempt to include auto-generated WGL header after wglext.h
#endif

#define __wglext_h_

#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN 1
#endif
#ifndef NOMINMAX
    #define NOMINMAX
#endif
#include <windows.h>

#ifdef CODEGEN_FUNCPTR
#undef CODEGEN_FUNCPTR
#endif // CODEGEN_FUNCPTR
#define CODEGEN_FUNCPTR WINAPI

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


struct _GPU_DEVICE {
    DWORD  cb;
    CHAR   DeviceName[32];
    CHAR   DeviceString[128];
    DWORD  Flags;
    RECT   rcVirtualScreen;
};
DECLARE_HANDLE(HPBUFFERARB);
DECLARE_HANDLE(HPBUFFEREXT);
DECLARE_HANDLE(HVIDEOOUTPUTDEVICENV);
DECLARE_HANDLE(HPVIDEODEV);
DECLARE_HANDLE(HGPUNV);
DECLARE_HANDLE(HVIDEOINPUTDEVICENV);
typedef struct _GPU_DEVICE *PGPU_DEVICE;

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

extern int sfwgl_ext_EXT_swap_control;
extern int sfwgl_ext_EXT_framebuffer_sRGB;
extern int sfwgl_ext_ARB_framebuffer_sRGB;
extern int sfwgl_ext_ARB_multisample;
extern int sfwgl_ext_ARB_pixel_format;
extern int sfwgl_ext_ARB_pbuffer;
extern int sfwgl_ext_ARB_create_context;
extern int sfwgl_ext_ARB_create_context_profile;

#define WGL_FRAMEBUFFER_SRGB_CAPABLE_EXT 0x20A9

#define WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB 0x20A9

#define WGL_SAMPLES_ARB 0x2042
#define WGL_SAMPLE_BUFFERS_ARB 0x2041

#define WGL_ACCELERATION_ARB 0x2003
#define WGL_ACCUM_ALPHA_BITS_ARB 0x2021
#define WGL_ACCUM_BITS_ARB 0x201D
#define WGL_ACCUM_BLUE_BITS_ARB 0x2020
#define WGL_ACCUM_GREEN_BITS_ARB 0x201F
#define WGL_ACCUM_RED_BITS_ARB 0x201E
#define WGL_ALPHA_BITS_ARB 0x201B
#define WGL_ALPHA_SHIFT_ARB 0x201C
#define WGL_AUX_BUFFERS_ARB 0x2024
#define WGL_BLUE_BITS_ARB 0x2019
#define WGL_BLUE_SHIFT_ARB 0x201A
#define WGL_COLOR_BITS_ARB 0x2014
#define WGL_DEPTH_BITS_ARB 0x2022
#define WGL_DOUBLE_BUFFER_ARB 0x2011
#define WGL_DRAW_TO_BITMAP_ARB 0x2002
#define WGL_DRAW_TO_WINDOW_ARB 0x2001
#define WGL_FULL_ACCELERATION_ARB 0x2027
#define WGL_GENERIC_ACCELERATION_ARB 0x2026
#define WGL_GREEN_BITS_ARB 0x2017
#define WGL_GREEN_SHIFT_ARB 0x2018
#define WGL_NEED_PALETTE_ARB 0x2004
#define WGL_NEED_SYSTEM_PALETTE_ARB 0x2005
#define WGL_NO_ACCELERATION_ARB 0x2025
#define WGL_NUMBER_OVERLAYS_ARB 0x2008
#define WGL_NUMBER_PIXEL_FORMATS_ARB 0x2000
#define WGL_NUMBER_UNDERLAYS_ARB 0x2009
#define WGL_PIXEL_TYPE_ARB 0x2013
#define WGL_RED_BITS_ARB 0x2015
#define WGL_RED_SHIFT_ARB 0x2016
#define WGL_SHARE_ACCUM_ARB 0x200E
#define WGL_SHARE_DEPTH_ARB 0x200C
#define WGL_SHARE_STENCIL_ARB 0x200D
#define WGL_STENCIL_BITS_ARB 0x2023
#define WGL_STEREO_ARB 0x2012
#define WGL_SUPPORT_GDI_ARB 0x200F
#define WGL_SUPPORT_OPENGL_ARB 0x2010
#define WGL_SWAP_COPY_ARB 0x2029
#define WGL_SWAP_EXCHANGE_ARB 0x2028
#define WGL_SWAP_LAYER_BUFFERS_ARB 0x2006
#define WGL_SWAP_METHOD_ARB 0x2007
#define WGL_SWAP_UNDEFINED_ARB 0x202A
#define WGL_TRANSPARENT_ALPHA_VALUE_ARB 0x203A
#define WGL_TRANSPARENT_ARB 0x200A
#define WGL_TRANSPARENT_BLUE_VALUE_ARB 0x2039
#define WGL_TRANSPARENT_GREEN_VALUE_ARB 0x2038
#define WGL_TRANSPARENT_INDEX_VALUE_ARB 0x203B
#define WGL_TRANSPARENT_RED_VALUE_ARB 0x2037
#define WGL_TYPE_COLORINDEX_ARB 0x202C
#define WGL_TYPE_RGBA_ARB 0x202B

#define WGL_DRAW_TO_PBUFFER_ARB 0x202D
#define WGL_MAX_PBUFFER_HEIGHT_ARB 0x2030
#define WGL_MAX_PBUFFER_PIXELS_ARB 0x202E
#define WGL_MAX_PBUFFER_WIDTH_ARB 0x202F
#define WGL_PBUFFER_HEIGHT_ARB 0x2035
#define WGL_PBUFFER_LARGEST_ARB 0x2033
#define WGL_PBUFFER_LOST_ARB 0x2036
#define WGL_PBUFFER_WIDTH_ARB 0x2034

#define WGL_CONTEXT_DEBUG_BIT_ARB 0x00000001
#define WGL_CONTEXT_FLAGS_ARB 0x2094
#define WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB 0x00000002
#define WGL_CONTEXT_LAYER_PLANE_ARB 0x2093
#define WGL_CONTEXT_MAJOR_VERSION_ARB 0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB 0x2092
#define WGL_ERROR_INVALID_VERSION_ARB 0x2095

#define WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB 0x00000002
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001
#define WGL_CONTEXT_PROFILE_MASK_ARB 0x9126
#define WGL_ERROR_INVALID_PROFILE_ARB 0x2096

#ifndef WGL_EXT_swap_control
#define WGL_EXT_swap_control 1
extern int (CODEGEN_FUNCPTR *sf_ptrc_wglGetSwapIntervalEXT)(void);
#define wglGetSwapIntervalEXT sf_ptrc_wglGetSwapIntervalEXT
extern BOOL (CODEGEN_FUNCPTR *sf_ptrc_wglSwapIntervalEXT)(int);
#define wglSwapIntervalEXT sf_ptrc_wglSwapIntervalEXT
#endif // WGL_EXT_swap_control


#ifndef WGL_ARB_pixel_format
#define WGL_ARB_pixel_format 1
extern BOOL (CODEGEN_FUNCPTR *sf_ptrc_wglChoosePixelFormatARB)(HDC, const int*, const FLOAT*, UINT, int*, UINT*);
#define wglChoosePixelFormatARB sf_ptrc_wglChoosePixelFormatARB
extern BOOL (CODEGEN_FUNCPTR *sf_ptrc_wglGetPixelFormatAttribfvARB)(HDC, int, int, UINT, const int*, FLOAT*);
#define wglGetPixelFormatAttribfvARB sf_ptrc_wglGetPixelFormatAttribfvARB
extern BOOL (CODEGEN_FUNCPTR *sf_ptrc_wglGetPixelFormatAttribivARB)(HDC, int, int, UINT, const int*, int*);
#define wglGetPixelFormatAttribivARB sf_ptrc_wglGetPixelFormatAttribivARB
#endif // WGL_ARB_pixel_format

#ifndef WGL_ARB_pbuffer
#define WGL_ARB_pbuffer 1
extern HPBUFFERARB (CODEGEN_FUNCPTR *sf_ptrc_wglCreatePbufferARB)(HDC, int, int, int, const int*);
#define wglCreatePbufferARB sf_ptrc_wglCreatePbufferARB
extern BOOL (CODEGEN_FUNCPTR *sf_ptrc_wglDestroyPbufferARB)(HPBUFFERARB);
#define wglDestroyPbufferARB sf_ptrc_wglDestroyPbufferARB
extern HDC (CODEGEN_FUNCPTR *sf_ptrc_wglGetPbufferDCARB)(HPBUFFERARB);
#define wglGetPbufferDCARB sf_ptrc_wglGetPbufferDCARB
extern BOOL (CODEGEN_FUNCPTR *sf_ptrc_wglQueryPbufferARB)(HPBUFFERARB, int, int*);
#define wglQueryPbufferARB sf_ptrc_wglQueryPbufferARB
extern int (CODEGEN_FUNCPTR *sf_ptrc_wglReleasePbufferDCARB)(HPBUFFERARB, HDC);
#define wglReleasePbufferDCARB sf_ptrc_wglReleasePbufferDCARB
#endif // WGL_ARB_pbuffer

#ifndef WGL_ARB_create_context
#define WGL_ARB_create_context 1
extern HGLRC (CODEGEN_FUNCPTR *sf_ptrc_wglCreateContextAttribsARB)(HDC, HGLRC, const int*);
#define wglCreateContextAttribsARB sf_ptrc_wglCreateContextAttribsARB
#endif // WGL_ARB_create_context


enum sfwgl_LoadStatus
{
    sfwgl_LOAD_FAILED = 0,
    sfwgl_LOAD_SUCCEEDED = 1
};

int sfwgl_LoadFunctions(HDC hdc);


#ifdef __cplusplus
}
#endif // __cplusplus

#endif // SF_POINTER_C_GENERATED_HEADER_WINDOWSGL_HPP
