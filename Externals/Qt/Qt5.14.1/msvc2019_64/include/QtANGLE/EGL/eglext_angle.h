//
// Copyright (c) 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// eglext_angle.h: ANGLE modifications to the eglext.h header file.
//   Currently we don't include this file directly, we patch eglext.h
//   to include it implicitly so it is visible throughout our code.

#ifndef INCLUDE_EGL_EGLEXT_ANGLE_
#define INCLUDE_EGL_EGLEXT_ANGLE_

// clang-format off

#ifndef EGL_ANGLE_robust_resource_initialization
#define EGL_ANGLE_robust_resource_initialization 1
#define EGL_ROBUST_RESOURCE_INITIALIZATION_ANGLE 0x3453
#endif /* EGL_ANGLE_robust_resource_initialization */

#ifndef EGL_ANGLE_keyed_mutex
#define EGL_ANGLE_keyed_mutex 1
#define EGL_DXGI_KEYED_MUTEX_ANGLE        0x33A2
#endif /* EGL_ANGLE_keyed_mutex */

#ifndef EGL_ANGLE_d3d_texture_client_buffer
#define EGL_ANGLE_d3d_texture_client_buffer 1
#define EGL_D3D_TEXTURE_ANGLE             0x33A3
#endif /* EGL_ANGLE_d3d_texture_client_buffer */

#ifndef EGL_ANGLE_software_display
#define EGL_ANGLE_software_display 1
#define EGL_SOFTWARE_DISPLAY_ANGLE ((EGLNativeDisplayType)-1)
#endif /* EGL_ANGLE_software_display */

#ifndef EGL_ANGLE_direct3d_display
#define EGL_ANGLE_direct3d_display 1
#define EGL_D3D11_ELSE_D3D9_DISPLAY_ANGLE ((EGLNativeDisplayType)-2)
#define EGL_D3D11_ONLY_DISPLAY_ANGLE ((EGLNativeDisplayType)-3)
#endif /* EGL_ANGLE_direct3d_display */

#ifndef EGL_ANGLE_direct_composition
#define EGL_ANGLE_direct_composition 1
#define EGL_DIRECT_COMPOSITION_ANGLE 0x33A5
#endif /* EGL_ANGLE_direct_composition */

#ifndef EGL_ANGLE_platform_angle
#define EGL_ANGLE_platform_angle 1
#define EGL_PLATFORM_ANGLE_ANGLE          0x3202
#define EGL_PLATFORM_ANGLE_TYPE_ANGLE     0x3203
#define EGL_PLATFORM_ANGLE_MAX_VERSION_MAJOR_ANGLE 0x3204
#define EGL_PLATFORM_ANGLE_MAX_VERSION_MINOR_ANGLE 0x3205
#define EGL_PLATFORM_ANGLE_TYPE_DEFAULT_ANGLE 0x3206
#define EGL_PLATFORM_ANGLE_DEBUG_LAYERS_ENABLED_ANGLE 0x3451
#endif /* EGL_ANGLE_platform_angle */

#ifndef EGL_ANGLE_platform_angle_d3d
#define EGL_ANGLE_platform_angle_d3d 1
#define EGL_PLATFORM_ANGLE_TYPE_D3D9_ANGLE 0x3207
#define EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE 0x3208
#define EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE 0x3209
#define EGL_PLATFORM_ANGLE_DEVICE_TYPE_HARDWARE_ANGLE 0x320A
#define EGL_PLATFORM_ANGLE_DEVICE_TYPE_WARP_ANGLE 0x320B
#define EGL_PLATFORM_ANGLE_DEVICE_TYPE_REFERENCE_ANGLE 0x320C
#define EGL_PLATFORM_ANGLE_ENABLE_AUTOMATIC_TRIM_ANGLE 0x320F
#endif /* EGL_ANGLE_platform_angle_d3d */

#ifndef EGL_ANGLE_platform_angle_opengl
#define EGL_ANGLE_platform_angle_opengl 1
#define EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE 0x320D
#define EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE 0x320E
#endif /* EGL_ANGLE_platform_angle_opengl */

#ifndef EGL_ANGLE_platform_angle_null
#define EGL_ANGLE_platform_angle_null 1
#define EGL_PLATFORM_ANGLE_TYPE_NULL_ANGLE 0x33AE
#endif /* EGL_ANGLE_platform_angle_null */

#ifndef EGL_ANGLE_platform_angle_vulkan
#define EGL_ANGLE_platform_angle_vulkan 1
#define EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE 0x3450
#endif /* EGL_ANGLE_platform_angle_vulkan */

#ifndef EGL_ANGLE_x11_visual
#define EGL_ANGLE_x11_visual
#define EGL_X11_VISUAL_ID_ANGLE 0x33A3
#endif /* EGL_ANGLE_x11_visual */

#ifndef EGL_ANGLE_flexible_surface_compatibility
#define EGL_ANGLE_flexible_surface_compatibility 1
#define EGL_FLEXIBLE_SURFACE_COMPATIBILITY_SUPPORTED_ANGLE 0x33A6
#endif /* EGL_ANGLE_flexible_surface_compatibility */

#ifndef EGL_ANGLE_surface_orientation
#define EGL_ANGLE_surface_orientation
#define EGL_OPTIMAL_SURFACE_ORIENTATION_ANGLE 0x33A7
#define EGL_SURFACE_ORIENTATION_ANGLE 0x33A8
#define EGL_SURFACE_ORIENTATION_INVERT_X_ANGLE 0x0001
#define EGL_SURFACE_ORIENTATION_INVERT_Y_ANGLE 0x0002
#endif /* EGL_ANGLE_surface_orientation */

#ifndef EGL_ANGLE_experimental_present_path
#define EGL_ANGLE_experimental_present_path
#define EGL_EXPERIMENTAL_PRESENT_PATH_ANGLE 0x33A4
#define EGL_EXPERIMENTAL_PRESENT_PATH_FAST_ANGLE 0x33A9
#define EGL_EXPERIMENTAL_PRESENT_PATH_COPY_ANGLE 0x33AA
#endif /* EGL_ANGLE_experimental_present_path */

#ifndef EGL_ANGLE_stream_producer_d3d_texture_nv12
#define EGL_ANGLE_stream_producer_d3d_texture_nv12
#define EGL_D3D_TEXTURE_SUBRESOURCE_ID_ANGLE 0x33AB
typedef EGLBoolean(EGLAPIENTRYP PFNEGLCREATESTREAMPRODUCERD3DTEXTURENV12ANGLEPROC)(EGLDisplay dpy, EGLStreamKHR stream, const EGLAttrib *attrib_list);
typedef EGLBoolean(EGLAPIENTRYP PFNEGLSTREAMPOSTD3DTEXTURENV12ANGLEPROC)(EGLDisplay dpy, EGLStreamKHR stream, void *texture, const EGLAttrib *attrib_list);
#ifdef EGL_EGLEXT_PROTOTYPES
EGLAPI EGLBoolean EGLAPIENTRY eglCreateStreamProducerD3DTextureNV12ANGLE(EGLDisplay dpy, EGLStreamKHR stream, const EGLAttrib *attrib_list);
EGLAPI EGLBoolean EGLAPIENTRY eglStreamPostD3DTextureNV12ANGLE(EGLDisplay dpy, EGLStreamKHR stream, void *texture, const EGLAttrib *attrib_list);
#endif
#endif /* EGL_ANGLE_stream_producer_d3d_texture_nv12 */

#ifndef EGL_ANGLE_create_context_webgl_compatibility
#define EGL_ANGLE_create_context_webgl_compatibility 1
#define EGL_CONTEXT_WEBGL_COMPATIBILITY_ANGLE 0x3AAC
#endif /* EGL_ANGLE_create_context_webgl_compatibility */

#ifndef EGL_ANGLE_display_texture_share_group
#define EGL_ANGLE_display_texture_share_group 1
#define EGL_DISPLAY_TEXTURE_SHARE_GROUP_ANGLE 0x3AAF
#endif /* EGL_ANGLE_display_texture_share_group */

#ifndef EGL_CHROMIUM_create_context_bind_generates_resource
#define EGL_CHROMIUM_create_context_bind_generates_resource 1
#define EGL_CONTEXT_BIND_GENERATES_RESOURCE_CHROMIUM 0x3AAD
#endif /* EGL_CHROMIUM_create_context_bind_generates_resource */

#ifndef EGL_ANGLE_create_context_client_arrays
#define EGL_ANGLE_create_context_client_arrays 1
#define EGL_CONTEXT_CLIENT_ARRAYS_ENABLED_ANGLE 0x3452
#endif /* EGL_ANGLE_create_context_client_arrays */

#ifndef EGL_ANGLE_device_creation
#define EGL_ANGLE_device_creation 1
typedef EGLDeviceEXT(EGLAPIENTRYP PFNEGLCREATEDEVICEANGLEPROC) (EGLint device_type, void *native_device, const EGLAttrib *attrib_list);
typedef EGLBoolean(EGLAPIENTRYP PFNEGLRELEASEDEVICEANGLEPROC) (EGLDeviceEXT device);
#ifdef EGL_EGLEXT_PROTOTYPES
EGLAPI EGLDeviceEXT EGLAPIENTRY eglCreateDeviceANGLE(EGLint device_type, void *native_device, const EGLAttrib *attrib_list);
EGLAPI EGLBoolean EGLAPIENTRY eglReleaseDeviceANGLE(EGLDeviceEXT device);
#endif
#endif /* EGL_ANGLE_device_creation */

#ifndef EGL_ANGLE_program_cache_control
#define EGL_ANGLE_program_cache_control 1
#define EGL_PROGRAM_CACHE_SIZE_ANGLE 0x3455
#define EGL_PROGRAM_CACHE_KEY_LENGTH_ANGLE 0x3456
#define EGL_PROGRAM_CACHE_RESIZE_ANGLE 0x3457
#define EGL_PROGRAM_CACHE_TRIM_ANGLE 0x3458
#define EGL_CONTEXT_PROGRAM_BINARY_CACHE_ENABLED_ANGLE 0x3459
typedef EGLint (EGLAPIENTRYP PFNEGLPROGRAMCACHEGETATTRIBANGLEPROC) (EGLDisplay dpy, EGLenum attrib);
typedef void (EGLAPIENTRYP PFNEGLPROGRAMCACHEQUERYANGLEPROC) (EGLDisplay dpy, EGLint index, void *key, EGLint *keysize, void *binary, EGLint *binarysize);
typedef void (EGLAPIENTRYP PFNEGPROGRAMCACHELPOPULATEANGLEPROC) (EGLDisplay dpy, const void *key, EGLint keysize, const void *binary, EGLint binarysize);
typedef EGLint (EGLAPIENTRYP PFNEGLPROGRAMCACHERESIZEANGLEPROC) (EGLDisplay dpy, EGLint limit, EGLenum mode);
#ifdef EGL_EGLEXT_PROTOTYPES
EGLAPI EGLint EGLAPIENTRY eglProgramCacheGetAttribANGLE(EGLDisplay dpy, EGLenum attrib);
EGLAPI void EGLAPIENTRY eglProgramCacheQueryANGLE(EGLDisplay dpy, EGLint index, void *key, EGLint *keysize, void *binary, EGLint *binarysize);
EGLAPI void EGLAPIENTRY eglProgramCachePopulateANGLE(EGLDisplay dpy, const void *key, EGLint keysize, const void *binary, EGLint binarysize);
EGLAPI EGLint EGLAPIENTRY eglProgramCacheResizeANGLE(EGLDisplay dpy, EGLint limit, EGLenum mode);
#endif
#endif /* EGL_ANGLE_program_cache_control */

// clang-format on

#endif  // INCLUDE_EGL_EGLEXT_ANGLE_
