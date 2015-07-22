/* GL dispatch header.
 * This is code-generated from the GL API XML files from Khronos.
 */

#pragma once
#include <inttypes.h>
#include <stddef.h>

#include "epoxy/gl.h"
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
DECLARE_HANDLE(HPGPUNV);
DECLARE_HANDLE(HGPUNV);
DECLARE_HANDLE(HVIDEOINPUTDEVICENV);
typedef struct _GPU_DEVICE GPU_DEVICE;
typedef struct _GPU_DEVICE *PGPU_DEVICE;

#define WGL_VERSION_1_0 1

#define WGL_3DFX_multisample 1
#define WGL_3DL_stereo_control 1
#define WGL_AMD_gpu_association 1
#define WGL_ARB_buffer_region 1
#define WGL_ARB_context_flush_control 1
#define WGL_ARB_create_context 1
#define WGL_ARB_create_context_profile 1
#define WGL_ARB_create_context_robustness 1
#define WGL_ARB_extensions_string 1
#define WGL_ARB_framebuffer_sRGB 1
#define WGL_ARB_make_current_read 1
#define WGL_ARB_multisample 1
#define WGL_ARB_pbuffer 1
#define WGL_ARB_pixel_format 1
#define WGL_ARB_pixel_format_float 1
#define WGL_ARB_render_texture 1
#define WGL_ARB_robustness_application_isolation 1
#define WGL_ARB_robustness_share_group_isolation 1
#define WGL_ATI_pixel_format_float 1
#define WGL_EXT_create_context_es2_profile 1
#define WGL_EXT_create_context_es_profile 1
#define WGL_EXT_depth_float 1
#define WGL_EXT_display_color_table 1
#define WGL_EXT_extensions_string 1
#define WGL_EXT_framebuffer_sRGB 1
#define WGL_EXT_make_current_read 1
#define WGL_EXT_multisample 1
#define WGL_EXT_pbuffer 1
#define WGL_EXT_pixel_format 1
#define WGL_EXT_pixel_format_packed_float 1
#define WGL_EXT_swap_control 1
#define WGL_EXT_swap_control_tear 1
#define WGL_I3D_digital_video_control 1
#define WGL_I3D_gamma 1
#define WGL_I3D_genlock 1
#define WGL_I3D_image_buffer 1
#define WGL_I3D_swap_frame_lock 1
#define WGL_I3D_swap_frame_usage 1
#define WGL_NV_DX_interop 1
#define WGL_NV_DX_interop2 1
#define WGL_NV_copy_image 1
#define WGL_NV_delay_before_swap 1
#define WGL_NV_float_buffer 1
#define WGL_NV_gpu_affinity 1
#define WGL_NV_multisample_coverage 1
#define WGL_NV_present_video 1
#define WGL_NV_render_depth_texture 1
#define WGL_NV_render_texture_rectangle 1
#define WGL_NV_swap_group 1
#define WGL_NV_vertex_array_range 1
#define WGL_NV_video_capture 1
#define WGL_NV_video_output 1
#define WGL_OML_sync_control 1

#define WGL_CONTEXT_RELEASE_BEHAVIOR_NONE_ARB            0
#define WGL_FONT_LINES                                   0
#define WGL_ACCESS_READ_ONLY_NV                          0x00000000
#define WGL_ACCESS_READ_WRITE_NV                         0x00000001
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB                 0x00000001
#define WGL_CONTEXT_DEBUG_BIT_ARB                        0x00000001
#define WGL_FRONT_COLOR_BUFFER_BIT_ARB                   0x00000001
#define WGL_IMAGE_BUFFER_MIN_ACCESS_I3D                  0x00000001
#define WGL_ACCESS_WRITE_DISCARD_NV                      0x00000002
#define WGL_BACK_COLOR_BUFFER_BIT_ARB                    0x00000002
#define WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB        0x00000002
#define WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB           0x00000002
#define WGL_IMAGE_BUFFER_LOCK_I3D                        0x00000002
#define WGL_CONTEXT_ES2_PROFILE_BIT_EXT                  0x00000004
#define WGL_CONTEXT_ES_PROFILE_BIT_EXT                   0x00000004
#define WGL_CONTEXT_ROBUST_ACCESS_BIT_ARB                0x00000004
#define WGL_DEPTH_BUFFER_BIT_ARB                         0x00000004
#define WGL_CONTEXT_RESET_ISOLATION_BIT_ARB              0x00000008
#define WGL_STENCIL_BUFFER_BIT_ARB                       0x00000008
#define WGL_GPU_VENDOR_AMD                               0x1F00
#define WGL_GPU_RENDERER_STRING_AMD                      0x1F01
#define WGL_GPU_OPENGL_VERSION_STRING_AMD                0x1F02
#define WGL_NUMBER_PIXEL_FORMATS_ARB                     0x2000
#define WGL_NUMBER_PIXEL_FORMATS_EXT                     0x2000
#define WGL_DRAW_TO_WINDOW_ARB                           0x2001
#define WGL_DRAW_TO_WINDOW_EXT                           0x2001
#define WGL_DRAW_TO_BITMAP_ARB                           0x2002
#define WGL_DRAW_TO_BITMAP_EXT                           0x2002
#define WGL_ACCELERATION_ARB                             0x2003
#define WGL_ACCELERATION_EXT                             0x2003
#define WGL_NEED_PALETTE_ARB                             0x2004
#define WGL_NEED_PALETTE_EXT                             0x2004
#define WGL_NEED_SYSTEM_PALETTE_ARB                      0x2005
#define WGL_NEED_SYSTEM_PALETTE_EXT                      0x2005
#define WGL_SWAP_LAYER_BUFFERS_ARB                       0x2006
#define WGL_SWAP_LAYER_BUFFERS_EXT                       0x2006
#define WGL_SWAP_METHOD_ARB                              0x2007
#define WGL_SWAP_METHOD_EXT                              0x2007
#define WGL_NUMBER_OVERLAYS_ARB                          0x2008
#define WGL_NUMBER_OVERLAYS_EXT                          0x2008
#define WGL_NUMBER_UNDERLAYS_ARB                         0x2009
#define WGL_NUMBER_UNDERLAYS_EXT                         0x2009
#define WGL_TRANSPARENT_ARB                              0x200A
#define WGL_TRANSPARENT_EXT                              0x200A
#define WGL_TRANSPARENT_VALUE_EXT                        0x200B
#define WGL_SHARE_DEPTH_ARB                              0x200C
#define WGL_SHARE_DEPTH_EXT                              0x200C
#define WGL_SHARE_STENCIL_ARB                            0x200D
#define WGL_SHARE_STENCIL_EXT                            0x200D
#define WGL_SHARE_ACCUM_ARB                              0x200E
#define WGL_SHARE_ACCUM_EXT                              0x200E
#define WGL_SUPPORT_GDI_ARB                              0x200F
#define WGL_SUPPORT_GDI_EXT                              0x200F
#define WGL_SUPPORT_OPENGL_ARB                           0x2010
#define WGL_SUPPORT_OPENGL_EXT                           0x2010
#define WGL_DOUBLE_BUFFER_ARB                            0x2011
#define WGL_DOUBLE_BUFFER_EXT                            0x2011
#define WGL_STEREO_ARB                                   0x2012
#define WGL_STEREO_EXT                                   0x2012
#define WGL_PIXEL_TYPE_ARB                               0x2013
#define WGL_PIXEL_TYPE_EXT                               0x2013
#define WGL_COLOR_BITS_ARB                               0x2014
#define WGL_COLOR_BITS_EXT                               0x2014
#define WGL_RED_BITS_ARB                                 0x2015
#define WGL_RED_BITS_EXT                                 0x2015
#define WGL_RED_SHIFT_ARB                                0x2016
#define WGL_RED_SHIFT_EXT                                0x2016
#define WGL_GREEN_BITS_ARB                               0x2017
#define WGL_GREEN_BITS_EXT                               0x2017
#define WGL_GREEN_SHIFT_ARB                              0x2018
#define WGL_GREEN_SHIFT_EXT                              0x2018
#define WGL_BLUE_BITS_ARB                                0x2019
#define WGL_BLUE_BITS_EXT                                0x2019
#define WGL_BLUE_SHIFT_ARB                               0x201A
#define WGL_BLUE_SHIFT_EXT                               0x201A
#define WGL_ALPHA_BITS_ARB                               0x201B
#define WGL_ALPHA_BITS_EXT                               0x201B
#define WGL_ALPHA_SHIFT_ARB                              0x201C
#define WGL_ALPHA_SHIFT_EXT                              0x201C
#define WGL_ACCUM_BITS_ARB                               0x201D
#define WGL_ACCUM_BITS_EXT                               0x201D
#define WGL_ACCUM_RED_BITS_ARB                           0x201E
#define WGL_ACCUM_RED_BITS_EXT                           0x201E
#define WGL_ACCUM_GREEN_BITS_ARB                         0x201F
#define WGL_ACCUM_GREEN_BITS_EXT                         0x201F
#define WGL_ACCUM_BLUE_BITS_ARB                          0x2020
#define WGL_ACCUM_BLUE_BITS_EXT                          0x2020
#define WGL_ACCUM_ALPHA_BITS_ARB                         0x2021
#define WGL_ACCUM_ALPHA_BITS_EXT                         0x2021
#define WGL_DEPTH_BITS_ARB                               0x2022
#define WGL_DEPTH_BITS_EXT                               0x2022
#define WGL_STENCIL_BITS_ARB                             0x2023
#define WGL_STENCIL_BITS_EXT                             0x2023
#define WGL_AUX_BUFFERS_ARB                              0x2024
#define WGL_AUX_BUFFERS_EXT                              0x2024
#define WGL_NO_ACCELERATION_ARB                          0x2025
#define WGL_NO_ACCELERATION_EXT                          0x2025
#define WGL_GENERIC_ACCELERATION_ARB                     0x2026
#define WGL_GENERIC_ACCELERATION_EXT                     0x2026
#define WGL_FULL_ACCELERATION_ARB                        0x2027
#define WGL_FULL_ACCELERATION_EXT                        0x2027
#define WGL_SWAP_EXCHANGE_ARB                            0x2028
#define WGL_SWAP_EXCHANGE_EXT                            0x2028
#define WGL_SWAP_COPY_ARB                                0x2029
#define WGL_SWAP_COPY_EXT                                0x2029
#define WGL_SWAP_UNDEFINED_ARB                           0x202A
#define WGL_SWAP_UNDEFINED_EXT                           0x202A
#define WGL_TYPE_RGBA_ARB                                0x202B
#define WGL_TYPE_RGBA_EXT                                0x202B
#define WGL_TYPE_COLORINDEX_ARB                          0x202C
#define WGL_TYPE_COLORINDEX_EXT                          0x202C
#define WGL_DRAW_TO_PBUFFER_ARB                          0x202D
#define WGL_DRAW_TO_PBUFFER_EXT                          0x202D
#define WGL_MAX_PBUFFER_PIXELS_ARB                       0x202E
#define WGL_MAX_PBUFFER_PIXELS_EXT                       0x202E
#define WGL_MAX_PBUFFER_WIDTH_ARB                        0x202F
#define WGL_MAX_PBUFFER_WIDTH_EXT                        0x202F
#define WGL_MAX_PBUFFER_HEIGHT_ARB                       0x2030
#define WGL_MAX_PBUFFER_HEIGHT_EXT                       0x2030
#define WGL_OPTIMAL_PBUFFER_WIDTH_EXT                    0x2031
#define WGL_OPTIMAL_PBUFFER_HEIGHT_EXT                   0x2032
#define WGL_PBUFFER_LARGEST_ARB                          0x2033
#define WGL_PBUFFER_LARGEST_EXT                          0x2033
#define WGL_PBUFFER_WIDTH_ARB                            0x2034
#define WGL_PBUFFER_WIDTH_EXT                            0x2034
#define WGL_PBUFFER_HEIGHT_ARB                           0x2035
#define WGL_PBUFFER_HEIGHT_EXT                           0x2035
#define WGL_PBUFFER_LOST_ARB                             0x2036
#define WGL_TRANSPARENT_RED_VALUE_ARB                    0x2037
#define WGL_TRANSPARENT_GREEN_VALUE_ARB                  0x2038
#define WGL_TRANSPARENT_BLUE_VALUE_ARB                   0x2039
#define WGL_TRANSPARENT_ALPHA_VALUE_ARB                  0x203A
#define WGL_TRANSPARENT_INDEX_VALUE_ARB                  0x203B
#define WGL_DEPTH_FLOAT_EXT                              0x2040
#define WGL_SAMPLE_BUFFERS_ARB                           0x2041
#define WGL_SAMPLE_BUFFERS_EXT                           0x2041
#define WGL_COVERAGE_SAMPLES_NV                          0x2042
#define WGL_SAMPLES_ARB                                  0x2042
#define WGL_SAMPLES_EXT                                  0x2042
#define ERROR_INVALID_PIXEL_TYPE_ARB                     0x2043
#define ERROR_INVALID_PIXEL_TYPE_EXT                     0x2043
#define WGL_GENLOCK_SOURCE_MULTIVIEW_I3D                 0x2044
#define WGL_GENLOCK_SOURCE_EXTERNAL_SYNC_I3D             0x2045
#define WGL_GENLOCK_SOURCE_EXTERNAL_FIELD_I3D            0x2046
#define WGL_GENLOCK_SOURCE_EXTERNAL_TTL_I3D              0x2047
#define WGL_GENLOCK_SOURCE_DIGITAL_SYNC_I3D              0x2048
#define WGL_GENLOCK_SOURCE_DIGITAL_FIELD_I3D             0x2049
#define WGL_GENLOCK_SOURCE_EDGE_FALLING_I3D              0x204A
#define WGL_GENLOCK_SOURCE_EDGE_RISING_I3D               0x204B
#define WGL_GENLOCK_SOURCE_EDGE_BOTH_I3D                 0x204C
#define WGL_GAMMA_TABLE_SIZE_I3D                         0x204E
#define WGL_GAMMA_EXCLUDE_DESKTOP_I3D                    0x204F
#define WGL_DIGITAL_VIDEO_CURSOR_ALPHA_FRAMEBUFFER_I3D   0x2050
#define WGL_DIGITAL_VIDEO_CURSOR_ALPHA_VALUE_I3D         0x2051
#define WGL_DIGITAL_VIDEO_CURSOR_INCLUDED_I3D            0x2052
#define WGL_DIGITAL_VIDEO_GAMMA_CORRECTED_I3D            0x2053
#define ERROR_INCOMPATIBLE_DEVICE_CONTEXTS_ARB           0x2054
#define WGL_STEREO_EMITTER_ENABLE_3DL                    0x2055
#define WGL_STEREO_EMITTER_DISABLE_3DL                   0x2056
#define WGL_STEREO_POLARITY_NORMAL_3DL                   0x2057
#define WGL_STEREO_POLARITY_INVERT_3DL                   0x2058
#define WGL_SAMPLE_BUFFERS_3DFX                          0x2060
#define WGL_SAMPLES_3DFX                                 0x2061
#define WGL_BIND_TO_TEXTURE_RGB_ARB                      0x2070
#define WGL_BIND_TO_TEXTURE_RGBA_ARB                     0x2071
#define WGL_TEXTURE_FORMAT_ARB                           0x2072
#define WGL_TEXTURE_TARGET_ARB                           0x2073
#define WGL_MIPMAP_TEXTURE_ARB                           0x2074
#define WGL_TEXTURE_RGB_ARB                              0x2075
#define WGL_TEXTURE_RGBA_ARB                             0x2076
#define WGL_NO_TEXTURE_ARB                               0x2077
#define WGL_TEXTURE_CUBE_MAP_ARB                         0x2078
#define WGL_TEXTURE_1D_ARB                               0x2079
#define WGL_TEXTURE_2D_ARB                               0x207A
#define WGL_MIPMAP_LEVEL_ARB                             0x207B
#define WGL_CUBE_MAP_FACE_ARB                            0x207C
#define WGL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB              0x207D
#define WGL_TEXTURE_CUBE_MAP_NEGATIVE_X_ARB              0x207E
#define WGL_TEXTURE_CUBE_MAP_POSITIVE_Y_ARB              0x207F
#define WGL_TEXTURE_CUBE_MAP_NEGATIVE_Y_ARB              0x2080
#define WGL_TEXTURE_CUBE_MAP_POSITIVE_Z_ARB              0x2081
#define WGL_TEXTURE_CUBE_MAP_NEGATIVE_Z_ARB              0x2082
#define WGL_FRONT_LEFT_ARB                               0x2083
#define WGL_FRONT_RIGHT_ARB                              0x2084
#define WGL_BACK_LEFT_ARB                                0x2085
#define WGL_BACK_RIGHT_ARB                               0x2086
#define WGL_AUX0_ARB                                     0x2087
#define WGL_AUX1_ARB                                     0x2088
#define WGL_AUX2_ARB                                     0x2089
#define WGL_AUX3_ARB                                     0x208A
#define WGL_AUX4_ARB                                     0x208B
#define WGL_AUX5_ARB                                     0x208C
#define WGL_AUX6_ARB                                     0x208D
#define WGL_AUX7_ARB                                     0x208E
#define WGL_AUX8_ARB                                     0x208F
#define WGL_AUX9_ARB                                     0x2090
#define WGL_CONTEXT_MAJOR_VERSION_ARB                    0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB                    0x2092
#define WGL_CONTEXT_LAYER_PLANE_ARB                      0x2093
#define WGL_CONTEXT_FLAGS_ARB                            0x2094
#define ERROR_INVALID_VERSION_ARB                        0x2095
#define ERROR_INVALID_PROFILE_ARB                        0x2096
#define WGL_CONTEXT_RELEASE_BEHAVIOR_ARB                 0x2097
#define WGL_CONTEXT_RELEASE_BEHAVIOR_FLUSH_ARB           0x2098
#define WGL_BIND_TO_TEXTURE_RECTANGLE_RGB_NV             0x20A0
#define WGL_BIND_TO_TEXTURE_RECTANGLE_RGBA_NV            0x20A1
#define WGL_TEXTURE_RECTANGLE_NV                         0x20A2
#define WGL_BIND_TO_TEXTURE_DEPTH_NV                     0x20A3
#define WGL_BIND_TO_TEXTURE_RECTANGLE_DEPTH_NV           0x20A4
#define WGL_DEPTH_TEXTURE_FORMAT_NV                      0x20A5
#define WGL_TEXTURE_DEPTH_COMPONENT_NV                   0x20A6
#define WGL_DEPTH_COMPONENT_NV                           0x20A7
#define WGL_TYPE_RGBA_UNSIGNED_FLOAT_EXT                 0x20A8
#define WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB                 0x20A9
#define WGL_FRAMEBUFFER_SRGB_CAPABLE_EXT                 0x20A9
#define WGL_FLOAT_COMPONENTS_NV                          0x20B0
#define WGL_BIND_TO_TEXTURE_RECTANGLE_FLOAT_R_NV         0x20B1
#define WGL_BIND_TO_TEXTURE_RECTANGLE_FLOAT_RG_NV        0x20B2
#define WGL_BIND_TO_TEXTURE_RECTANGLE_FLOAT_RGB_NV       0x20B3
#define WGL_BIND_TO_TEXTURE_RECTANGLE_FLOAT_RGBA_NV      0x20B4
#define WGL_TEXTURE_FLOAT_R_NV                           0x20B5
#define WGL_TEXTURE_FLOAT_RG_NV                          0x20B6
#define WGL_TEXTURE_FLOAT_RGB_NV                         0x20B7
#define WGL_TEXTURE_FLOAT_RGBA_NV                        0x20B8
#define WGL_COLOR_SAMPLES_NV                             0x20B9
#define WGL_BIND_TO_VIDEO_RGB_NV                         0x20C0
#define WGL_BIND_TO_VIDEO_RGBA_NV                        0x20C1
#define WGL_BIND_TO_VIDEO_RGB_AND_DEPTH_NV               0x20C2
#define WGL_VIDEO_OUT_COLOR_NV                           0x20C3
#define WGL_VIDEO_OUT_ALPHA_NV                           0x20C4
#define WGL_VIDEO_OUT_DEPTH_NV                           0x20C5
#define WGL_VIDEO_OUT_COLOR_AND_ALPHA_NV                 0x20C6
#define WGL_VIDEO_OUT_COLOR_AND_DEPTH_NV                 0x20C7
#define WGL_VIDEO_OUT_FRAME                              0x20C8
#define WGL_VIDEO_OUT_FIELD_1                            0x20C9
#define WGL_VIDEO_OUT_FIELD_2                            0x20CA
#define WGL_VIDEO_OUT_STACKED_FIELDS_1_2                 0x20CB
#define WGL_VIDEO_OUT_STACKED_FIELDS_2_1                 0x20CC
#define WGL_UNIQUE_ID_NV                                 0x20CE
#define WGL_NUM_VIDEO_CAPTURE_SLOTS_NV                   0x20CF
#define ERROR_INCOMPATIBLE_AFFINITY_MASKS_NV             0x20D0
#define ERROR_MISSING_AFFINITY_MASK_NV                   0x20D1
#define WGL_NUM_VIDEO_SLOTS_NV                           0x20F0
#define WGL_TYPE_RGBA_FLOAT_ARB                          0x21A0
#define WGL_TYPE_RGBA_FLOAT_ATI                          0x21A0
#define WGL_GPU_FASTEST_TARGET_GPUS_AMD                  0x21A2
#define WGL_GPU_RAM_AMD                                  0x21A3
#define WGL_GPU_CLOCK_AMD                                0x21A4
#define WGL_GPU_NUM_PIPES_AMD                            0x21A5
#define WGL_GPU_NUM_SIMD_AMD                             0x21A6
#define WGL_GPU_NUM_RB_AMD                               0x21A7
#define WGL_GPU_NUM_SPI_AMD                              0x21A8
#define WGL_LOSE_CONTEXT_ON_RESET_ARB                    0x8252
#define WGL_CONTEXT_RESET_NOTIFICATION_STRATEGY_ARB      0x8256
#define WGL_NO_RESET_NOTIFICATION_ARB                    0x8261
#define WGL_CONTEXT_PROFILE_MASK_ARB                     0x9126
#define WGL_FONT_POLYGONS                                1

typedef void * (GLAPIENTRY *PFNWGLALLOCATEMEMORYNVPROC)(GLsizei size, GLfloat readfreq, GLfloat writefreq, GLfloat priority);
typedef BOOL (GLAPIENTRY *PFNWGLASSOCIATEIMAGEBUFFEREVENTSI3DPROC)(HDC hDC, const HANDLE * pEvent, const LPVOID * pAddress, const DWORD * pSize, UINT count);
typedef BOOL (GLAPIENTRY *PFNWGLBEGINFRAMETRACKINGI3DPROC)(void);
typedef GLboolean (GLAPIENTRY *PFNWGLBINDDISPLAYCOLORTABLEEXTPROC)(GLushort id);
typedef BOOL (GLAPIENTRY *PFNWGLBINDSWAPBARRIERNVPROC)(GLuint group, GLuint barrier);
typedef BOOL (GLAPIENTRY *PFNWGLBINDTEXIMAGEARBPROC)(HPBUFFERARB hPbuffer, int iBuffer);
typedef BOOL (GLAPIENTRY *PFNWGLBINDVIDEOCAPTUREDEVICENVPROC)(UINT uVideoSlot, HVIDEOINPUTDEVICENV hDevice);
typedef BOOL (GLAPIENTRY *PFNWGLBINDVIDEODEVICENVPROC)(HDC hDC, unsigned int uVideoSlot, HVIDEOOUTPUTDEVICENV hVideoDevice, const int * piAttribList);
typedef BOOL (GLAPIENTRY *PFNWGLBINDVIDEOIMAGENVPROC)(HPVIDEODEV hVideoDevice, HPBUFFERARB hPbuffer, int iVideoBuffer);
typedef VOID (GLAPIENTRY *PFNWGLBLITCONTEXTFRAMEBUFFERAMDPROC)(HGLRC dstCtx, GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);
typedef BOOL (GLAPIENTRY *PFNWGLCHOOSEPIXELFORMATARBPROC)(HDC hdc, const int * piAttribIList, const FLOAT * pfAttribFList, UINT nMaxFormats, int * piFormats, UINT * nNumFormats);
typedef BOOL (GLAPIENTRY *PFNWGLCHOOSEPIXELFORMATEXTPROC)(HDC hdc, const int * piAttribIList, const FLOAT * pfAttribFList, UINT nMaxFormats, int * piFormats, UINT * nNumFormats);
typedef BOOL (GLAPIENTRY *PFNWGLCOPYCONTEXTPROC)(HGLRC hglrcSrc, HGLRC hglrcDst, UINT mask);
typedef BOOL (GLAPIENTRY *PFNWGLCOPYIMAGESUBDATANVPROC)(HGLRC hSrcRC, GLuint srcName, GLenum srcTarget, GLint srcLevel, GLint srcX, GLint srcY, GLint srcZ, HGLRC hDstRC, GLuint dstName, GLenum dstTarget, GLint dstLevel, GLint dstX, GLint dstY, GLint dstZ, GLsizei width, GLsizei height, GLsizei depth);
typedef HDC (GLAPIENTRY *PFNWGLCREATEAFFINITYDCNVPROC)(const HGPUNV * phGpuList);
typedef HGLRC (GLAPIENTRY *PFNWGLCREATEASSOCIATEDCONTEXTAMDPROC)(UINT id);
typedef HGLRC (GLAPIENTRY *PFNWGLCREATEASSOCIATEDCONTEXTATTRIBSAMDPROC)(UINT id, HGLRC hShareContext, const int * attribList);
typedef HANDLE (GLAPIENTRY *PFNWGLCREATEBUFFERREGIONARBPROC)(HDC hDC, int iLayerPlane, UINT uType);
typedef HGLRC (GLAPIENTRY *PFNWGLCREATECONTEXTPROC)(HDC hDc);
typedef HGLRC (GLAPIENTRY *PFNWGLCREATECONTEXTATTRIBSARBPROC)(HDC hDC, HGLRC hShareContext, const int * attribList);
typedef GLboolean (GLAPIENTRY *PFNWGLCREATEDISPLAYCOLORTABLEEXTPROC)(GLushort id);
typedef LPVOID (GLAPIENTRY *PFNWGLCREATEIMAGEBUFFERI3DPROC)(HDC hDC, DWORD dwSize, UINT uFlags);
typedef HGLRC (GLAPIENTRY *PFNWGLCREATELAYERCONTEXTPROC)(HDC hDc, int level);
typedef HPBUFFERARB (GLAPIENTRY *PFNWGLCREATEPBUFFERARBPROC)(HDC hDC, int iPixelFormat, int iWidth, int iHeight, const int * piAttribList);
typedef HPBUFFEREXT (GLAPIENTRY *PFNWGLCREATEPBUFFEREXTPROC)(HDC hDC, int iPixelFormat, int iWidth, int iHeight, const int * piAttribList);
typedef BOOL (GLAPIENTRY *PFNWGLDXCLOSEDEVICENVPROC)(HANDLE hDevice);
typedef BOOL (GLAPIENTRY *PFNWGLDXLOCKOBJECTSNVPROC)(HANDLE hDevice, GLint count, HANDLE * hObjects);
typedef BOOL (GLAPIENTRY *PFNWGLDXOBJECTACCESSNVPROC)(HANDLE hObject, GLenum access);
typedef HANDLE (GLAPIENTRY *PFNWGLDXOPENDEVICENVPROC)(void * dxDevice);
typedef HANDLE (GLAPIENTRY *PFNWGLDXREGISTEROBJECTNVPROC)(HANDLE hDevice, void * dxObject, GLuint name, GLenum type, GLenum access);
typedef BOOL (GLAPIENTRY *PFNWGLDXSETRESOURCESHAREHANDLENVPROC)(void * dxObject, HANDLE shareHandle);
typedef BOOL (GLAPIENTRY *PFNWGLDXUNLOCKOBJECTSNVPROC)(HANDLE hDevice, GLint count, HANDLE * hObjects);
typedef BOOL (GLAPIENTRY *PFNWGLDXUNREGISTEROBJECTNVPROC)(HANDLE hDevice, HANDLE hObject);
typedef BOOL (GLAPIENTRY *PFNWGLDELAYBEFORESWAPNVPROC)(HDC hDC, GLfloat seconds);
typedef BOOL (GLAPIENTRY *PFNWGLDELETEASSOCIATEDCONTEXTAMDPROC)(HGLRC hglrc);
typedef VOID (GLAPIENTRY *PFNWGLDELETEBUFFERREGIONARBPROC)(HANDLE hRegion);
typedef BOOL (GLAPIENTRY *PFNWGLDELETECONTEXTPROC)(HGLRC oldContext);
typedef BOOL (GLAPIENTRY *PFNWGLDELETEDCNVPROC)(HDC hdc);
typedef BOOL (GLAPIENTRY *PFNWGLDESCRIBELAYERPLANEPROC)(HDC hDc, int pixelFormat, int layerPlane, UINT nBytes, const LAYERPLANEDESCRIPTOR * plpd);
typedef VOID (GLAPIENTRY *PFNWGLDESTROYDISPLAYCOLORTABLEEXTPROC)(GLushort id);
typedef BOOL (GLAPIENTRY *PFNWGLDESTROYIMAGEBUFFERI3DPROC)(HDC hDC, LPVOID pAddress);
typedef BOOL (GLAPIENTRY *PFNWGLDESTROYPBUFFERARBPROC)(HPBUFFERARB hPbuffer);
typedef BOOL (GLAPIENTRY *PFNWGLDESTROYPBUFFEREXTPROC)(HPBUFFEREXT hPbuffer);
typedef BOOL (GLAPIENTRY *PFNWGLDISABLEFRAMELOCKI3DPROC)(void);
typedef BOOL (GLAPIENTRY *PFNWGLDISABLEGENLOCKI3DPROC)(HDC hDC);
typedef BOOL (GLAPIENTRY *PFNWGLENABLEFRAMELOCKI3DPROC)(void);
typedef BOOL (GLAPIENTRY *PFNWGLENABLEGENLOCKI3DPROC)(HDC hDC);
typedef BOOL (GLAPIENTRY *PFNWGLENDFRAMETRACKINGI3DPROC)(void);
typedef BOOL (GLAPIENTRY *PFNWGLENUMGPUDEVICESNVPROC)(HGPUNV hGpu, UINT iDeviceIndex, PGPU_DEVICE lpGpuDevice);
typedef BOOL (GLAPIENTRY *PFNWGLENUMGPUSFROMAFFINITYDCNVPROC)(HDC hAffinityDC, UINT iGpuIndex, HGPUNV * hGpu);
typedef BOOL (GLAPIENTRY *PFNWGLENUMGPUSNVPROC)(UINT iGpuIndex, HGPUNV * phGpu);
typedef UINT (GLAPIENTRY *PFNWGLENUMERATEVIDEOCAPTUREDEVICESNVPROC)(HDC hDc, HVIDEOINPUTDEVICENV * phDeviceList);
typedef int (GLAPIENTRY *PFNWGLENUMERATEVIDEODEVICESNVPROC)(HDC hDC, HVIDEOOUTPUTDEVICENV * phDeviceList);
typedef void (GLAPIENTRY *PFNWGLFREEMEMORYNVPROC)(void * pointer);
typedef BOOL (GLAPIENTRY *PFNWGLGENLOCKSAMPLERATEI3DPROC)(HDC hDC, UINT uRate);
typedef BOOL (GLAPIENTRY *PFNWGLGENLOCKSOURCEDELAYI3DPROC)(HDC hDC, UINT uDelay);
typedef BOOL (GLAPIENTRY *PFNWGLGENLOCKSOURCEEDGEI3DPROC)(HDC hDC, UINT uEdge);
typedef BOOL (GLAPIENTRY *PFNWGLGENLOCKSOURCEI3DPROC)(HDC hDC, UINT uSource);
typedef UINT (GLAPIENTRY *PFNWGLGETCONTEXTGPUIDAMDPROC)(HGLRC hglrc);
typedef HGLRC (GLAPIENTRY *PFNWGLGETCURRENTASSOCIATEDCONTEXTAMDPROC)(void);
typedef HGLRC (GLAPIENTRY *PFNWGLGETCURRENTCONTEXTPROC)(void);
typedef HDC (GLAPIENTRY *PFNWGLGETCURRENTDCPROC)(void);
typedef HDC (GLAPIENTRY *PFNWGLGETCURRENTREADDCARBPROC)(void);
typedef HDC (GLAPIENTRY *PFNWGLGETCURRENTREADDCEXTPROC)(void);
typedef PROC (GLAPIENTRY *PFNWGLGETDEFAULTPROCADDRESSPROC)(LPCSTR lpszProc);
typedef BOOL (GLAPIENTRY *PFNWGLGETDIGITALVIDEOPARAMETERSI3DPROC)(HDC hDC, int iAttribute, int * piValue);
typedef const char * (GLAPIENTRY *PFNWGLGETEXTENSIONSSTRINGARBPROC)(HDC hdc);
typedef const char * (GLAPIENTRY *PFNWGLGETEXTENSIONSSTRINGEXTPROC)(void);
typedef BOOL (GLAPIENTRY *PFNWGLGETFRAMEUSAGEI3DPROC)(float * pUsage);
typedef UINT (GLAPIENTRY *PFNWGLGETGPUIDSAMDPROC)(UINT maxCount, UINT * ids);
typedef INT (GLAPIENTRY *PFNWGLGETGPUINFOAMDPROC)(UINT id, int property, GLenum dataType, UINT size, void * data);
typedef BOOL (GLAPIENTRY *PFNWGLGETGAMMATABLEI3DPROC)(HDC hDC, int iEntries, USHORT * puRed, USHORT * puGreen, USHORT * puBlue);
typedef BOOL (GLAPIENTRY *PFNWGLGETGAMMATABLEPARAMETERSI3DPROC)(HDC hDC, int iAttribute, int * piValue);
typedef BOOL (GLAPIENTRY *PFNWGLGETGENLOCKSAMPLERATEI3DPROC)(HDC hDC, UINT * uRate);
typedef BOOL (GLAPIENTRY *PFNWGLGETGENLOCKSOURCEDELAYI3DPROC)(HDC hDC, UINT * uDelay);
typedef BOOL (GLAPIENTRY *PFNWGLGETGENLOCKSOURCEEDGEI3DPROC)(HDC hDC, UINT * uEdge);
typedef BOOL (GLAPIENTRY *PFNWGLGETGENLOCKSOURCEI3DPROC)(HDC hDC, UINT * uSource);
typedef int (GLAPIENTRY *PFNWGLGETLAYERPALETTEENTRIESPROC)(HDC hdc, int iLayerPlane, int iStart, int cEntries, const COLORREF * pcr);
typedef BOOL (GLAPIENTRY *PFNWGLGETMSCRATEOMLPROC)(HDC hdc, INT32 * numerator, INT32 * denominator);
typedef HDC (GLAPIENTRY *PFNWGLGETPBUFFERDCARBPROC)(HPBUFFERARB hPbuffer);
typedef HDC (GLAPIENTRY *PFNWGLGETPBUFFERDCEXTPROC)(HPBUFFEREXT hPbuffer);
typedef BOOL (GLAPIENTRY *PFNWGLGETPIXELFORMATATTRIBFVARBPROC)(HDC hdc, int iPixelFormat, int iLayerPlane, UINT nAttributes, const int * piAttributes, FLOAT * pfValues);
typedef BOOL (GLAPIENTRY *PFNWGLGETPIXELFORMATATTRIBFVEXTPROC)(HDC hdc, int iPixelFormat, int iLayerPlane, UINT nAttributes, int * piAttributes, FLOAT * pfValues);
typedef BOOL (GLAPIENTRY *PFNWGLGETPIXELFORMATATTRIBIVARBPROC)(HDC hdc, int iPixelFormat, int iLayerPlane, UINT nAttributes, const int * piAttributes, int * piValues);
typedef BOOL (GLAPIENTRY *PFNWGLGETPIXELFORMATATTRIBIVEXTPROC)(HDC hdc, int iPixelFormat, int iLayerPlane, UINT nAttributes, int * piAttributes, int * piValues);
typedef PROC (GLAPIENTRY *PFNWGLGETPROCADDRESSPROC)(LPCSTR lpszProc);
typedef int (GLAPIENTRY *PFNWGLGETSWAPINTERVALEXTPROC)(void);
typedef BOOL (GLAPIENTRY *PFNWGLGETSYNCVALUESOMLPROC)(HDC hdc, INT64 * ust, INT64 * msc, INT64 * sbc);
typedef BOOL (GLAPIENTRY *PFNWGLGETVIDEODEVICENVPROC)(HDC hDC, int numDevices, HPVIDEODEV * hVideoDevice);
typedef BOOL (GLAPIENTRY *PFNWGLGETVIDEOINFONVPROC)(HPVIDEODEV hpVideoDevice, unsigned long * pulCounterOutputPbuffer, unsigned long * pulCounterOutputVideo);
typedef BOOL (GLAPIENTRY *PFNWGLISENABLEDFRAMELOCKI3DPROC)(BOOL * pFlag);
typedef BOOL (GLAPIENTRY *PFNWGLISENABLEDGENLOCKI3DPROC)(HDC hDC, BOOL * pFlag);
typedef BOOL (GLAPIENTRY *PFNWGLJOINSWAPGROUPNVPROC)(HDC hDC, GLuint group);
typedef GLboolean (GLAPIENTRY *PFNWGLLOADDISPLAYCOLORTABLEEXTPROC)(const GLushort * table, GLuint length);
typedef BOOL (GLAPIENTRY *PFNWGLLOCKVIDEOCAPTUREDEVICENVPROC)(HDC hDc, HVIDEOINPUTDEVICENV hDevice);
typedef BOOL (GLAPIENTRY *PFNWGLMAKEASSOCIATEDCONTEXTCURRENTAMDPROC)(HGLRC hglrc);
typedef BOOL (GLAPIENTRY *PFNWGLMAKECONTEXTCURRENTARBPROC)(HDC hDrawDC, HDC hReadDC, HGLRC hglrc);
typedef BOOL (GLAPIENTRY *PFNWGLMAKECONTEXTCURRENTEXTPROC)(HDC hDrawDC, HDC hReadDC, HGLRC hglrc);
typedef BOOL (GLAPIENTRY *PFNWGLMAKECURRENTPROC)(HDC hDc, HGLRC newContext);
typedef BOOL (GLAPIENTRY *PFNWGLQUERYCURRENTCONTEXTNVPROC)(int iAttribute, int * piValue);
typedef BOOL (GLAPIENTRY *PFNWGLQUERYFRAMECOUNTNVPROC)(HDC hDC, GLuint * count);
typedef BOOL (GLAPIENTRY *PFNWGLQUERYFRAMELOCKMASTERI3DPROC)(BOOL * pFlag);
typedef BOOL (GLAPIENTRY *PFNWGLQUERYFRAMETRACKINGI3DPROC)(DWORD * pFrameCount, DWORD * pMissedFrames, float * pLastMissedUsage);
typedef BOOL (GLAPIENTRY *PFNWGLQUERYGENLOCKMAXSOURCEDELAYI3DPROC)(HDC hDC, UINT * uMaxLineDelay, UINT * uMaxPixelDelay);
typedef BOOL (GLAPIENTRY *PFNWGLQUERYMAXSWAPGROUPSNVPROC)(HDC hDC, GLuint * maxGroups, GLuint * maxBarriers);
typedef BOOL (GLAPIENTRY *PFNWGLQUERYPBUFFERARBPROC)(HPBUFFERARB hPbuffer, int iAttribute, int * piValue);
typedef BOOL (GLAPIENTRY *PFNWGLQUERYPBUFFEREXTPROC)(HPBUFFEREXT hPbuffer, int iAttribute, int * piValue);
typedef BOOL (GLAPIENTRY *PFNWGLQUERYSWAPGROUPNVPROC)(HDC hDC, GLuint * group, GLuint * barrier);
typedef BOOL (GLAPIENTRY *PFNWGLQUERYVIDEOCAPTUREDEVICENVPROC)(HDC hDc, HVIDEOINPUTDEVICENV hDevice, int iAttribute, int * piValue);
typedef BOOL (GLAPIENTRY *PFNWGLREALIZELAYERPALETTEPROC)(HDC hdc, int iLayerPlane, BOOL bRealize);
typedef BOOL (GLAPIENTRY *PFNWGLRELEASEIMAGEBUFFEREVENTSI3DPROC)(HDC hDC, const LPVOID * pAddress, UINT count);
typedef int (GLAPIENTRY *PFNWGLRELEASEPBUFFERDCARBPROC)(HPBUFFERARB hPbuffer, HDC hDC);
typedef int (GLAPIENTRY *PFNWGLRELEASEPBUFFERDCEXTPROC)(HPBUFFEREXT hPbuffer, HDC hDC);
typedef BOOL (GLAPIENTRY *PFNWGLRELEASETEXIMAGEARBPROC)(HPBUFFERARB hPbuffer, int iBuffer);
typedef BOOL (GLAPIENTRY *PFNWGLRELEASEVIDEOCAPTUREDEVICENVPROC)(HDC hDc, HVIDEOINPUTDEVICENV hDevice);
typedef BOOL (GLAPIENTRY *PFNWGLRELEASEVIDEODEVICENVPROC)(HPVIDEODEV hVideoDevice);
typedef BOOL (GLAPIENTRY *PFNWGLRELEASEVIDEOIMAGENVPROC)(HPBUFFERARB hPbuffer, int iVideoBuffer);
typedef BOOL (GLAPIENTRY *PFNWGLRESETFRAMECOUNTNVPROC)(HDC hDC);
typedef BOOL (GLAPIENTRY *PFNWGLRESTOREBUFFERREGIONARBPROC)(HANDLE hRegion, int x, int y, int width, int height, int xSrc, int ySrc);
typedef BOOL (GLAPIENTRY *PFNWGLSAVEBUFFERREGIONARBPROC)(HANDLE hRegion, int x, int y, int width, int height);
typedef BOOL (GLAPIENTRY *PFNWGLSENDPBUFFERTOVIDEONVPROC)(HPBUFFERARB hPbuffer, int iBufferType, unsigned long * pulCounterPbuffer, BOOL bBlock);
typedef BOOL (GLAPIENTRY *PFNWGLSETDIGITALVIDEOPARAMETERSI3DPROC)(HDC hDC, int iAttribute, const int * piValue);
typedef BOOL (GLAPIENTRY *PFNWGLSETGAMMATABLEI3DPROC)(HDC hDC, int iEntries, const USHORT * puRed, const USHORT * puGreen, const USHORT * puBlue);
typedef BOOL (GLAPIENTRY *PFNWGLSETGAMMATABLEPARAMETERSI3DPROC)(HDC hDC, int iAttribute, const int * piValue);
typedef int (GLAPIENTRY *PFNWGLSETLAYERPALETTEENTRIESPROC)(HDC hdc, int iLayerPlane, int iStart, int cEntries, const COLORREF * pcr);
typedef BOOL (GLAPIENTRY *PFNWGLSETPBUFFERATTRIBARBPROC)(HPBUFFERARB hPbuffer, const int * piAttribList);
typedef BOOL (GLAPIENTRY *PFNWGLSETSTEREOEMITTERSTATE3DLPROC)(HDC hDC, UINT uState);
typedef BOOL (GLAPIENTRY *PFNWGLSHARELISTSPROC)(HGLRC hrcSrvShare, HGLRC hrcSrvSource);
typedef INT64 (GLAPIENTRY *PFNWGLSWAPBUFFERSMSCOMLPROC)(HDC hdc, INT64 target_msc, INT64 divisor, INT64 remainder);
typedef BOOL (GLAPIENTRY *PFNWGLSWAPINTERVALEXTPROC)(int interval);
typedef BOOL (GLAPIENTRY *PFNWGLSWAPLAYERBUFFERSPROC)(HDC hdc, UINT fuFlags);
typedef INT64 (GLAPIENTRY *PFNWGLSWAPLAYERBUFFERSMSCOMLPROC)(HDC hdc, int fuPlanes, INT64 target_msc, INT64 divisor, INT64 remainder);
typedef BOOL (GLAPIENTRY *PFNWGLUSEFONTBITMAPSAPROC)(HDC hDC, DWORD first, DWORD count, DWORD listBase);
typedef BOOL (GLAPIENTRY *PFNWGLUSEFONTBITMAPSWPROC)(HDC hDC, DWORD first, DWORD count, DWORD listBase);
typedef BOOL (GLAPIENTRY *PFNWGLUSEFONTOUTLINESPROC)(HDC hDC, DWORD first, DWORD count, DWORD listBase, FLOAT deviation, FLOAT extrusion, int format, LPGLYPHMETRICSFLOAT lpgmf);
typedef BOOL (GLAPIENTRY *PFNWGLUSEFONTOUTLINESAPROC)(HDC hDC, DWORD first, DWORD count, DWORD listBase, FLOAT deviation, FLOAT extrusion, int format, LPGLYPHMETRICSFLOAT lpgmf);
typedef BOOL (GLAPIENTRY *PFNWGLUSEFONTOUTLINESWPROC)(HDC hDC, DWORD first, DWORD count, DWORD listBase, FLOAT deviation, FLOAT extrusion, int format, LPGLYPHMETRICSFLOAT lpgmf);
typedef BOOL (GLAPIENTRY *PFNWGLWAITFORMSCOMLPROC)(HDC hdc, INT64 target_msc, INT64 divisor, INT64 remainder, INT64 * ust, INT64 * msc, INT64 * sbc);
typedef BOOL (GLAPIENTRY *PFNWGLWAITFORSBCOMLPROC)(HDC hdc, INT64 target_sbc, INT64 * ust, INT64 * msc, INT64 * sbc);
extern EPOXY_IMPORTEXPORT void * (EPOXY_CALLSPEC *epoxy_wglAllocateMemoryNV)(GLsizei size, GLfloat readfreq, GLfloat writefreq, GLfloat priority);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglAssociateImageBufferEventsI3D)(HDC hDC, const HANDLE * pEvent, const LPVOID * pAddress, const DWORD * pSize, UINT count);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglBeginFrameTrackingI3D)(void);

extern EPOXY_IMPORTEXPORT GLboolean (EPOXY_CALLSPEC *epoxy_wglBindDisplayColorTableEXT)(GLushort id);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglBindSwapBarrierNV)(GLuint group, GLuint barrier);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglBindTexImageARB)(HPBUFFERARB hPbuffer, int iBuffer);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglBindVideoCaptureDeviceNV)(UINT uVideoSlot, HVIDEOINPUTDEVICENV hDevice);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglBindVideoDeviceNV)(HDC hDC, unsigned int uVideoSlot, HVIDEOOUTPUTDEVICENV hVideoDevice, const int * piAttribList);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglBindVideoImageNV)(HPVIDEODEV hVideoDevice, HPBUFFERARB hPbuffer, int iVideoBuffer);

extern EPOXY_IMPORTEXPORT VOID (EPOXY_CALLSPEC *epoxy_wglBlitContextFramebufferAMD)(HGLRC dstCtx, GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglChoosePixelFormatARB)(HDC hdc, const int * piAttribIList, const FLOAT * pfAttribFList, UINT nMaxFormats, int * piFormats, UINT * nNumFormats);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglChoosePixelFormatEXT)(HDC hdc, const int * piAttribIList, const FLOAT * pfAttribFList, UINT nMaxFormats, int * piFormats, UINT * nNumFormats);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglCopyContext)(HGLRC hglrcSrc, HGLRC hglrcDst, UINT mask);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglCopyImageSubDataNV)(HGLRC hSrcRC, GLuint srcName, GLenum srcTarget, GLint srcLevel, GLint srcX, GLint srcY, GLint srcZ, HGLRC hDstRC, GLuint dstName, GLenum dstTarget, GLint dstLevel, GLint dstX, GLint dstY, GLint dstZ, GLsizei width, GLsizei height, GLsizei depth);

extern EPOXY_IMPORTEXPORT HDC (EPOXY_CALLSPEC *epoxy_wglCreateAffinityDCNV)(const HGPUNV * phGpuList);

extern EPOXY_IMPORTEXPORT HGLRC (EPOXY_CALLSPEC *epoxy_wglCreateAssociatedContextAMD)(UINT id);

extern EPOXY_IMPORTEXPORT HGLRC (EPOXY_CALLSPEC *epoxy_wglCreateAssociatedContextAttribsAMD)(UINT id, HGLRC hShareContext, const int * attribList);

extern EPOXY_IMPORTEXPORT HANDLE (EPOXY_CALLSPEC *epoxy_wglCreateBufferRegionARB)(HDC hDC, int iLayerPlane, UINT uType);

extern EPOXY_IMPORTEXPORT HGLRC (EPOXY_CALLSPEC *epoxy_wglCreateContext)(HDC hDc);

extern EPOXY_IMPORTEXPORT HGLRC (EPOXY_CALLSPEC *epoxy_wglCreateContextAttribsARB)(HDC hDC, HGLRC hShareContext, const int * attribList);

extern EPOXY_IMPORTEXPORT GLboolean (EPOXY_CALLSPEC *epoxy_wglCreateDisplayColorTableEXT)(GLushort id);

extern EPOXY_IMPORTEXPORT LPVOID (EPOXY_CALLSPEC *epoxy_wglCreateImageBufferI3D)(HDC hDC, DWORD dwSize, UINT uFlags);

extern EPOXY_IMPORTEXPORT HGLRC (EPOXY_CALLSPEC *epoxy_wglCreateLayerContext)(HDC hDc, int level);

extern EPOXY_IMPORTEXPORT HPBUFFERARB (EPOXY_CALLSPEC *epoxy_wglCreatePbufferARB)(HDC hDC, int iPixelFormat, int iWidth, int iHeight, const int * piAttribList);

extern EPOXY_IMPORTEXPORT HPBUFFEREXT (EPOXY_CALLSPEC *epoxy_wglCreatePbufferEXT)(HDC hDC, int iPixelFormat, int iWidth, int iHeight, const int * piAttribList);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglDXCloseDeviceNV)(HANDLE hDevice);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglDXLockObjectsNV)(HANDLE hDevice, GLint count, HANDLE * hObjects);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglDXObjectAccessNV)(HANDLE hObject, GLenum access);

extern EPOXY_IMPORTEXPORT HANDLE (EPOXY_CALLSPEC *epoxy_wglDXOpenDeviceNV)(void * dxDevice);

extern EPOXY_IMPORTEXPORT HANDLE (EPOXY_CALLSPEC *epoxy_wglDXRegisterObjectNV)(HANDLE hDevice, void * dxObject, GLuint name, GLenum type, GLenum access);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglDXSetResourceShareHandleNV)(void * dxObject, HANDLE shareHandle);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglDXUnlockObjectsNV)(HANDLE hDevice, GLint count, HANDLE * hObjects);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglDXUnregisterObjectNV)(HANDLE hDevice, HANDLE hObject);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglDelayBeforeSwapNV)(HDC hDC, GLfloat seconds);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglDeleteAssociatedContextAMD)(HGLRC hglrc);

extern EPOXY_IMPORTEXPORT VOID (EPOXY_CALLSPEC *epoxy_wglDeleteBufferRegionARB)(HANDLE hRegion);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglDeleteContext)(HGLRC oldContext);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglDeleteDCNV)(HDC hdc);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglDescribeLayerPlane)(HDC hDc, int pixelFormat, int layerPlane, UINT nBytes, const LAYERPLANEDESCRIPTOR * plpd);

extern EPOXY_IMPORTEXPORT VOID (EPOXY_CALLSPEC *epoxy_wglDestroyDisplayColorTableEXT)(GLushort id);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglDestroyImageBufferI3D)(HDC hDC, LPVOID pAddress);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglDestroyPbufferARB)(HPBUFFERARB hPbuffer);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglDestroyPbufferEXT)(HPBUFFEREXT hPbuffer);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglDisableFrameLockI3D)(void);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglDisableGenlockI3D)(HDC hDC);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglEnableFrameLockI3D)(void);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglEnableGenlockI3D)(HDC hDC);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglEndFrameTrackingI3D)(void);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglEnumGpuDevicesNV)(HGPUNV hGpu, UINT iDeviceIndex, PGPU_DEVICE lpGpuDevice);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglEnumGpusFromAffinityDCNV)(HDC hAffinityDC, UINT iGpuIndex, HGPUNV * hGpu);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglEnumGpusNV)(UINT iGpuIndex, HGPUNV * phGpu);

extern EPOXY_IMPORTEXPORT UINT (EPOXY_CALLSPEC *epoxy_wglEnumerateVideoCaptureDevicesNV)(HDC hDc, HVIDEOINPUTDEVICENV * phDeviceList);

extern EPOXY_IMPORTEXPORT int (EPOXY_CALLSPEC *epoxy_wglEnumerateVideoDevicesNV)(HDC hDC, HVIDEOOUTPUTDEVICENV * phDeviceList);

extern EPOXY_IMPORTEXPORT void (EPOXY_CALLSPEC *epoxy_wglFreeMemoryNV)(void * pointer);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglGenlockSampleRateI3D)(HDC hDC, UINT uRate);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglGenlockSourceDelayI3D)(HDC hDC, UINT uDelay);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglGenlockSourceEdgeI3D)(HDC hDC, UINT uEdge);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglGenlockSourceI3D)(HDC hDC, UINT uSource);

extern EPOXY_IMPORTEXPORT UINT (EPOXY_CALLSPEC *epoxy_wglGetContextGPUIDAMD)(HGLRC hglrc);

extern EPOXY_IMPORTEXPORT HGLRC (EPOXY_CALLSPEC *epoxy_wglGetCurrentAssociatedContextAMD)(void);

extern EPOXY_IMPORTEXPORT HGLRC (EPOXY_CALLSPEC *epoxy_wglGetCurrentContext)(void);

extern EPOXY_IMPORTEXPORT HDC (EPOXY_CALLSPEC *epoxy_wglGetCurrentDC)(void);

extern EPOXY_IMPORTEXPORT HDC (EPOXY_CALLSPEC *epoxy_wglGetCurrentReadDCARB)(void);

extern EPOXY_IMPORTEXPORT HDC (EPOXY_CALLSPEC *epoxy_wglGetCurrentReadDCEXT)(void);

extern EPOXY_IMPORTEXPORT PROC (EPOXY_CALLSPEC *epoxy_wglGetDefaultProcAddress)(LPCSTR lpszProc);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglGetDigitalVideoParametersI3D)(HDC hDC, int iAttribute, int * piValue);

extern EPOXY_IMPORTEXPORT const char * (EPOXY_CALLSPEC *epoxy_wglGetExtensionsStringARB)(HDC hdc);

extern EPOXY_IMPORTEXPORT const char * (EPOXY_CALLSPEC *epoxy_wglGetExtensionsStringEXT)(void);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglGetFrameUsageI3D)(float * pUsage);

extern EPOXY_IMPORTEXPORT UINT (EPOXY_CALLSPEC *epoxy_wglGetGPUIDsAMD)(UINT maxCount, UINT * ids);

extern EPOXY_IMPORTEXPORT INT (EPOXY_CALLSPEC *epoxy_wglGetGPUInfoAMD)(UINT id, int property, GLenum dataType, UINT size, void * data);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglGetGammaTableI3D)(HDC hDC, int iEntries, USHORT * puRed, USHORT * puGreen, USHORT * puBlue);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglGetGammaTableParametersI3D)(HDC hDC, int iAttribute, int * piValue);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglGetGenlockSampleRateI3D)(HDC hDC, UINT * uRate);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglGetGenlockSourceDelayI3D)(HDC hDC, UINT * uDelay);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglGetGenlockSourceEdgeI3D)(HDC hDC, UINT * uEdge);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglGetGenlockSourceI3D)(HDC hDC, UINT * uSource);

extern EPOXY_IMPORTEXPORT int (EPOXY_CALLSPEC *epoxy_wglGetLayerPaletteEntries)(HDC hdc, int iLayerPlane, int iStart, int cEntries, const COLORREF * pcr);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglGetMscRateOML)(HDC hdc, INT32 * numerator, INT32 * denominator);

extern EPOXY_IMPORTEXPORT HDC (EPOXY_CALLSPEC *epoxy_wglGetPbufferDCARB)(HPBUFFERARB hPbuffer);

extern EPOXY_IMPORTEXPORT HDC (EPOXY_CALLSPEC *epoxy_wglGetPbufferDCEXT)(HPBUFFEREXT hPbuffer);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglGetPixelFormatAttribfvARB)(HDC hdc, int iPixelFormat, int iLayerPlane, UINT nAttributes, const int * piAttributes, FLOAT * pfValues);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglGetPixelFormatAttribfvEXT)(HDC hdc, int iPixelFormat, int iLayerPlane, UINT nAttributes, int * piAttributes, FLOAT * pfValues);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglGetPixelFormatAttribivARB)(HDC hdc, int iPixelFormat, int iLayerPlane, UINT nAttributes, const int * piAttributes, int * piValues);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglGetPixelFormatAttribivEXT)(HDC hdc, int iPixelFormat, int iLayerPlane, UINT nAttributes, int * piAttributes, int * piValues);

extern EPOXY_IMPORTEXPORT PROC (EPOXY_CALLSPEC *epoxy_wglGetProcAddress)(LPCSTR lpszProc);

extern EPOXY_IMPORTEXPORT int (EPOXY_CALLSPEC *epoxy_wglGetSwapIntervalEXT)(void);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglGetSyncValuesOML)(HDC hdc, INT64 * ust, INT64 * msc, INT64 * sbc);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglGetVideoDeviceNV)(HDC hDC, int numDevices, HPVIDEODEV * hVideoDevice);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglGetVideoInfoNV)(HPVIDEODEV hpVideoDevice, unsigned long * pulCounterOutputPbuffer, unsigned long * pulCounterOutputVideo);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglIsEnabledFrameLockI3D)(BOOL * pFlag);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglIsEnabledGenlockI3D)(HDC hDC, BOOL * pFlag);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglJoinSwapGroupNV)(HDC hDC, GLuint group);

extern EPOXY_IMPORTEXPORT GLboolean (EPOXY_CALLSPEC *epoxy_wglLoadDisplayColorTableEXT)(const GLushort * table, GLuint length);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglLockVideoCaptureDeviceNV)(HDC hDc, HVIDEOINPUTDEVICENV hDevice);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglMakeAssociatedContextCurrentAMD)(HGLRC hglrc);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglMakeContextCurrentARB)(HDC hDrawDC, HDC hReadDC, HGLRC hglrc);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglMakeContextCurrentEXT)(HDC hDrawDC, HDC hReadDC, HGLRC hglrc);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglMakeCurrent)(HDC hDc, HGLRC newContext);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglQueryCurrentContextNV)(int iAttribute, int * piValue);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglQueryFrameCountNV)(HDC hDC, GLuint * count);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglQueryFrameLockMasterI3D)(BOOL * pFlag);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglQueryFrameTrackingI3D)(DWORD * pFrameCount, DWORD * pMissedFrames, float * pLastMissedUsage);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglQueryGenlockMaxSourceDelayI3D)(HDC hDC, UINT * uMaxLineDelay, UINT * uMaxPixelDelay);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglQueryMaxSwapGroupsNV)(HDC hDC, GLuint * maxGroups, GLuint * maxBarriers);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglQueryPbufferARB)(HPBUFFERARB hPbuffer, int iAttribute, int * piValue);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglQueryPbufferEXT)(HPBUFFEREXT hPbuffer, int iAttribute, int * piValue);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglQuerySwapGroupNV)(HDC hDC, GLuint * group, GLuint * barrier);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglQueryVideoCaptureDeviceNV)(HDC hDc, HVIDEOINPUTDEVICENV hDevice, int iAttribute, int * piValue);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglRealizeLayerPalette)(HDC hdc, int iLayerPlane, BOOL bRealize);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglReleaseImageBufferEventsI3D)(HDC hDC, const LPVOID * pAddress, UINT count);

extern EPOXY_IMPORTEXPORT int (EPOXY_CALLSPEC *epoxy_wglReleasePbufferDCARB)(HPBUFFERARB hPbuffer, HDC hDC);

extern EPOXY_IMPORTEXPORT int (EPOXY_CALLSPEC *epoxy_wglReleasePbufferDCEXT)(HPBUFFEREXT hPbuffer, HDC hDC);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglReleaseTexImageARB)(HPBUFFERARB hPbuffer, int iBuffer);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglReleaseVideoCaptureDeviceNV)(HDC hDc, HVIDEOINPUTDEVICENV hDevice);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglReleaseVideoDeviceNV)(HPVIDEODEV hVideoDevice);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglReleaseVideoImageNV)(HPBUFFERARB hPbuffer, int iVideoBuffer);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglResetFrameCountNV)(HDC hDC);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglRestoreBufferRegionARB)(HANDLE hRegion, int x, int y, int width, int height, int xSrc, int ySrc);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglSaveBufferRegionARB)(HANDLE hRegion, int x, int y, int width, int height);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglSendPbufferToVideoNV)(HPBUFFERARB hPbuffer, int iBufferType, unsigned long * pulCounterPbuffer, BOOL bBlock);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglSetDigitalVideoParametersI3D)(HDC hDC, int iAttribute, const int * piValue);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglSetGammaTableI3D)(HDC hDC, int iEntries, const USHORT * puRed, const USHORT * puGreen, const USHORT * puBlue);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglSetGammaTableParametersI3D)(HDC hDC, int iAttribute, const int * piValue);

extern EPOXY_IMPORTEXPORT int (EPOXY_CALLSPEC *epoxy_wglSetLayerPaletteEntries)(HDC hdc, int iLayerPlane, int iStart, int cEntries, const COLORREF * pcr);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglSetPbufferAttribARB)(HPBUFFERARB hPbuffer, const int * piAttribList);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglSetStereoEmitterState3DL)(HDC hDC, UINT uState);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglShareLists)(HGLRC hrcSrvShare, HGLRC hrcSrvSource);

extern EPOXY_IMPORTEXPORT INT64 (EPOXY_CALLSPEC *epoxy_wglSwapBuffersMscOML)(HDC hdc, INT64 target_msc, INT64 divisor, INT64 remainder);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglSwapIntervalEXT)(int interval);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglSwapLayerBuffers)(HDC hdc, UINT fuFlags);

extern EPOXY_IMPORTEXPORT INT64 (EPOXY_CALLSPEC *epoxy_wglSwapLayerBuffersMscOML)(HDC hdc, int fuPlanes, INT64 target_msc, INT64 divisor, INT64 remainder);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglUseFontBitmapsA)(HDC hDC, DWORD first, DWORD count, DWORD listBase);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglUseFontBitmapsW)(HDC hDC, DWORD first, DWORD count, DWORD listBase);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglUseFontOutlines)(HDC hDC, DWORD first, DWORD count, DWORD listBase, FLOAT deviation, FLOAT extrusion, int format, LPGLYPHMETRICSFLOAT lpgmf);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglUseFontOutlinesA)(HDC hDC, DWORD first, DWORD count, DWORD listBase, FLOAT deviation, FLOAT extrusion, int format, LPGLYPHMETRICSFLOAT lpgmf);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglUseFontOutlinesW)(HDC hDC, DWORD first, DWORD count, DWORD listBase, FLOAT deviation, FLOAT extrusion, int format, LPGLYPHMETRICSFLOAT lpgmf);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglWaitForMscOML)(HDC hdc, INT64 target_msc, INT64 divisor, INT64 remainder, INT64 * ust, INT64 * msc, INT64 * sbc);

extern EPOXY_IMPORTEXPORT BOOL (EPOXY_CALLSPEC *epoxy_wglWaitForSbcOML)(HDC hdc, INT64 target_sbc, INT64 * ust, INT64 * msc, INT64 * sbc);

#define wglAllocateMemoryNV epoxy_wglAllocateMemoryNV
#define wglAssociateImageBufferEventsI3D epoxy_wglAssociateImageBufferEventsI3D
#define wglBeginFrameTrackingI3D epoxy_wglBeginFrameTrackingI3D
#define wglBindDisplayColorTableEXT epoxy_wglBindDisplayColorTableEXT
#define wglBindSwapBarrierNV epoxy_wglBindSwapBarrierNV
#define wglBindTexImageARB epoxy_wglBindTexImageARB
#define wglBindVideoCaptureDeviceNV epoxy_wglBindVideoCaptureDeviceNV
#define wglBindVideoDeviceNV epoxy_wglBindVideoDeviceNV
#define wglBindVideoImageNV epoxy_wglBindVideoImageNV
#define wglBlitContextFramebufferAMD epoxy_wglBlitContextFramebufferAMD
#define wglChoosePixelFormatARB epoxy_wglChoosePixelFormatARB
#define wglChoosePixelFormatEXT epoxy_wglChoosePixelFormatEXT
#define wglCopyContext epoxy_wglCopyContext
#define wglCopyImageSubDataNV epoxy_wglCopyImageSubDataNV
#define wglCreateAffinityDCNV epoxy_wglCreateAffinityDCNV
#define wglCreateAssociatedContextAMD epoxy_wglCreateAssociatedContextAMD
#define wglCreateAssociatedContextAttribsAMD epoxy_wglCreateAssociatedContextAttribsAMD
#define wglCreateBufferRegionARB epoxy_wglCreateBufferRegionARB
#define wglCreateContext epoxy_wglCreateContext
#define wglCreateContextAttribsARB epoxy_wglCreateContextAttribsARB
#define wglCreateDisplayColorTableEXT epoxy_wglCreateDisplayColorTableEXT
#define wglCreateImageBufferI3D epoxy_wglCreateImageBufferI3D
#define wglCreateLayerContext epoxy_wglCreateLayerContext
#define wglCreatePbufferARB epoxy_wglCreatePbufferARB
#define wglCreatePbufferEXT epoxy_wglCreatePbufferEXT
#define wglDXCloseDeviceNV epoxy_wglDXCloseDeviceNV
#define wglDXLockObjectsNV epoxy_wglDXLockObjectsNV
#define wglDXObjectAccessNV epoxy_wglDXObjectAccessNV
#define wglDXOpenDeviceNV epoxy_wglDXOpenDeviceNV
#define wglDXRegisterObjectNV epoxy_wglDXRegisterObjectNV
#define wglDXSetResourceShareHandleNV epoxy_wglDXSetResourceShareHandleNV
#define wglDXUnlockObjectsNV epoxy_wglDXUnlockObjectsNV
#define wglDXUnregisterObjectNV epoxy_wglDXUnregisterObjectNV
#define wglDelayBeforeSwapNV epoxy_wglDelayBeforeSwapNV
#define wglDeleteAssociatedContextAMD epoxy_wglDeleteAssociatedContextAMD
#define wglDeleteBufferRegionARB epoxy_wglDeleteBufferRegionARB
#define wglDeleteContext epoxy_wglDeleteContext
#define wglDeleteDCNV epoxy_wglDeleteDCNV
#define wglDescribeLayerPlane epoxy_wglDescribeLayerPlane
#define wglDestroyDisplayColorTableEXT epoxy_wglDestroyDisplayColorTableEXT
#define wglDestroyImageBufferI3D epoxy_wglDestroyImageBufferI3D
#define wglDestroyPbufferARB epoxy_wglDestroyPbufferARB
#define wglDestroyPbufferEXT epoxy_wglDestroyPbufferEXT
#define wglDisableFrameLockI3D epoxy_wglDisableFrameLockI3D
#define wglDisableGenlockI3D epoxy_wglDisableGenlockI3D
#define wglEnableFrameLockI3D epoxy_wglEnableFrameLockI3D
#define wglEnableGenlockI3D epoxy_wglEnableGenlockI3D
#define wglEndFrameTrackingI3D epoxy_wglEndFrameTrackingI3D
#define wglEnumGpuDevicesNV epoxy_wglEnumGpuDevicesNV
#define wglEnumGpusFromAffinityDCNV epoxy_wglEnumGpusFromAffinityDCNV
#define wglEnumGpusNV epoxy_wglEnumGpusNV
#define wglEnumerateVideoCaptureDevicesNV epoxy_wglEnumerateVideoCaptureDevicesNV
#define wglEnumerateVideoDevicesNV epoxy_wglEnumerateVideoDevicesNV
#define wglFreeMemoryNV epoxy_wglFreeMemoryNV
#define wglGenlockSampleRateI3D epoxy_wglGenlockSampleRateI3D
#define wglGenlockSourceDelayI3D epoxy_wglGenlockSourceDelayI3D
#define wglGenlockSourceEdgeI3D epoxy_wglGenlockSourceEdgeI3D
#define wglGenlockSourceI3D epoxy_wglGenlockSourceI3D
#define wglGetContextGPUIDAMD epoxy_wglGetContextGPUIDAMD
#define wglGetCurrentAssociatedContextAMD epoxy_wglGetCurrentAssociatedContextAMD
#define wglGetCurrentContext epoxy_wglGetCurrentContext
#define wglGetCurrentDC epoxy_wglGetCurrentDC
#define wglGetCurrentReadDCARB epoxy_wglGetCurrentReadDCARB
#define wglGetCurrentReadDCEXT epoxy_wglGetCurrentReadDCEXT
#define wglGetDefaultProcAddress epoxy_wglGetDefaultProcAddress
#define wglGetDigitalVideoParametersI3D epoxy_wglGetDigitalVideoParametersI3D
#define wglGetExtensionsStringARB epoxy_wglGetExtensionsStringARB
#define wglGetExtensionsStringEXT epoxy_wglGetExtensionsStringEXT
#define wglGetFrameUsageI3D epoxy_wglGetFrameUsageI3D
#define wglGetGPUIDsAMD epoxy_wglGetGPUIDsAMD
#define wglGetGPUInfoAMD epoxy_wglGetGPUInfoAMD
#define wglGetGammaTableI3D epoxy_wglGetGammaTableI3D
#define wglGetGammaTableParametersI3D epoxy_wglGetGammaTableParametersI3D
#define wglGetGenlockSampleRateI3D epoxy_wglGetGenlockSampleRateI3D
#define wglGetGenlockSourceDelayI3D epoxy_wglGetGenlockSourceDelayI3D
#define wglGetGenlockSourceEdgeI3D epoxy_wglGetGenlockSourceEdgeI3D
#define wglGetGenlockSourceI3D epoxy_wglGetGenlockSourceI3D
#define wglGetLayerPaletteEntries epoxy_wglGetLayerPaletteEntries
#define wglGetMscRateOML epoxy_wglGetMscRateOML
#define wglGetPbufferDCARB epoxy_wglGetPbufferDCARB
#define wglGetPbufferDCEXT epoxy_wglGetPbufferDCEXT
#define wglGetPixelFormatAttribfvARB epoxy_wglGetPixelFormatAttribfvARB
#define wglGetPixelFormatAttribfvEXT epoxy_wglGetPixelFormatAttribfvEXT
#define wglGetPixelFormatAttribivARB epoxy_wglGetPixelFormatAttribivARB
#define wglGetPixelFormatAttribivEXT epoxy_wglGetPixelFormatAttribivEXT
#define wglGetProcAddress epoxy_wglGetProcAddress
#define wglGetSwapIntervalEXT epoxy_wglGetSwapIntervalEXT
#define wglGetSyncValuesOML epoxy_wglGetSyncValuesOML
#define wglGetVideoDeviceNV epoxy_wglGetVideoDeviceNV
#define wglGetVideoInfoNV epoxy_wglGetVideoInfoNV
#define wglIsEnabledFrameLockI3D epoxy_wglIsEnabledFrameLockI3D
#define wglIsEnabledGenlockI3D epoxy_wglIsEnabledGenlockI3D
#define wglJoinSwapGroupNV epoxy_wglJoinSwapGroupNV
#define wglLoadDisplayColorTableEXT epoxy_wglLoadDisplayColorTableEXT
#define wglLockVideoCaptureDeviceNV epoxy_wglLockVideoCaptureDeviceNV
#define wglMakeAssociatedContextCurrentAMD epoxy_wglMakeAssociatedContextCurrentAMD
#define wglMakeContextCurrentARB epoxy_wglMakeContextCurrentARB
#define wglMakeContextCurrentEXT epoxy_wglMakeContextCurrentEXT
#define wglMakeCurrent epoxy_wglMakeCurrent
#define wglQueryCurrentContextNV epoxy_wglQueryCurrentContextNV
#define wglQueryFrameCountNV epoxy_wglQueryFrameCountNV
#define wglQueryFrameLockMasterI3D epoxy_wglQueryFrameLockMasterI3D
#define wglQueryFrameTrackingI3D epoxy_wglQueryFrameTrackingI3D
#define wglQueryGenlockMaxSourceDelayI3D epoxy_wglQueryGenlockMaxSourceDelayI3D
#define wglQueryMaxSwapGroupsNV epoxy_wglQueryMaxSwapGroupsNV
#define wglQueryPbufferARB epoxy_wglQueryPbufferARB
#define wglQueryPbufferEXT epoxy_wglQueryPbufferEXT
#define wglQuerySwapGroupNV epoxy_wglQuerySwapGroupNV
#define wglQueryVideoCaptureDeviceNV epoxy_wglQueryVideoCaptureDeviceNV
#define wglRealizeLayerPalette epoxy_wglRealizeLayerPalette
#define wglReleaseImageBufferEventsI3D epoxy_wglReleaseImageBufferEventsI3D
#define wglReleasePbufferDCARB epoxy_wglReleasePbufferDCARB
#define wglReleasePbufferDCEXT epoxy_wglReleasePbufferDCEXT
#define wglReleaseTexImageARB epoxy_wglReleaseTexImageARB
#define wglReleaseVideoCaptureDeviceNV epoxy_wglReleaseVideoCaptureDeviceNV
#define wglReleaseVideoDeviceNV epoxy_wglReleaseVideoDeviceNV
#define wglReleaseVideoImageNV epoxy_wglReleaseVideoImageNV
#define wglResetFrameCountNV epoxy_wglResetFrameCountNV
#define wglRestoreBufferRegionARB epoxy_wglRestoreBufferRegionARB
#define wglSaveBufferRegionARB epoxy_wglSaveBufferRegionARB
#define wglSendPbufferToVideoNV epoxy_wglSendPbufferToVideoNV
#define wglSetDigitalVideoParametersI3D epoxy_wglSetDigitalVideoParametersI3D
#define wglSetGammaTableI3D epoxy_wglSetGammaTableI3D
#define wglSetGammaTableParametersI3D epoxy_wglSetGammaTableParametersI3D
#define wglSetLayerPaletteEntries epoxy_wglSetLayerPaletteEntries
#define wglSetPbufferAttribARB epoxy_wglSetPbufferAttribARB
#define wglSetStereoEmitterState3DL epoxy_wglSetStereoEmitterState3DL
#define wglShareLists epoxy_wglShareLists
#define wglSwapBuffersMscOML epoxy_wglSwapBuffersMscOML
#define wglSwapIntervalEXT epoxy_wglSwapIntervalEXT
#define wglSwapLayerBuffers epoxy_wglSwapLayerBuffers
#define wglSwapLayerBuffersMscOML epoxy_wglSwapLayerBuffersMscOML
#define wglUseFontBitmapsA epoxy_wglUseFontBitmapsA
#define wglUseFontBitmapsW epoxy_wglUseFontBitmapsW
#define wglUseFontOutlines epoxy_wglUseFontOutlines
#define wglUseFontOutlinesA epoxy_wglUseFontOutlinesA
#define wglUseFontOutlinesW epoxy_wglUseFontOutlinesW
#define wglWaitForMscOML epoxy_wglWaitForMscOML
#define wglWaitForSbcOML epoxy_wglWaitForSbcOML
