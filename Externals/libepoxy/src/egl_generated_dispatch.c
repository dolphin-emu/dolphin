/* GL dispatch code.
 * This is code-generated from the GL API XML files from Khronos.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "dispatch_common.h"
#include "epoxy/egl.h"

struct dispatch_table {
    PFNEGLBINDAPIPROC epoxy_eglBindAPI;
    PFNEGLBINDTEXIMAGEPROC epoxy_eglBindTexImage;
    PFNEGLCHOOSECONFIGPROC epoxy_eglChooseConfig;
    PFNEGLCLIENTWAITSYNCPROC epoxy_eglClientWaitSync;
    PFNEGLCLIENTWAITSYNCKHRPROC epoxy_eglClientWaitSyncKHR;
    PFNEGLCLIENTWAITSYNCNVPROC epoxy_eglClientWaitSyncNV;
    PFNEGLCOPYBUFFERSPROC epoxy_eglCopyBuffers;
    PFNEGLCREATECONTEXTPROC epoxy_eglCreateContext;
    PFNEGLCREATEDRMIMAGEMESAPROC epoxy_eglCreateDRMImageMESA;
    PFNEGLCREATEFENCESYNCNVPROC epoxy_eglCreateFenceSyncNV;
    PFNEGLCREATEIMAGEKHRPROC epoxy_eglCreateImageKHR;
    PFNEGLCREATEPBUFFERFROMCLIENTBUFFERPROC epoxy_eglCreatePbufferFromClientBuffer;
    PFNEGLCREATEPBUFFERSURFACEPROC epoxy_eglCreatePbufferSurface;
    PFNEGLCREATEPIXMAPSURFACEPROC epoxy_eglCreatePixmapSurface;
    PFNEGLCREATEPIXMAPSURFACEHIPROC epoxy_eglCreatePixmapSurfaceHI;
    PFNEGLCREATEPLATFORMPIXMAPSURFACEPROC epoxy_eglCreatePlatformPixmapSurface;
    PFNEGLCREATEPLATFORMPIXMAPSURFACEEXTPROC epoxy_eglCreatePlatformPixmapSurfaceEXT;
    PFNEGLCREATEPLATFORMWINDOWSURFACEPROC epoxy_eglCreatePlatformWindowSurface;
    PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC epoxy_eglCreatePlatformWindowSurfaceEXT;
    PFNEGLCREATESTREAMFROMFILEDESCRIPTORKHRPROC epoxy_eglCreateStreamFromFileDescriptorKHR;
    PFNEGLCREATESTREAMKHRPROC epoxy_eglCreateStreamKHR;
    PFNEGLCREATESTREAMPRODUCERSURFACEKHRPROC epoxy_eglCreateStreamProducerSurfaceKHR;
    PFNEGLCREATESTREAMSYNCNVPROC epoxy_eglCreateStreamSyncNV;
    PFNEGLCREATESYNCPROC epoxy_eglCreateSync;
    PFNEGLCREATESYNC64KHRPROC epoxy_eglCreateSync64KHR;
    PFNEGLCREATESYNCKHRPROC epoxy_eglCreateSyncKHR;
    PFNEGLCREATEWINDOWSURFACEPROC epoxy_eglCreateWindowSurface;
    PFNEGLDESTROYCONTEXTPROC epoxy_eglDestroyContext;
    PFNEGLDESTROYIMAGEKHRPROC epoxy_eglDestroyImageKHR;
    PFNEGLDESTROYSTREAMKHRPROC epoxy_eglDestroyStreamKHR;
    PFNEGLDESTROYSURFACEPROC epoxy_eglDestroySurface;
    PFNEGLDESTROYSYNCPROC epoxy_eglDestroySync;
    PFNEGLDESTROYSYNCKHRPROC epoxy_eglDestroySyncKHR;
    PFNEGLDESTROYSYNCNVPROC epoxy_eglDestroySyncNV;
    PFNEGLDUPNATIVEFENCEFDANDROIDPROC epoxy_eglDupNativeFenceFDANDROID;
    PFNEGLEXPORTDRMIMAGEMESAPROC epoxy_eglExportDRMImageMESA;
    PFNEGLFENCENVPROC epoxy_eglFenceNV;
    PFNEGLGETCONFIGATTRIBPROC epoxy_eglGetConfigAttrib;
    PFNEGLGETCONFIGSPROC epoxy_eglGetConfigs;
    PFNEGLGETCURRENTCONTEXTPROC epoxy_eglGetCurrentContext;
    PFNEGLGETCURRENTDISPLAYPROC epoxy_eglGetCurrentDisplay;
    PFNEGLGETCURRENTSURFACEPROC epoxy_eglGetCurrentSurface;
    PFNEGLGETDISPLAYPROC epoxy_eglGetDisplay;
    PFNEGLGETERRORPROC epoxy_eglGetError;
    PFNEGLGETPLATFORMDISPLAYPROC epoxy_eglGetPlatformDisplay;
    PFNEGLGETPLATFORMDISPLAYEXTPROC epoxy_eglGetPlatformDisplayEXT;
    PFNEGLGETPROCADDRESSPROC epoxy_eglGetProcAddress;
    PFNEGLGETSTREAMFILEDESCRIPTORKHRPROC epoxy_eglGetStreamFileDescriptorKHR;
    PFNEGLGETSYNCATTRIBPROC epoxy_eglGetSyncAttrib;
    PFNEGLGETSYNCATTRIBKHRPROC epoxy_eglGetSyncAttribKHR;
    PFNEGLGETSYNCATTRIBNVPROC epoxy_eglGetSyncAttribNV;
    PFNEGLGETSYSTEMTIMEFREQUENCYNVPROC epoxy_eglGetSystemTimeFrequencyNV;
    PFNEGLGETSYSTEMTIMENVPROC epoxy_eglGetSystemTimeNV;
    PFNEGLINITIALIZEPROC epoxy_eglInitialize;
    PFNEGLLOCKSURFACEKHRPROC epoxy_eglLockSurfaceKHR;
    PFNEGLMAKECURRENTPROC epoxy_eglMakeCurrent;
    PFNEGLPOSTSUBBUFFERNVPROC epoxy_eglPostSubBufferNV;
    PFNEGLQUERYAPIPROC epoxy_eglQueryAPI;
    PFNEGLQUERYCONTEXTPROC epoxy_eglQueryContext;
    PFNEGLQUERYNATIVEDISPLAYNVPROC epoxy_eglQueryNativeDisplayNV;
    PFNEGLQUERYNATIVEPIXMAPNVPROC epoxy_eglQueryNativePixmapNV;
    PFNEGLQUERYNATIVEWINDOWNVPROC epoxy_eglQueryNativeWindowNV;
    PFNEGLQUERYSTREAMKHRPROC epoxy_eglQueryStreamKHR;
    PFNEGLQUERYSTREAMTIMEKHRPROC epoxy_eglQueryStreamTimeKHR;
    PFNEGLQUERYSTREAMU64KHRPROC epoxy_eglQueryStreamu64KHR;
    PFNEGLQUERYSTRINGPROC epoxy_eglQueryString;
    PFNEGLQUERYSURFACEPROC epoxy_eglQuerySurface;
    PFNEGLQUERYSURFACE64KHRPROC epoxy_eglQuerySurface64KHR;
    PFNEGLQUERYSURFACEPOINTERANGLEPROC epoxy_eglQuerySurfacePointerANGLE;
    PFNEGLRELEASETEXIMAGEPROC epoxy_eglReleaseTexImage;
    PFNEGLRELEASETHREADPROC epoxy_eglReleaseThread;
    PFNEGLSETBLOBCACHEFUNCSANDROIDPROC epoxy_eglSetBlobCacheFuncsANDROID;
    PFNEGLSIGNALSYNCKHRPROC epoxy_eglSignalSyncKHR;
    PFNEGLSIGNALSYNCNVPROC epoxy_eglSignalSyncNV;
    PFNEGLSTREAMATTRIBKHRPROC epoxy_eglStreamAttribKHR;
    PFNEGLSTREAMCONSUMERACQUIREKHRPROC epoxy_eglStreamConsumerAcquireKHR;
    PFNEGLSTREAMCONSUMERGLTEXTUREEXTERNALKHRPROC epoxy_eglStreamConsumerGLTextureExternalKHR;
    PFNEGLSTREAMCONSUMERRELEASEKHRPROC epoxy_eglStreamConsumerReleaseKHR;
    PFNEGLSURFACEATTRIBPROC epoxy_eglSurfaceAttrib;
    PFNEGLSWAPBUFFERSPROC epoxy_eglSwapBuffers;
    PFNEGLSWAPBUFFERSREGION2NOKPROC epoxy_eglSwapBuffersRegion2NOK;
    PFNEGLSWAPBUFFERSREGIONNOKPROC epoxy_eglSwapBuffersRegionNOK;
    PFNEGLSWAPBUFFERSWITHDAMAGEEXTPROC epoxy_eglSwapBuffersWithDamageEXT;
    PFNEGLSWAPINTERVALPROC epoxy_eglSwapInterval;
    PFNEGLTERMINATEPROC epoxy_eglTerminate;
    PFNEGLUNLOCKSURFACEKHRPROC epoxy_eglUnlockSurfaceKHR;
    PFNEGLWAITCLIENTPROC epoxy_eglWaitClient;
    PFNEGLWAITGLPROC epoxy_eglWaitGL;
    PFNEGLWAITNATIVEPROC epoxy_eglWaitNative;
    PFNEGLWAITSYNCPROC epoxy_eglWaitSync;
    PFNEGLWAITSYNCKHRPROC epoxy_eglWaitSyncKHR;
};

#if USING_DISPATCH_TABLE
static inline struct dispatch_table *
get_dispatch_table(void);

#endif
enum egl_provider {
    egl_provider_terminator = 0,
    EGL_10,
    EGL_11,
    EGL_12,
    EGL_14,
    EGL_15,
    EGL_extension_EGL_ANDROID_blob_cache,
    EGL_extension_EGL_ANDROID_native_fence_sync,
    EGL_extension_EGL_ANGLE_query_surface_pointer,
    EGL_extension_EGL_EXT_platform_base,
    EGL_extension_EGL_EXT_swap_buffers_with_damage,
    EGL_extension_EGL_HI_clientpixmap,
    EGL_extension_EGL_KHR_cl_event2,
    EGL_extension_EGL_KHR_image,
    EGL_extension_EGL_KHR_image_base,
    EGL_extension_EGL_KHR_lock_surface3,
    EGL_extension_EGL_KHR_lock_surface,
    EGL_extension_EGL_KHR_reusable_sync,
    EGL_extension_EGL_KHR_stream,
    EGL_extension_EGL_KHR_stream_consumer_gltexture,
    EGL_extension_EGL_KHR_stream_cross_process_fd,
    EGL_extension_EGL_KHR_stream_fifo,
    EGL_extension_EGL_KHR_stream_producer_eglsurface,
    EGL_extension_EGL_KHR_wait_sync,
    EGL_extension_EGL_MESA_drm_image,
    EGL_extension_EGL_NOK_swap_region2,
    EGL_extension_EGL_NOK_swap_region,
    EGL_extension_EGL_NV_native_query,
    EGL_extension_EGL_NV_post_sub_buffer,
    EGL_extension_EGL_NV_stream_sync,
    EGL_extension_EGL_NV_sync,
    EGL_extension_EGL_NV_system_time,
};

static const char *enum_strings[] = {
    [EGL_10] = "EGL 10",
    [EGL_11] = "EGL 11",
    [EGL_12] = "EGL 12",
    [EGL_14] = "EGL 14",
    [EGL_15] = "EGL 15",
    [EGL_extension_EGL_ANDROID_blob_cache] = "EGL extension \"EGL_ANDROID_blob_cache\"",
    [EGL_extension_EGL_ANDROID_native_fence_sync] = "EGL extension \"EGL_ANDROID_native_fence_sync\"",
    [EGL_extension_EGL_ANGLE_query_surface_pointer] = "EGL extension \"EGL_ANGLE_query_surface_pointer\"",
    [EGL_extension_EGL_EXT_platform_base] = "EGL extension \"EGL_EXT_platform_base\"",
    [EGL_extension_EGL_EXT_swap_buffers_with_damage] = "EGL extension \"EGL_EXT_swap_buffers_with_damage\"",
    [EGL_extension_EGL_HI_clientpixmap] = "EGL extension \"EGL_HI_clientpixmap\"",
    [EGL_extension_EGL_KHR_cl_event2] = "EGL extension \"EGL_KHR_cl_event2\"",
    [EGL_extension_EGL_KHR_image] = "EGL extension \"EGL_KHR_image\"",
    [EGL_extension_EGL_KHR_image_base] = "EGL extension \"EGL_KHR_image_base\"",
    [EGL_extension_EGL_KHR_lock_surface3] = "EGL extension \"EGL_KHR_lock_surface3\"",
    [EGL_extension_EGL_KHR_lock_surface] = "EGL extension \"EGL_KHR_lock_surface\"",
    [EGL_extension_EGL_KHR_reusable_sync] = "EGL extension \"EGL_KHR_reusable_sync\"",
    [EGL_extension_EGL_KHR_stream] = "EGL extension \"EGL_KHR_stream\"",
    [EGL_extension_EGL_KHR_stream_consumer_gltexture] = "EGL extension \"EGL_KHR_stream_consumer_gltexture\"",
    [EGL_extension_EGL_KHR_stream_cross_process_fd] = "EGL extension \"EGL_KHR_stream_cross_process_fd\"",
    [EGL_extension_EGL_KHR_stream_fifo] = "EGL extension \"EGL_KHR_stream_fifo\"",
    [EGL_extension_EGL_KHR_stream_producer_eglsurface] = "EGL extension \"EGL_KHR_stream_producer_eglsurface\"",
    [EGL_extension_EGL_KHR_wait_sync] = "EGL extension \"EGL_KHR_wait_sync\"",
    [EGL_extension_EGL_MESA_drm_image] = "EGL extension \"EGL_MESA_drm_image\"",
    [EGL_extension_EGL_NOK_swap_region2] = "EGL extension \"EGL_NOK_swap_region2\"",
    [EGL_extension_EGL_NOK_swap_region] = "EGL extension \"EGL_NOK_swap_region\"",
    [EGL_extension_EGL_NV_native_query] = "EGL extension \"EGL_NV_native_query\"",
    [EGL_extension_EGL_NV_post_sub_buffer] = "EGL extension \"EGL_NV_post_sub_buffer\"",
    [EGL_extension_EGL_NV_stream_sync] = "EGL extension \"EGL_NV_stream_sync\"",
    [EGL_extension_EGL_NV_sync] = "EGL extension \"EGL_NV_sync\"",
    [EGL_extension_EGL_NV_system_time] = "EGL extension \"EGL_NV_system_time\"",
};

static const char entrypoint_strings[] = 
   "eglBindAPI\0"
   "eglBindTexImage\0"
   "eglChooseConfig\0"
   "eglClientWaitSync\0"
   "eglClientWaitSyncKHR\0"
   "eglClientWaitSyncNV\0"
   "eglCopyBuffers\0"
   "eglCreateContext\0"
   "eglCreateDRMImageMESA\0"
   "eglCreateFenceSyncNV\0"
   "eglCreateImageKHR\0"
   "eglCreatePbufferFromClientBuffer\0"
   "eglCreatePbufferSurface\0"
   "eglCreatePixmapSurface\0"
   "eglCreatePixmapSurfaceHI\0"
   "eglCreatePlatformPixmapSurface\0"
   "eglCreatePlatformPixmapSurfaceEXT\0"
   "eglCreatePlatformWindowSurface\0"
   "eglCreatePlatformWindowSurfaceEXT\0"
   "eglCreateStreamFromFileDescriptorKHR\0"
   "eglCreateStreamKHR\0"
   "eglCreateStreamProducerSurfaceKHR\0"
   "eglCreateStreamSyncNV\0"
   "eglCreateSync\0"
   "eglCreateSync64KHR\0"
   "eglCreateSyncKHR\0"
   "eglCreateWindowSurface\0"
   "eglDestroyContext\0"
   "eglDestroyImageKHR\0"
   "eglDestroyStreamKHR\0"
   "eglDestroySurface\0"
   "eglDestroySync\0"
   "eglDestroySyncKHR\0"
   "eglDestroySyncNV\0"
   "eglDupNativeFenceFDANDROID\0"
   "eglExportDRMImageMESA\0"
   "eglFenceNV\0"
   "eglGetConfigAttrib\0"
   "eglGetConfigs\0"
   "eglGetCurrentContext\0"
   "eglGetCurrentDisplay\0"
   "eglGetCurrentSurface\0"
   "eglGetDisplay\0"
   "eglGetError\0"
   "eglGetPlatformDisplay\0"
   "eglGetPlatformDisplayEXT\0"
   "eglGetProcAddress\0"
   "eglGetStreamFileDescriptorKHR\0"
   "eglGetSyncAttrib\0"
   "eglGetSyncAttribKHR\0"
   "eglGetSyncAttribNV\0"
   "eglGetSystemTimeFrequencyNV\0"
   "eglGetSystemTimeNV\0"
   "eglInitialize\0"
   "eglLockSurfaceKHR\0"
   "eglMakeCurrent\0"
   "eglPostSubBufferNV\0"
   "eglQueryAPI\0"
   "eglQueryContext\0"
   "eglQueryNativeDisplayNV\0"
   "eglQueryNativePixmapNV\0"
   "eglQueryNativeWindowNV\0"
   "eglQueryStreamKHR\0"
   "eglQueryStreamTimeKHR\0"
   "eglQueryStreamu64KHR\0"
   "eglQueryString\0"
   "eglQuerySurface\0"
   "eglQuerySurface64KHR\0"
   "eglQuerySurfacePointerANGLE\0"
   "eglReleaseTexImage\0"
   "eglReleaseThread\0"
   "eglSetBlobCacheFuncsANDROID\0"
   "eglSignalSyncKHR\0"
   "eglSignalSyncNV\0"
   "eglStreamAttribKHR\0"
   "eglStreamConsumerAcquireKHR\0"
   "eglStreamConsumerGLTextureExternalKHR\0"
   "eglStreamConsumerReleaseKHR\0"
   "eglSurfaceAttrib\0"
   "eglSwapBuffers\0"
   "eglSwapBuffersRegion2NOK\0"
   "eglSwapBuffersRegionNOK\0"
   "eglSwapBuffersWithDamageEXT\0"
   "eglSwapInterval\0"
   "eglTerminate\0"
   "eglUnlockSurfaceKHR\0"
   "eglWaitClient\0"
   "eglWaitGL\0"
   "eglWaitNative\0"
   "eglWaitSync\0"
   "eglWaitSyncKHR\0"
    ;

static void *egl_provider_resolver(const char *name,
                                   const enum egl_provider *providers,
                                   const uint16_t *entrypoints)
{
    int i;
    for (i = 0; providers[i] != egl_provider_terminator; i++) {
        switch (providers[i]) {
        case EGL_10:
            if (true)
                return epoxy_egl_dlsym(entrypoint_strings + entrypoints[i]);
            break;
        case EGL_11:
            if (epoxy_conservative_egl_version() >= 11)
                return epoxy_egl_dlsym(entrypoint_strings + entrypoints[i]);
            break;
        case EGL_12:
            if (epoxy_conservative_egl_version() >= 12)
                return epoxy_egl_dlsym(entrypoint_strings + entrypoints[i]);
            break;
        case EGL_14:
            if (epoxy_conservative_egl_version() >= 14)
                return epoxy_egl_dlsym(entrypoint_strings + entrypoints[i]);
            break;
        case EGL_15:
            if (epoxy_conservative_egl_version() >= 15)
                return epoxy_egl_dlsym(entrypoint_strings + entrypoints[i]);
            break;
        case EGL_extension_EGL_ANDROID_blob_cache:
            if (epoxy_conservative_has_egl_extension("EGL_ANDROID_blob_cache"))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case EGL_extension_EGL_ANDROID_native_fence_sync:
            if (epoxy_conservative_has_egl_extension("EGL_ANDROID_native_fence_sync"))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case EGL_extension_EGL_ANGLE_query_surface_pointer:
            if (epoxy_conservative_has_egl_extension("EGL_ANGLE_query_surface_pointer"))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case EGL_extension_EGL_EXT_platform_base:
            if (epoxy_conservative_has_egl_extension("EGL_EXT_platform_base"))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case EGL_extension_EGL_EXT_swap_buffers_with_damage:
            if (epoxy_conservative_has_egl_extension("EGL_EXT_swap_buffers_with_damage"))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case EGL_extension_EGL_HI_clientpixmap:
            if (epoxy_conservative_has_egl_extension("EGL_HI_clientpixmap"))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case EGL_extension_EGL_KHR_cl_event2:
            if (epoxy_conservative_has_egl_extension("EGL_KHR_cl_event2"))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case EGL_extension_EGL_KHR_image:
            if (epoxy_conservative_has_egl_extension("EGL_KHR_image"))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case EGL_extension_EGL_KHR_image_base:
            if (epoxy_conservative_has_egl_extension("EGL_KHR_image_base"))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case EGL_extension_EGL_KHR_lock_surface3:
            if (epoxy_conservative_has_egl_extension("EGL_KHR_lock_surface3"))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case EGL_extension_EGL_KHR_lock_surface:
            if (epoxy_conservative_has_egl_extension("EGL_KHR_lock_surface"))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case EGL_extension_EGL_KHR_reusable_sync:
            if (epoxy_conservative_has_egl_extension("EGL_KHR_reusable_sync"))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case EGL_extension_EGL_KHR_stream:
            if (epoxy_conservative_has_egl_extension("EGL_KHR_stream"))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case EGL_extension_EGL_KHR_stream_consumer_gltexture:
            if (epoxy_conservative_has_egl_extension("EGL_KHR_stream_consumer_gltexture"))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case EGL_extension_EGL_KHR_stream_cross_process_fd:
            if (epoxy_conservative_has_egl_extension("EGL_KHR_stream_cross_process_fd"))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case EGL_extension_EGL_KHR_stream_fifo:
            if (epoxy_conservative_has_egl_extension("EGL_KHR_stream_fifo"))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case EGL_extension_EGL_KHR_stream_producer_eglsurface:
            if (epoxy_conservative_has_egl_extension("EGL_KHR_stream_producer_eglsurface"))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case EGL_extension_EGL_KHR_wait_sync:
            if (epoxy_conservative_has_egl_extension("EGL_KHR_wait_sync"))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case EGL_extension_EGL_MESA_drm_image:
            if (epoxy_conservative_has_egl_extension("EGL_MESA_drm_image"))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case EGL_extension_EGL_NOK_swap_region2:
            if (epoxy_conservative_has_egl_extension("EGL_NOK_swap_region2"))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case EGL_extension_EGL_NOK_swap_region:
            if (epoxy_conservative_has_egl_extension("EGL_NOK_swap_region"))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case EGL_extension_EGL_NV_native_query:
            if (epoxy_conservative_has_egl_extension("EGL_NV_native_query"))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case EGL_extension_EGL_NV_post_sub_buffer:
            if (epoxy_conservative_has_egl_extension("EGL_NV_post_sub_buffer"))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case EGL_extension_EGL_NV_stream_sync:
            if (epoxy_conservative_has_egl_extension("EGL_NV_stream_sync"))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case EGL_extension_EGL_NV_sync:
            if (epoxy_conservative_has_egl_extension("EGL_NV_sync"))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case EGL_extension_EGL_NV_system_time:
            if (epoxy_conservative_has_egl_extension("EGL_NV_system_time"))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case egl_provider_terminator:
            abort(); /* Not reached */
        }
    }

    epoxy_print_failure_reasons(name, enum_strings, (const int *)providers);
    abort();
}

static void *
egl_single_resolver(enum egl_provider provider, uint16_t entrypoint_offset) __attribute__((noinline));

static void *
egl_single_resolver(enum egl_provider provider, uint16_t entrypoint_offset)
{
    enum egl_provider providers[] = {
        provider,
        egl_provider_terminator
    };
    return egl_provider_resolver(entrypoint_strings + entrypoint_offset,
                                providers, &entrypoint_offset);
}

static PFNEGLBINDAPIPROC
epoxy_eglBindAPI_resolver(void)
{
    return egl_single_resolver(EGL_12, 0 /* eglBindAPI */);
}

static PFNEGLBINDTEXIMAGEPROC
epoxy_eglBindTexImage_resolver(void)
{
    return egl_single_resolver(EGL_11, 11 /* eglBindTexImage */);
}

static PFNEGLCHOOSECONFIGPROC
epoxy_eglChooseConfig_resolver(void)
{
    return egl_single_resolver(EGL_10, 27 /* eglChooseConfig */);
}

static PFNEGLCLIENTWAITSYNCPROC
epoxy_eglClientWaitSync_resolver(void)
{
    static const enum egl_provider providers[] = {
        EGL_15,
        EGL_extension_EGL_KHR_reusable_sync,
        egl_provider_terminator
    };
    static const uint16_t entrypoints[] = {
        43 /* "eglClientWaitSync" */,
        61 /* "eglClientWaitSyncKHR" */,
    };
    return egl_provider_resolver(entrypoint_strings + 43 /* "eglClientWaitSync" */,
                                providers, entrypoints);
}

static PFNEGLCLIENTWAITSYNCKHRPROC
epoxy_eglClientWaitSyncKHR_resolver(void)
{
    static const enum egl_provider providers[] = {
        EGL_extension_EGL_KHR_reusable_sync,
        EGL_15,
        egl_provider_terminator
    };
    static const uint16_t entrypoints[] = {
        61 /* "eglClientWaitSyncKHR" */,
        43 /* "eglClientWaitSync" */,
    };
    return egl_provider_resolver(entrypoint_strings + 61 /* "eglClientWaitSyncKHR" */,
                                providers, entrypoints);
}

static PFNEGLCLIENTWAITSYNCNVPROC
epoxy_eglClientWaitSyncNV_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_NV_sync, 82 /* eglClientWaitSyncNV */);
}

static PFNEGLCOPYBUFFERSPROC
epoxy_eglCopyBuffers_resolver(void)
{
    return egl_single_resolver(EGL_10, 102 /* eglCopyBuffers */);
}

static PFNEGLCREATECONTEXTPROC
epoxy_eglCreateContext_resolver(void)
{
    return egl_single_resolver(EGL_10, 117 /* eglCreateContext */);
}

static PFNEGLCREATEDRMIMAGEMESAPROC
epoxy_eglCreateDRMImageMESA_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_MESA_drm_image, 134 /* eglCreateDRMImageMESA */);
}

static PFNEGLCREATEFENCESYNCNVPROC
epoxy_eglCreateFenceSyncNV_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_NV_sync, 156 /* eglCreateFenceSyncNV */);
}

static PFNEGLCREATEIMAGEKHRPROC
epoxy_eglCreateImageKHR_resolver(void)
{
    static const enum egl_provider providers[] = {
        EGL_extension_EGL_KHR_image_base,
        EGL_extension_EGL_KHR_image,
        egl_provider_terminator
    };
    static const uint16_t entrypoints[] = {
        177 /* "eglCreateImageKHR" */,
        177 /* "eglCreateImageKHR" */,
    };
    return egl_provider_resolver(entrypoint_strings + 177 /* "eglCreateImageKHR" */,
                                providers, entrypoints);
}

static PFNEGLCREATEPBUFFERFROMCLIENTBUFFERPROC
epoxy_eglCreatePbufferFromClientBuffer_resolver(void)
{
    return egl_single_resolver(EGL_12, 195 /* eglCreatePbufferFromClientBuffer */);
}

static PFNEGLCREATEPBUFFERSURFACEPROC
epoxy_eglCreatePbufferSurface_resolver(void)
{
    return egl_single_resolver(EGL_10, 228 /* eglCreatePbufferSurface */);
}

static PFNEGLCREATEPIXMAPSURFACEPROC
epoxy_eglCreatePixmapSurface_resolver(void)
{
    return egl_single_resolver(EGL_10, 252 /* eglCreatePixmapSurface */);
}

static PFNEGLCREATEPIXMAPSURFACEHIPROC
epoxy_eglCreatePixmapSurfaceHI_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_HI_clientpixmap, 275 /* eglCreatePixmapSurfaceHI */);
}

static PFNEGLCREATEPLATFORMPIXMAPSURFACEPROC
epoxy_eglCreatePlatformPixmapSurface_resolver(void)
{
    return egl_single_resolver(EGL_15, 300 /* eglCreatePlatformPixmapSurface */);
}

static PFNEGLCREATEPLATFORMPIXMAPSURFACEEXTPROC
epoxy_eglCreatePlatformPixmapSurfaceEXT_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_EXT_platform_base, 331 /* eglCreatePlatformPixmapSurfaceEXT */);
}

static PFNEGLCREATEPLATFORMWINDOWSURFACEPROC
epoxy_eglCreatePlatformWindowSurface_resolver(void)
{
    return egl_single_resolver(EGL_15, 365 /* eglCreatePlatformWindowSurface */);
}

static PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC
epoxy_eglCreatePlatformWindowSurfaceEXT_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_EXT_platform_base, 396 /* eglCreatePlatformWindowSurfaceEXT */);
}

static PFNEGLCREATESTREAMFROMFILEDESCRIPTORKHRPROC
epoxy_eglCreateStreamFromFileDescriptorKHR_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_KHR_stream_cross_process_fd, 430 /* eglCreateStreamFromFileDescriptorKHR */);
}

static PFNEGLCREATESTREAMKHRPROC
epoxy_eglCreateStreamKHR_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_KHR_stream, 467 /* eglCreateStreamKHR */);
}

static PFNEGLCREATESTREAMPRODUCERSURFACEKHRPROC
epoxy_eglCreateStreamProducerSurfaceKHR_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_KHR_stream_producer_eglsurface, 486 /* eglCreateStreamProducerSurfaceKHR */);
}

static PFNEGLCREATESTREAMSYNCNVPROC
epoxy_eglCreateStreamSyncNV_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_NV_stream_sync, 520 /* eglCreateStreamSyncNV */);
}

static PFNEGLCREATESYNCPROC
epoxy_eglCreateSync_resolver(void)
{
    static const enum egl_provider providers[] = {
        EGL_15,
        EGL_extension_EGL_KHR_cl_event2,
        egl_provider_terminator
    };
    static const uint16_t entrypoints[] = {
        542 /* "eglCreateSync" */,
        556 /* "eglCreateSync64KHR" */,
    };
    return egl_provider_resolver(entrypoint_strings + 542 /* "eglCreateSync" */,
                                providers, entrypoints);
}

static PFNEGLCREATESYNC64KHRPROC
epoxy_eglCreateSync64KHR_resolver(void)
{
    static const enum egl_provider providers[] = {
        EGL_extension_EGL_KHR_cl_event2,
        EGL_15,
        egl_provider_terminator
    };
    static const uint16_t entrypoints[] = {
        556 /* "eglCreateSync64KHR" */,
        542 /* "eglCreateSync" */,
    };
    return egl_provider_resolver(entrypoint_strings + 556 /* "eglCreateSync64KHR" */,
                                providers, entrypoints);
}

static PFNEGLCREATESYNCKHRPROC
epoxy_eglCreateSyncKHR_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_KHR_reusable_sync, 575 /* eglCreateSyncKHR */);
}

static PFNEGLCREATEWINDOWSURFACEPROC
epoxy_eglCreateWindowSurface_resolver(void)
{
    return egl_single_resolver(EGL_10, 592 /* eglCreateWindowSurface */);
}

static PFNEGLDESTROYCONTEXTPROC
epoxy_eglDestroyContext_resolver(void)
{
    return egl_single_resolver(EGL_10, 615 /* eglDestroyContext */);
}

static PFNEGLDESTROYIMAGEKHRPROC
epoxy_eglDestroyImageKHR_resolver(void)
{
    static const enum egl_provider providers[] = {
        EGL_extension_EGL_KHR_image_base,
        EGL_extension_EGL_KHR_image,
        egl_provider_terminator
    };
    static const uint16_t entrypoints[] = {
        633 /* "eglDestroyImageKHR" */,
        633 /* "eglDestroyImageKHR" */,
    };
    return egl_provider_resolver(entrypoint_strings + 633 /* "eglDestroyImageKHR" */,
                                providers, entrypoints);
}

static PFNEGLDESTROYSTREAMKHRPROC
epoxy_eglDestroyStreamKHR_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_KHR_stream, 652 /* eglDestroyStreamKHR */);
}

static PFNEGLDESTROYSURFACEPROC
epoxy_eglDestroySurface_resolver(void)
{
    return egl_single_resolver(EGL_10, 672 /* eglDestroySurface */);
}

static PFNEGLDESTROYSYNCPROC
epoxy_eglDestroySync_resolver(void)
{
    static const enum egl_provider providers[] = {
        EGL_15,
        EGL_extension_EGL_KHR_reusable_sync,
        egl_provider_terminator
    };
    static const uint16_t entrypoints[] = {
        690 /* "eglDestroySync" */,
        705 /* "eglDestroySyncKHR" */,
    };
    return egl_provider_resolver(entrypoint_strings + 690 /* "eglDestroySync" */,
                                providers, entrypoints);
}

static PFNEGLDESTROYSYNCKHRPROC
epoxy_eglDestroySyncKHR_resolver(void)
{
    static const enum egl_provider providers[] = {
        EGL_extension_EGL_KHR_reusable_sync,
        EGL_15,
        egl_provider_terminator
    };
    static const uint16_t entrypoints[] = {
        705 /* "eglDestroySyncKHR" */,
        690 /* "eglDestroySync" */,
    };
    return egl_provider_resolver(entrypoint_strings + 705 /* "eglDestroySyncKHR" */,
                                providers, entrypoints);
}

static PFNEGLDESTROYSYNCNVPROC
epoxy_eglDestroySyncNV_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_NV_sync, 723 /* eglDestroySyncNV */);
}

static PFNEGLDUPNATIVEFENCEFDANDROIDPROC
epoxy_eglDupNativeFenceFDANDROID_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_ANDROID_native_fence_sync, 740 /* eglDupNativeFenceFDANDROID */);
}

static PFNEGLEXPORTDRMIMAGEMESAPROC
epoxy_eglExportDRMImageMESA_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_MESA_drm_image, 767 /* eglExportDRMImageMESA */);
}

static PFNEGLFENCENVPROC
epoxy_eglFenceNV_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_NV_sync, 789 /* eglFenceNV */);
}

static PFNEGLGETCONFIGATTRIBPROC
epoxy_eglGetConfigAttrib_resolver(void)
{
    return egl_single_resolver(EGL_10, 800 /* eglGetConfigAttrib */);
}

static PFNEGLGETCONFIGSPROC
epoxy_eglGetConfigs_resolver(void)
{
    return egl_single_resolver(EGL_10, 819 /* eglGetConfigs */);
}

static PFNEGLGETCURRENTCONTEXTPROC
epoxy_eglGetCurrentContext_resolver(void)
{
    return egl_single_resolver(EGL_14, 833 /* eglGetCurrentContext */);
}

static PFNEGLGETCURRENTDISPLAYPROC
epoxy_eglGetCurrentDisplay_resolver(void)
{
    return egl_single_resolver(EGL_10, 854 /* eglGetCurrentDisplay */);
}

static PFNEGLGETCURRENTSURFACEPROC
epoxy_eglGetCurrentSurface_resolver(void)
{
    return egl_single_resolver(EGL_10, 875 /* eglGetCurrentSurface */);
}

static PFNEGLGETDISPLAYPROC
epoxy_eglGetDisplay_resolver(void)
{
    return egl_single_resolver(EGL_10, 896 /* eglGetDisplay */);
}

static PFNEGLGETERRORPROC
epoxy_eglGetError_resolver(void)
{
    return egl_single_resolver(EGL_10, 910 /* eglGetError */);
}

static PFNEGLGETPLATFORMDISPLAYPROC
epoxy_eglGetPlatformDisplay_resolver(void)
{
    return egl_single_resolver(EGL_15, 922 /* eglGetPlatformDisplay */);
}

static PFNEGLGETPLATFORMDISPLAYEXTPROC
epoxy_eglGetPlatformDisplayEXT_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_EXT_platform_base, 944 /* eglGetPlatformDisplayEXT */);
}

static PFNEGLGETPROCADDRESSPROC
epoxy_eglGetProcAddress_resolver(void)
{
    return egl_single_resolver(EGL_10, 969 /* eglGetProcAddress */);
}

static PFNEGLGETSTREAMFILEDESCRIPTORKHRPROC
epoxy_eglGetStreamFileDescriptorKHR_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_KHR_stream_cross_process_fd, 987 /* eglGetStreamFileDescriptorKHR */);
}

static PFNEGLGETSYNCATTRIBPROC
epoxy_eglGetSyncAttrib_resolver(void)
{
    return egl_single_resolver(EGL_15, 1017 /* eglGetSyncAttrib */);
}

static PFNEGLGETSYNCATTRIBKHRPROC
epoxy_eglGetSyncAttribKHR_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_KHR_reusable_sync, 1034 /* eglGetSyncAttribKHR */);
}

static PFNEGLGETSYNCATTRIBNVPROC
epoxy_eglGetSyncAttribNV_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_NV_sync, 1054 /* eglGetSyncAttribNV */);
}

static PFNEGLGETSYSTEMTIMEFREQUENCYNVPROC
epoxy_eglGetSystemTimeFrequencyNV_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_NV_system_time, 1073 /* eglGetSystemTimeFrequencyNV */);
}

static PFNEGLGETSYSTEMTIMENVPROC
epoxy_eglGetSystemTimeNV_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_NV_system_time, 1101 /* eglGetSystemTimeNV */);
}

static PFNEGLINITIALIZEPROC
epoxy_eglInitialize_resolver(void)
{
    return egl_single_resolver(EGL_10, 1120 /* eglInitialize */);
}

static PFNEGLLOCKSURFACEKHRPROC
epoxy_eglLockSurfaceKHR_resolver(void)
{
    static const enum egl_provider providers[] = {
        EGL_extension_EGL_KHR_lock_surface3,
        EGL_extension_EGL_KHR_lock_surface,
        egl_provider_terminator
    };
    static const uint16_t entrypoints[] = {
        1134 /* "eglLockSurfaceKHR" */,
        1134 /* "eglLockSurfaceKHR" */,
    };
    return egl_provider_resolver(entrypoint_strings + 1134 /* "eglLockSurfaceKHR" */,
                                providers, entrypoints);
}

static PFNEGLMAKECURRENTPROC
epoxy_eglMakeCurrent_resolver(void)
{
    return egl_single_resolver(EGL_10, 1152 /* eglMakeCurrent */);
}

static PFNEGLPOSTSUBBUFFERNVPROC
epoxy_eglPostSubBufferNV_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_NV_post_sub_buffer, 1167 /* eglPostSubBufferNV */);
}

static PFNEGLQUERYAPIPROC
epoxy_eglQueryAPI_resolver(void)
{
    return egl_single_resolver(EGL_12, 1186 /* eglQueryAPI */);
}

static PFNEGLQUERYCONTEXTPROC
epoxy_eglQueryContext_resolver(void)
{
    return egl_single_resolver(EGL_10, 1198 /* eglQueryContext */);
}

static PFNEGLQUERYNATIVEDISPLAYNVPROC
epoxy_eglQueryNativeDisplayNV_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_NV_native_query, 1214 /* eglQueryNativeDisplayNV */);
}

static PFNEGLQUERYNATIVEPIXMAPNVPROC
epoxy_eglQueryNativePixmapNV_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_NV_native_query, 1238 /* eglQueryNativePixmapNV */);
}

static PFNEGLQUERYNATIVEWINDOWNVPROC
epoxy_eglQueryNativeWindowNV_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_NV_native_query, 1261 /* eglQueryNativeWindowNV */);
}

static PFNEGLQUERYSTREAMKHRPROC
epoxy_eglQueryStreamKHR_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_KHR_stream, 1284 /* eglQueryStreamKHR */);
}

static PFNEGLQUERYSTREAMTIMEKHRPROC
epoxy_eglQueryStreamTimeKHR_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_KHR_stream_fifo, 1302 /* eglQueryStreamTimeKHR */);
}

static PFNEGLQUERYSTREAMU64KHRPROC
epoxy_eglQueryStreamu64KHR_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_KHR_stream, 1324 /* eglQueryStreamu64KHR */);
}

static PFNEGLQUERYSTRINGPROC
epoxy_eglQueryString_resolver(void)
{
    return egl_single_resolver(EGL_10, 1345 /* eglQueryString */);
}

static PFNEGLQUERYSURFACEPROC
epoxy_eglQuerySurface_resolver(void)
{
    return egl_single_resolver(EGL_10, 1360 /* eglQuerySurface */);
}

static PFNEGLQUERYSURFACE64KHRPROC
epoxy_eglQuerySurface64KHR_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_KHR_lock_surface3, 1376 /* eglQuerySurface64KHR */);
}

static PFNEGLQUERYSURFACEPOINTERANGLEPROC
epoxy_eglQuerySurfacePointerANGLE_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_ANGLE_query_surface_pointer, 1397 /* eglQuerySurfacePointerANGLE */);
}

static PFNEGLRELEASETEXIMAGEPROC
epoxy_eglReleaseTexImage_resolver(void)
{
    return egl_single_resolver(EGL_11, 1425 /* eglReleaseTexImage */);
}

static PFNEGLRELEASETHREADPROC
epoxy_eglReleaseThread_resolver(void)
{
    return egl_single_resolver(EGL_12, 1444 /* eglReleaseThread */);
}

static PFNEGLSETBLOBCACHEFUNCSANDROIDPROC
epoxy_eglSetBlobCacheFuncsANDROID_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_ANDROID_blob_cache, 1461 /* eglSetBlobCacheFuncsANDROID */);
}

static PFNEGLSIGNALSYNCKHRPROC
epoxy_eglSignalSyncKHR_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_KHR_reusable_sync, 1489 /* eglSignalSyncKHR */);
}

static PFNEGLSIGNALSYNCNVPROC
epoxy_eglSignalSyncNV_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_NV_sync, 1506 /* eglSignalSyncNV */);
}

static PFNEGLSTREAMATTRIBKHRPROC
epoxy_eglStreamAttribKHR_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_KHR_stream, 1522 /* eglStreamAttribKHR */);
}

static PFNEGLSTREAMCONSUMERACQUIREKHRPROC
epoxy_eglStreamConsumerAcquireKHR_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_KHR_stream_consumer_gltexture, 1541 /* eglStreamConsumerAcquireKHR */);
}

static PFNEGLSTREAMCONSUMERGLTEXTUREEXTERNALKHRPROC
epoxy_eglStreamConsumerGLTextureExternalKHR_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_KHR_stream_consumer_gltexture, 1569 /* eglStreamConsumerGLTextureExternalKHR */);
}

static PFNEGLSTREAMCONSUMERRELEASEKHRPROC
epoxy_eglStreamConsumerReleaseKHR_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_KHR_stream_consumer_gltexture, 1607 /* eglStreamConsumerReleaseKHR */);
}

static PFNEGLSURFACEATTRIBPROC
epoxy_eglSurfaceAttrib_resolver(void)
{
    return egl_single_resolver(EGL_11, 1635 /* eglSurfaceAttrib */);
}

static PFNEGLSWAPBUFFERSPROC
epoxy_eglSwapBuffers_resolver(void)
{
    return egl_single_resolver(EGL_10, 1652 /* eglSwapBuffers */);
}

static PFNEGLSWAPBUFFERSREGION2NOKPROC
epoxy_eglSwapBuffersRegion2NOK_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_NOK_swap_region2, 1667 /* eglSwapBuffersRegion2NOK */);
}

static PFNEGLSWAPBUFFERSREGIONNOKPROC
epoxy_eglSwapBuffersRegionNOK_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_NOK_swap_region, 1692 /* eglSwapBuffersRegionNOK */);
}

static PFNEGLSWAPBUFFERSWITHDAMAGEEXTPROC
epoxy_eglSwapBuffersWithDamageEXT_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_EXT_swap_buffers_with_damage, 1716 /* eglSwapBuffersWithDamageEXT */);
}

static PFNEGLSWAPINTERVALPROC
epoxy_eglSwapInterval_resolver(void)
{
    return egl_single_resolver(EGL_11, 1744 /* eglSwapInterval */);
}

static PFNEGLTERMINATEPROC
epoxy_eglTerminate_resolver(void)
{
    return egl_single_resolver(EGL_10, 1760 /* eglTerminate */);
}

static PFNEGLUNLOCKSURFACEKHRPROC
epoxy_eglUnlockSurfaceKHR_resolver(void)
{
    static const enum egl_provider providers[] = {
        EGL_extension_EGL_KHR_lock_surface3,
        EGL_extension_EGL_KHR_lock_surface,
        egl_provider_terminator
    };
    static const uint16_t entrypoints[] = {
        1773 /* "eglUnlockSurfaceKHR" */,
        1773 /* "eglUnlockSurfaceKHR" */,
    };
    return egl_provider_resolver(entrypoint_strings + 1773 /* "eglUnlockSurfaceKHR" */,
                                providers, entrypoints);
}

static PFNEGLWAITCLIENTPROC
epoxy_eglWaitClient_resolver(void)
{
    return egl_single_resolver(EGL_12, 1793 /* eglWaitClient */);
}

static PFNEGLWAITGLPROC
epoxy_eglWaitGL_resolver(void)
{
    return egl_single_resolver(EGL_10, 1807 /* eglWaitGL */);
}

static PFNEGLWAITNATIVEPROC
epoxy_eglWaitNative_resolver(void)
{
    return egl_single_resolver(EGL_10, 1817 /* eglWaitNative */);
}

static PFNEGLWAITSYNCPROC
epoxy_eglWaitSync_resolver(void)
{
    return egl_single_resolver(EGL_15, 1831 /* eglWaitSync */);
}

static PFNEGLWAITSYNCKHRPROC
epoxy_eglWaitSyncKHR_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_KHR_wait_sync, 1843 /* eglWaitSyncKHR */);
}

GEN_THUNKS_RET(EGLBoolean, eglBindAPI, (EGLenum api), (api))
GEN_THUNKS_RET(EGLBoolean, eglBindTexImage, (EGLDisplay dpy, EGLSurface surface, EGLint buffer), (dpy, surface, buffer))
GEN_THUNKS_RET(EGLBoolean, eglChooseConfig, (EGLDisplay dpy, const EGLint * attrib_list, EGLConfig * configs, EGLint config_size, EGLint * num_config), (dpy, attrib_list, configs, config_size, num_config))
GEN_THUNKS_RET(EGLint, eglClientWaitSync, (EGLDisplay dpy, EGLSync sync, EGLint flags, EGLTime timeout), (dpy, sync, flags, timeout))
GEN_THUNKS_RET(EGLint, eglClientWaitSyncKHR, (EGLDisplay dpy, EGLSyncKHR sync, EGLint flags, EGLTimeKHR timeout), (dpy, sync, flags, timeout))
GEN_THUNKS_RET(EGLint, eglClientWaitSyncNV, (EGLSyncNV sync, EGLint flags, EGLTimeNV timeout), (sync, flags, timeout))
GEN_THUNKS_RET(EGLBoolean, eglCopyBuffers, (EGLDisplay dpy, EGLSurface surface, EGLNativePixmapType target), (dpy, surface, target))
GEN_THUNKS_RET(EGLContext, eglCreateContext, (EGLDisplay dpy, EGLConfig config, EGLContext share_context, const EGLint * attrib_list), (dpy, config, share_context, attrib_list))
GEN_THUNKS_RET(EGLImageKHR, eglCreateDRMImageMESA, (EGLDisplay dpy, const EGLint * attrib_list), (dpy, attrib_list))
GEN_THUNKS_RET(EGLSyncNV, eglCreateFenceSyncNV, (EGLDisplay dpy, EGLenum condition, const EGLint * attrib_list), (dpy, condition, attrib_list))
GEN_THUNKS_RET(EGLImageKHR, eglCreateImageKHR, (EGLDisplay dpy, EGLContext ctx, EGLenum target, EGLClientBuffer buffer, const EGLint * attrib_list), (dpy, ctx, target, buffer, attrib_list))
GEN_THUNKS_RET(EGLSurface, eglCreatePbufferFromClientBuffer, (EGLDisplay dpy, EGLenum buftype, EGLClientBuffer buffer, EGLConfig config, const EGLint * attrib_list), (dpy, buftype, buffer, config, attrib_list))
GEN_THUNKS_RET(EGLSurface, eglCreatePbufferSurface, (EGLDisplay dpy, EGLConfig config, const EGLint * attrib_list), (dpy, config, attrib_list))
GEN_THUNKS_RET(EGLSurface, eglCreatePixmapSurface, (EGLDisplay dpy, EGLConfig config, EGLNativePixmapType pixmap, const EGLint * attrib_list), (dpy, config, pixmap, attrib_list))
GEN_THUNKS_RET(EGLSurface, eglCreatePixmapSurfaceHI, (EGLDisplay dpy, EGLConfig config, struct EGLClientPixmapHI * pixmap), (dpy, config, pixmap))
GEN_THUNKS_RET(EGLSurface, eglCreatePlatformPixmapSurface, (EGLDisplay dpy, EGLConfig config, void * native_pixmap, const EGLAttrib * attrib_list), (dpy, config, native_pixmap, attrib_list))
GEN_THUNKS_RET(EGLSurface, eglCreatePlatformPixmapSurfaceEXT, (EGLDisplay dpy, EGLConfig config, void * native_pixmap, const EGLint * attrib_list), (dpy, config, native_pixmap, attrib_list))
GEN_THUNKS_RET(EGLSurface, eglCreatePlatformWindowSurface, (EGLDisplay dpy, EGLConfig config, void * native_window, const EGLAttrib * attrib_list), (dpy, config, native_window, attrib_list))
GEN_THUNKS_RET(EGLSurface, eglCreatePlatformWindowSurfaceEXT, (EGLDisplay dpy, EGLConfig config, void * native_window, const EGLint * attrib_list), (dpy, config, native_window, attrib_list))
GEN_THUNKS_RET(EGLStreamKHR, eglCreateStreamFromFileDescriptorKHR, (EGLDisplay dpy, EGLNativeFileDescriptorKHR file_descriptor), (dpy, file_descriptor))
GEN_THUNKS_RET(EGLStreamKHR, eglCreateStreamKHR, (EGLDisplay dpy, const EGLint * attrib_list), (dpy, attrib_list))
GEN_THUNKS_RET(EGLSurface, eglCreateStreamProducerSurfaceKHR, (EGLDisplay dpy, EGLConfig config, EGLStreamKHR stream, const EGLint * attrib_list), (dpy, config, stream, attrib_list))
GEN_THUNKS_RET(EGLSyncKHR, eglCreateStreamSyncNV, (EGLDisplay dpy, EGLStreamKHR stream, EGLenum type, const EGLint * attrib_list), (dpy, stream, type, attrib_list))
GEN_THUNKS_RET(EGLSync, eglCreateSync, (EGLDisplay dpy, EGLenum type, const EGLAttrib * attrib_list), (dpy, type, attrib_list))
GEN_THUNKS_RET(EGLSyncKHR, eglCreateSync64KHR, (EGLDisplay dpy, EGLenum type, const EGLAttribKHR * attrib_list), (dpy, type, attrib_list))
GEN_THUNKS_RET(EGLSyncKHR, eglCreateSyncKHR, (EGLDisplay dpy, EGLenum type, const EGLint * attrib_list), (dpy, type, attrib_list))
GEN_THUNKS_RET(EGLSurface, eglCreateWindowSurface, (EGLDisplay dpy, EGLConfig config, EGLNativeWindowType win, const EGLint * attrib_list), (dpy, config, win, attrib_list))
GEN_THUNKS_RET(EGLBoolean, eglDestroyContext, (EGLDisplay dpy, EGLContext ctx), (dpy, ctx))
GEN_THUNKS_RET(EGLBoolean, eglDestroyImageKHR, (EGLDisplay dpy, EGLImageKHR image), (dpy, image))
GEN_THUNKS_RET(EGLBoolean, eglDestroyStreamKHR, (EGLDisplay dpy, EGLStreamKHR stream), (dpy, stream))
GEN_THUNKS_RET(EGLBoolean, eglDestroySurface, (EGLDisplay dpy, EGLSurface surface), (dpy, surface))
GEN_THUNKS_RET(EGLBoolean, eglDestroySync, (EGLDisplay dpy, EGLSync sync), (dpy, sync))
GEN_THUNKS_RET(EGLBoolean, eglDestroySyncKHR, (EGLDisplay dpy, EGLSyncKHR sync), (dpy, sync))
GEN_THUNKS_RET(EGLBoolean, eglDestroySyncNV, (EGLSyncNV sync), (sync))
GEN_THUNKS_RET(EGLint, eglDupNativeFenceFDANDROID, (EGLDisplay dpy, EGLSyncKHR sync), (dpy, sync))
GEN_THUNKS_RET(EGLBoolean, eglExportDRMImageMESA, (EGLDisplay dpy, EGLImageKHR image, EGLint * name, EGLint * handle, EGLint * stride), (dpy, image, name, handle, stride))
GEN_THUNKS_RET(EGLBoolean, eglFenceNV, (EGLSyncNV sync), (sync))
GEN_THUNKS_RET(EGLBoolean, eglGetConfigAttrib, (EGLDisplay dpy, EGLConfig config, EGLint attribute, EGLint * value), (dpy, config, attribute, value))
GEN_THUNKS_RET(EGLBoolean, eglGetConfigs, (EGLDisplay dpy, EGLConfig * configs, EGLint config_size, EGLint * num_config), (dpy, configs, config_size, num_config))
GEN_THUNKS_RET(EGLContext, eglGetCurrentContext, (void), ())
GEN_THUNKS_RET(EGLDisplay, eglGetCurrentDisplay, (void), ())
GEN_THUNKS_RET(EGLSurface, eglGetCurrentSurface, (EGLint readdraw), (readdraw))
GEN_THUNKS_RET(EGLDisplay, eglGetDisplay, (EGLNativeDisplayType display_id), (display_id))
GEN_THUNKS_RET(EGLint, eglGetError, (void), ())
GEN_THUNKS_RET(EGLDisplay, eglGetPlatformDisplay, (EGLenum platform, void * native_display, const EGLAttrib * attrib_list), (platform, native_display, attrib_list))
GEN_THUNKS_RET(EGLDisplay, eglGetPlatformDisplayEXT, (EGLenum platform, void * native_display, const EGLint * attrib_list), (platform, native_display, attrib_list))
GEN_THUNKS_RET(__eglMustCastToProperFunctionPointerType, eglGetProcAddress, (const char * procname), (procname))
GEN_THUNKS_RET(EGLNativeFileDescriptorKHR, eglGetStreamFileDescriptorKHR, (EGLDisplay dpy, EGLStreamKHR stream), (dpy, stream))
GEN_THUNKS_RET(EGLBoolean, eglGetSyncAttrib, (EGLDisplay dpy, EGLSync sync, EGLint attribute, EGLAttrib * value), (dpy, sync, attribute, value))
GEN_THUNKS_RET(EGLBoolean, eglGetSyncAttribKHR, (EGLDisplay dpy, EGLSyncKHR sync, EGLint attribute, EGLint * value), (dpy, sync, attribute, value))
GEN_THUNKS_RET(EGLBoolean, eglGetSyncAttribNV, (EGLSyncNV sync, EGLint attribute, EGLint * value), (sync, attribute, value))
GEN_THUNKS_RET(EGLuint64NV, eglGetSystemTimeFrequencyNV, (void), ())
GEN_THUNKS_RET(EGLuint64NV, eglGetSystemTimeNV, (void), ())
GEN_THUNKS_RET(EGLBoolean, eglInitialize, (EGLDisplay dpy, EGLint * major, EGLint * minor), (dpy, major, minor))
GEN_THUNKS_RET(EGLBoolean, eglLockSurfaceKHR, (EGLDisplay dpy, EGLSurface surface, const EGLint * attrib_list), (dpy, surface, attrib_list))
GEN_THUNKS_RET(EGLBoolean, eglMakeCurrent, (EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx), (dpy, draw, read, ctx))
GEN_THUNKS_RET(EGLBoolean, eglPostSubBufferNV, (EGLDisplay dpy, EGLSurface surface, EGLint x, EGLint y, EGLint width, EGLint height), (dpy, surface, x, y, width, height))
GEN_THUNKS_RET(EGLenum, eglQueryAPI, (void), ())
GEN_THUNKS_RET(EGLBoolean, eglQueryContext, (EGLDisplay dpy, EGLContext ctx, EGLint attribute, EGLint * value), (dpy, ctx, attribute, value))
GEN_THUNKS_RET(EGLBoolean, eglQueryNativeDisplayNV, (EGLDisplay dpy, EGLNativeDisplayType * display_id), (dpy, display_id))
GEN_THUNKS_RET(EGLBoolean, eglQueryNativePixmapNV, (EGLDisplay dpy, EGLSurface surf, EGLNativePixmapType * pixmap), (dpy, surf, pixmap))
GEN_THUNKS_RET(EGLBoolean, eglQueryNativeWindowNV, (EGLDisplay dpy, EGLSurface surf, EGLNativeWindowType * window), (dpy, surf, window))
GEN_THUNKS_RET(EGLBoolean, eglQueryStreamKHR, (EGLDisplay dpy, EGLStreamKHR stream, EGLenum attribute, EGLint * value), (dpy, stream, attribute, value))
GEN_THUNKS_RET(EGLBoolean, eglQueryStreamTimeKHR, (EGLDisplay dpy, EGLStreamKHR stream, EGLenum attribute, EGLTimeKHR * value), (dpy, stream, attribute, value))
GEN_THUNKS_RET(EGLBoolean, eglQueryStreamu64KHR, (EGLDisplay dpy, EGLStreamKHR stream, EGLenum attribute, EGLuint64KHR * value), (dpy, stream, attribute, value))
GEN_THUNKS_RET(const char *, eglQueryString, (EGLDisplay dpy, EGLint name), (dpy, name))
GEN_THUNKS_RET(EGLBoolean, eglQuerySurface, (EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint * value), (dpy, surface, attribute, value))
GEN_THUNKS_RET(EGLBoolean, eglQuerySurface64KHR, (EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLAttribKHR * value), (dpy, surface, attribute, value))
GEN_THUNKS_RET(EGLBoolean, eglQuerySurfacePointerANGLE, (EGLDisplay dpy, EGLSurface surface, EGLint attribute, void ** value), (dpy, surface, attribute, value))
GEN_THUNKS_RET(EGLBoolean, eglReleaseTexImage, (EGLDisplay dpy, EGLSurface surface, EGLint buffer), (dpy, surface, buffer))
GEN_THUNKS_RET(EGLBoolean, eglReleaseThread, (void), ())
GEN_THUNKS(eglSetBlobCacheFuncsANDROID, (EGLDisplay dpy, EGLSetBlobFuncANDROID set, EGLGetBlobFuncANDROID get), (dpy, set, get))
GEN_THUNKS_RET(EGLBoolean, eglSignalSyncKHR, (EGLDisplay dpy, EGLSyncKHR sync, EGLenum mode), (dpy, sync, mode))
GEN_THUNKS_RET(EGLBoolean, eglSignalSyncNV, (EGLSyncNV sync, EGLenum mode), (sync, mode))
GEN_THUNKS_RET(EGLBoolean, eglStreamAttribKHR, (EGLDisplay dpy, EGLStreamKHR stream, EGLenum attribute, EGLint value), (dpy, stream, attribute, value))
GEN_THUNKS_RET(EGLBoolean, eglStreamConsumerAcquireKHR, (EGLDisplay dpy, EGLStreamKHR stream), (dpy, stream))
GEN_THUNKS_RET(EGLBoolean, eglStreamConsumerGLTextureExternalKHR, (EGLDisplay dpy, EGLStreamKHR stream), (dpy, stream))
GEN_THUNKS_RET(EGLBoolean, eglStreamConsumerReleaseKHR, (EGLDisplay dpy, EGLStreamKHR stream), (dpy, stream))
GEN_THUNKS_RET(EGLBoolean, eglSurfaceAttrib, (EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint value), (dpy, surface, attribute, value))
GEN_THUNKS_RET(EGLBoolean, eglSwapBuffers, (EGLDisplay dpy, EGLSurface surface), (dpy, surface))
GEN_THUNKS_RET(EGLBoolean, eglSwapBuffersRegion2NOK, (EGLDisplay dpy, EGLSurface surface, EGLint numRects, const EGLint * rects), (dpy, surface, numRects, rects))
GEN_THUNKS_RET(EGLBoolean, eglSwapBuffersRegionNOK, (EGLDisplay dpy, EGLSurface surface, EGLint numRects, const EGLint * rects), (dpy, surface, numRects, rects))
GEN_THUNKS_RET(EGLBoolean, eglSwapBuffersWithDamageEXT, (EGLDisplay dpy, EGLSurface surface, EGLint * rects, EGLint n_rects), (dpy, surface, rects, n_rects))
GEN_THUNKS_RET(EGLBoolean, eglSwapInterval, (EGLDisplay dpy, EGLint interval), (dpy, interval))
GEN_THUNKS_RET(EGLBoolean, eglTerminate, (EGLDisplay dpy), (dpy))
GEN_THUNKS_RET(EGLBoolean, eglUnlockSurfaceKHR, (EGLDisplay dpy, EGLSurface surface), (dpy, surface))
GEN_THUNKS_RET(EGLBoolean, eglWaitClient, (void), ())
GEN_THUNKS_RET(EGLBoolean, eglWaitGL, (void), ())
GEN_THUNKS_RET(EGLBoolean, eglWaitNative, (EGLint engine), (engine))
GEN_THUNKS_RET(EGLBoolean, eglWaitSync, (EGLDisplay dpy, EGLSync sync, EGLint flags), (dpy, sync, flags))
GEN_THUNKS_RET(EGLint, eglWaitSyncKHR, (EGLDisplay dpy, EGLSyncKHR sync, EGLint flags), (dpy, sync, flags))
#if USING_DISPATCH_TABLE
static struct dispatch_table resolver_table = {
    .eglBindAPI = epoxy_eglBindAPI_dispatch_table_rewrite_ptr,
    .eglBindTexImage = epoxy_eglBindTexImage_dispatch_table_rewrite_ptr,
    .eglChooseConfig = epoxy_eglChooseConfig_dispatch_table_rewrite_ptr,
    .eglClientWaitSync = epoxy_eglClientWaitSync_dispatch_table_rewrite_ptr,
    .eglClientWaitSyncKHR = epoxy_eglClientWaitSyncKHR_dispatch_table_rewrite_ptr,
    .eglClientWaitSyncNV = epoxy_eglClientWaitSyncNV_dispatch_table_rewrite_ptr,
    .eglCopyBuffers = epoxy_eglCopyBuffers_dispatch_table_rewrite_ptr,
    .eglCreateContext = epoxy_eglCreateContext_dispatch_table_rewrite_ptr,
    .eglCreateDRMImageMESA = epoxy_eglCreateDRMImageMESA_dispatch_table_rewrite_ptr,
    .eglCreateFenceSyncNV = epoxy_eglCreateFenceSyncNV_dispatch_table_rewrite_ptr,
    .eglCreateImageKHR = epoxy_eglCreateImageKHR_dispatch_table_rewrite_ptr,
    .eglCreatePbufferFromClientBuffer = epoxy_eglCreatePbufferFromClientBuffer_dispatch_table_rewrite_ptr,
    .eglCreatePbufferSurface = epoxy_eglCreatePbufferSurface_dispatch_table_rewrite_ptr,
    .eglCreatePixmapSurface = epoxy_eglCreatePixmapSurface_dispatch_table_rewrite_ptr,
    .eglCreatePixmapSurfaceHI = epoxy_eglCreatePixmapSurfaceHI_dispatch_table_rewrite_ptr,
    .eglCreatePlatformPixmapSurface = epoxy_eglCreatePlatformPixmapSurface_dispatch_table_rewrite_ptr,
    .eglCreatePlatformPixmapSurfaceEXT = epoxy_eglCreatePlatformPixmapSurfaceEXT_dispatch_table_rewrite_ptr,
    .eglCreatePlatformWindowSurface = epoxy_eglCreatePlatformWindowSurface_dispatch_table_rewrite_ptr,
    .eglCreatePlatformWindowSurfaceEXT = epoxy_eglCreatePlatformWindowSurfaceEXT_dispatch_table_rewrite_ptr,
    .eglCreateStreamFromFileDescriptorKHR = epoxy_eglCreateStreamFromFileDescriptorKHR_dispatch_table_rewrite_ptr,
    .eglCreateStreamKHR = epoxy_eglCreateStreamKHR_dispatch_table_rewrite_ptr,
    .eglCreateStreamProducerSurfaceKHR = epoxy_eglCreateStreamProducerSurfaceKHR_dispatch_table_rewrite_ptr,
    .eglCreateStreamSyncNV = epoxy_eglCreateStreamSyncNV_dispatch_table_rewrite_ptr,
    .eglCreateSync = epoxy_eglCreateSync_dispatch_table_rewrite_ptr,
    .eglCreateSync64KHR = epoxy_eglCreateSync64KHR_dispatch_table_rewrite_ptr,
    .eglCreateSyncKHR = epoxy_eglCreateSyncKHR_dispatch_table_rewrite_ptr,
    .eglCreateWindowSurface = epoxy_eglCreateWindowSurface_dispatch_table_rewrite_ptr,
    .eglDestroyContext = epoxy_eglDestroyContext_dispatch_table_rewrite_ptr,
    .eglDestroyImageKHR = epoxy_eglDestroyImageKHR_dispatch_table_rewrite_ptr,
    .eglDestroyStreamKHR = epoxy_eglDestroyStreamKHR_dispatch_table_rewrite_ptr,
    .eglDestroySurface = epoxy_eglDestroySurface_dispatch_table_rewrite_ptr,
    .eglDestroySync = epoxy_eglDestroySync_dispatch_table_rewrite_ptr,
    .eglDestroySyncKHR = epoxy_eglDestroySyncKHR_dispatch_table_rewrite_ptr,
    .eglDestroySyncNV = epoxy_eglDestroySyncNV_dispatch_table_rewrite_ptr,
    .eglDupNativeFenceFDANDROID = epoxy_eglDupNativeFenceFDANDROID_dispatch_table_rewrite_ptr,
    .eglExportDRMImageMESA = epoxy_eglExportDRMImageMESA_dispatch_table_rewrite_ptr,
    .eglFenceNV = epoxy_eglFenceNV_dispatch_table_rewrite_ptr,
    .eglGetConfigAttrib = epoxy_eglGetConfigAttrib_dispatch_table_rewrite_ptr,
    .eglGetConfigs = epoxy_eglGetConfigs_dispatch_table_rewrite_ptr,
    .eglGetCurrentContext = epoxy_eglGetCurrentContext_dispatch_table_rewrite_ptr,
    .eglGetCurrentDisplay = epoxy_eglGetCurrentDisplay_dispatch_table_rewrite_ptr,
    .eglGetCurrentSurface = epoxy_eglGetCurrentSurface_dispatch_table_rewrite_ptr,
    .eglGetDisplay = epoxy_eglGetDisplay_dispatch_table_rewrite_ptr,
    .eglGetError = epoxy_eglGetError_dispatch_table_rewrite_ptr,
    .eglGetPlatformDisplay = epoxy_eglGetPlatformDisplay_dispatch_table_rewrite_ptr,
    .eglGetPlatformDisplayEXT = epoxy_eglGetPlatformDisplayEXT_dispatch_table_rewrite_ptr,
    .eglGetProcAddress = epoxy_eglGetProcAddress_dispatch_table_rewrite_ptr,
    .eglGetStreamFileDescriptorKHR = epoxy_eglGetStreamFileDescriptorKHR_dispatch_table_rewrite_ptr,
    .eglGetSyncAttrib = epoxy_eglGetSyncAttrib_dispatch_table_rewrite_ptr,
    .eglGetSyncAttribKHR = epoxy_eglGetSyncAttribKHR_dispatch_table_rewrite_ptr,
    .eglGetSyncAttribNV = epoxy_eglGetSyncAttribNV_dispatch_table_rewrite_ptr,
    .eglGetSystemTimeFrequencyNV = epoxy_eglGetSystemTimeFrequencyNV_dispatch_table_rewrite_ptr,
    .eglGetSystemTimeNV = epoxy_eglGetSystemTimeNV_dispatch_table_rewrite_ptr,
    .eglInitialize = epoxy_eglInitialize_dispatch_table_rewrite_ptr,
    .eglLockSurfaceKHR = epoxy_eglLockSurfaceKHR_dispatch_table_rewrite_ptr,
    .eglMakeCurrent = epoxy_eglMakeCurrent_dispatch_table_rewrite_ptr,
    .eglPostSubBufferNV = epoxy_eglPostSubBufferNV_dispatch_table_rewrite_ptr,
    .eglQueryAPI = epoxy_eglQueryAPI_dispatch_table_rewrite_ptr,
    .eglQueryContext = epoxy_eglQueryContext_dispatch_table_rewrite_ptr,
    .eglQueryNativeDisplayNV = epoxy_eglQueryNativeDisplayNV_dispatch_table_rewrite_ptr,
    .eglQueryNativePixmapNV = epoxy_eglQueryNativePixmapNV_dispatch_table_rewrite_ptr,
    .eglQueryNativeWindowNV = epoxy_eglQueryNativeWindowNV_dispatch_table_rewrite_ptr,
    .eglQueryStreamKHR = epoxy_eglQueryStreamKHR_dispatch_table_rewrite_ptr,
    .eglQueryStreamTimeKHR = epoxy_eglQueryStreamTimeKHR_dispatch_table_rewrite_ptr,
    .eglQueryStreamu64KHR = epoxy_eglQueryStreamu64KHR_dispatch_table_rewrite_ptr,
    .eglQueryString = epoxy_eglQueryString_dispatch_table_rewrite_ptr,
    .eglQuerySurface = epoxy_eglQuerySurface_dispatch_table_rewrite_ptr,
    .eglQuerySurface64KHR = epoxy_eglQuerySurface64KHR_dispatch_table_rewrite_ptr,
    .eglQuerySurfacePointerANGLE = epoxy_eglQuerySurfacePointerANGLE_dispatch_table_rewrite_ptr,
    .eglReleaseTexImage = epoxy_eglReleaseTexImage_dispatch_table_rewrite_ptr,
    .eglReleaseThread = epoxy_eglReleaseThread_dispatch_table_rewrite_ptr,
    .eglSetBlobCacheFuncsANDROID = epoxy_eglSetBlobCacheFuncsANDROID_dispatch_table_rewrite_ptr,
    .eglSignalSyncKHR = epoxy_eglSignalSyncKHR_dispatch_table_rewrite_ptr,
    .eglSignalSyncNV = epoxy_eglSignalSyncNV_dispatch_table_rewrite_ptr,
    .eglStreamAttribKHR = epoxy_eglStreamAttribKHR_dispatch_table_rewrite_ptr,
    .eglStreamConsumerAcquireKHR = epoxy_eglStreamConsumerAcquireKHR_dispatch_table_rewrite_ptr,
    .eglStreamConsumerGLTextureExternalKHR = epoxy_eglStreamConsumerGLTextureExternalKHR_dispatch_table_rewrite_ptr,
    .eglStreamConsumerReleaseKHR = epoxy_eglStreamConsumerReleaseKHR_dispatch_table_rewrite_ptr,
    .eglSurfaceAttrib = epoxy_eglSurfaceAttrib_dispatch_table_rewrite_ptr,
    .eglSwapBuffers = epoxy_eglSwapBuffers_dispatch_table_rewrite_ptr,
    .eglSwapBuffersRegion2NOK = epoxy_eglSwapBuffersRegion2NOK_dispatch_table_rewrite_ptr,
    .eglSwapBuffersRegionNOK = epoxy_eglSwapBuffersRegionNOK_dispatch_table_rewrite_ptr,
    .eglSwapBuffersWithDamageEXT = epoxy_eglSwapBuffersWithDamageEXT_dispatch_table_rewrite_ptr,
    .eglSwapInterval = epoxy_eglSwapInterval_dispatch_table_rewrite_ptr,
    .eglTerminate = epoxy_eglTerminate_dispatch_table_rewrite_ptr,
    .eglUnlockSurfaceKHR = epoxy_eglUnlockSurfaceKHR_dispatch_table_rewrite_ptr,
    .eglWaitClient = epoxy_eglWaitClient_dispatch_table_rewrite_ptr,
    .eglWaitGL = epoxy_eglWaitGL_dispatch_table_rewrite_ptr,
    .eglWaitNative = epoxy_eglWaitNative_dispatch_table_rewrite_ptr,
    .eglWaitSync = epoxy_eglWaitSync_dispatch_table_rewrite_ptr,
    .eglWaitSyncKHR = epoxy_eglWaitSyncKHR_dispatch_table_rewrite_ptr,
};

uint32_t egl_tls_index;
uint32_t egl_tls_size = sizeof(struct dispatch_table);

static inline struct dispatch_table *
get_dispatch_table(void)
{
	return TlsGetValue(egl_tls_index);
}

void
egl_init_dispatch_table(void)
{
    struct dispatch_table *dispatch_table = get_dispatch_table();
    memcpy(dispatch_table, &resolver_table, sizeof(resolver_table));
}

void
egl_switch_to_dispatch_table(void)
{
    epoxy_eglBindAPI = epoxy_eglBindAPI_dispatch_table_thunk;
    epoxy_eglBindTexImage = epoxy_eglBindTexImage_dispatch_table_thunk;
    epoxy_eglChooseConfig = epoxy_eglChooseConfig_dispatch_table_thunk;
    epoxy_eglClientWaitSync = epoxy_eglClientWaitSync_dispatch_table_thunk;
    epoxy_eglClientWaitSyncKHR = epoxy_eglClientWaitSyncKHR_dispatch_table_thunk;
    epoxy_eglClientWaitSyncNV = epoxy_eglClientWaitSyncNV_dispatch_table_thunk;
    epoxy_eglCopyBuffers = epoxy_eglCopyBuffers_dispatch_table_thunk;
    epoxy_eglCreateContext = epoxy_eglCreateContext_dispatch_table_thunk;
    epoxy_eglCreateDRMImageMESA = epoxy_eglCreateDRMImageMESA_dispatch_table_thunk;
    epoxy_eglCreateFenceSyncNV = epoxy_eglCreateFenceSyncNV_dispatch_table_thunk;
    epoxy_eglCreateImageKHR = epoxy_eglCreateImageKHR_dispatch_table_thunk;
    epoxy_eglCreatePbufferFromClientBuffer = epoxy_eglCreatePbufferFromClientBuffer_dispatch_table_thunk;
    epoxy_eglCreatePbufferSurface = epoxy_eglCreatePbufferSurface_dispatch_table_thunk;
    epoxy_eglCreatePixmapSurface = epoxy_eglCreatePixmapSurface_dispatch_table_thunk;
    epoxy_eglCreatePixmapSurfaceHI = epoxy_eglCreatePixmapSurfaceHI_dispatch_table_thunk;
    epoxy_eglCreatePlatformPixmapSurface = epoxy_eglCreatePlatformPixmapSurface_dispatch_table_thunk;
    epoxy_eglCreatePlatformPixmapSurfaceEXT = epoxy_eglCreatePlatformPixmapSurfaceEXT_dispatch_table_thunk;
    epoxy_eglCreatePlatformWindowSurface = epoxy_eglCreatePlatformWindowSurface_dispatch_table_thunk;
    epoxy_eglCreatePlatformWindowSurfaceEXT = epoxy_eglCreatePlatformWindowSurfaceEXT_dispatch_table_thunk;
    epoxy_eglCreateStreamFromFileDescriptorKHR = epoxy_eglCreateStreamFromFileDescriptorKHR_dispatch_table_thunk;
    epoxy_eglCreateStreamKHR = epoxy_eglCreateStreamKHR_dispatch_table_thunk;
    epoxy_eglCreateStreamProducerSurfaceKHR = epoxy_eglCreateStreamProducerSurfaceKHR_dispatch_table_thunk;
    epoxy_eglCreateStreamSyncNV = epoxy_eglCreateStreamSyncNV_dispatch_table_thunk;
    epoxy_eglCreateSync = epoxy_eglCreateSync_dispatch_table_thunk;
    epoxy_eglCreateSync64KHR = epoxy_eglCreateSync64KHR_dispatch_table_thunk;
    epoxy_eglCreateSyncKHR = epoxy_eglCreateSyncKHR_dispatch_table_thunk;
    epoxy_eglCreateWindowSurface = epoxy_eglCreateWindowSurface_dispatch_table_thunk;
    epoxy_eglDestroyContext = epoxy_eglDestroyContext_dispatch_table_thunk;
    epoxy_eglDestroyImageKHR = epoxy_eglDestroyImageKHR_dispatch_table_thunk;
    epoxy_eglDestroyStreamKHR = epoxy_eglDestroyStreamKHR_dispatch_table_thunk;
    epoxy_eglDestroySurface = epoxy_eglDestroySurface_dispatch_table_thunk;
    epoxy_eglDestroySync = epoxy_eglDestroySync_dispatch_table_thunk;
    epoxy_eglDestroySyncKHR = epoxy_eglDestroySyncKHR_dispatch_table_thunk;
    epoxy_eglDestroySyncNV = epoxy_eglDestroySyncNV_dispatch_table_thunk;
    epoxy_eglDupNativeFenceFDANDROID = epoxy_eglDupNativeFenceFDANDROID_dispatch_table_thunk;
    epoxy_eglExportDRMImageMESA = epoxy_eglExportDRMImageMESA_dispatch_table_thunk;
    epoxy_eglFenceNV = epoxy_eglFenceNV_dispatch_table_thunk;
    epoxy_eglGetConfigAttrib = epoxy_eglGetConfigAttrib_dispatch_table_thunk;
    epoxy_eglGetConfigs = epoxy_eglGetConfigs_dispatch_table_thunk;
    epoxy_eglGetCurrentContext = epoxy_eglGetCurrentContext_dispatch_table_thunk;
    epoxy_eglGetCurrentDisplay = epoxy_eglGetCurrentDisplay_dispatch_table_thunk;
    epoxy_eglGetCurrentSurface = epoxy_eglGetCurrentSurface_dispatch_table_thunk;
    epoxy_eglGetDisplay = epoxy_eglGetDisplay_dispatch_table_thunk;
    epoxy_eglGetError = epoxy_eglGetError_dispatch_table_thunk;
    epoxy_eglGetPlatformDisplay = epoxy_eglGetPlatformDisplay_dispatch_table_thunk;
    epoxy_eglGetPlatformDisplayEXT = epoxy_eglGetPlatformDisplayEXT_dispatch_table_thunk;
    epoxy_eglGetProcAddress = epoxy_eglGetProcAddress_dispatch_table_thunk;
    epoxy_eglGetStreamFileDescriptorKHR = epoxy_eglGetStreamFileDescriptorKHR_dispatch_table_thunk;
    epoxy_eglGetSyncAttrib = epoxy_eglGetSyncAttrib_dispatch_table_thunk;
    epoxy_eglGetSyncAttribKHR = epoxy_eglGetSyncAttribKHR_dispatch_table_thunk;
    epoxy_eglGetSyncAttribNV = epoxy_eglGetSyncAttribNV_dispatch_table_thunk;
    epoxy_eglGetSystemTimeFrequencyNV = epoxy_eglGetSystemTimeFrequencyNV_dispatch_table_thunk;
    epoxy_eglGetSystemTimeNV = epoxy_eglGetSystemTimeNV_dispatch_table_thunk;
    epoxy_eglInitialize = epoxy_eglInitialize_dispatch_table_thunk;
    epoxy_eglLockSurfaceKHR = epoxy_eglLockSurfaceKHR_dispatch_table_thunk;
    epoxy_eglMakeCurrent = epoxy_eglMakeCurrent_dispatch_table_thunk;
    epoxy_eglPostSubBufferNV = epoxy_eglPostSubBufferNV_dispatch_table_thunk;
    epoxy_eglQueryAPI = epoxy_eglQueryAPI_dispatch_table_thunk;
    epoxy_eglQueryContext = epoxy_eglQueryContext_dispatch_table_thunk;
    epoxy_eglQueryNativeDisplayNV = epoxy_eglQueryNativeDisplayNV_dispatch_table_thunk;
    epoxy_eglQueryNativePixmapNV = epoxy_eglQueryNativePixmapNV_dispatch_table_thunk;
    epoxy_eglQueryNativeWindowNV = epoxy_eglQueryNativeWindowNV_dispatch_table_thunk;
    epoxy_eglQueryStreamKHR = epoxy_eglQueryStreamKHR_dispatch_table_thunk;
    epoxy_eglQueryStreamTimeKHR = epoxy_eglQueryStreamTimeKHR_dispatch_table_thunk;
    epoxy_eglQueryStreamu64KHR = epoxy_eglQueryStreamu64KHR_dispatch_table_thunk;
    epoxy_eglQueryString = epoxy_eglQueryString_dispatch_table_thunk;
    epoxy_eglQuerySurface = epoxy_eglQuerySurface_dispatch_table_thunk;
    epoxy_eglQuerySurface64KHR = epoxy_eglQuerySurface64KHR_dispatch_table_thunk;
    epoxy_eglQuerySurfacePointerANGLE = epoxy_eglQuerySurfacePointerANGLE_dispatch_table_thunk;
    epoxy_eglReleaseTexImage = epoxy_eglReleaseTexImage_dispatch_table_thunk;
    epoxy_eglReleaseThread = epoxy_eglReleaseThread_dispatch_table_thunk;
    epoxy_eglSetBlobCacheFuncsANDROID = epoxy_eglSetBlobCacheFuncsANDROID_dispatch_table_thunk;
    epoxy_eglSignalSyncKHR = epoxy_eglSignalSyncKHR_dispatch_table_thunk;
    epoxy_eglSignalSyncNV = epoxy_eglSignalSyncNV_dispatch_table_thunk;
    epoxy_eglStreamAttribKHR = epoxy_eglStreamAttribKHR_dispatch_table_thunk;
    epoxy_eglStreamConsumerAcquireKHR = epoxy_eglStreamConsumerAcquireKHR_dispatch_table_thunk;
    epoxy_eglStreamConsumerGLTextureExternalKHR = epoxy_eglStreamConsumerGLTextureExternalKHR_dispatch_table_thunk;
    epoxy_eglStreamConsumerReleaseKHR = epoxy_eglStreamConsumerReleaseKHR_dispatch_table_thunk;
    epoxy_eglSurfaceAttrib = epoxy_eglSurfaceAttrib_dispatch_table_thunk;
    epoxy_eglSwapBuffers = epoxy_eglSwapBuffers_dispatch_table_thunk;
    epoxy_eglSwapBuffersRegion2NOK = epoxy_eglSwapBuffersRegion2NOK_dispatch_table_thunk;
    epoxy_eglSwapBuffersRegionNOK = epoxy_eglSwapBuffersRegionNOK_dispatch_table_thunk;
    epoxy_eglSwapBuffersWithDamageEXT = epoxy_eglSwapBuffersWithDamageEXT_dispatch_table_thunk;
    epoxy_eglSwapInterval = epoxy_eglSwapInterval_dispatch_table_thunk;
    epoxy_eglTerminate = epoxy_eglTerminate_dispatch_table_thunk;
    epoxy_eglUnlockSurfaceKHR = epoxy_eglUnlockSurfaceKHR_dispatch_table_thunk;
    epoxy_eglWaitClient = epoxy_eglWaitClient_dispatch_table_thunk;
    epoxy_eglWaitGL = epoxy_eglWaitGL_dispatch_table_thunk;
    epoxy_eglWaitNative = epoxy_eglWaitNative_dispatch_table_thunk;
    epoxy_eglWaitSync = epoxy_eglWaitSync_dispatch_table_thunk;
    epoxy_eglWaitSyncKHR = epoxy_eglWaitSyncKHR_dispatch_table_thunk;
}

#endif /* !USING_DISPATCH_TABLE */
PUBLIC PFNEGLBINDAPIPROC epoxy_eglBindAPI = epoxy_eglBindAPI_global_rewrite_ptr;

PUBLIC PFNEGLBINDTEXIMAGEPROC epoxy_eglBindTexImage = epoxy_eglBindTexImage_global_rewrite_ptr;

PUBLIC PFNEGLCHOOSECONFIGPROC epoxy_eglChooseConfig = epoxy_eglChooseConfig_global_rewrite_ptr;

PUBLIC PFNEGLCLIENTWAITSYNCPROC epoxy_eglClientWaitSync = epoxy_eglClientWaitSync_global_rewrite_ptr;

PUBLIC PFNEGLCLIENTWAITSYNCKHRPROC epoxy_eglClientWaitSyncKHR = epoxy_eglClientWaitSyncKHR_global_rewrite_ptr;

PUBLIC PFNEGLCLIENTWAITSYNCNVPROC epoxy_eglClientWaitSyncNV = epoxy_eglClientWaitSyncNV_global_rewrite_ptr;

PUBLIC PFNEGLCOPYBUFFERSPROC epoxy_eglCopyBuffers = epoxy_eglCopyBuffers_global_rewrite_ptr;

PUBLIC PFNEGLCREATECONTEXTPROC epoxy_eglCreateContext = epoxy_eglCreateContext_global_rewrite_ptr;

PUBLIC PFNEGLCREATEDRMIMAGEMESAPROC epoxy_eglCreateDRMImageMESA = epoxy_eglCreateDRMImageMESA_global_rewrite_ptr;

PUBLIC PFNEGLCREATEFENCESYNCNVPROC epoxy_eglCreateFenceSyncNV = epoxy_eglCreateFenceSyncNV_global_rewrite_ptr;

PUBLIC PFNEGLCREATEIMAGEKHRPROC epoxy_eglCreateImageKHR = epoxy_eglCreateImageKHR_global_rewrite_ptr;

PUBLIC PFNEGLCREATEPBUFFERFROMCLIENTBUFFERPROC epoxy_eglCreatePbufferFromClientBuffer = epoxy_eglCreatePbufferFromClientBuffer_global_rewrite_ptr;

PUBLIC PFNEGLCREATEPBUFFERSURFACEPROC epoxy_eglCreatePbufferSurface = epoxy_eglCreatePbufferSurface_global_rewrite_ptr;

PUBLIC PFNEGLCREATEPIXMAPSURFACEPROC epoxy_eglCreatePixmapSurface = epoxy_eglCreatePixmapSurface_global_rewrite_ptr;

PUBLIC PFNEGLCREATEPIXMAPSURFACEHIPROC epoxy_eglCreatePixmapSurfaceHI = epoxy_eglCreatePixmapSurfaceHI_global_rewrite_ptr;

PUBLIC PFNEGLCREATEPLATFORMPIXMAPSURFACEPROC epoxy_eglCreatePlatformPixmapSurface = epoxy_eglCreatePlatformPixmapSurface_global_rewrite_ptr;

PUBLIC PFNEGLCREATEPLATFORMPIXMAPSURFACEEXTPROC epoxy_eglCreatePlatformPixmapSurfaceEXT = epoxy_eglCreatePlatformPixmapSurfaceEXT_global_rewrite_ptr;

PUBLIC PFNEGLCREATEPLATFORMWINDOWSURFACEPROC epoxy_eglCreatePlatformWindowSurface = epoxy_eglCreatePlatformWindowSurface_global_rewrite_ptr;

PUBLIC PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC epoxy_eglCreatePlatformWindowSurfaceEXT = epoxy_eglCreatePlatformWindowSurfaceEXT_global_rewrite_ptr;

PUBLIC PFNEGLCREATESTREAMFROMFILEDESCRIPTORKHRPROC epoxy_eglCreateStreamFromFileDescriptorKHR = epoxy_eglCreateStreamFromFileDescriptorKHR_global_rewrite_ptr;

PUBLIC PFNEGLCREATESTREAMKHRPROC epoxy_eglCreateStreamKHR = epoxy_eglCreateStreamKHR_global_rewrite_ptr;

PUBLIC PFNEGLCREATESTREAMPRODUCERSURFACEKHRPROC epoxy_eglCreateStreamProducerSurfaceKHR = epoxy_eglCreateStreamProducerSurfaceKHR_global_rewrite_ptr;

PUBLIC PFNEGLCREATESTREAMSYNCNVPROC epoxy_eglCreateStreamSyncNV = epoxy_eglCreateStreamSyncNV_global_rewrite_ptr;

PUBLIC PFNEGLCREATESYNCPROC epoxy_eglCreateSync = epoxy_eglCreateSync_global_rewrite_ptr;

PUBLIC PFNEGLCREATESYNC64KHRPROC epoxy_eglCreateSync64KHR = epoxy_eglCreateSync64KHR_global_rewrite_ptr;

PUBLIC PFNEGLCREATESYNCKHRPROC epoxy_eglCreateSyncKHR = epoxy_eglCreateSyncKHR_global_rewrite_ptr;

PUBLIC PFNEGLCREATEWINDOWSURFACEPROC epoxy_eglCreateWindowSurface = epoxy_eglCreateWindowSurface_global_rewrite_ptr;

PUBLIC PFNEGLDESTROYCONTEXTPROC epoxy_eglDestroyContext = epoxy_eglDestroyContext_global_rewrite_ptr;

PUBLIC PFNEGLDESTROYIMAGEKHRPROC epoxy_eglDestroyImageKHR = epoxy_eglDestroyImageKHR_global_rewrite_ptr;

PUBLIC PFNEGLDESTROYSTREAMKHRPROC epoxy_eglDestroyStreamKHR = epoxy_eglDestroyStreamKHR_global_rewrite_ptr;

PUBLIC PFNEGLDESTROYSURFACEPROC epoxy_eglDestroySurface = epoxy_eglDestroySurface_global_rewrite_ptr;

PUBLIC PFNEGLDESTROYSYNCPROC epoxy_eglDestroySync = epoxy_eglDestroySync_global_rewrite_ptr;

PUBLIC PFNEGLDESTROYSYNCKHRPROC epoxy_eglDestroySyncKHR = epoxy_eglDestroySyncKHR_global_rewrite_ptr;

PUBLIC PFNEGLDESTROYSYNCNVPROC epoxy_eglDestroySyncNV = epoxy_eglDestroySyncNV_global_rewrite_ptr;

PUBLIC PFNEGLDUPNATIVEFENCEFDANDROIDPROC epoxy_eglDupNativeFenceFDANDROID = epoxy_eglDupNativeFenceFDANDROID_global_rewrite_ptr;

PUBLIC PFNEGLEXPORTDRMIMAGEMESAPROC epoxy_eglExportDRMImageMESA = epoxy_eglExportDRMImageMESA_global_rewrite_ptr;

PUBLIC PFNEGLFENCENVPROC epoxy_eglFenceNV = epoxy_eglFenceNV_global_rewrite_ptr;

PUBLIC PFNEGLGETCONFIGATTRIBPROC epoxy_eglGetConfigAttrib = epoxy_eglGetConfigAttrib_global_rewrite_ptr;

PUBLIC PFNEGLGETCONFIGSPROC epoxy_eglGetConfigs = epoxy_eglGetConfigs_global_rewrite_ptr;

PUBLIC PFNEGLGETCURRENTCONTEXTPROC epoxy_eglGetCurrentContext = epoxy_eglGetCurrentContext_global_rewrite_ptr;

PUBLIC PFNEGLGETCURRENTDISPLAYPROC epoxy_eglGetCurrentDisplay = epoxy_eglGetCurrentDisplay_global_rewrite_ptr;

PUBLIC PFNEGLGETCURRENTSURFACEPROC epoxy_eglGetCurrentSurface = epoxy_eglGetCurrentSurface_global_rewrite_ptr;

PUBLIC PFNEGLGETDISPLAYPROC epoxy_eglGetDisplay = epoxy_eglGetDisplay_global_rewrite_ptr;

PUBLIC PFNEGLGETERRORPROC epoxy_eglGetError = epoxy_eglGetError_global_rewrite_ptr;

PUBLIC PFNEGLGETPLATFORMDISPLAYPROC epoxy_eglGetPlatformDisplay = epoxy_eglGetPlatformDisplay_global_rewrite_ptr;

PUBLIC PFNEGLGETPLATFORMDISPLAYEXTPROC epoxy_eglGetPlatformDisplayEXT = epoxy_eglGetPlatformDisplayEXT_global_rewrite_ptr;

PUBLIC PFNEGLGETPROCADDRESSPROC epoxy_eglGetProcAddress = epoxy_eglGetProcAddress_global_rewrite_ptr;

PUBLIC PFNEGLGETSTREAMFILEDESCRIPTORKHRPROC epoxy_eglGetStreamFileDescriptorKHR = epoxy_eglGetStreamFileDescriptorKHR_global_rewrite_ptr;

PUBLIC PFNEGLGETSYNCATTRIBPROC epoxy_eglGetSyncAttrib = epoxy_eglGetSyncAttrib_global_rewrite_ptr;

PUBLIC PFNEGLGETSYNCATTRIBKHRPROC epoxy_eglGetSyncAttribKHR = epoxy_eglGetSyncAttribKHR_global_rewrite_ptr;

PUBLIC PFNEGLGETSYNCATTRIBNVPROC epoxy_eglGetSyncAttribNV = epoxy_eglGetSyncAttribNV_global_rewrite_ptr;

PUBLIC PFNEGLGETSYSTEMTIMEFREQUENCYNVPROC epoxy_eglGetSystemTimeFrequencyNV = epoxy_eglGetSystemTimeFrequencyNV_global_rewrite_ptr;

PUBLIC PFNEGLGETSYSTEMTIMENVPROC epoxy_eglGetSystemTimeNV = epoxy_eglGetSystemTimeNV_global_rewrite_ptr;

PUBLIC PFNEGLINITIALIZEPROC epoxy_eglInitialize = epoxy_eglInitialize_global_rewrite_ptr;

PUBLIC PFNEGLLOCKSURFACEKHRPROC epoxy_eglLockSurfaceKHR = epoxy_eglLockSurfaceKHR_global_rewrite_ptr;

PUBLIC PFNEGLMAKECURRENTPROC epoxy_eglMakeCurrent = epoxy_eglMakeCurrent_global_rewrite_ptr;

PUBLIC PFNEGLPOSTSUBBUFFERNVPROC epoxy_eglPostSubBufferNV = epoxy_eglPostSubBufferNV_global_rewrite_ptr;

PUBLIC PFNEGLQUERYAPIPROC epoxy_eglQueryAPI = epoxy_eglQueryAPI_global_rewrite_ptr;

PUBLIC PFNEGLQUERYCONTEXTPROC epoxy_eglQueryContext = epoxy_eglQueryContext_global_rewrite_ptr;

PUBLIC PFNEGLQUERYNATIVEDISPLAYNVPROC epoxy_eglQueryNativeDisplayNV = epoxy_eglQueryNativeDisplayNV_global_rewrite_ptr;

PUBLIC PFNEGLQUERYNATIVEPIXMAPNVPROC epoxy_eglQueryNativePixmapNV = epoxy_eglQueryNativePixmapNV_global_rewrite_ptr;

PUBLIC PFNEGLQUERYNATIVEWINDOWNVPROC epoxy_eglQueryNativeWindowNV = epoxy_eglQueryNativeWindowNV_global_rewrite_ptr;

PUBLIC PFNEGLQUERYSTREAMKHRPROC epoxy_eglQueryStreamKHR = epoxy_eglQueryStreamKHR_global_rewrite_ptr;

PUBLIC PFNEGLQUERYSTREAMTIMEKHRPROC epoxy_eglQueryStreamTimeKHR = epoxy_eglQueryStreamTimeKHR_global_rewrite_ptr;

PUBLIC PFNEGLQUERYSTREAMU64KHRPROC epoxy_eglQueryStreamu64KHR = epoxy_eglQueryStreamu64KHR_global_rewrite_ptr;

PUBLIC PFNEGLQUERYSTRINGPROC epoxy_eglQueryString = epoxy_eglQueryString_global_rewrite_ptr;

PUBLIC PFNEGLQUERYSURFACEPROC epoxy_eglQuerySurface = epoxy_eglQuerySurface_global_rewrite_ptr;

PUBLIC PFNEGLQUERYSURFACE64KHRPROC epoxy_eglQuerySurface64KHR = epoxy_eglQuerySurface64KHR_global_rewrite_ptr;

PUBLIC PFNEGLQUERYSURFACEPOINTERANGLEPROC epoxy_eglQuerySurfacePointerANGLE = epoxy_eglQuerySurfacePointerANGLE_global_rewrite_ptr;

PUBLIC PFNEGLRELEASETEXIMAGEPROC epoxy_eglReleaseTexImage = epoxy_eglReleaseTexImage_global_rewrite_ptr;

PUBLIC PFNEGLRELEASETHREADPROC epoxy_eglReleaseThread = epoxy_eglReleaseThread_global_rewrite_ptr;

PUBLIC PFNEGLSETBLOBCACHEFUNCSANDROIDPROC epoxy_eglSetBlobCacheFuncsANDROID = epoxy_eglSetBlobCacheFuncsANDROID_global_rewrite_ptr;

PUBLIC PFNEGLSIGNALSYNCKHRPROC epoxy_eglSignalSyncKHR = epoxy_eglSignalSyncKHR_global_rewrite_ptr;

PUBLIC PFNEGLSIGNALSYNCNVPROC epoxy_eglSignalSyncNV = epoxy_eglSignalSyncNV_global_rewrite_ptr;

PUBLIC PFNEGLSTREAMATTRIBKHRPROC epoxy_eglStreamAttribKHR = epoxy_eglStreamAttribKHR_global_rewrite_ptr;

PUBLIC PFNEGLSTREAMCONSUMERACQUIREKHRPROC epoxy_eglStreamConsumerAcquireKHR = epoxy_eglStreamConsumerAcquireKHR_global_rewrite_ptr;

PUBLIC PFNEGLSTREAMCONSUMERGLTEXTUREEXTERNALKHRPROC epoxy_eglStreamConsumerGLTextureExternalKHR = epoxy_eglStreamConsumerGLTextureExternalKHR_global_rewrite_ptr;

PUBLIC PFNEGLSTREAMCONSUMERRELEASEKHRPROC epoxy_eglStreamConsumerReleaseKHR = epoxy_eglStreamConsumerReleaseKHR_global_rewrite_ptr;

PUBLIC PFNEGLSURFACEATTRIBPROC epoxy_eglSurfaceAttrib = epoxy_eglSurfaceAttrib_global_rewrite_ptr;

PUBLIC PFNEGLSWAPBUFFERSPROC epoxy_eglSwapBuffers = epoxy_eglSwapBuffers_global_rewrite_ptr;

PUBLIC PFNEGLSWAPBUFFERSREGION2NOKPROC epoxy_eglSwapBuffersRegion2NOK = epoxy_eglSwapBuffersRegion2NOK_global_rewrite_ptr;

PUBLIC PFNEGLSWAPBUFFERSREGIONNOKPROC epoxy_eglSwapBuffersRegionNOK = epoxy_eglSwapBuffersRegionNOK_global_rewrite_ptr;

PUBLIC PFNEGLSWAPBUFFERSWITHDAMAGEEXTPROC epoxy_eglSwapBuffersWithDamageEXT = epoxy_eglSwapBuffersWithDamageEXT_global_rewrite_ptr;

PUBLIC PFNEGLSWAPINTERVALPROC epoxy_eglSwapInterval = epoxy_eglSwapInterval_global_rewrite_ptr;

PUBLIC PFNEGLTERMINATEPROC epoxy_eglTerminate = epoxy_eglTerminate_global_rewrite_ptr;

PUBLIC PFNEGLUNLOCKSURFACEKHRPROC epoxy_eglUnlockSurfaceKHR = epoxy_eglUnlockSurfaceKHR_global_rewrite_ptr;

PUBLIC PFNEGLWAITCLIENTPROC epoxy_eglWaitClient = epoxy_eglWaitClient_global_rewrite_ptr;

PUBLIC PFNEGLWAITGLPROC epoxy_eglWaitGL = epoxy_eglWaitGL_global_rewrite_ptr;

PUBLIC PFNEGLWAITNATIVEPROC epoxy_eglWaitNative = epoxy_eglWaitNative_global_rewrite_ptr;

PUBLIC PFNEGLWAITSYNCPROC epoxy_eglWaitSync = epoxy_eglWaitSync_global_rewrite_ptr;

PUBLIC PFNEGLWAITSYNCKHRPROC epoxy_eglWaitSyncKHR = epoxy_eglWaitSyncKHR_global_rewrite_ptr;

