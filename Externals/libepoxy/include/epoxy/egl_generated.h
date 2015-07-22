/* GL dispatch header.
 * This is code-generated from the GL API XML files from Khronos.
 */

#pragma once
#include <inttypes.h>
#include <stddef.h>

#include "epoxy/gl.h"
#include "EGL/eglplatform.h"
typedef unsigned int EGLBoolean;
typedef unsigned int EGLenum;
typedef intptr_t EGLAttribKHR;
typedef intptr_t EGLAttrib;
typedef void *EGLClientBuffer;
typedef void *EGLConfig;
typedef void *EGLContext;
typedef void *EGLDeviceEXT;
typedef void *EGLDisplay;
typedef void *EGLImage;
typedef void *EGLImageKHR;
typedef void *EGLOutputLayerEXT;
typedef void *EGLOutputPortEXT;
typedef void *EGLStreamKHR;
typedef void *EGLSurface;
typedef void *EGLSync;
typedef void *EGLSyncKHR;
typedef void *EGLSyncNV;
typedef void (*__eglMustCastToProperFunctionPointerType)(void);
typedef khronos_utime_nanoseconds_t EGLTimeKHR;
typedef khronos_utime_nanoseconds_t EGLTime;
typedef khronos_utime_nanoseconds_t EGLTimeNV;
typedef khronos_utime_nanoseconds_t EGLuint64NV;
typedef khronos_uint64_t EGLuint64KHR;
typedef int EGLNativeFileDescriptorKHR;
typedef khronos_ssize_t EGLsizeiANDROID;
typedef void (*EGLSetBlobFuncANDROID) (const void *key, EGLsizeiANDROID keySize, const void *value, EGLsizeiANDROID valueSize);
typedef EGLsizeiANDROID (*EGLGetBlobFuncANDROID) (const void *key, EGLsizeiANDROID keySize, void *value, EGLsizeiANDROID valueSize);
struct EGLClientPixmapHI {
    void  *pData;
    EGLint iWidth;
    EGLint iHeight;
    EGLint iStride;
};

#define EGL_VERSION_1_0 1
#define EGL_VERSION_1_1 1
#define EGL_VERSION_1_2 1
#define EGL_VERSION_1_3 1
#define EGL_VERSION_1_4 1
#define EGL_VERSION_1_5 1

#define EGL_ANDROID_blob_cache 1
#define EGL_ANDROID_framebuffer_target 1
#define EGL_ANDROID_image_native_buffer 1
#define EGL_ANDROID_native_fence_sync 1
#define EGL_ANDROID_recordable 1
#define EGL_ANGLE_d3d_share_handle_client_buffer 1
#define EGL_ANGLE_device_d3d 1
#define EGL_ANGLE_query_surface_pointer 1
#define EGL_ANGLE_surface_d3d_texture_2d_share_handle 1
#define EGL_ANGLE_window_fixed_size 1
#define EGL_ARM_pixmap_multisample_discard 1
#define EGL_EXT_buffer_age 1
#define EGL_EXT_client_extensions 1
#define EGL_EXT_create_context_robustness 1
#define EGL_EXT_device_base 1
#define EGL_EXT_device_drm 1
#define EGL_EXT_device_enumeration 1
#define EGL_EXT_device_openwf 1
#define EGL_EXT_device_query 1
#define EGL_EXT_image_dma_buf_import 1
#define EGL_EXT_multiview_window 1
#define EGL_EXT_output_base 1
#define EGL_EXT_output_drm 1
#define EGL_EXT_output_openwf 1
#define EGL_EXT_platform_base 1
#define EGL_EXT_platform_device 1
#define EGL_EXT_platform_wayland 1
#define EGL_EXT_platform_x11 1
#define EGL_EXT_protected_surface 1
#define EGL_EXT_stream_consumer_egloutput 1
#define EGL_EXT_swap_buffers_with_damage 1
#define EGL_EXT_yuv_surface 1
#define EGL_HI_clientpixmap 1
#define EGL_HI_colorformats 1
#define EGL_IMG_context_priority 1
#define EGL_KHR_cl_event 1
#define EGL_KHR_cl_event2 1
#define EGL_KHR_client_get_all_proc_addresses 1
#define EGL_KHR_config_attribs 1
#define EGL_KHR_create_context 1
#define EGL_KHR_create_context_no_error 1
#define EGL_KHR_fence_sync 1
#define EGL_KHR_get_all_proc_addresses 1
#define EGL_KHR_gl_colorspace 1
#define EGL_KHR_gl_renderbuffer_image 1
#define EGL_KHR_gl_texture_2D_image 1
#define EGL_KHR_gl_texture_3D_image 1
#define EGL_KHR_gl_texture_cubemap_image 1
#define EGL_KHR_image 1
#define EGL_KHR_image_base 1
#define EGL_KHR_image_pixmap 1
#define EGL_KHR_lock_surface 1
#define EGL_KHR_lock_surface2 1
#define EGL_KHR_lock_surface3 1
#define EGL_KHR_partial_update 1
#define EGL_KHR_platform_android 1
#define EGL_KHR_platform_gbm 1
#define EGL_KHR_platform_wayland 1
#define EGL_KHR_platform_x11 1
#define EGL_KHR_reusable_sync 1
#define EGL_KHR_stream 1
#define EGL_KHR_stream_consumer_gltexture 1
#define EGL_KHR_stream_cross_process_fd 1
#define EGL_KHR_stream_fifo 1
#define EGL_KHR_stream_producer_aldatalocator 1
#define EGL_KHR_stream_producer_eglsurface 1
#define EGL_KHR_surfaceless_context 1
#define EGL_KHR_swap_buffers_with_damage 1
#define EGL_KHR_vg_parent_image 1
#define EGL_KHR_wait_sync 1
#define EGL_MESA_drm_image 1
#define EGL_MESA_image_dma_buf_export 1
#define EGL_MESA_platform_gbm 1
#define EGL_NOK_swap_region 1
#define EGL_NOK_swap_region2 1
#define EGL_NOK_texture_from_pixmap 1
#define EGL_NV_3dvision_surface 1
#define EGL_NV_coverage_sample 1
#define EGL_NV_coverage_sample_resolve 1
#define EGL_NV_cuda_event 1
#define EGL_NV_depth_nonlinear 1
#define EGL_NV_device_cuda 1
#define EGL_NV_native_query 1
#define EGL_NV_post_convert_rounding 1
#define EGL_NV_post_sub_buffer 1
#define EGL_NV_stream_sync 1
#define EGL_NV_sync 1
#define EGL_NV_system_time 1
#define EGL_TIZEN_image_native_buffer 1
#define EGL_TIZEN_image_native_surface 1

#define EGL_NO_CONTEXT                                       ((EGLContext)0)
#define EGL_NO_DEVICE_EXT                                    ((EGLDeviceEXT)(0))
#define EGL_NO_DISPLAY                                       ((EGLDisplay)0)
#define EGL_NO_IMAGE                                         ((EGLImage)0)
#define EGL_NO_IMAGE_KHR                                     ((EGLImageKHR)0)
#define EGL_DEFAULT_DISPLAY                                  ((EGLNativeDisplayType)0)
#define EGL_NO_FILE_DESCRIPTOR_KHR                           ((EGLNativeFileDescriptorKHR)(-1))
#define EGL_NO_OUTPUT_LAYER_EXT                              ((EGLOutputLayerEXT)0)
#define EGL_NO_OUTPUT_PORT_EXT                               ((EGLOutputPortEXT)0)
#define EGL_NO_STREAM_KHR                                    ((EGLStreamKHR)0)
#define EGL_NO_SURFACE                                       ((EGLSurface)0)
#define EGL_NO_SYNC                                          ((EGLSync)0)
#define EGL_NO_SYNC_KHR                                      ((EGLSyncKHR)0)
#define EGL_NO_SYNC_NV                                       ((EGLSyncNV)0)
#define EGL_DONT_CARE                                        ((EGLint)-1)
#define EGL_UNKNOWN                                          ((EGLint)-1)
#define EGL_NO_NATIVE_FENCE_FD_ANDROID                       -1
#define EGL_DEPTH_ENCODING_NONE_NV                           0
#define EGL_FALSE                                            0
#define EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT                  0x00000001
#define EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR              0x00000001
#define EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR                     0x00000001
#define EGL_DRM_BUFFER_USE_SCANOUT_MESA                      0x00000001
#define EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT         0x00000002
#define EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT_KHR     0x00000002
#define EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR        0x00000002
#define EGL_DRM_BUFFER_USE_SHARE_MESA                        0x00000002
#define EGL_CONTEXT_OPENGL_ROBUST_ACCESS_BIT_KHR             0x00000004
#define EGL_OPENGL_ES3_BIT                                   0x00000040
#define EGL_OPENGL_ES3_BIT_KHR                               0x00000040
#define EGL_OPENGL_ES_BIT                                    0x0001
#define EGL_PBUFFER_BIT                                      0x0001
#define EGL_READ_SURFACE_BIT_KHR                             0x0001
#define EGL_SYNC_FLUSH_COMMANDS_BIT                          0x0001
#define EGL_SYNC_FLUSH_COMMANDS_BIT_KHR                      0x0001
#define EGL_SYNC_FLUSH_COMMANDS_BIT_NV                       0x0001
#define EGL_OPENVG_BIT                                       0x0002
#define EGL_PIXMAP_BIT                                       0x0002
#define EGL_WRITE_SURFACE_BIT_KHR                            0x0002
#define EGL_OPENGL_ES2_BIT                                   0x0004
#define EGL_WINDOW_BIT                                       0x0004
#define EGL_OPENGL_BIT                                       0x0008
#define EGL_PBUFFER_IMAGE_BIT_TAO                            0x0008
#define EGL_INTEROP_BIT_KHR                                  0x0010
#define EGL_PBUFFER_PALETTE_IMAGE_BIT_TAO                    0x0010
#define EGL_OPENMAX_IL_BIT_KHR                               0x0020
#define EGL_VG_COLORSPACE_LINEAR_BIT                         0x0020
#define EGL_VG_COLORSPACE_LINEAR_BIT_KHR                     0x0020
#define EGL_VG_ALPHA_FORMAT_PRE_BIT                          0x0040
#define EGL_VG_ALPHA_FORMAT_PRE_BIT_KHR                      0x0040
#define EGL_LOCK_SURFACE_BIT_KHR                             0x0080
#define EGL_OPTIMAL_FORMAT_BIT_KHR                           0x0100
#define EGL_MULTISAMPLE_RESOLVE_BOX_BIT                      0x0200
#define EGL_SWAP_BEHAVIOR_PRESERVED_BIT                      0x0400
#define EGL_STREAM_BIT_KHR                                   0x0800
#define EGL_SUCCESS                                          0x3000
#define EGL_NOT_INITIALIZED                                  0x3001
#define EGL_BAD_ACCESS                                       0x3002
#define EGL_BAD_ALLOC                                        0x3003
#define EGL_BAD_ATTRIBUTE                                    0x3004
#define EGL_BAD_CONFIG                                       0x3005
#define EGL_BAD_CONTEXT                                      0x3006
#define EGL_BAD_CURRENT_SURFACE                              0x3007
#define EGL_BAD_DISPLAY                                      0x3008
#define EGL_BAD_MATCH                                        0x3009
#define EGL_BAD_NATIVE_PIXMAP                                0x300A
#define EGL_BAD_NATIVE_WINDOW                                0x300B
#define EGL_BAD_PARAMETER                                    0x300C
#define EGL_BAD_SURFACE                                      0x300D
#define EGL_CONTEXT_LOST                                     0x300E
#define EGL_BUFFER_SIZE                                      0x3020
#define EGL_ALPHA_SIZE                                       0x3021
#define EGL_BLUE_SIZE                                        0x3022
#define EGL_GREEN_SIZE                                       0x3023
#define EGL_RED_SIZE                                         0x3024
#define EGL_DEPTH_SIZE                                       0x3025
#define EGL_STENCIL_SIZE                                     0x3026
#define EGL_CONFIG_CAVEAT                                    0x3027
#define EGL_CONFIG_ID                                        0x3028
#define EGL_LEVEL                                            0x3029
#define EGL_MAX_PBUFFER_HEIGHT                               0x302A
#define EGL_MAX_PBUFFER_PIXELS                               0x302B
#define EGL_MAX_PBUFFER_WIDTH                                0x302C
#define EGL_NATIVE_RENDERABLE                                0x302D
#define EGL_NATIVE_VISUAL_ID                                 0x302E
#define EGL_NATIVE_VISUAL_TYPE                               0x302F
#define EGL_SAMPLES                                          0x3031
#define EGL_SAMPLE_BUFFERS                                   0x3032
#define EGL_SURFACE_TYPE                                     0x3033
#define EGL_TRANSPARENT_TYPE                                 0x3034
#define EGL_TRANSPARENT_BLUE_VALUE                           0x3035
#define EGL_TRANSPARENT_GREEN_VALUE                          0x3036
#define EGL_TRANSPARENT_RED_VALUE                            0x3037
#define EGL_NONE                                             0x3038
#define EGL_BIND_TO_TEXTURE_RGB                              0x3039
#define EGL_BIND_TO_TEXTURE_RGBA                             0x303A
#define EGL_MIN_SWAP_INTERVAL                                0x303B
#define EGL_MAX_SWAP_INTERVAL                                0x303C
#define EGL_LUMINANCE_SIZE                                   0x303D
#define EGL_ALPHA_MASK_SIZE                                  0x303E
#define EGL_COLOR_BUFFER_TYPE                                0x303F
#define EGL_RENDERABLE_TYPE                                  0x3040
#define EGL_MATCH_NATIVE_PIXMAP                              0x3041
#define EGL_CONFORMANT                                       0x3042
#define EGL_CONFORMANT_KHR                                   0x3042
#define EGL_MATCH_FORMAT_KHR                                 0x3043
#define EGL_SLOW_CONFIG                                      0x3050
#define EGL_NON_CONFORMANT_CONFIG                            0x3051
#define EGL_TRANSPARENT_RGB                                  0x3052
#define EGL_VENDOR                                           0x3053
#define EGL_VERSION                                          0x3054
#define EGL_EXTENSIONS                                       0x3055
#define EGL_HEIGHT                                           0x3056
#define EGL_WIDTH                                            0x3057
#define EGL_LARGEST_PBUFFER                                  0x3058
#define EGL_DRAW                                             0x3059
#define EGL_READ                                             0x305A
#define EGL_CORE_NATIVE_ENGINE                               0x305B
#define EGL_NO_TEXTURE                                       0x305C
#define EGL_TEXTURE_RGB                                      0x305D
#define EGL_TEXTURE_RGBA                                     0x305E
#define EGL_TEXTURE_2D                                       0x305F
#define EGL_Y_INVERTED_NOK                                   0x307F
#define EGL_TEXTURE_FORMAT                                   0x3080
#define EGL_TEXTURE_TARGET                                   0x3081
#define EGL_MIPMAP_TEXTURE                                   0x3082
#define EGL_MIPMAP_LEVEL                                     0x3083
#define EGL_BACK_BUFFER                                      0x3084
#define EGL_SINGLE_BUFFER                                    0x3085
#define EGL_RENDER_BUFFER                                    0x3086
#define EGL_COLORSPACE                                       0x3087
#define EGL_VG_COLORSPACE                                    0x3087
#define EGL_ALPHA_FORMAT                                     0x3088
#define EGL_VG_ALPHA_FORMAT                                  0x3088
#define EGL_COLORSPACE_sRGB                                  0x3089
#define EGL_GL_COLORSPACE_SRGB                               0x3089
#define EGL_GL_COLORSPACE_SRGB_KHR                           0x3089
#define EGL_VG_COLORSPACE_sRGB                               0x3089
#define EGL_COLORSPACE_LINEAR                                0x308A
#define EGL_GL_COLORSPACE_LINEAR                             0x308A
#define EGL_GL_COLORSPACE_LINEAR_KHR                         0x308A
#define EGL_VG_COLORSPACE_LINEAR                             0x308A
#define EGL_ALPHA_FORMAT_NONPRE                              0x308B
#define EGL_VG_ALPHA_FORMAT_NONPRE                           0x308B
#define EGL_ALPHA_FORMAT_PRE                                 0x308C
#define EGL_VG_ALPHA_FORMAT_PRE                              0x308C
#define EGL_CLIENT_APIS                                      0x308D
#define EGL_RGB_BUFFER                                       0x308E
#define EGL_LUMINANCE_BUFFER                                 0x308F
#define EGL_HORIZONTAL_RESOLUTION                            0x3090
#define EGL_VERTICAL_RESOLUTION                              0x3091
#define EGL_PIXEL_ASPECT_RATIO                               0x3092
#define EGL_SWAP_BEHAVIOR                                    0x3093
#define EGL_BUFFER_PRESERVED                                 0x3094
#define EGL_BUFFER_DESTROYED                                 0x3095
#define EGL_OPENVG_IMAGE                                     0x3096
#define EGL_CONTEXT_CLIENT_TYPE                              0x3097
#define EGL_CONTEXT_CLIENT_VERSION                           0x3098
#define EGL_CONTEXT_MAJOR_VERSION                            0x3098
#define EGL_CONTEXT_MAJOR_VERSION_KHR                        0x3098
#define EGL_MULTISAMPLE_RESOLVE                              0x3099
#define EGL_MULTISAMPLE_RESOLVE_DEFAULT                      0x309A
#define EGL_MULTISAMPLE_RESOLVE_BOX                          0x309B
#define EGL_CL_EVENT_HANDLE                                  0x309C
#define EGL_CL_EVENT_HANDLE_KHR                              0x309C
#define EGL_GL_COLORSPACE                                    0x309D
#define EGL_GL_COLORSPACE_KHR                                0x309D
#define EGL_OPENGL_ES_API                                    0x30A0
#define EGL_OPENVG_API                                       0x30A1
#define EGL_OPENGL_API                                       0x30A2
#define EGL_NATIVE_PIXMAP_KHR                                0x30B0
#define EGL_GL_TEXTURE_2D                                    0x30B1
#define EGL_GL_TEXTURE_2D_KHR                                0x30B1
#define EGL_GL_TEXTURE_3D                                    0x30B2
#define EGL_GL_TEXTURE_3D_KHR                                0x30B2
#define EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_X                   0x30B3
#define EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_X_KHR               0x30B3
#define EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_X                   0x30B4
#define EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_X_KHR               0x30B4
#define EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_Y                   0x30B5
#define EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_Y_KHR               0x30B5
#define EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Y                   0x30B6
#define EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_KHR               0x30B6
#define EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_Z                   0x30B7
#define EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_Z_KHR               0x30B7
#define EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z                   0x30B8
#define EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_KHR               0x30B8
#define EGL_GL_RENDERBUFFER                                  0x30B9
#define EGL_GL_RENDERBUFFER_KHR                              0x30B9
#define EGL_VG_PARENT_IMAGE_KHR                              0x30BA
#define EGL_GL_TEXTURE_LEVEL                                 0x30BC
#define EGL_GL_TEXTURE_LEVEL_KHR                             0x30BC
#define EGL_GL_TEXTURE_ZOFFSET                               0x30BD
#define EGL_GL_TEXTURE_ZOFFSET_KHR                           0x30BD
#define EGL_POST_SUB_BUFFER_SUPPORTED_NV                     0x30BE
#define EGL_CONTEXT_OPENGL_ROBUST_ACCESS_EXT                 0x30BF
#define EGL_FORMAT_RGB_565_EXACT_KHR                         0x30C0
#define EGL_FORMAT_RGB_565_KHR                               0x30C1
#define EGL_FORMAT_RGBA_8888_EXACT_KHR                       0x30C2
#define EGL_FORMAT_RGBA_8888_KHR                             0x30C3
#define EGL_MAP_PRESERVE_PIXELS_KHR                          0x30C4
#define EGL_LOCK_USAGE_HINT_KHR                              0x30C5
#define EGL_BITMAP_POINTER_KHR                               0x30C6
#define EGL_BITMAP_PITCH_KHR                                 0x30C7
#define EGL_BITMAP_ORIGIN_KHR                                0x30C8
#define EGL_BITMAP_PIXEL_RED_OFFSET_KHR                      0x30C9
#define EGL_BITMAP_PIXEL_GREEN_OFFSET_KHR                    0x30CA
#define EGL_BITMAP_PIXEL_BLUE_OFFSET_KHR                     0x30CB
#define EGL_BITMAP_PIXEL_ALPHA_OFFSET_KHR                    0x30CC
#define EGL_BITMAP_PIXEL_LUMINANCE_OFFSET_KHR                0x30CD
#define EGL_LOWER_LEFT_KHR                                   0x30CE
#define EGL_UPPER_LEFT_KHR                                   0x30CF
#define EGL_IMAGE_PRESERVED                                  0x30D2
#define EGL_IMAGE_PRESERVED_KHR                              0x30D2
#define EGL_SHARED_IMAGE_NOK                                 0x30DA
#define EGL_COVERAGE_BUFFERS_NV                              0x30E0
#define EGL_COVERAGE_SAMPLES_NV                              0x30E1
#define EGL_DEPTH_ENCODING_NV                                0x30E2
#define EGL_DEPTH_ENCODING_NONLINEAR_NV                      0x30E3
#define EGL_SYNC_PRIOR_COMMANDS_COMPLETE_NV                  0x30E6
#define EGL_SYNC_STATUS_NV                                   0x30E7
#define EGL_SIGNALED_NV                                      0x30E8
#define EGL_UNSIGNALED_NV                                    0x30E9
#define EGL_ALREADY_SIGNALED_NV                              0x30EA
#define EGL_TIMEOUT_EXPIRED_NV                               0x30EB
#define EGL_CONDITION_SATISFIED_NV                           0x30EC
#define EGL_SYNC_TYPE_NV                                     0x30ED
#define EGL_SYNC_CONDITION_NV                                0x30EE
#define EGL_SYNC_FENCE_NV                                    0x30EF
#define EGL_SYNC_PRIOR_COMMANDS_COMPLETE                     0x30F0
#define EGL_SYNC_PRIOR_COMMANDS_COMPLETE_KHR                 0x30F0
#define EGL_SYNC_STATUS                                      0x30F1
#define EGL_SYNC_STATUS_KHR                                  0x30F1
#define EGL_SIGNALED                                         0x30F2
#define EGL_SIGNALED_KHR                                     0x30F2
#define EGL_UNSIGNALED                                       0x30F3
#define EGL_UNSIGNALED_KHR                                   0x30F3
#define EGL_TIMEOUT_EXPIRED                                  0x30F5
#define EGL_TIMEOUT_EXPIRED_KHR                              0x30F5
#define EGL_CONDITION_SATISFIED                              0x30F6
#define EGL_CONDITION_SATISFIED_KHR                          0x30F6
#define EGL_SYNC_TYPE                                        0x30F7
#define EGL_SYNC_TYPE_KHR                                    0x30F7
#define EGL_SYNC_CONDITION                                   0x30F8
#define EGL_SYNC_CONDITION_KHR                               0x30F8
#define EGL_SYNC_FENCE                                       0x30F9
#define EGL_SYNC_FENCE_KHR                                   0x30F9
#define EGL_SYNC_REUSABLE_KHR                                0x30FA
#define EGL_CONTEXT_MINOR_VERSION                            0x30FB
#define EGL_CONTEXT_MINOR_VERSION_KHR                        0x30FB
#define EGL_CONTEXT_FLAGS_KHR                                0x30FC
#define EGL_CONTEXT_OPENGL_PROFILE_MASK                      0x30FD
#define EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR                  0x30FD
#define EGL_SYNC_CL_EVENT                                    0x30FE
#define EGL_SYNC_CL_EVENT_KHR                                0x30FE
#define EGL_SYNC_CL_EVENT_COMPLETE                           0x30FF
#define EGL_SYNC_CL_EVENT_COMPLETE_KHR                       0x30FF
#define EGL_CONTEXT_PRIORITY_LEVEL_IMG                       0x3100
#define EGL_CONTEXT_PRIORITY_HIGH_IMG                        0x3101
#define EGL_CONTEXT_PRIORITY_MEDIUM_IMG                      0x3102
#define EGL_CONTEXT_PRIORITY_LOW_IMG                         0x3103
#define EGL_BITMAP_PIXEL_SIZE_KHR                            0x3110
#define EGL_COVERAGE_SAMPLE_RESOLVE_NV                       0x3131
#define EGL_COVERAGE_SAMPLE_RESOLVE_DEFAULT_NV               0x3132
#define EGL_COVERAGE_SAMPLE_RESOLVE_NONE_NV                  0x3133
#define EGL_MULTIVIEW_VIEW_COUNT_EXT                         0x3134
#define EGL_AUTO_STEREO_NV                                   0x3136
#define EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_EXT   0x3138
#define EGL_BUFFER_AGE_EXT                                   0x313D
#define EGL_BUFFER_AGE_KHR                                   0x313D
#define EGL_PLATFORM_DEVICE_EXT                              0x313F
#define EGL_NATIVE_BUFFER_ANDROID                            0x3140
#define EGL_PLATFORM_ANDROID_KHR                             0x3141
#define EGL_RECORDABLE_ANDROID                               0x3142
#define EGL_SYNC_NATIVE_FENCE_ANDROID                        0x3144
#define EGL_SYNC_NATIVE_FENCE_FD_ANDROID                     0x3145
#define EGL_SYNC_NATIVE_FENCE_SIGNALED_ANDROID               0x3146
#define EGL_FRAMEBUFFER_TARGET_ANDROID                       0x3147
#define EGL_CONTEXT_OPENGL_DEBUG                             0x31B0
#define EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE                0x31B1
#define EGL_CONTEXT_OPENGL_ROBUST_ACCESS                     0x31B2
#define EGL_CONTEXT_OPENGL_NO_ERROR_KHR                      0x31B3
#define EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY       0x31BD
#define EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_KHR   0x31BD
#define EGL_NO_RESET_NOTIFICATION                            0x31BE
#define EGL_NO_RESET_NOTIFICATION_EXT                        0x31BE
#define EGL_NO_RESET_NOTIFICATION_KHR                        0x31BE
#define EGL_LOSE_CONTEXT_ON_RESET                            0x31BF
#define EGL_LOSE_CONTEXT_ON_RESET_EXT                        0x31BF
#define EGL_LOSE_CONTEXT_ON_RESET_KHR                        0x31BF
#define EGL_DRM_BUFFER_FORMAT_MESA                           0x31D0
#define EGL_DRM_BUFFER_USE_MESA                              0x31D1
#define EGL_DRM_BUFFER_FORMAT_ARGB32_MESA                    0x31D2
#define EGL_DRM_BUFFER_MESA                                  0x31D3
#define EGL_DRM_BUFFER_STRIDE_MESA                           0x31D4
#define EGL_PLATFORM_X11_EXT                                 0x31D5
#define EGL_PLATFORM_X11_KHR                                 0x31D5
#define EGL_PLATFORM_X11_SCREEN_EXT                          0x31D6
#define EGL_PLATFORM_X11_SCREEN_KHR                          0x31D6
#define EGL_PLATFORM_GBM_KHR                                 0x31D7
#define EGL_PLATFORM_GBM_MESA                                0x31D7
#define EGL_PLATFORM_WAYLAND_EXT                             0x31D8
#define EGL_PLATFORM_WAYLAND_KHR                             0x31D8
#define EGL_STREAM_FIFO_LENGTH_KHR                           0x31FC
#define EGL_STREAM_TIME_NOW_KHR                              0x31FD
#define EGL_STREAM_TIME_CONSUMER_KHR                         0x31FE
#define EGL_STREAM_TIME_PRODUCER_KHR                         0x31FF
#define EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE                0x3200
#define EGL_FIXED_SIZE_ANGLE                                 0x3201
#define EGL_CONSUMER_LATENCY_USEC_KHR                        0x3210
#define EGL_PRODUCER_FRAME_KHR                               0x3212
#define EGL_CONSUMER_FRAME_KHR                               0x3213
#define EGL_STREAM_STATE_KHR                                 0x3214
#define EGL_STREAM_STATE_CREATED_KHR                         0x3215
#define EGL_STREAM_STATE_CONNECTING_KHR                      0x3216
#define EGL_STREAM_STATE_EMPTY_KHR                           0x3217
#define EGL_STREAM_STATE_NEW_FRAME_AVAILABLE_KHR             0x3218
#define EGL_STREAM_STATE_OLD_FRAME_AVAILABLE_KHR             0x3219
#define EGL_STREAM_STATE_DISCONNECTED_KHR                    0x321A
#define EGL_BAD_STREAM_KHR                                   0x321B
#define EGL_BAD_STATE_KHR                                    0x321C
#define EGL_BUFFER_COUNT_NV                                  0x321D
#define EGL_CONSUMER_ACQUIRE_TIMEOUT_USEC_KHR                0x321E
#define EGL_SYNC_NEW_FRAME_NV                                0x321F
#define EGL_BAD_DEVICE_EXT                                   0x322B
#define EGL_DEVICE_EXT                                       0x322C
#define EGL_BAD_OUTPUT_LAYER_EXT                             0x322D
#define EGL_BAD_OUTPUT_PORT_EXT                              0x322E
#define EGL_SWAP_INTERVAL_EXT                                0x322F
#define EGL_DRM_DEVICE_FILE_EXT                              0x3233
#define EGL_DRM_CRTC_EXT                                     0x3234
#define EGL_DRM_PLANE_EXT                                    0x3235
#define EGL_DRM_CONNECTOR_EXT                                0x3236
#define EGL_OPENWF_DEVICE_ID_EXT                             0x3237
#define EGL_OPENWF_PIPELINE_ID_EXT                           0x3238
#define EGL_OPENWF_PORT_ID_EXT                               0x3239
#define EGL_CUDA_DEVICE_NV                                   0x323A
#define EGL_CUDA_EVENT_HANDLE_NV                             0x323B
#define EGL_SYNC_CUDA_EVENT_NV                               0x323C
#define EGL_SYNC_CUDA_EVENT_COMPLETE_NV                      0x323D
#define EGL_LINUX_DMA_BUF_EXT                                0x3270
#define EGL_LINUX_DRM_FOURCC_EXT                             0x3271
#define EGL_DMA_BUF_PLANE0_FD_EXT                            0x3272
#define EGL_DMA_BUF_PLANE0_OFFSET_EXT                        0x3273
#define EGL_DMA_BUF_PLANE0_PITCH_EXT                         0x3274
#define EGL_DMA_BUF_PLANE1_FD_EXT                            0x3275
#define EGL_DMA_BUF_PLANE1_OFFSET_EXT                        0x3276
#define EGL_DMA_BUF_PLANE1_PITCH_EXT                         0x3277
#define EGL_DMA_BUF_PLANE2_FD_EXT                            0x3278
#define EGL_DMA_BUF_PLANE2_OFFSET_EXT                        0x3279
#define EGL_DMA_BUF_PLANE2_PITCH_EXT                         0x327A
#define EGL_YUV_COLOR_SPACE_HINT_EXT                         0x327B
#define EGL_SAMPLE_RANGE_HINT_EXT                            0x327C
#define EGL_YUV_CHROMA_HORIZONTAL_SITING_HINT_EXT            0x327D
#define EGL_YUV_CHROMA_VERTICAL_SITING_HINT_EXT              0x327E
#define EGL_ITU_REC601_EXT                                   0x327F
#define EGL_ITU_REC709_EXT                                   0x3280
#define EGL_ITU_REC2020_EXT                                  0x3281
#define EGL_YUV_FULL_RANGE_EXT                               0x3282
#define EGL_YUV_NARROW_RANGE_EXT                             0x3283
#define EGL_YUV_CHROMA_SITING_0_EXT                          0x3284
#define EGL_YUV_CHROMA_SITING_0_5_EXT                        0x3285
#define EGL_DISCARD_SAMPLES_ARM                              0x3286
#define EGL_NATIVE_BUFFER_TIZEN                              0x32A0
#define EGL_NATIVE_SURFACE_TIZEN                             0x32A1
#define EGL_PROTECTED_CONTENT_EXT                            0x32C0
#define EGL_YUV_BUFFER_EXT                                   0x3300
#define EGL_YUV_ORDER_EXT                                    0x3301
#define EGL_YUV_ORDER_YUV_EXT                                0x3302
#define EGL_YUV_ORDER_YVU_EXT                                0x3303
#define EGL_YUV_ORDER_YUYV_EXT                               0x3304
#define EGL_YUV_ORDER_UYVY_EXT                               0x3305
#define EGL_YUV_ORDER_YVYU_EXT                               0x3306
#define EGL_YUV_ORDER_VYUY_EXT                               0x3307
#define EGL_YUV_ORDER_AYUV_EXT                               0x3308
#define EGL_YUV_CSC_STANDARD_EXT                             0x330A
#define EGL_YUV_CSC_STANDARD_601_EXT                         0x330B
#define EGL_YUV_CSC_STANDARD_709_EXT                         0x330C
#define EGL_YUV_CSC_STANDARD_2020_EXT                        0x330D
#define EGL_YUV_NUMBER_OF_PLANES_EXT                         0x3311
#define EGL_YUV_SUBSAMPLE_EXT                                0x3312
#define EGL_YUV_SUBSAMPLE_4_2_0_EXT                          0x3313
#define EGL_YUV_SUBSAMPLE_4_2_2_EXT                          0x3314
#define EGL_YUV_SUBSAMPLE_4_4_4_EXT                          0x3315
#define EGL_YUV_DEPTH_RANGE_EXT                              0x3317
#define EGL_YUV_DEPTH_RANGE_LIMITED_EXT                      0x3318
#define EGL_YUV_DEPTH_RANGE_FULL_EXT                         0x3319
#define EGL_YUV_PLANE_BPP_EXT                                0x331A
#define EGL_YUV_PLANE_BPP_0_EXT                              0x331B
#define EGL_YUV_PLANE_BPP_8_EXT                              0x331C
#define EGL_YUV_PLANE_BPP_10_EXT                             0x331D
#define EGL_D3D9_DEVICE_ANGLE                                0x33A0
#define EGL_D3D11_DEVICE_ANGLE                               0x33A1
#define EGL_COLOR_FORMAT_HI                                  0x8F70
#define EGL_COLOR_RGB_HI                                     0x8F71
#define EGL_COLOR_RGBA_HI                                    0x8F72
#define EGL_COLOR_ARGB_HI                                    0x8F73
#define EGL_CLIENT_PIXMAP_POINTER_HI                         0x8F74
#define EGL_FOREVER                                          0xFFFFFFFFFFFFFFFF
#define EGL_FOREVER_KHR                                      0xFFFFFFFFFFFFFFFF
#define EGL_FOREVER_NV                                       0xFFFFFFFFFFFFFFFF
#define EGL_TRUE                                             1
#define EGL_DISPLAY_SCALING                                  10000

typedef EGLBoolean (GLAPIENTRY *PFNEGLBINDAPIPROC)(EGLenum api);
typedef EGLBoolean (GLAPIENTRY *PFNEGLBINDTEXIMAGEPROC)(EGLDisplay dpy, EGLSurface surface, EGLint buffer);
typedef EGLBoolean (GLAPIENTRY *PFNEGLCHOOSECONFIGPROC)(EGLDisplay dpy, const EGLint * attrib_list, EGLConfig * configs, EGLint config_size, EGLint * num_config);
typedef EGLint (GLAPIENTRY *PFNEGLCLIENTWAITSYNCPROC)(EGLDisplay dpy, EGLSync sync, EGLint flags, EGLTime timeout);
typedef EGLint (GLAPIENTRY *PFNEGLCLIENTWAITSYNCKHRPROC)(EGLDisplay dpy, EGLSyncKHR sync, EGLint flags, EGLTimeKHR timeout);
typedef EGLint (GLAPIENTRY *PFNEGLCLIENTWAITSYNCNVPROC)(EGLSyncNV sync, EGLint flags, EGLTimeNV timeout);
typedef EGLBoolean (GLAPIENTRY *PFNEGLCOPYBUFFERSPROC)(EGLDisplay dpy, EGLSurface surface, EGLNativePixmapType target);
typedef EGLContext (GLAPIENTRY *PFNEGLCREATECONTEXTPROC)(EGLDisplay dpy, EGLConfig config, EGLContext share_context, const EGLint * attrib_list);
typedef EGLImageKHR (GLAPIENTRY *PFNEGLCREATEDRMIMAGEMESAPROC)(EGLDisplay dpy, const EGLint * attrib_list);
typedef EGLSyncNV (GLAPIENTRY *PFNEGLCREATEFENCESYNCNVPROC)(EGLDisplay dpy, EGLenum condition, const EGLint * attrib_list);
typedef EGLImage (GLAPIENTRY *PFNEGLCREATEIMAGEPROC)(EGLDisplay dpy, EGLContext ctx, EGLenum target, EGLClientBuffer buffer, const EGLAttrib * attrib_list);
typedef EGLImageKHR (GLAPIENTRY *PFNEGLCREATEIMAGEKHRPROC)(EGLDisplay dpy, EGLContext ctx, EGLenum target, EGLClientBuffer buffer, const EGLint * attrib_list);
typedef EGLSurface (GLAPIENTRY *PFNEGLCREATEPBUFFERFROMCLIENTBUFFERPROC)(EGLDisplay dpy, EGLenum buftype, EGLClientBuffer buffer, EGLConfig config, const EGLint * attrib_list);
typedef EGLSurface (GLAPIENTRY *PFNEGLCREATEPBUFFERSURFACEPROC)(EGLDisplay dpy, EGLConfig config, const EGLint * attrib_list);
typedef EGLSurface (GLAPIENTRY *PFNEGLCREATEPIXMAPSURFACEPROC)(EGLDisplay dpy, EGLConfig config, EGLNativePixmapType pixmap, const EGLint * attrib_list);
typedef EGLSurface (GLAPIENTRY *PFNEGLCREATEPIXMAPSURFACEHIPROC)(EGLDisplay dpy, EGLConfig config, struct EGLClientPixmapHI * pixmap);
typedef EGLSurface (GLAPIENTRY *PFNEGLCREATEPLATFORMPIXMAPSURFACEPROC)(EGLDisplay dpy, EGLConfig config, void * native_pixmap, const EGLAttrib * attrib_list);
typedef EGLSurface (GLAPIENTRY *PFNEGLCREATEPLATFORMPIXMAPSURFACEEXTPROC)(EGLDisplay dpy, EGLConfig config, void * native_pixmap, const EGLint * attrib_list);
typedef EGLSurface (GLAPIENTRY *PFNEGLCREATEPLATFORMWINDOWSURFACEPROC)(EGLDisplay dpy, EGLConfig config, void * native_window, const EGLAttrib * attrib_list);
typedef EGLSurface (GLAPIENTRY *PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC)(EGLDisplay dpy, EGLConfig config, void * native_window, const EGLint * attrib_list);
typedef EGLStreamKHR (GLAPIENTRY *PFNEGLCREATESTREAMFROMFILEDESCRIPTORKHRPROC)(EGLDisplay dpy, EGLNativeFileDescriptorKHR file_descriptor);
typedef EGLStreamKHR (GLAPIENTRY *PFNEGLCREATESTREAMKHRPROC)(EGLDisplay dpy, const EGLint * attrib_list);
typedef EGLSurface (GLAPIENTRY *PFNEGLCREATESTREAMPRODUCERSURFACEKHRPROC)(EGLDisplay dpy, EGLConfig config, EGLStreamKHR stream, const EGLint * attrib_list);
typedef EGLSyncKHR (GLAPIENTRY *PFNEGLCREATESTREAMSYNCNVPROC)(EGLDisplay dpy, EGLStreamKHR stream, EGLenum type, const EGLint * attrib_list);
typedef EGLSync (GLAPIENTRY *PFNEGLCREATESYNCPROC)(EGLDisplay dpy, EGLenum type, const EGLAttrib * attrib_list);
typedef EGLSyncKHR (GLAPIENTRY *PFNEGLCREATESYNC64KHRPROC)(EGLDisplay dpy, EGLenum type, const EGLAttribKHR * attrib_list);
typedef EGLSyncKHR (GLAPIENTRY *PFNEGLCREATESYNCKHRPROC)(EGLDisplay dpy, EGLenum type, const EGLint * attrib_list);
typedef EGLSurface (GLAPIENTRY *PFNEGLCREATEWINDOWSURFACEPROC)(EGLDisplay dpy, EGLConfig config, EGLNativeWindowType win, const EGLint * attrib_list);
typedef EGLBoolean (GLAPIENTRY *PFNEGLDESTROYCONTEXTPROC)(EGLDisplay dpy, EGLContext ctx);
typedef EGLBoolean (GLAPIENTRY *PFNEGLDESTROYIMAGEPROC)(EGLDisplay dpy, EGLImage image);
typedef EGLBoolean (GLAPIENTRY *PFNEGLDESTROYIMAGEKHRPROC)(EGLDisplay dpy, EGLImageKHR image);
typedef EGLBoolean (GLAPIENTRY *PFNEGLDESTROYSTREAMKHRPROC)(EGLDisplay dpy, EGLStreamKHR stream);
typedef EGLBoolean (GLAPIENTRY *PFNEGLDESTROYSURFACEPROC)(EGLDisplay dpy, EGLSurface surface);
typedef EGLBoolean (GLAPIENTRY *PFNEGLDESTROYSYNCPROC)(EGLDisplay dpy, EGLSync sync);
typedef EGLBoolean (GLAPIENTRY *PFNEGLDESTROYSYNCKHRPROC)(EGLDisplay dpy, EGLSyncKHR sync);
typedef EGLBoolean (GLAPIENTRY *PFNEGLDESTROYSYNCNVPROC)(EGLSyncNV sync);
typedef EGLint (GLAPIENTRY *PFNEGLDUPNATIVEFENCEFDANDROIDPROC)(EGLDisplay dpy, EGLSyncKHR sync);
typedef EGLBoolean (GLAPIENTRY *PFNEGLEXPORTDMABUFIMAGEMESAPROC)(EGLDisplay dpy, EGLImageKHR image, int * fds, EGLint * strides, EGLint * offsets);
typedef EGLBoolean (GLAPIENTRY *PFNEGLEXPORTDMABUFIMAGEQUERYMESAPROC)(EGLDisplay dpy, EGLImageKHR image, int * fourcc, int * num_planes, EGLuint64KHR * modifiers);
typedef EGLBoolean (GLAPIENTRY *PFNEGLEXPORTDRMIMAGEMESAPROC)(EGLDisplay dpy, EGLImageKHR image, EGLint * name, EGLint * handle, EGLint * stride);
typedef EGLBoolean (GLAPIENTRY *PFNEGLFENCENVPROC)(EGLSyncNV sync);
typedef EGLBoolean (GLAPIENTRY *PFNEGLGETCONFIGATTRIBPROC)(EGLDisplay dpy, EGLConfig config, EGLint attribute, EGLint * value);
typedef EGLBoolean (GLAPIENTRY *PFNEGLGETCONFIGSPROC)(EGLDisplay dpy, EGLConfig * configs, EGLint config_size, EGLint * num_config);
typedef EGLContext (GLAPIENTRY *PFNEGLGETCURRENTCONTEXTPROC)(void);
typedef EGLDisplay (GLAPIENTRY *PFNEGLGETCURRENTDISPLAYPROC)(void);
typedef EGLSurface (GLAPIENTRY *PFNEGLGETCURRENTSURFACEPROC)(EGLint readdraw);
typedef EGLDisplay (GLAPIENTRY *PFNEGLGETDISPLAYPROC)(EGLNativeDisplayType display_id);
typedef EGLint (GLAPIENTRY *PFNEGLGETERRORPROC)(void);
typedef EGLBoolean (GLAPIENTRY *PFNEGLGETOUTPUTLAYERSEXTPROC)(EGLDisplay dpy, const EGLAttrib * attrib_list, EGLOutputLayerEXT * layers, EGLint max_layers, EGLint * num_layers);
typedef EGLBoolean (GLAPIENTRY *PFNEGLGETOUTPUTPORTSEXTPROC)(EGLDisplay dpy, const EGLAttrib * attrib_list, EGLOutputPortEXT * ports, EGLint max_ports, EGLint * num_ports);
typedef EGLDisplay (GLAPIENTRY *PFNEGLGETPLATFORMDISPLAYPROC)(EGLenum platform, void * native_display, const EGLAttrib * attrib_list);
typedef EGLDisplay (GLAPIENTRY *PFNEGLGETPLATFORMDISPLAYEXTPROC)(EGLenum platform, void * native_display, const EGLint * attrib_list);
typedef __eglMustCastToProperFunctionPointerType (GLAPIENTRY *PFNEGLGETPROCADDRESSPROC)(const char * procname);
typedef EGLNativeFileDescriptorKHR (GLAPIENTRY *PFNEGLGETSTREAMFILEDESCRIPTORKHRPROC)(EGLDisplay dpy, EGLStreamKHR stream);
typedef EGLBoolean (GLAPIENTRY *PFNEGLGETSYNCATTRIBPROC)(EGLDisplay dpy, EGLSync sync, EGLint attribute, EGLAttrib * value);
typedef EGLBoolean (GLAPIENTRY *PFNEGLGETSYNCATTRIBKHRPROC)(EGLDisplay dpy, EGLSyncKHR sync, EGLint attribute, EGLint * value);
typedef EGLBoolean (GLAPIENTRY *PFNEGLGETSYNCATTRIBNVPROC)(EGLSyncNV sync, EGLint attribute, EGLint * value);
typedef EGLuint64NV (GLAPIENTRY *PFNEGLGETSYSTEMTIMEFREQUENCYNVPROC)(void);
typedef EGLuint64NV (GLAPIENTRY *PFNEGLGETSYSTEMTIMENVPROC)(void);
typedef EGLBoolean (GLAPIENTRY *PFNEGLINITIALIZEPROC)(EGLDisplay dpy, EGLint * major, EGLint * minor);
typedef EGLBoolean (GLAPIENTRY *PFNEGLLOCKSURFACEKHRPROC)(EGLDisplay dpy, EGLSurface surface, const EGLint * attrib_list);
typedef EGLBoolean (GLAPIENTRY *PFNEGLMAKECURRENTPROC)(EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx);
typedef EGLBoolean (GLAPIENTRY *PFNEGLOUTPUTLAYERATTRIBEXTPROC)(EGLDisplay dpy, EGLOutputLayerEXT layer, EGLint attribute, EGLAttrib value);
typedef EGLBoolean (GLAPIENTRY *PFNEGLOUTPUTPORTATTRIBEXTPROC)(EGLDisplay dpy, EGLOutputPortEXT port, EGLint attribute, EGLAttrib value);
typedef EGLBoolean (GLAPIENTRY *PFNEGLPOSTSUBBUFFERNVPROC)(EGLDisplay dpy, EGLSurface surface, EGLint x, EGLint y, EGLint width, EGLint height);
typedef EGLenum (GLAPIENTRY *PFNEGLQUERYAPIPROC)(void);
typedef EGLBoolean (GLAPIENTRY *PFNEGLQUERYCONTEXTPROC)(EGLDisplay dpy, EGLContext ctx, EGLint attribute, EGLint * value);
typedef EGLBoolean (GLAPIENTRY *PFNEGLQUERYDEVICEATTRIBEXTPROC)(EGLDeviceEXT device, EGLint attribute, EGLAttrib * value);
typedef const char * (GLAPIENTRY *PFNEGLQUERYDEVICESTRINGEXTPROC)(EGLDeviceEXT device, EGLint name);
typedef EGLBoolean (GLAPIENTRY *PFNEGLQUERYDEVICESEXTPROC)(EGLint max_devices, EGLDeviceEXT * devices, EGLint * num_devices);
typedef EGLBoolean (GLAPIENTRY *PFNEGLQUERYDISPLAYATTRIBEXTPROC)(EGLDisplay dpy, EGLint attribute, EGLAttrib * value);
typedef EGLBoolean (GLAPIENTRY *PFNEGLQUERYNATIVEDISPLAYNVPROC)(EGLDisplay dpy, EGLNativeDisplayType * display_id);
typedef EGLBoolean (GLAPIENTRY *PFNEGLQUERYNATIVEPIXMAPNVPROC)(EGLDisplay dpy, EGLSurface surf, EGLNativePixmapType * pixmap);
typedef EGLBoolean (GLAPIENTRY *PFNEGLQUERYNATIVEWINDOWNVPROC)(EGLDisplay dpy, EGLSurface surf, EGLNativeWindowType * window);
typedef EGLBoolean (GLAPIENTRY *PFNEGLQUERYOUTPUTLAYERATTRIBEXTPROC)(EGLDisplay dpy, EGLOutputLayerEXT layer, EGLint attribute, EGLAttrib * value);
typedef const char * (GLAPIENTRY *PFNEGLQUERYOUTPUTLAYERSTRINGEXTPROC)(EGLDisplay dpy, EGLOutputLayerEXT layer, EGLint name);
typedef EGLBoolean (GLAPIENTRY *PFNEGLQUERYOUTPUTPORTATTRIBEXTPROC)(EGLDisplay dpy, EGLOutputPortEXT port, EGLint attribute, EGLAttrib * value);
typedef const char * (GLAPIENTRY *PFNEGLQUERYOUTPUTPORTSTRINGEXTPROC)(EGLDisplay dpy, EGLOutputPortEXT port, EGLint name);
typedef EGLBoolean (GLAPIENTRY *PFNEGLQUERYSTREAMKHRPROC)(EGLDisplay dpy, EGLStreamKHR stream, EGLenum attribute, EGLint * value);
typedef EGLBoolean (GLAPIENTRY *PFNEGLQUERYSTREAMTIMEKHRPROC)(EGLDisplay dpy, EGLStreamKHR stream, EGLenum attribute, EGLTimeKHR * value);
typedef EGLBoolean (GLAPIENTRY *PFNEGLQUERYSTREAMU64KHRPROC)(EGLDisplay dpy, EGLStreamKHR stream, EGLenum attribute, EGLuint64KHR * value);
typedef const char * (GLAPIENTRY *PFNEGLQUERYSTRINGPROC)(EGLDisplay dpy, EGLint name);
typedef EGLBoolean (GLAPIENTRY *PFNEGLQUERYSURFACEPROC)(EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint * value);
typedef EGLBoolean (GLAPIENTRY *PFNEGLQUERYSURFACE64KHRPROC)(EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLAttribKHR * value);
typedef EGLBoolean (GLAPIENTRY *PFNEGLQUERYSURFACEPOINTERANGLEPROC)(EGLDisplay dpy, EGLSurface surface, EGLint attribute, void ** value);
typedef EGLBoolean (GLAPIENTRY *PFNEGLRELEASETEXIMAGEPROC)(EGLDisplay dpy, EGLSurface surface, EGLint buffer);
typedef EGLBoolean (GLAPIENTRY *PFNEGLRELEASETHREADPROC)(void);
typedef void (GLAPIENTRY *PFNEGLSETBLOBCACHEFUNCSANDROIDPROC)(EGLDisplay dpy, EGLSetBlobFuncANDROID set, EGLGetBlobFuncANDROID get);
typedef EGLBoolean (GLAPIENTRY *PFNEGLSETDAMAGEREGIONKHRPROC)(EGLDisplay dpy, EGLSurface surface, EGLint * rects, EGLint n_rects);
typedef EGLBoolean (GLAPIENTRY *PFNEGLSIGNALSYNCKHRPROC)(EGLDisplay dpy, EGLSyncKHR sync, EGLenum mode);
typedef EGLBoolean (GLAPIENTRY *PFNEGLSIGNALSYNCNVPROC)(EGLSyncNV sync, EGLenum mode);
typedef EGLBoolean (GLAPIENTRY *PFNEGLSTREAMATTRIBKHRPROC)(EGLDisplay dpy, EGLStreamKHR stream, EGLenum attribute, EGLint value);
typedef EGLBoolean (GLAPIENTRY *PFNEGLSTREAMCONSUMERACQUIREKHRPROC)(EGLDisplay dpy, EGLStreamKHR stream);
typedef EGLBoolean (GLAPIENTRY *PFNEGLSTREAMCONSUMERGLTEXTUREEXTERNALKHRPROC)(EGLDisplay dpy, EGLStreamKHR stream);
typedef EGLBoolean (GLAPIENTRY *PFNEGLSTREAMCONSUMEROUTPUTEXTPROC)(EGLDisplay dpy, EGLStreamKHR stream, EGLOutputLayerEXT layer);
typedef EGLBoolean (GLAPIENTRY *PFNEGLSTREAMCONSUMERRELEASEKHRPROC)(EGLDisplay dpy, EGLStreamKHR stream);
typedef EGLBoolean (GLAPIENTRY *PFNEGLSURFACEATTRIBPROC)(EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint value);
typedef EGLBoolean (GLAPIENTRY *PFNEGLSWAPBUFFERSPROC)(EGLDisplay dpy, EGLSurface surface);
typedef EGLBoolean (GLAPIENTRY *PFNEGLSWAPBUFFERSREGION2NOKPROC)(EGLDisplay dpy, EGLSurface surface, EGLint numRects, const EGLint * rects);
typedef EGLBoolean (GLAPIENTRY *PFNEGLSWAPBUFFERSREGIONNOKPROC)(EGLDisplay dpy, EGLSurface surface, EGLint numRects, const EGLint * rects);
typedef EGLBoolean (GLAPIENTRY *PFNEGLSWAPBUFFERSWITHDAMAGEEXTPROC)(EGLDisplay dpy, EGLSurface surface, EGLint * rects, EGLint n_rects);
typedef EGLBoolean (GLAPIENTRY *PFNEGLSWAPBUFFERSWITHDAMAGEKHRPROC)(EGLDisplay dpy, EGLSurface surface, EGLint * rects, EGLint n_rects);
typedef EGLBoolean (GLAPIENTRY *PFNEGLSWAPINTERVALPROC)(EGLDisplay dpy, EGLint interval);
typedef EGLBoolean (GLAPIENTRY *PFNEGLTERMINATEPROC)(EGLDisplay dpy);
typedef EGLBoolean (GLAPIENTRY *PFNEGLUNLOCKSURFACEKHRPROC)(EGLDisplay dpy, EGLSurface surface);
typedef EGLBoolean (GLAPIENTRY *PFNEGLWAITCLIENTPROC)(void);
typedef EGLBoolean (GLAPIENTRY *PFNEGLWAITGLPROC)(void);
typedef EGLBoolean (GLAPIENTRY *PFNEGLWAITNATIVEPROC)(EGLint engine);
typedef EGLBoolean (GLAPIENTRY *PFNEGLWAITSYNCPROC)(EGLDisplay dpy, EGLSync sync, EGLint flags);
typedef EGLint (GLAPIENTRY *PFNEGLWAITSYNCKHRPROC)(EGLDisplay dpy, EGLSyncKHR sync, EGLint flags);
extern EPOXY_IMPORTEXPORT EGLBoolean (EPOXY_CALLSPEC *epoxy_eglBindAPI)(EGLenum api);

extern EPOXY_IMPORTEXPORT EGLBoolean (EPOXY_CALLSPEC *epoxy_eglBindTexImage)(EGLDisplay dpy, EGLSurface surface, EGLint buffer);

extern EPOXY_IMPORTEXPORT EGLBoolean (EPOXY_CALLSPEC *epoxy_eglChooseConfig)(EGLDisplay dpy, const EGLint * attrib_list, EGLConfig * configs, EGLint config_size, EGLint * num_config);

extern EPOXY_IMPORTEXPORT EGLint (EPOXY_CALLSPEC *epoxy_eglClientWaitSync)(EGLDisplay dpy, EGLSync sync, EGLint flags, EGLTime timeout);

extern EPOXY_IMPORTEXPORT EGLint (EPOXY_CALLSPEC *epoxy_eglClientWaitSyncKHR)(EGLDisplay dpy, EGLSyncKHR sync, EGLint flags, EGLTimeKHR timeout);

extern EPOXY_IMPORTEXPORT EGLint (EPOXY_CALLSPEC *epoxy_eglClientWaitSyncNV)(EGLSyncNV sync, EGLint flags, EGLTimeNV timeout);

extern EPOXY_IMPORTEXPORT EGLBoolean (EPOXY_CALLSPEC *epoxy_eglCopyBuffers)(EGLDisplay dpy, EGLSurface surface, EGLNativePixmapType target);

extern EPOXY_IMPORTEXPORT EGLContext (EPOXY_CALLSPEC *epoxy_eglCreateContext)(EGLDisplay dpy, EGLConfig config, EGLContext share_context, const EGLint * attrib_list);

extern EPOXY_IMPORTEXPORT EGLImageKHR (EPOXY_CALLSPEC *epoxy_eglCreateDRMImageMESA)(EGLDisplay dpy, const EGLint * attrib_list);

extern EPOXY_IMPORTEXPORT EGLSyncNV (EPOXY_CALLSPEC *epoxy_eglCreateFenceSyncNV)(EGLDisplay dpy, EGLenum condition, const EGLint * attrib_list);

extern EPOXY_IMPORTEXPORT EGLImage (EPOXY_CALLSPEC *epoxy_eglCreateImage)(EGLDisplay dpy, EGLContext ctx, EGLenum target, EGLClientBuffer buffer, const EGLAttrib * attrib_list);

extern EPOXY_IMPORTEXPORT EGLImageKHR (EPOXY_CALLSPEC *epoxy_eglCreateImageKHR)(EGLDisplay dpy, EGLContext ctx, EGLenum target, EGLClientBuffer buffer, const EGLint * attrib_list);

extern EPOXY_IMPORTEXPORT EGLSurface (EPOXY_CALLSPEC *epoxy_eglCreatePbufferFromClientBuffer)(EGLDisplay dpy, EGLenum buftype, EGLClientBuffer buffer, EGLConfig config, const EGLint * attrib_list);

extern EPOXY_IMPORTEXPORT EGLSurface (EPOXY_CALLSPEC *epoxy_eglCreatePbufferSurface)(EGLDisplay dpy, EGLConfig config, const EGLint * attrib_list);

extern EPOXY_IMPORTEXPORT EGLSurface (EPOXY_CALLSPEC *epoxy_eglCreatePixmapSurface)(EGLDisplay dpy, EGLConfig config, EGLNativePixmapType pixmap, const EGLint * attrib_list);

extern EPOXY_IMPORTEXPORT EGLSurface (EPOXY_CALLSPEC *epoxy_eglCreatePixmapSurfaceHI)(EGLDisplay dpy, EGLConfig config, struct EGLClientPixmapHI * pixmap);

extern EPOXY_IMPORTEXPORT EGLSurface (EPOXY_CALLSPEC *epoxy_eglCreatePlatformPixmapSurface)(EGLDisplay dpy, EGLConfig config, void * native_pixmap, const EGLAttrib * attrib_list);

extern EPOXY_IMPORTEXPORT EGLSurface (EPOXY_CALLSPEC *epoxy_eglCreatePlatformPixmapSurfaceEXT)(EGLDisplay dpy, EGLConfig config, void * native_pixmap, const EGLint * attrib_list);

extern EPOXY_IMPORTEXPORT EGLSurface (EPOXY_CALLSPEC *epoxy_eglCreatePlatformWindowSurface)(EGLDisplay dpy, EGLConfig config, void * native_window, const EGLAttrib * attrib_list);

extern EPOXY_IMPORTEXPORT EGLSurface (EPOXY_CALLSPEC *epoxy_eglCreatePlatformWindowSurfaceEXT)(EGLDisplay dpy, EGLConfig config, void * native_window, const EGLint * attrib_list);

extern EPOXY_IMPORTEXPORT EGLStreamKHR (EPOXY_CALLSPEC *epoxy_eglCreateStreamFromFileDescriptorKHR)(EGLDisplay dpy, EGLNativeFileDescriptorKHR file_descriptor);

extern EPOXY_IMPORTEXPORT EGLStreamKHR (EPOXY_CALLSPEC *epoxy_eglCreateStreamKHR)(EGLDisplay dpy, const EGLint * attrib_list);

extern EPOXY_IMPORTEXPORT EGLSurface (EPOXY_CALLSPEC *epoxy_eglCreateStreamProducerSurfaceKHR)(EGLDisplay dpy, EGLConfig config, EGLStreamKHR stream, const EGLint * attrib_list);

extern EPOXY_IMPORTEXPORT EGLSyncKHR (EPOXY_CALLSPEC *epoxy_eglCreateStreamSyncNV)(EGLDisplay dpy, EGLStreamKHR stream, EGLenum type, const EGLint * attrib_list);

extern EPOXY_IMPORTEXPORT EGLSync (EPOXY_CALLSPEC *epoxy_eglCreateSync)(EGLDisplay dpy, EGLenum type, const EGLAttrib * attrib_list);

extern EPOXY_IMPORTEXPORT EGLSyncKHR (EPOXY_CALLSPEC *epoxy_eglCreateSync64KHR)(EGLDisplay dpy, EGLenum type, const EGLAttribKHR * attrib_list);

extern EPOXY_IMPORTEXPORT EGLSyncKHR (EPOXY_CALLSPEC *epoxy_eglCreateSyncKHR)(EGLDisplay dpy, EGLenum type, const EGLint * attrib_list);

extern EPOXY_IMPORTEXPORT EGLSurface (EPOXY_CALLSPEC *epoxy_eglCreateWindowSurface)(EGLDisplay dpy, EGLConfig config, EGLNativeWindowType win, const EGLint * attrib_list);

extern EPOXY_IMPORTEXPORT EGLBoolean (EPOXY_CALLSPEC *epoxy_eglDestroyContext)(EGLDisplay dpy, EGLContext ctx);

extern EPOXY_IMPORTEXPORT EGLBoolean (EPOXY_CALLSPEC *epoxy_eglDestroyImage)(EGLDisplay dpy, EGLImage image);

extern EPOXY_IMPORTEXPORT EGLBoolean (EPOXY_CALLSPEC *epoxy_eglDestroyImageKHR)(EGLDisplay dpy, EGLImageKHR image);

extern EPOXY_IMPORTEXPORT EGLBoolean (EPOXY_CALLSPEC *epoxy_eglDestroyStreamKHR)(EGLDisplay dpy, EGLStreamKHR stream);

extern EPOXY_IMPORTEXPORT EGLBoolean (EPOXY_CALLSPEC *epoxy_eglDestroySurface)(EGLDisplay dpy, EGLSurface surface);

extern EPOXY_IMPORTEXPORT EGLBoolean (EPOXY_CALLSPEC *epoxy_eglDestroySync)(EGLDisplay dpy, EGLSync sync);

extern EPOXY_IMPORTEXPORT EGLBoolean (EPOXY_CALLSPEC *epoxy_eglDestroySyncKHR)(EGLDisplay dpy, EGLSyncKHR sync);

extern EPOXY_IMPORTEXPORT EGLBoolean (EPOXY_CALLSPEC *epoxy_eglDestroySyncNV)(EGLSyncNV sync);

extern EPOXY_IMPORTEXPORT EGLint (EPOXY_CALLSPEC *epoxy_eglDupNativeFenceFDANDROID)(EGLDisplay dpy, EGLSyncKHR sync);

extern EPOXY_IMPORTEXPORT EGLBoolean (EPOXY_CALLSPEC *epoxy_eglExportDMABUFImageMESA)(EGLDisplay dpy, EGLImageKHR image, int * fds, EGLint * strides, EGLint * offsets);

extern EPOXY_IMPORTEXPORT EGLBoolean (EPOXY_CALLSPEC *epoxy_eglExportDMABUFImageQueryMESA)(EGLDisplay dpy, EGLImageKHR image, int * fourcc, int * num_planes, EGLuint64KHR * modifiers);

extern EPOXY_IMPORTEXPORT EGLBoolean (EPOXY_CALLSPEC *epoxy_eglExportDRMImageMESA)(EGLDisplay dpy, EGLImageKHR image, EGLint * name, EGLint * handle, EGLint * stride);

extern EPOXY_IMPORTEXPORT EGLBoolean (EPOXY_CALLSPEC *epoxy_eglFenceNV)(EGLSyncNV sync);

extern EPOXY_IMPORTEXPORT EGLBoolean (EPOXY_CALLSPEC *epoxy_eglGetConfigAttrib)(EGLDisplay dpy, EGLConfig config, EGLint attribute, EGLint * value);

extern EPOXY_IMPORTEXPORT EGLBoolean (EPOXY_CALLSPEC *epoxy_eglGetConfigs)(EGLDisplay dpy, EGLConfig * configs, EGLint config_size, EGLint * num_config);

extern EPOXY_IMPORTEXPORT EGLContext (EPOXY_CALLSPEC *epoxy_eglGetCurrentContext)(void);

extern EPOXY_IMPORTEXPORT EGLDisplay (EPOXY_CALLSPEC *epoxy_eglGetCurrentDisplay)(void);

extern EPOXY_IMPORTEXPORT EGLSurface (EPOXY_CALLSPEC *epoxy_eglGetCurrentSurface)(EGLint readdraw);

extern EPOXY_IMPORTEXPORT EGLDisplay (EPOXY_CALLSPEC *epoxy_eglGetDisplay)(EGLNativeDisplayType display_id);

extern EPOXY_IMPORTEXPORT EGLint (EPOXY_CALLSPEC *epoxy_eglGetError)(void);

extern EPOXY_IMPORTEXPORT EGLBoolean (EPOXY_CALLSPEC *epoxy_eglGetOutputLayersEXT)(EGLDisplay dpy, const EGLAttrib * attrib_list, EGLOutputLayerEXT * layers, EGLint max_layers, EGLint * num_layers);

extern EPOXY_IMPORTEXPORT EGLBoolean (EPOXY_CALLSPEC *epoxy_eglGetOutputPortsEXT)(EGLDisplay dpy, const EGLAttrib * attrib_list, EGLOutputPortEXT * ports, EGLint max_ports, EGLint * num_ports);

extern EPOXY_IMPORTEXPORT EGLDisplay (EPOXY_CALLSPEC *epoxy_eglGetPlatformDisplay)(EGLenum platform, void * native_display, const EGLAttrib * attrib_list);

extern EPOXY_IMPORTEXPORT EGLDisplay (EPOXY_CALLSPEC *epoxy_eglGetPlatformDisplayEXT)(EGLenum platform, void * native_display, const EGLint * attrib_list);

extern EPOXY_IMPORTEXPORT __eglMustCastToProperFunctionPointerType (EPOXY_CALLSPEC *epoxy_eglGetProcAddress)(const char * procname);

extern EPOXY_IMPORTEXPORT EGLNativeFileDescriptorKHR (EPOXY_CALLSPEC *epoxy_eglGetStreamFileDescriptorKHR)(EGLDisplay dpy, EGLStreamKHR stream);

extern EPOXY_IMPORTEXPORT EGLBoolean (EPOXY_CALLSPEC *epoxy_eglGetSyncAttrib)(EGLDisplay dpy, EGLSync sync, EGLint attribute, EGLAttrib * value);

extern EPOXY_IMPORTEXPORT EGLBoolean (EPOXY_CALLSPEC *epoxy_eglGetSyncAttribKHR)(EGLDisplay dpy, EGLSyncKHR sync, EGLint attribute, EGLint * value);

extern EPOXY_IMPORTEXPORT EGLBoolean (EPOXY_CALLSPEC *epoxy_eglGetSyncAttribNV)(EGLSyncNV sync, EGLint attribute, EGLint * value);

extern EPOXY_IMPORTEXPORT EGLuint64NV (EPOXY_CALLSPEC *epoxy_eglGetSystemTimeFrequencyNV)(void);

extern EPOXY_IMPORTEXPORT EGLuint64NV (EPOXY_CALLSPEC *epoxy_eglGetSystemTimeNV)(void);

extern EPOXY_IMPORTEXPORT EGLBoolean (EPOXY_CALLSPEC *epoxy_eglInitialize)(EGLDisplay dpy, EGLint * major, EGLint * minor);

extern EPOXY_IMPORTEXPORT EGLBoolean (EPOXY_CALLSPEC *epoxy_eglLockSurfaceKHR)(EGLDisplay dpy, EGLSurface surface, const EGLint * attrib_list);

extern EPOXY_IMPORTEXPORT EGLBoolean (EPOXY_CALLSPEC *epoxy_eglMakeCurrent)(EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx);

extern EPOXY_IMPORTEXPORT EGLBoolean (EPOXY_CALLSPEC *epoxy_eglOutputLayerAttribEXT)(EGLDisplay dpy, EGLOutputLayerEXT layer, EGLint attribute, EGLAttrib value);

extern EPOXY_IMPORTEXPORT EGLBoolean (EPOXY_CALLSPEC *epoxy_eglOutputPortAttribEXT)(EGLDisplay dpy, EGLOutputPortEXT port, EGLint attribute, EGLAttrib value);

extern EPOXY_IMPORTEXPORT EGLBoolean (EPOXY_CALLSPEC *epoxy_eglPostSubBufferNV)(EGLDisplay dpy, EGLSurface surface, EGLint x, EGLint y, EGLint width, EGLint height);

extern EPOXY_IMPORTEXPORT EGLenum (EPOXY_CALLSPEC *epoxy_eglQueryAPI)(void);

extern EPOXY_IMPORTEXPORT EGLBoolean (EPOXY_CALLSPEC *epoxy_eglQueryContext)(EGLDisplay dpy, EGLContext ctx, EGLint attribute, EGLint * value);

extern EPOXY_IMPORTEXPORT EGLBoolean (EPOXY_CALLSPEC *epoxy_eglQueryDeviceAttribEXT)(EGLDeviceEXT device, EGLint attribute, EGLAttrib * value);

extern EPOXY_IMPORTEXPORT const char * (EPOXY_CALLSPEC *epoxy_eglQueryDeviceStringEXT)(EGLDeviceEXT device, EGLint name);

extern EPOXY_IMPORTEXPORT EGLBoolean (EPOXY_CALLSPEC *epoxy_eglQueryDevicesEXT)(EGLint max_devices, EGLDeviceEXT * devices, EGLint * num_devices);

extern EPOXY_IMPORTEXPORT EGLBoolean (EPOXY_CALLSPEC *epoxy_eglQueryDisplayAttribEXT)(EGLDisplay dpy, EGLint attribute, EGLAttrib * value);

extern EPOXY_IMPORTEXPORT EGLBoolean (EPOXY_CALLSPEC *epoxy_eglQueryNativeDisplayNV)(EGLDisplay dpy, EGLNativeDisplayType * display_id);

extern EPOXY_IMPORTEXPORT EGLBoolean (EPOXY_CALLSPEC *epoxy_eglQueryNativePixmapNV)(EGLDisplay dpy, EGLSurface surf, EGLNativePixmapType * pixmap);

extern EPOXY_IMPORTEXPORT EGLBoolean (EPOXY_CALLSPEC *epoxy_eglQueryNativeWindowNV)(EGLDisplay dpy, EGLSurface surf, EGLNativeWindowType * window);

extern EPOXY_IMPORTEXPORT EGLBoolean (EPOXY_CALLSPEC *epoxy_eglQueryOutputLayerAttribEXT)(EGLDisplay dpy, EGLOutputLayerEXT layer, EGLint attribute, EGLAttrib * value);

extern EPOXY_IMPORTEXPORT const char * (EPOXY_CALLSPEC *epoxy_eglQueryOutputLayerStringEXT)(EGLDisplay dpy, EGLOutputLayerEXT layer, EGLint name);

extern EPOXY_IMPORTEXPORT EGLBoolean (EPOXY_CALLSPEC *epoxy_eglQueryOutputPortAttribEXT)(EGLDisplay dpy, EGLOutputPortEXT port, EGLint attribute, EGLAttrib * value);

extern EPOXY_IMPORTEXPORT const char * (EPOXY_CALLSPEC *epoxy_eglQueryOutputPortStringEXT)(EGLDisplay dpy, EGLOutputPortEXT port, EGLint name);

extern EPOXY_IMPORTEXPORT EGLBoolean (EPOXY_CALLSPEC *epoxy_eglQueryStreamKHR)(EGLDisplay dpy, EGLStreamKHR stream, EGLenum attribute, EGLint * value);

extern EPOXY_IMPORTEXPORT EGLBoolean (EPOXY_CALLSPEC *epoxy_eglQueryStreamTimeKHR)(EGLDisplay dpy, EGLStreamKHR stream, EGLenum attribute, EGLTimeKHR * value);

extern EPOXY_IMPORTEXPORT EGLBoolean (EPOXY_CALLSPEC *epoxy_eglQueryStreamu64KHR)(EGLDisplay dpy, EGLStreamKHR stream, EGLenum attribute, EGLuint64KHR * value);

extern EPOXY_IMPORTEXPORT const char * (EPOXY_CALLSPEC *epoxy_eglQueryString)(EGLDisplay dpy, EGLint name);

extern EPOXY_IMPORTEXPORT EGLBoolean (EPOXY_CALLSPEC *epoxy_eglQuerySurface)(EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint * value);

extern EPOXY_IMPORTEXPORT EGLBoolean (EPOXY_CALLSPEC *epoxy_eglQuerySurface64KHR)(EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLAttribKHR * value);

extern EPOXY_IMPORTEXPORT EGLBoolean (EPOXY_CALLSPEC *epoxy_eglQuerySurfacePointerANGLE)(EGLDisplay dpy, EGLSurface surface, EGLint attribute, void ** value);

extern EPOXY_IMPORTEXPORT EGLBoolean (EPOXY_CALLSPEC *epoxy_eglReleaseTexImage)(EGLDisplay dpy, EGLSurface surface, EGLint buffer);

extern EPOXY_IMPORTEXPORT EGLBoolean (EPOXY_CALLSPEC *epoxy_eglReleaseThread)(void);

extern EPOXY_IMPORTEXPORT void (EPOXY_CALLSPEC *epoxy_eglSetBlobCacheFuncsANDROID)(EGLDisplay dpy, EGLSetBlobFuncANDROID set, EGLGetBlobFuncANDROID get);

extern EPOXY_IMPORTEXPORT EGLBoolean (EPOXY_CALLSPEC *epoxy_eglSetDamageRegionKHR)(EGLDisplay dpy, EGLSurface surface, EGLint * rects, EGLint n_rects);

extern EPOXY_IMPORTEXPORT EGLBoolean (EPOXY_CALLSPEC *epoxy_eglSignalSyncKHR)(EGLDisplay dpy, EGLSyncKHR sync, EGLenum mode);

extern EPOXY_IMPORTEXPORT EGLBoolean (EPOXY_CALLSPEC *epoxy_eglSignalSyncNV)(EGLSyncNV sync, EGLenum mode);

extern EPOXY_IMPORTEXPORT EGLBoolean (EPOXY_CALLSPEC *epoxy_eglStreamAttribKHR)(EGLDisplay dpy, EGLStreamKHR stream, EGLenum attribute, EGLint value);

extern EPOXY_IMPORTEXPORT EGLBoolean (EPOXY_CALLSPEC *epoxy_eglStreamConsumerAcquireKHR)(EGLDisplay dpy, EGLStreamKHR stream);

extern EPOXY_IMPORTEXPORT EGLBoolean (EPOXY_CALLSPEC *epoxy_eglStreamConsumerGLTextureExternalKHR)(EGLDisplay dpy, EGLStreamKHR stream);

extern EPOXY_IMPORTEXPORT EGLBoolean (EPOXY_CALLSPEC *epoxy_eglStreamConsumerOutputEXT)(EGLDisplay dpy, EGLStreamKHR stream, EGLOutputLayerEXT layer);

extern EPOXY_IMPORTEXPORT EGLBoolean (EPOXY_CALLSPEC *epoxy_eglStreamConsumerReleaseKHR)(EGLDisplay dpy, EGLStreamKHR stream);

extern EPOXY_IMPORTEXPORT EGLBoolean (EPOXY_CALLSPEC *epoxy_eglSurfaceAttrib)(EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint value);

extern EPOXY_IMPORTEXPORT EGLBoolean (EPOXY_CALLSPEC *epoxy_eglSwapBuffers)(EGLDisplay dpy, EGLSurface surface);

extern EPOXY_IMPORTEXPORT EGLBoolean (EPOXY_CALLSPEC *epoxy_eglSwapBuffersRegion2NOK)(EGLDisplay dpy, EGLSurface surface, EGLint numRects, const EGLint * rects);

extern EPOXY_IMPORTEXPORT EGLBoolean (EPOXY_CALLSPEC *epoxy_eglSwapBuffersRegionNOK)(EGLDisplay dpy, EGLSurface surface, EGLint numRects, const EGLint * rects);

extern EPOXY_IMPORTEXPORT EGLBoolean (EPOXY_CALLSPEC *epoxy_eglSwapBuffersWithDamageEXT)(EGLDisplay dpy, EGLSurface surface, EGLint * rects, EGLint n_rects);

extern EPOXY_IMPORTEXPORT EGLBoolean (EPOXY_CALLSPEC *epoxy_eglSwapBuffersWithDamageKHR)(EGLDisplay dpy, EGLSurface surface, EGLint * rects, EGLint n_rects);

extern EPOXY_IMPORTEXPORT EGLBoolean (EPOXY_CALLSPEC *epoxy_eglSwapInterval)(EGLDisplay dpy, EGLint interval);

extern EPOXY_IMPORTEXPORT EGLBoolean (EPOXY_CALLSPEC *epoxy_eglTerminate)(EGLDisplay dpy);

extern EPOXY_IMPORTEXPORT EGLBoolean (EPOXY_CALLSPEC *epoxy_eglUnlockSurfaceKHR)(EGLDisplay dpy, EGLSurface surface);

extern EPOXY_IMPORTEXPORT EGLBoolean (EPOXY_CALLSPEC *epoxy_eglWaitClient)(void);

extern EPOXY_IMPORTEXPORT EGLBoolean (EPOXY_CALLSPEC *epoxy_eglWaitGL)(void);

extern EPOXY_IMPORTEXPORT EGLBoolean (EPOXY_CALLSPEC *epoxy_eglWaitNative)(EGLint engine);

extern EPOXY_IMPORTEXPORT EGLBoolean (EPOXY_CALLSPEC *epoxy_eglWaitSync)(EGLDisplay dpy, EGLSync sync, EGLint flags);

extern EPOXY_IMPORTEXPORT EGLint (EPOXY_CALLSPEC *epoxy_eglWaitSyncKHR)(EGLDisplay dpy, EGLSyncKHR sync, EGLint flags);

#define eglBindAPI epoxy_eglBindAPI
#define eglBindTexImage epoxy_eglBindTexImage
#define eglChooseConfig epoxy_eglChooseConfig
#define eglClientWaitSync epoxy_eglClientWaitSync
#define eglClientWaitSyncKHR epoxy_eglClientWaitSyncKHR
#define eglClientWaitSyncNV epoxy_eglClientWaitSyncNV
#define eglCopyBuffers epoxy_eglCopyBuffers
#define eglCreateContext epoxy_eglCreateContext
#define eglCreateDRMImageMESA epoxy_eglCreateDRMImageMESA
#define eglCreateFenceSyncNV epoxy_eglCreateFenceSyncNV
#define eglCreateImage epoxy_eglCreateImage
#define eglCreateImageKHR epoxy_eglCreateImageKHR
#define eglCreatePbufferFromClientBuffer epoxy_eglCreatePbufferFromClientBuffer
#define eglCreatePbufferSurface epoxy_eglCreatePbufferSurface
#define eglCreatePixmapSurface epoxy_eglCreatePixmapSurface
#define eglCreatePixmapSurfaceHI epoxy_eglCreatePixmapSurfaceHI
#define eglCreatePlatformPixmapSurface epoxy_eglCreatePlatformPixmapSurface
#define eglCreatePlatformPixmapSurfaceEXT epoxy_eglCreatePlatformPixmapSurfaceEXT
#define eglCreatePlatformWindowSurface epoxy_eglCreatePlatformWindowSurface
#define eglCreatePlatformWindowSurfaceEXT epoxy_eglCreatePlatformWindowSurfaceEXT
#define eglCreateStreamFromFileDescriptorKHR epoxy_eglCreateStreamFromFileDescriptorKHR
#define eglCreateStreamKHR epoxy_eglCreateStreamKHR
#define eglCreateStreamProducerSurfaceKHR epoxy_eglCreateStreamProducerSurfaceKHR
#define eglCreateStreamSyncNV epoxy_eglCreateStreamSyncNV
#define eglCreateSync epoxy_eglCreateSync
#define eglCreateSync64KHR epoxy_eglCreateSync64KHR
#define eglCreateSyncKHR epoxy_eglCreateSyncKHR
#define eglCreateWindowSurface epoxy_eglCreateWindowSurface
#define eglDestroyContext epoxy_eglDestroyContext
#define eglDestroyImage epoxy_eglDestroyImage
#define eglDestroyImageKHR epoxy_eglDestroyImageKHR
#define eglDestroyStreamKHR epoxy_eglDestroyStreamKHR
#define eglDestroySurface epoxy_eglDestroySurface
#define eglDestroySync epoxy_eglDestroySync
#define eglDestroySyncKHR epoxy_eglDestroySyncKHR
#define eglDestroySyncNV epoxy_eglDestroySyncNV
#define eglDupNativeFenceFDANDROID epoxy_eglDupNativeFenceFDANDROID
#define eglExportDMABUFImageMESA epoxy_eglExportDMABUFImageMESA
#define eglExportDMABUFImageQueryMESA epoxy_eglExportDMABUFImageQueryMESA
#define eglExportDRMImageMESA epoxy_eglExportDRMImageMESA
#define eglFenceNV epoxy_eglFenceNV
#define eglGetConfigAttrib epoxy_eglGetConfigAttrib
#define eglGetConfigs epoxy_eglGetConfigs
#define eglGetCurrentContext epoxy_eglGetCurrentContext
#define eglGetCurrentDisplay epoxy_eglGetCurrentDisplay
#define eglGetCurrentSurface epoxy_eglGetCurrentSurface
#define eglGetDisplay epoxy_eglGetDisplay
#define eglGetError epoxy_eglGetError
#define eglGetOutputLayersEXT epoxy_eglGetOutputLayersEXT
#define eglGetOutputPortsEXT epoxy_eglGetOutputPortsEXT
#define eglGetPlatformDisplay epoxy_eglGetPlatformDisplay
#define eglGetPlatformDisplayEXT epoxy_eglGetPlatformDisplayEXT
#define eglGetProcAddress epoxy_eglGetProcAddress
#define eglGetStreamFileDescriptorKHR epoxy_eglGetStreamFileDescriptorKHR
#define eglGetSyncAttrib epoxy_eglGetSyncAttrib
#define eglGetSyncAttribKHR epoxy_eglGetSyncAttribKHR
#define eglGetSyncAttribNV epoxy_eglGetSyncAttribNV
#define eglGetSystemTimeFrequencyNV epoxy_eglGetSystemTimeFrequencyNV
#define eglGetSystemTimeNV epoxy_eglGetSystemTimeNV
#define eglInitialize epoxy_eglInitialize
#define eglLockSurfaceKHR epoxy_eglLockSurfaceKHR
#define eglMakeCurrent epoxy_eglMakeCurrent
#define eglOutputLayerAttribEXT epoxy_eglOutputLayerAttribEXT
#define eglOutputPortAttribEXT epoxy_eglOutputPortAttribEXT
#define eglPostSubBufferNV epoxy_eglPostSubBufferNV
#define eglQueryAPI epoxy_eglQueryAPI
#define eglQueryContext epoxy_eglQueryContext
#define eglQueryDeviceAttribEXT epoxy_eglQueryDeviceAttribEXT
#define eglQueryDeviceStringEXT epoxy_eglQueryDeviceStringEXT
#define eglQueryDevicesEXT epoxy_eglQueryDevicesEXT
#define eglQueryDisplayAttribEXT epoxy_eglQueryDisplayAttribEXT
#define eglQueryNativeDisplayNV epoxy_eglQueryNativeDisplayNV
#define eglQueryNativePixmapNV epoxy_eglQueryNativePixmapNV
#define eglQueryNativeWindowNV epoxy_eglQueryNativeWindowNV
#define eglQueryOutputLayerAttribEXT epoxy_eglQueryOutputLayerAttribEXT
#define eglQueryOutputLayerStringEXT epoxy_eglQueryOutputLayerStringEXT
#define eglQueryOutputPortAttribEXT epoxy_eglQueryOutputPortAttribEXT
#define eglQueryOutputPortStringEXT epoxy_eglQueryOutputPortStringEXT
#define eglQueryStreamKHR epoxy_eglQueryStreamKHR
#define eglQueryStreamTimeKHR epoxy_eglQueryStreamTimeKHR
#define eglQueryStreamu64KHR epoxy_eglQueryStreamu64KHR
#define eglQueryString epoxy_eglQueryString
#define eglQuerySurface epoxy_eglQuerySurface
#define eglQuerySurface64KHR epoxy_eglQuerySurface64KHR
#define eglQuerySurfacePointerANGLE epoxy_eglQuerySurfacePointerANGLE
#define eglReleaseTexImage epoxy_eglReleaseTexImage
#define eglReleaseThread epoxy_eglReleaseThread
#define eglSetBlobCacheFuncsANDROID epoxy_eglSetBlobCacheFuncsANDROID
#define eglSetDamageRegionKHR epoxy_eglSetDamageRegionKHR
#define eglSignalSyncKHR epoxy_eglSignalSyncKHR
#define eglSignalSyncNV epoxy_eglSignalSyncNV
#define eglStreamAttribKHR epoxy_eglStreamAttribKHR
#define eglStreamConsumerAcquireKHR epoxy_eglStreamConsumerAcquireKHR
#define eglStreamConsumerGLTextureExternalKHR epoxy_eglStreamConsumerGLTextureExternalKHR
#define eglStreamConsumerOutputEXT epoxy_eglStreamConsumerOutputEXT
#define eglStreamConsumerReleaseKHR epoxy_eglStreamConsumerReleaseKHR
#define eglSurfaceAttrib epoxy_eglSurfaceAttrib
#define eglSwapBuffers epoxy_eglSwapBuffers
#define eglSwapBuffersRegion2NOK epoxy_eglSwapBuffersRegion2NOK
#define eglSwapBuffersRegionNOK epoxy_eglSwapBuffersRegionNOK
#define eglSwapBuffersWithDamageEXT epoxy_eglSwapBuffersWithDamageEXT
#define eglSwapBuffersWithDamageKHR epoxy_eglSwapBuffersWithDamageKHR
#define eglSwapInterval epoxy_eglSwapInterval
#define eglTerminate epoxy_eglTerminate
#define eglUnlockSurfaceKHR epoxy_eglUnlockSurfaceKHR
#define eglWaitClient epoxy_eglWaitClient
#define eglWaitGL epoxy_eglWaitGL
#define eglWaitNative epoxy_eglWaitNative
#define eglWaitSync epoxy_eglWaitSync
#define eglWaitSyncKHR epoxy_eglWaitSyncKHR
