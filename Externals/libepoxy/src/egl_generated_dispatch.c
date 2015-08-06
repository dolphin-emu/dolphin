/* GL dispatch code.
 * This is code-generated from the GL API XML files from Khronos.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "dispatch_common.h"
#include "epoxy/egl.h"

#ifdef __GNUC__
#define EPOXY_NOINLINE __attribute__((noinline))
#elif defined (_MSC_VER)
#define EPOXY_NOINLINE __declspec(noinline)
#endif
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
    PFNEGLCREATEIMAGEPROC epoxy_eglCreateImage;
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
    PFNEGLDESTROYIMAGEPROC epoxy_eglDestroyImage;
    PFNEGLDESTROYIMAGEKHRPROC epoxy_eglDestroyImageKHR;
    PFNEGLDESTROYSTREAMKHRPROC epoxy_eglDestroyStreamKHR;
    PFNEGLDESTROYSURFACEPROC epoxy_eglDestroySurface;
    PFNEGLDESTROYSYNCPROC epoxy_eglDestroySync;
    PFNEGLDESTROYSYNCKHRPROC epoxy_eglDestroySyncKHR;
    PFNEGLDESTROYSYNCNVPROC epoxy_eglDestroySyncNV;
    PFNEGLDUPNATIVEFENCEFDANDROIDPROC epoxy_eglDupNativeFenceFDANDROID;
    PFNEGLEXPORTDMABUFIMAGEMESAPROC epoxy_eglExportDMABUFImageMESA;
    PFNEGLEXPORTDMABUFIMAGEQUERYMESAPROC epoxy_eglExportDMABUFImageQueryMESA;
    PFNEGLEXPORTDRMIMAGEMESAPROC epoxy_eglExportDRMImageMESA;
    PFNEGLFENCENVPROC epoxy_eglFenceNV;
    PFNEGLGETCONFIGATTRIBPROC epoxy_eglGetConfigAttrib;
    PFNEGLGETCONFIGSPROC epoxy_eglGetConfigs;
    PFNEGLGETCURRENTCONTEXTPROC epoxy_eglGetCurrentContext;
    PFNEGLGETCURRENTDISPLAYPROC epoxy_eglGetCurrentDisplay;
    PFNEGLGETCURRENTSURFACEPROC epoxy_eglGetCurrentSurface;
    PFNEGLGETDISPLAYPROC epoxy_eglGetDisplay;
    PFNEGLGETERRORPROC epoxy_eglGetError;
    PFNEGLGETOUTPUTLAYERSEXTPROC epoxy_eglGetOutputLayersEXT;
    PFNEGLGETOUTPUTPORTSEXTPROC epoxy_eglGetOutputPortsEXT;
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
    PFNEGLOUTPUTLAYERATTRIBEXTPROC epoxy_eglOutputLayerAttribEXT;
    PFNEGLOUTPUTPORTATTRIBEXTPROC epoxy_eglOutputPortAttribEXT;
    PFNEGLPOSTSUBBUFFERNVPROC epoxy_eglPostSubBufferNV;
    PFNEGLQUERYAPIPROC epoxy_eglQueryAPI;
    PFNEGLQUERYCONTEXTPROC epoxy_eglQueryContext;
    PFNEGLQUERYDEVICEATTRIBEXTPROC epoxy_eglQueryDeviceAttribEXT;
    PFNEGLQUERYDEVICESTRINGEXTPROC epoxy_eglQueryDeviceStringEXT;
    PFNEGLQUERYDEVICESEXTPROC epoxy_eglQueryDevicesEXT;
    PFNEGLQUERYDISPLAYATTRIBEXTPROC epoxy_eglQueryDisplayAttribEXT;
    PFNEGLQUERYNATIVEDISPLAYNVPROC epoxy_eglQueryNativeDisplayNV;
    PFNEGLQUERYNATIVEPIXMAPNVPROC epoxy_eglQueryNativePixmapNV;
    PFNEGLQUERYNATIVEWINDOWNVPROC epoxy_eglQueryNativeWindowNV;
    PFNEGLQUERYOUTPUTLAYERATTRIBEXTPROC epoxy_eglQueryOutputLayerAttribEXT;
    PFNEGLQUERYOUTPUTLAYERSTRINGEXTPROC epoxy_eglQueryOutputLayerStringEXT;
    PFNEGLQUERYOUTPUTPORTATTRIBEXTPROC epoxy_eglQueryOutputPortAttribEXT;
    PFNEGLQUERYOUTPUTPORTSTRINGEXTPROC epoxy_eglQueryOutputPortStringEXT;
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
    PFNEGLSETDAMAGEREGIONKHRPROC epoxy_eglSetDamageRegionKHR;
    PFNEGLSIGNALSYNCKHRPROC epoxy_eglSignalSyncKHR;
    PFNEGLSIGNALSYNCNVPROC epoxy_eglSignalSyncNV;
    PFNEGLSTREAMATTRIBKHRPROC epoxy_eglStreamAttribKHR;
    PFNEGLSTREAMCONSUMERACQUIREKHRPROC epoxy_eglStreamConsumerAcquireKHR;
    PFNEGLSTREAMCONSUMERGLTEXTUREEXTERNALKHRPROC epoxy_eglStreamConsumerGLTextureExternalKHR;
    PFNEGLSTREAMCONSUMEROUTPUTEXTPROC epoxy_eglStreamConsumerOutputEXT;
    PFNEGLSTREAMCONSUMERRELEASEKHRPROC epoxy_eglStreamConsumerReleaseKHR;
    PFNEGLSURFACEATTRIBPROC epoxy_eglSurfaceAttrib;
    PFNEGLSWAPBUFFERSPROC epoxy_eglSwapBuffers;
    PFNEGLSWAPBUFFERSREGION2NOKPROC epoxy_eglSwapBuffersRegion2NOK;
    PFNEGLSWAPBUFFERSREGIONNOKPROC epoxy_eglSwapBuffersRegionNOK;
    PFNEGLSWAPBUFFERSWITHDAMAGEEXTPROC epoxy_eglSwapBuffersWithDamageEXT;
    PFNEGLSWAPBUFFERSWITHDAMAGEKHRPROC epoxy_eglSwapBuffersWithDamageKHR;
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
    EGL_extension_EGL_EXT_device_base,
    EGL_extension_EGL_EXT_device_enumeration,
    EGL_extension_EGL_EXT_device_query,
    EGL_extension_EGL_EXT_output_base,
    EGL_extension_EGL_EXT_platform_base,
    EGL_extension_EGL_EXT_stream_consumer_egloutput,
    EGL_extension_EGL_EXT_swap_buffers_with_damage,
    EGL_extension_EGL_HI_clientpixmap,
    EGL_extension_EGL_KHR_cl_event2,
    EGL_extension_EGL_KHR_fence_sync,
    EGL_extension_EGL_KHR_image,
    EGL_extension_EGL_KHR_image_base,
    EGL_extension_EGL_KHR_lock_surface3,
    EGL_extension_EGL_KHR_lock_surface,
    EGL_extension_EGL_KHR_partial_update,
    EGL_extension_EGL_KHR_reusable_sync,
    EGL_extension_EGL_KHR_stream,
    EGL_extension_EGL_KHR_stream_consumer_gltexture,
    EGL_extension_EGL_KHR_stream_cross_process_fd,
    EGL_extension_EGL_KHR_stream_fifo,
    EGL_extension_EGL_KHR_stream_producer_eglsurface,
    EGL_extension_EGL_KHR_swap_buffers_with_damage,
    EGL_extension_EGL_KHR_wait_sync,
    EGL_extension_EGL_MESA_drm_image,
    EGL_extension_EGL_MESA_image_dma_buf_export,
    EGL_extension_EGL_NOK_swap_region2,
    EGL_extension_EGL_NOK_swap_region,
    EGL_extension_EGL_NV_native_query,
    EGL_extension_EGL_NV_post_sub_buffer,
    EGL_extension_EGL_NV_stream_sync,
    EGL_extension_EGL_NV_sync,
    EGL_extension_EGL_NV_system_time,
} PACKED;

static const char *enum_string =
    "EGL 10\0"
    "EGL 11\0"
    "EGL 12\0"
    "EGL 14\0"
    "EGL 15\0"
    "EGL extension \"EGL_ANDROID_blob_cache\"\0"
    "EGL extension \"EGL_ANDROID_native_fence_sync\"\0"
    "EGL extension \"EGL_ANGLE_query_surface_pointer\"\0"
    "EGL extension \"EGL_EXT_device_base\"\0"
    "EGL extension \"EGL_EXT_device_enumeration\"\0"
    "EGL extension \"EGL_EXT_device_query\"\0"
    "EGL extension \"EGL_EXT_output_base\"\0"
    "EGL extension \"EGL_EXT_platform_base\"\0"
    "EGL extension \"EGL_EXT_stream_consumer_egloutput\"\0"
    "EGL extension \"EGL_EXT_swap_buffers_with_damage\"\0"
    "EGL extension \"EGL_HI_clientpixmap\"\0"
    "EGL extension \"EGL_KHR_cl_event2\"\0"
    "EGL extension \"EGL_KHR_fence_sync\"\0"
    "EGL extension \"EGL_KHR_image\"\0"
    "EGL extension \"EGL_KHR_image_base\"\0"
    "EGL extension \"EGL_KHR_lock_surface3\"\0"
    "EGL extension \"EGL_KHR_lock_surface\"\0"
    "EGL extension \"EGL_KHR_partial_update\"\0"
    "EGL extension \"EGL_KHR_reusable_sync\"\0"
    "EGL extension \"EGL_KHR_stream\"\0"
    "EGL extension \"EGL_KHR_stream_consumer_gltexture\"\0"
    "EGL extension \"EGL_KHR_stream_cross_process_fd\"\0"
    "EGL extension \"EGL_KHR_stream_fifo\"\0"
    "EGL extension \"EGL_KHR_stream_producer_eglsurface\"\0"
    "EGL extension \"EGL_KHR_swap_buffers_with_damage\"\0"
    "EGL extension \"EGL_KHR_wait_sync\"\0"
    "EGL extension \"EGL_MESA_drm_image\"\0"
    "EGL extension \"EGL_MESA_image_dma_buf_export\"\0"
    "EGL extension \"EGL_NOK_swap_region2\"\0"
    "EGL extension \"EGL_NOK_swap_region\"\0"
    "EGL extension \"EGL_NV_native_query\"\0"
    "EGL extension \"EGL_NV_post_sub_buffer\"\0"
    "EGL extension \"EGL_NV_stream_sync\"\0"
    "EGL extension \"EGL_NV_sync\"\0"
    "EGL extension \"EGL_NV_system_time\"\0"
     ;

static const uint16_t enum_string_offsets[] = {
    [EGL_10] = 0,
    [EGL_11] = 7,
    [EGL_12] = 14,
    [EGL_14] = 21,
    [EGL_15] = 28,
    [EGL_extension_EGL_ANDROID_blob_cache] = 35,
    [EGL_extension_EGL_ANDROID_native_fence_sync] = 74,
    [EGL_extension_EGL_ANGLE_query_surface_pointer] = 120,
    [EGL_extension_EGL_EXT_device_base] = 168,
    [EGL_extension_EGL_EXT_device_enumeration] = 204,
    [EGL_extension_EGL_EXT_device_query] = 247,
    [EGL_extension_EGL_EXT_output_base] = 284,
    [EGL_extension_EGL_EXT_platform_base] = 320,
    [EGL_extension_EGL_EXT_stream_consumer_egloutput] = 358,
    [EGL_extension_EGL_EXT_swap_buffers_with_damage] = 408,
    [EGL_extension_EGL_HI_clientpixmap] = 457,
    [EGL_extension_EGL_KHR_cl_event2] = 493,
    [EGL_extension_EGL_KHR_fence_sync] = 527,
    [EGL_extension_EGL_KHR_image] = 562,
    [EGL_extension_EGL_KHR_image_base] = 592,
    [EGL_extension_EGL_KHR_lock_surface3] = 627,
    [EGL_extension_EGL_KHR_lock_surface] = 665,
    [EGL_extension_EGL_KHR_partial_update] = 702,
    [EGL_extension_EGL_KHR_reusable_sync] = 741,
    [EGL_extension_EGL_KHR_stream] = 779,
    [EGL_extension_EGL_KHR_stream_consumer_gltexture] = 810,
    [EGL_extension_EGL_KHR_stream_cross_process_fd] = 860,
    [EGL_extension_EGL_KHR_stream_fifo] = 908,
    [EGL_extension_EGL_KHR_stream_producer_eglsurface] = 944,
    [EGL_extension_EGL_KHR_swap_buffers_with_damage] = 995,
    [EGL_extension_EGL_KHR_wait_sync] = 1044,
    [EGL_extension_EGL_MESA_drm_image] = 1078,
    [EGL_extension_EGL_MESA_image_dma_buf_export] = 1113,
    [EGL_extension_EGL_NOK_swap_region2] = 1159,
    [EGL_extension_EGL_NOK_swap_region] = 1196,
    [EGL_extension_EGL_NV_native_query] = 1232,
    [EGL_extension_EGL_NV_post_sub_buffer] = 1268,
    [EGL_extension_EGL_NV_stream_sync] = 1307,
    [EGL_extension_EGL_NV_sync] = 1342,
    [EGL_extension_EGL_NV_system_time] = 1370,
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
   "eglCreateImage\0"
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
   "eglDestroyImage\0"
   "eglDestroyImageKHR\0"
   "eglDestroyStreamKHR\0"
   "eglDestroySurface\0"
   "eglDestroySync\0"
   "eglDestroySyncKHR\0"
   "eglDestroySyncNV\0"
   "eglDupNativeFenceFDANDROID\0"
   "eglExportDMABUFImageMESA\0"
   "eglExportDMABUFImageQueryMESA\0"
   "eglExportDRMImageMESA\0"
   "eglFenceNV\0"
   "eglGetConfigAttrib\0"
   "eglGetConfigs\0"
   "eglGetCurrentContext\0"
   "eglGetCurrentDisplay\0"
   "eglGetCurrentSurface\0"
   "eglGetDisplay\0"
   "eglGetError\0"
   "eglGetOutputLayersEXT\0"
   "eglGetOutputPortsEXT\0"
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
   "eglOutputLayerAttribEXT\0"
   "eglOutputPortAttribEXT\0"
   "eglPostSubBufferNV\0"
   "eglQueryAPI\0"
   "eglQueryContext\0"
   "eglQueryDeviceAttribEXT\0"
   "eglQueryDeviceStringEXT\0"
   "eglQueryDevicesEXT\0"
   "eglQueryDisplayAttribEXT\0"
   "eglQueryNativeDisplayNV\0"
   "eglQueryNativePixmapNV\0"
   "eglQueryNativeWindowNV\0"
   "eglQueryOutputLayerAttribEXT\0"
   "eglQueryOutputLayerStringEXT\0"
   "eglQueryOutputPortAttribEXT\0"
   "eglQueryOutputPortStringEXT\0"
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
   "eglSetDamageRegionKHR\0"
   "eglSignalSyncKHR\0"
   "eglSignalSyncNV\0"
   "eglStreamAttribKHR\0"
   "eglStreamConsumerAcquireKHR\0"
   "eglStreamConsumerGLTextureExternalKHR\0"
   "eglStreamConsumerOutputEXT\0"
   "eglStreamConsumerReleaseKHR\0"
   "eglSurfaceAttrib\0"
   "eglSwapBuffers\0"
   "eglSwapBuffersRegion2NOK\0"
   "eglSwapBuffersRegionNOK\0"
   "eglSwapBuffersWithDamageEXT\0"
   "eglSwapBuffersWithDamageKHR\0"
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
        case EGL_extension_EGL_EXT_device_base:
            if (epoxy_conservative_has_egl_extension("EGL_EXT_device_base"))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case EGL_extension_EGL_EXT_device_enumeration:
            if (epoxy_conservative_has_egl_extension("EGL_EXT_device_enumeration"))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case EGL_extension_EGL_EXT_device_query:
            if (epoxy_conservative_has_egl_extension("EGL_EXT_device_query"))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case EGL_extension_EGL_EXT_output_base:
            if (epoxy_conservative_has_egl_extension("EGL_EXT_output_base"))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case EGL_extension_EGL_EXT_platform_base:
            if (epoxy_conservative_has_egl_extension("EGL_EXT_platform_base"))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case EGL_extension_EGL_EXT_stream_consumer_egloutput:
            if (epoxy_conservative_has_egl_extension("EGL_EXT_stream_consumer_egloutput"))
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
        case EGL_extension_EGL_KHR_fence_sync:
            if (epoxy_conservative_has_egl_extension("EGL_KHR_fence_sync"))
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
        case EGL_extension_EGL_KHR_partial_update:
            if (epoxy_conservative_has_egl_extension("EGL_KHR_partial_update"))
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
        case EGL_extension_EGL_KHR_swap_buffers_with_damage:
            if (epoxy_conservative_has_egl_extension("EGL_KHR_swap_buffers_with_damage"))
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
        case EGL_extension_EGL_MESA_image_dma_buf_export:
            if (epoxy_conservative_has_egl_extension("EGL_MESA_image_dma_buf_export"))
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

    fprintf(stderr, "No provider of %s found.  Requires one of:\n", name);
    for (i = 0; providers[i] != egl_provider_terminator; i++) {
        fprintf(stderr, "    %s\n", enum_string + enum_string_offsets[providers[i]]);
    }
    if (providers[0] == egl_provider_terminator) {
        fprintf(stderr, "    No known providers.  This is likely a bug "
                        "in libepoxy code generation\n");
    }
    abort();
}

EPOXY_NOINLINE static void *
egl_single_resolver(enum egl_provider provider, uint16_t entrypoint_offset);

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
        EGL_extension_EGL_KHR_fence_sync,
        EGL_extension_EGL_KHR_reusable_sync,
        egl_provider_terminator
    };
    static const uint16_t entrypoints[] = {
        43 /* "eglClientWaitSync" */,
        61 /* "eglClientWaitSyncKHR" */,
        61 /* "eglClientWaitSyncKHR" */,
    };
    return egl_provider_resolver(entrypoint_strings + 43 /* "eglClientWaitSync" */,
                                providers, entrypoints);
}

static PFNEGLCLIENTWAITSYNCKHRPROC
epoxy_eglClientWaitSyncKHR_resolver(void)
{
    static const enum egl_provider providers[] = {
        EGL_extension_EGL_KHR_fence_sync,
        EGL_extension_EGL_KHR_reusable_sync,
        EGL_15,
        egl_provider_terminator
    };
    static const uint16_t entrypoints[] = {
        61 /* "eglClientWaitSyncKHR" */,
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

static PFNEGLCREATEIMAGEPROC
epoxy_eglCreateImage_resolver(void)
{
    return egl_single_resolver(EGL_15, 177 /* eglCreateImage */);
}

static PFNEGLCREATEIMAGEKHRPROC
epoxy_eglCreateImageKHR_resolver(void)
{
    static const enum egl_provider providers[] = {
        EGL_extension_EGL_KHR_image,
        EGL_extension_EGL_KHR_image_base,
        egl_provider_terminator
    };
    static const uint16_t entrypoints[] = {
        192 /* "eglCreateImageKHR" */,
        192 /* "eglCreateImageKHR" */,
    };
    return egl_provider_resolver(entrypoint_strings + 192 /* "eglCreateImageKHR" */,
                                providers, entrypoints);
}

static PFNEGLCREATEPBUFFERFROMCLIENTBUFFERPROC
epoxy_eglCreatePbufferFromClientBuffer_resolver(void)
{
    return egl_single_resolver(EGL_12, 210 /* eglCreatePbufferFromClientBuffer */);
}

static PFNEGLCREATEPBUFFERSURFACEPROC
epoxy_eglCreatePbufferSurface_resolver(void)
{
    return egl_single_resolver(EGL_10, 243 /* eglCreatePbufferSurface */);
}

static PFNEGLCREATEPIXMAPSURFACEPROC
epoxy_eglCreatePixmapSurface_resolver(void)
{
    return egl_single_resolver(EGL_10, 267 /* eglCreatePixmapSurface */);
}

static PFNEGLCREATEPIXMAPSURFACEHIPROC
epoxy_eglCreatePixmapSurfaceHI_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_HI_clientpixmap, 290 /* eglCreatePixmapSurfaceHI */);
}

static PFNEGLCREATEPLATFORMPIXMAPSURFACEPROC
epoxy_eglCreatePlatformPixmapSurface_resolver(void)
{
    return egl_single_resolver(EGL_15, 315 /* eglCreatePlatformPixmapSurface */);
}

static PFNEGLCREATEPLATFORMPIXMAPSURFACEEXTPROC
epoxy_eglCreatePlatformPixmapSurfaceEXT_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_EXT_platform_base, 346 /* eglCreatePlatformPixmapSurfaceEXT */);
}

static PFNEGLCREATEPLATFORMWINDOWSURFACEPROC
epoxy_eglCreatePlatformWindowSurface_resolver(void)
{
    return egl_single_resolver(EGL_15, 380 /* eglCreatePlatformWindowSurface */);
}

static PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC
epoxy_eglCreatePlatformWindowSurfaceEXT_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_EXT_platform_base, 411 /* eglCreatePlatformWindowSurfaceEXT */);
}

static PFNEGLCREATESTREAMFROMFILEDESCRIPTORKHRPROC
epoxy_eglCreateStreamFromFileDescriptorKHR_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_KHR_stream_cross_process_fd, 445 /* eglCreateStreamFromFileDescriptorKHR */);
}

static PFNEGLCREATESTREAMKHRPROC
epoxy_eglCreateStreamKHR_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_KHR_stream, 482 /* eglCreateStreamKHR */);
}

static PFNEGLCREATESTREAMPRODUCERSURFACEKHRPROC
epoxy_eglCreateStreamProducerSurfaceKHR_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_KHR_stream_producer_eglsurface, 501 /* eglCreateStreamProducerSurfaceKHR */);
}

static PFNEGLCREATESTREAMSYNCNVPROC
epoxy_eglCreateStreamSyncNV_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_NV_stream_sync, 535 /* eglCreateStreamSyncNV */);
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
        557 /* "eglCreateSync" */,
        571 /* "eglCreateSync64KHR" */,
    };
    return egl_provider_resolver(entrypoint_strings + 557 /* "eglCreateSync" */,
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
        571 /* "eglCreateSync64KHR" */,
        557 /* "eglCreateSync" */,
    };
    return egl_provider_resolver(entrypoint_strings + 571 /* "eglCreateSync64KHR" */,
                                providers, entrypoints);
}

static PFNEGLCREATESYNCKHRPROC
epoxy_eglCreateSyncKHR_resolver(void)
{
    static const enum egl_provider providers[] = {
        EGL_extension_EGL_KHR_fence_sync,
        EGL_extension_EGL_KHR_reusable_sync,
        egl_provider_terminator
    };
    static const uint16_t entrypoints[] = {
        590 /* "eglCreateSyncKHR" */,
        590 /* "eglCreateSyncKHR" */,
    };
    return egl_provider_resolver(entrypoint_strings + 590 /* "eglCreateSyncKHR" */,
                                providers, entrypoints);
}

static PFNEGLCREATEWINDOWSURFACEPROC
epoxy_eglCreateWindowSurface_resolver(void)
{
    return egl_single_resolver(EGL_10, 607 /* eglCreateWindowSurface */);
}

static PFNEGLDESTROYCONTEXTPROC
epoxy_eglDestroyContext_resolver(void)
{
    return egl_single_resolver(EGL_10, 630 /* eglDestroyContext */);
}

static PFNEGLDESTROYIMAGEPROC
epoxy_eglDestroyImage_resolver(void)
{
    static const enum egl_provider providers[] = {
        EGL_15,
        EGL_extension_EGL_KHR_image,
        EGL_extension_EGL_KHR_image_base,
        egl_provider_terminator
    };
    static const uint16_t entrypoints[] = {
        648 /* "eglDestroyImage" */,
        664 /* "eglDestroyImageKHR" */,
        664 /* "eglDestroyImageKHR" */,
    };
    return egl_provider_resolver(entrypoint_strings + 648 /* "eglDestroyImage" */,
                                providers, entrypoints);
}

static PFNEGLDESTROYIMAGEKHRPROC
epoxy_eglDestroyImageKHR_resolver(void)
{
    static const enum egl_provider providers[] = {
        EGL_extension_EGL_KHR_image,
        EGL_extension_EGL_KHR_image_base,
        EGL_15,
        egl_provider_terminator
    };
    static const uint16_t entrypoints[] = {
        664 /* "eglDestroyImageKHR" */,
        664 /* "eglDestroyImageKHR" */,
        648 /* "eglDestroyImage" */,
    };
    return egl_provider_resolver(entrypoint_strings + 664 /* "eglDestroyImageKHR" */,
                                providers, entrypoints);
}

static PFNEGLDESTROYSTREAMKHRPROC
epoxy_eglDestroyStreamKHR_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_KHR_stream, 683 /* eglDestroyStreamKHR */);
}

static PFNEGLDESTROYSURFACEPROC
epoxy_eglDestroySurface_resolver(void)
{
    return egl_single_resolver(EGL_10, 703 /* eglDestroySurface */);
}

static PFNEGLDESTROYSYNCPROC
epoxy_eglDestroySync_resolver(void)
{
    static const enum egl_provider providers[] = {
        EGL_15,
        EGL_extension_EGL_KHR_fence_sync,
        EGL_extension_EGL_KHR_reusable_sync,
        egl_provider_terminator
    };
    static const uint16_t entrypoints[] = {
        721 /* "eglDestroySync" */,
        736 /* "eglDestroySyncKHR" */,
        736 /* "eglDestroySyncKHR" */,
    };
    return egl_provider_resolver(entrypoint_strings + 721 /* "eglDestroySync" */,
                                providers, entrypoints);
}

static PFNEGLDESTROYSYNCKHRPROC
epoxy_eglDestroySyncKHR_resolver(void)
{
    static const enum egl_provider providers[] = {
        EGL_extension_EGL_KHR_fence_sync,
        EGL_extension_EGL_KHR_reusable_sync,
        EGL_15,
        egl_provider_terminator
    };
    static const uint16_t entrypoints[] = {
        736 /* "eglDestroySyncKHR" */,
        736 /* "eglDestroySyncKHR" */,
        721 /* "eglDestroySync" */,
    };
    return egl_provider_resolver(entrypoint_strings + 736 /* "eglDestroySyncKHR" */,
                                providers, entrypoints);
}

static PFNEGLDESTROYSYNCNVPROC
epoxy_eglDestroySyncNV_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_NV_sync, 754 /* eglDestroySyncNV */);
}

static PFNEGLDUPNATIVEFENCEFDANDROIDPROC
epoxy_eglDupNativeFenceFDANDROID_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_ANDROID_native_fence_sync, 771 /* eglDupNativeFenceFDANDROID */);
}

static PFNEGLEXPORTDMABUFIMAGEMESAPROC
epoxy_eglExportDMABUFImageMESA_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_MESA_image_dma_buf_export, 798 /* eglExportDMABUFImageMESA */);
}

static PFNEGLEXPORTDMABUFIMAGEQUERYMESAPROC
epoxy_eglExportDMABUFImageQueryMESA_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_MESA_image_dma_buf_export, 823 /* eglExportDMABUFImageQueryMESA */);
}

static PFNEGLEXPORTDRMIMAGEMESAPROC
epoxy_eglExportDRMImageMESA_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_MESA_drm_image, 853 /* eglExportDRMImageMESA */);
}

static PFNEGLFENCENVPROC
epoxy_eglFenceNV_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_NV_sync, 875 /* eglFenceNV */);
}

static PFNEGLGETCONFIGATTRIBPROC
epoxy_eglGetConfigAttrib_resolver(void)
{
    return egl_single_resolver(EGL_10, 886 /* eglGetConfigAttrib */);
}

static PFNEGLGETCONFIGSPROC
epoxy_eglGetConfigs_resolver(void)
{
    return egl_single_resolver(EGL_10, 905 /* eglGetConfigs */);
}

static PFNEGLGETCURRENTCONTEXTPROC
epoxy_eglGetCurrentContext_resolver(void)
{
    return egl_single_resolver(EGL_14, 919 /* eglGetCurrentContext */);
}

static PFNEGLGETCURRENTDISPLAYPROC
epoxy_eglGetCurrentDisplay_resolver(void)
{
    return egl_single_resolver(EGL_10, 940 /* eglGetCurrentDisplay */);
}

static PFNEGLGETCURRENTSURFACEPROC
epoxy_eglGetCurrentSurface_resolver(void)
{
    return egl_single_resolver(EGL_10, 961 /* eglGetCurrentSurface */);
}

static PFNEGLGETDISPLAYPROC
epoxy_eglGetDisplay_resolver(void)
{
    return egl_single_resolver(EGL_10, 982 /* eglGetDisplay */);
}

static PFNEGLGETERRORPROC
epoxy_eglGetError_resolver(void)
{
    return egl_single_resolver(EGL_10, 996 /* eglGetError */);
}

static PFNEGLGETOUTPUTLAYERSEXTPROC
epoxy_eglGetOutputLayersEXT_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_EXT_output_base, 1008 /* eglGetOutputLayersEXT */);
}

static PFNEGLGETOUTPUTPORTSEXTPROC
epoxy_eglGetOutputPortsEXT_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_EXT_output_base, 1030 /* eglGetOutputPortsEXT */);
}

static PFNEGLGETPLATFORMDISPLAYPROC
epoxy_eglGetPlatformDisplay_resolver(void)
{
    return egl_single_resolver(EGL_15, 1051 /* eglGetPlatformDisplay */);
}

static PFNEGLGETPLATFORMDISPLAYEXTPROC
epoxy_eglGetPlatformDisplayEXT_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_EXT_platform_base, 1073 /* eglGetPlatformDisplayEXT */);
}

static PFNEGLGETPROCADDRESSPROC
epoxy_eglGetProcAddress_resolver(void)
{
    return egl_single_resolver(EGL_10, 1098 /* eglGetProcAddress */);
}

static PFNEGLGETSTREAMFILEDESCRIPTORKHRPROC
epoxy_eglGetStreamFileDescriptorKHR_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_KHR_stream_cross_process_fd, 1116 /* eglGetStreamFileDescriptorKHR */);
}

static PFNEGLGETSYNCATTRIBPROC
epoxy_eglGetSyncAttrib_resolver(void)
{
    return egl_single_resolver(EGL_15, 1146 /* eglGetSyncAttrib */);
}

static PFNEGLGETSYNCATTRIBKHRPROC
epoxy_eglGetSyncAttribKHR_resolver(void)
{
    static const enum egl_provider providers[] = {
        EGL_extension_EGL_KHR_fence_sync,
        EGL_extension_EGL_KHR_reusable_sync,
        egl_provider_terminator
    };
    static const uint16_t entrypoints[] = {
        1163 /* "eglGetSyncAttribKHR" */,
        1163 /* "eglGetSyncAttribKHR" */,
    };
    return egl_provider_resolver(entrypoint_strings + 1163 /* "eglGetSyncAttribKHR" */,
                                providers, entrypoints);
}

static PFNEGLGETSYNCATTRIBNVPROC
epoxy_eglGetSyncAttribNV_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_NV_sync, 1183 /* eglGetSyncAttribNV */);
}

static PFNEGLGETSYSTEMTIMEFREQUENCYNVPROC
epoxy_eglGetSystemTimeFrequencyNV_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_NV_system_time, 1202 /* eglGetSystemTimeFrequencyNV */);
}

static PFNEGLGETSYSTEMTIMENVPROC
epoxy_eglGetSystemTimeNV_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_NV_system_time, 1230 /* eglGetSystemTimeNV */);
}

static PFNEGLINITIALIZEPROC
epoxy_eglInitialize_resolver(void)
{
    return egl_single_resolver(EGL_10, 1249 /* eglInitialize */);
}

static PFNEGLLOCKSURFACEKHRPROC
epoxy_eglLockSurfaceKHR_resolver(void)
{
    static const enum egl_provider providers[] = {
        EGL_extension_EGL_KHR_lock_surface,
        EGL_extension_EGL_KHR_lock_surface3,
        egl_provider_terminator
    };
    static const uint16_t entrypoints[] = {
        1263 /* "eglLockSurfaceKHR" */,
        1263 /* "eglLockSurfaceKHR" */,
    };
    return egl_provider_resolver(entrypoint_strings + 1263 /* "eglLockSurfaceKHR" */,
                                providers, entrypoints);
}

static PFNEGLMAKECURRENTPROC
epoxy_eglMakeCurrent_resolver(void)
{
    return egl_single_resolver(EGL_10, 1281 /* eglMakeCurrent */);
}

static PFNEGLOUTPUTLAYERATTRIBEXTPROC
epoxy_eglOutputLayerAttribEXT_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_EXT_output_base, 1296 /* eglOutputLayerAttribEXT */);
}

static PFNEGLOUTPUTPORTATTRIBEXTPROC
epoxy_eglOutputPortAttribEXT_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_EXT_output_base, 1320 /* eglOutputPortAttribEXT */);
}

static PFNEGLPOSTSUBBUFFERNVPROC
epoxy_eglPostSubBufferNV_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_NV_post_sub_buffer, 1343 /* eglPostSubBufferNV */);
}

static PFNEGLQUERYAPIPROC
epoxy_eglQueryAPI_resolver(void)
{
    return egl_single_resolver(EGL_12, 1362 /* eglQueryAPI */);
}

static PFNEGLQUERYCONTEXTPROC
epoxy_eglQueryContext_resolver(void)
{
    return egl_single_resolver(EGL_10, 1374 /* eglQueryContext */);
}

static PFNEGLQUERYDEVICEATTRIBEXTPROC
epoxy_eglQueryDeviceAttribEXT_resolver(void)
{
    static const enum egl_provider providers[] = {
        EGL_extension_EGL_EXT_device_base,
        EGL_extension_EGL_EXT_device_query,
        egl_provider_terminator
    };
    static const uint16_t entrypoints[] = {
        1390 /* "eglQueryDeviceAttribEXT" */,
        1390 /* "eglQueryDeviceAttribEXT" */,
    };
    return egl_provider_resolver(entrypoint_strings + 1390 /* "eglQueryDeviceAttribEXT" */,
                                providers, entrypoints);
}

static PFNEGLQUERYDEVICESTRINGEXTPROC
epoxy_eglQueryDeviceStringEXT_resolver(void)
{
    static const enum egl_provider providers[] = {
        EGL_extension_EGL_EXT_device_base,
        EGL_extension_EGL_EXT_device_query,
        egl_provider_terminator
    };
    static const uint16_t entrypoints[] = {
        1414 /* "eglQueryDeviceStringEXT" */,
        1414 /* "eglQueryDeviceStringEXT" */,
    };
    return egl_provider_resolver(entrypoint_strings + 1414 /* "eglQueryDeviceStringEXT" */,
                                providers, entrypoints);
}

static PFNEGLQUERYDEVICESEXTPROC
epoxy_eglQueryDevicesEXT_resolver(void)
{
    static const enum egl_provider providers[] = {
        EGL_extension_EGL_EXT_device_base,
        EGL_extension_EGL_EXT_device_enumeration,
        egl_provider_terminator
    };
    static const uint16_t entrypoints[] = {
        1438 /* "eglQueryDevicesEXT" */,
        1438 /* "eglQueryDevicesEXT" */,
    };
    return egl_provider_resolver(entrypoint_strings + 1438 /* "eglQueryDevicesEXT" */,
                                providers, entrypoints);
}

static PFNEGLQUERYDISPLAYATTRIBEXTPROC
epoxy_eglQueryDisplayAttribEXT_resolver(void)
{
    static const enum egl_provider providers[] = {
        EGL_extension_EGL_EXT_device_base,
        EGL_extension_EGL_EXT_device_query,
        egl_provider_terminator
    };
    static const uint16_t entrypoints[] = {
        1457 /* "eglQueryDisplayAttribEXT" */,
        1457 /* "eglQueryDisplayAttribEXT" */,
    };
    return egl_provider_resolver(entrypoint_strings + 1457 /* "eglQueryDisplayAttribEXT" */,
                                providers, entrypoints);
}

static PFNEGLQUERYNATIVEDISPLAYNVPROC
epoxy_eglQueryNativeDisplayNV_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_NV_native_query, 1482 /* eglQueryNativeDisplayNV */);
}

static PFNEGLQUERYNATIVEPIXMAPNVPROC
epoxy_eglQueryNativePixmapNV_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_NV_native_query, 1506 /* eglQueryNativePixmapNV */);
}

static PFNEGLQUERYNATIVEWINDOWNVPROC
epoxy_eglQueryNativeWindowNV_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_NV_native_query, 1529 /* eglQueryNativeWindowNV */);
}

static PFNEGLQUERYOUTPUTLAYERATTRIBEXTPROC
epoxy_eglQueryOutputLayerAttribEXT_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_EXT_output_base, 1552 /* eglQueryOutputLayerAttribEXT */);
}

static PFNEGLQUERYOUTPUTLAYERSTRINGEXTPROC
epoxy_eglQueryOutputLayerStringEXT_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_EXT_output_base, 1581 /* eglQueryOutputLayerStringEXT */);
}

static PFNEGLQUERYOUTPUTPORTATTRIBEXTPROC
epoxy_eglQueryOutputPortAttribEXT_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_EXT_output_base, 1610 /* eglQueryOutputPortAttribEXT */);
}

static PFNEGLQUERYOUTPUTPORTSTRINGEXTPROC
epoxy_eglQueryOutputPortStringEXT_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_EXT_output_base, 1638 /* eglQueryOutputPortStringEXT */);
}

static PFNEGLQUERYSTREAMKHRPROC
epoxy_eglQueryStreamKHR_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_KHR_stream, 1666 /* eglQueryStreamKHR */);
}

static PFNEGLQUERYSTREAMTIMEKHRPROC
epoxy_eglQueryStreamTimeKHR_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_KHR_stream_fifo, 1684 /* eglQueryStreamTimeKHR */);
}

static PFNEGLQUERYSTREAMU64KHRPROC
epoxy_eglQueryStreamu64KHR_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_KHR_stream, 1706 /* eglQueryStreamu64KHR */);
}

static PFNEGLQUERYSTRINGPROC
epoxy_eglQueryString_resolver(void)
{
    return egl_single_resolver(EGL_10, 1727 /* eglQueryString */);
}

static PFNEGLQUERYSURFACEPROC
epoxy_eglQuerySurface_resolver(void)
{
    return egl_single_resolver(EGL_10, 1742 /* eglQuerySurface */);
}

static PFNEGLQUERYSURFACE64KHRPROC
epoxy_eglQuerySurface64KHR_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_KHR_lock_surface3, 1758 /* eglQuerySurface64KHR */);
}

static PFNEGLQUERYSURFACEPOINTERANGLEPROC
epoxy_eglQuerySurfacePointerANGLE_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_ANGLE_query_surface_pointer, 1779 /* eglQuerySurfacePointerANGLE */);
}

static PFNEGLRELEASETEXIMAGEPROC
epoxy_eglReleaseTexImage_resolver(void)
{
    return egl_single_resolver(EGL_11, 1807 /* eglReleaseTexImage */);
}

static PFNEGLRELEASETHREADPROC
epoxy_eglReleaseThread_resolver(void)
{
    return egl_single_resolver(EGL_12, 1826 /* eglReleaseThread */);
}

static PFNEGLSETBLOBCACHEFUNCSANDROIDPROC
epoxy_eglSetBlobCacheFuncsANDROID_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_ANDROID_blob_cache, 1843 /* eglSetBlobCacheFuncsANDROID */);
}

static PFNEGLSETDAMAGEREGIONKHRPROC
epoxy_eglSetDamageRegionKHR_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_KHR_partial_update, 1871 /* eglSetDamageRegionKHR */);
}

static PFNEGLSIGNALSYNCKHRPROC
epoxy_eglSignalSyncKHR_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_KHR_reusable_sync, 1893 /* eglSignalSyncKHR */);
}

static PFNEGLSIGNALSYNCNVPROC
epoxy_eglSignalSyncNV_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_NV_sync, 1910 /* eglSignalSyncNV */);
}

static PFNEGLSTREAMATTRIBKHRPROC
epoxy_eglStreamAttribKHR_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_KHR_stream, 1926 /* eglStreamAttribKHR */);
}

static PFNEGLSTREAMCONSUMERACQUIREKHRPROC
epoxy_eglStreamConsumerAcquireKHR_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_KHR_stream_consumer_gltexture, 1945 /* eglStreamConsumerAcquireKHR */);
}

static PFNEGLSTREAMCONSUMERGLTEXTUREEXTERNALKHRPROC
epoxy_eglStreamConsumerGLTextureExternalKHR_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_KHR_stream_consumer_gltexture, 1973 /* eglStreamConsumerGLTextureExternalKHR */);
}

static PFNEGLSTREAMCONSUMEROUTPUTEXTPROC
epoxy_eglStreamConsumerOutputEXT_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_EXT_stream_consumer_egloutput, 2011 /* eglStreamConsumerOutputEXT */);
}

static PFNEGLSTREAMCONSUMERRELEASEKHRPROC
epoxy_eglStreamConsumerReleaseKHR_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_KHR_stream_consumer_gltexture, 2038 /* eglStreamConsumerReleaseKHR */);
}

static PFNEGLSURFACEATTRIBPROC
epoxy_eglSurfaceAttrib_resolver(void)
{
    return egl_single_resolver(EGL_11, 2066 /* eglSurfaceAttrib */);
}

static PFNEGLSWAPBUFFERSPROC
epoxy_eglSwapBuffers_resolver(void)
{
    return egl_single_resolver(EGL_10, 2083 /* eglSwapBuffers */);
}

static PFNEGLSWAPBUFFERSREGION2NOKPROC
epoxy_eglSwapBuffersRegion2NOK_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_NOK_swap_region2, 2098 /* eglSwapBuffersRegion2NOK */);
}

static PFNEGLSWAPBUFFERSREGIONNOKPROC
epoxy_eglSwapBuffersRegionNOK_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_NOK_swap_region, 2123 /* eglSwapBuffersRegionNOK */);
}

static PFNEGLSWAPBUFFERSWITHDAMAGEEXTPROC
epoxy_eglSwapBuffersWithDamageEXT_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_EXT_swap_buffers_with_damage, 2147 /* eglSwapBuffersWithDamageEXT */);
}

static PFNEGLSWAPBUFFERSWITHDAMAGEKHRPROC
epoxy_eglSwapBuffersWithDamageKHR_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_KHR_swap_buffers_with_damage, 2175 /* eglSwapBuffersWithDamageKHR */);
}

static PFNEGLSWAPINTERVALPROC
epoxy_eglSwapInterval_resolver(void)
{
    return egl_single_resolver(EGL_11, 2203 /* eglSwapInterval */);
}

static PFNEGLTERMINATEPROC
epoxy_eglTerminate_resolver(void)
{
    return egl_single_resolver(EGL_10, 2219 /* eglTerminate */);
}

static PFNEGLUNLOCKSURFACEKHRPROC
epoxy_eglUnlockSurfaceKHR_resolver(void)
{
    static const enum egl_provider providers[] = {
        EGL_extension_EGL_KHR_lock_surface,
        EGL_extension_EGL_KHR_lock_surface3,
        egl_provider_terminator
    };
    static const uint16_t entrypoints[] = {
        2232 /* "eglUnlockSurfaceKHR" */,
        2232 /* "eglUnlockSurfaceKHR" */,
    };
    return egl_provider_resolver(entrypoint_strings + 2232 /* "eglUnlockSurfaceKHR" */,
                                providers, entrypoints);
}

static PFNEGLWAITCLIENTPROC
epoxy_eglWaitClient_resolver(void)
{
    return egl_single_resolver(EGL_12, 2252 /* eglWaitClient */);
}

static PFNEGLWAITGLPROC
epoxy_eglWaitGL_resolver(void)
{
    return egl_single_resolver(EGL_10, 2266 /* eglWaitGL */);
}

static PFNEGLWAITNATIVEPROC
epoxy_eglWaitNative_resolver(void)
{
    return egl_single_resolver(EGL_10, 2276 /* eglWaitNative */);
}

static PFNEGLWAITSYNCPROC
epoxy_eglWaitSync_resolver(void)
{
    return egl_single_resolver(EGL_15, 2290 /* eglWaitSync */);
}

static PFNEGLWAITSYNCKHRPROC
epoxy_eglWaitSyncKHR_resolver(void)
{
    return egl_single_resolver(EGL_extension_EGL_KHR_wait_sync, 2302 /* eglWaitSyncKHR */);
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
GEN_THUNKS_RET(EGLImage, eglCreateImage, (EGLDisplay dpy, EGLContext ctx, EGLenum target, EGLClientBuffer buffer, const EGLAttrib * attrib_list), (dpy, ctx, target, buffer, attrib_list))
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
GEN_THUNKS_RET(EGLBoolean, eglDestroyImage, (EGLDisplay dpy, EGLImage image), (dpy, image))
GEN_THUNKS_RET(EGLBoolean, eglDestroyImageKHR, (EGLDisplay dpy, EGLImageKHR image), (dpy, image))
GEN_THUNKS_RET(EGLBoolean, eglDestroyStreamKHR, (EGLDisplay dpy, EGLStreamKHR stream), (dpy, stream))
GEN_THUNKS_RET(EGLBoolean, eglDestroySurface, (EGLDisplay dpy, EGLSurface surface), (dpy, surface))
GEN_THUNKS_RET(EGLBoolean, eglDestroySync, (EGLDisplay dpy, EGLSync sync), (dpy, sync))
GEN_THUNKS_RET(EGLBoolean, eglDestroySyncKHR, (EGLDisplay dpy, EGLSyncKHR sync), (dpy, sync))
GEN_THUNKS_RET(EGLBoolean, eglDestroySyncNV, (EGLSyncNV sync), (sync))
GEN_THUNKS_RET(EGLint, eglDupNativeFenceFDANDROID, (EGLDisplay dpy, EGLSyncKHR sync), (dpy, sync))
GEN_THUNKS_RET(EGLBoolean, eglExportDMABUFImageMESA, (EGLDisplay dpy, EGLImageKHR image, int * fds, EGLint * strides, EGLint * offsets), (dpy, image, fds, strides, offsets))
GEN_THUNKS_RET(EGLBoolean, eglExportDMABUFImageQueryMESA, (EGLDisplay dpy, EGLImageKHR image, int * fourcc, int * num_planes, EGLuint64KHR * modifiers), (dpy, image, fourcc, num_planes, modifiers))
GEN_THUNKS_RET(EGLBoolean, eglExportDRMImageMESA, (EGLDisplay dpy, EGLImageKHR image, EGLint * name, EGLint * handle, EGLint * stride), (dpy, image, name, handle, stride))
GEN_THUNKS_RET(EGLBoolean, eglFenceNV, (EGLSyncNV sync), (sync))
GEN_THUNKS_RET(EGLBoolean, eglGetConfigAttrib, (EGLDisplay dpy, EGLConfig config, EGLint attribute, EGLint * value), (dpy, config, attribute, value))
GEN_THUNKS_RET(EGLBoolean, eglGetConfigs, (EGLDisplay dpy, EGLConfig * configs, EGLint config_size, EGLint * num_config), (dpy, configs, config_size, num_config))
GEN_THUNKS_RET(EGLContext, eglGetCurrentContext, (void), ())
GEN_THUNKS_RET(EGLDisplay, eglGetCurrentDisplay, (void), ())
GEN_THUNKS_RET(EGLSurface, eglGetCurrentSurface, (EGLint readdraw), (readdraw))
GEN_THUNKS_RET(EGLDisplay, eglGetDisplay, (EGLNativeDisplayType display_id), (display_id))
GEN_THUNKS_RET(EGLint, eglGetError, (void), ())
GEN_THUNKS_RET(EGLBoolean, eglGetOutputLayersEXT, (EGLDisplay dpy, const EGLAttrib * attrib_list, EGLOutputLayerEXT * layers, EGLint max_layers, EGLint * num_layers), (dpy, attrib_list, layers, max_layers, num_layers))
GEN_THUNKS_RET(EGLBoolean, eglGetOutputPortsEXT, (EGLDisplay dpy, const EGLAttrib * attrib_list, EGLOutputPortEXT * ports, EGLint max_ports, EGLint * num_ports), (dpy, attrib_list, ports, max_ports, num_ports))
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
GEN_THUNKS_RET(EGLBoolean, eglOutputLayerAttribEXT, (EGLDisplay dpy, EGLOutputLayerEXT layer, EGLint attribute, EGLAttrib value), (dpy, layer, attribute, value))
GEN_THUNKS_RET(EGLBoolean, eglOutputPortAttribEXT, (EGLDisplay dpy, EGLOutputPortEXT port, EGLint attribute, EGLAttrib value), (dpy, port, attribute, value))
GEN_THUNKS_RET(EGLBoolean, eglPostSubBufferNV, (EGLDisplay dpy, EGLSurface surface, EGLint x, EGLint y, EGLint width, EGLint height), (dpy, surface, x, y, width, height))
GEN_THUNKS_RET(EGLenum, eglQueryAPI, (void), ())
GEN_THUNKS_RET(EGLBoolean, eglQueryContext, (EGLDisplay dpy, EGLContext ctx, EGLint attribute, EGLint * value), (dpy, ctx, attribute, value))
GEN_THUNKS_RET(EGLBoolean, eglQueryDeviceAttribEXT, (EGLDeviceEXT device, EGLint attribute, EGLAttrib * value), (device, attribute, value))
GEN_THUNKS_RET(const char *, eglQueryDeviceStringEXT, (EGLDeviceEXT device, EGLint name), (device, name))
GEN_THUNKS_RET(EGLBoolean, eglQueryDevicesEXT, (EGLint max_devices, EGLDeviceEXT * devices, EGLint * num_devices), (max_devices, devices, num_devices))
GEN_THUNKS_RET(EGLBoolean, eglQueryDisplayAttribEXT, (EGLDisplay dpy, EGLint attribute, EGLAttrib * value), (dpy, attribute, value))
GEN_THUNKS_RET(EGLBoolean, eglQueryNativeDisplayNV, (EGLDisplay dpy, EGLNativeDisplayType * display_id), (dpy, display_id))
GEN_THUNKS_RET(EGLBoolean, eglQueryNativePixmapNV, (EGLDisplay dpy, EGLSurface surf, EGLNativePixmapType * pixmap), (dpy, surf, pixmap))
GEN_THUNKS_RET(EGLBoolean, eglQueryNativeWindowNV, (EGLDisplay dpy, EGLSurface surf, EGLNativeWindowType * window), (dpy, surf, window))
GEN_THUNKS_RET(EGLBoolean, eglQueryOutputLayerAttribEXT, (EGLDisplay dpy, EGLOutputLayerEXT layer, EGLint attribute, EGLAttrib * value), (dpy, layer, attribute, value))
GEN_THUNKS_RET(const char *, eglQueryOutputLayerStringEXT, (EGLDisplay dpy, EGLOutputLayerEXT layer, EGLint name), (dpy, layer, name))
GEN_THUNKS_RET(EGLBoolean, eglQueryOutputPortAttribEXT, (EGLDisplay dpy, EGLOutputPortEXT port, EGLint attribute, EGLAttrib * value), (dpy, port, attribute, value))
GEN_THUNKS_RET(const char *, eglQueryOutputPortStringEXT, (EGLDisplay dpy, EGLOutputPortEXT port, EGLint name), (dpy, port, name))
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
GEN_THUNKS_RET(EGLBoolean, eglSetDamageRegionKHR, (EGLDisplay dpy, EGLSurface surface, EGLint * rects, EGLint n_rects), (dpy, surface, rects, n_rects))
GEN_THUNKS_RET(EGLBoolean, eglSignalSyncKHR, (EGLDisplay dpy, EGLSyncKHR sync, EGLenum mode), (dpy, sync, mode))
GEN_THUNKS_RET(EGLBoolean, eglSignalSyncNV, (EGLSyncNV sync, EGLenum mode), (sync, mode))
GEN_THUNKS_RET(EGLBoolean, eglStreamAttribKHR, (EGLDisplay dpy, EGLStreamKHR stream, EGLenum attribute, EGLint value), (dpy, stream, attribute, value))
GEN_THUNKS_RET(EGLBoolean, eglStreamConsumerAcquireKHR, (EGLDisplay dpy, EGLStreamKHR stream), (dpy, stream))
GEN_THUNKS_RET(EGLBoolean, eglStreamConsumerGLTextureExternalKHR, (EGLDisplay dpy, EGLStreamKHR stream), (dpy, stream))
GEN_THUNKS_RET(EGLBoolean, eglStreamConsumerOutputEXT, (EGLDisplay dpy, EGLStreamKHR stream, EGLOutputLayerEXT layer), (dpy, stream, layer))
GEN_THUNKS_RET(EGLBoolean, eglStreamConsumerReleaseKHR, (EGLDisplay dpy, EGLStreamKHR stream), (dpy, stream))
GEN_THUNKS_RET(EGLBoolean, eglSurfaceAttrib, (EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint value), (dpy, surface, attribute, value))
GEN_THUNKS_RET(EGLBoolean, eglSwapBuffers, (EGLDisplay dpy, EGLSurface surface), (dpy, surface))
GEN_THUNKS_RET(EGLBoolean, eglSwapBuffersRegion2NOK, (EGLDisplay dpy, EGLSurface surface, EGLint numRects, const EGLint * rects), (dpy, surface, numRects, rects))
GEN_THUNKS_RET(EGLBoolean, eglSwapBuffersRegionNOK, (EGLDisplay dpy, EGLSurface surface, EGLint numRects, const EGLint * rects), (dpy, surface, numRects, rects))
GEN_THUNKS_RET(EGLBoolean, eglSwapBuffersWithDamageEXT, (EGLDisplay dpy, EGLSurface surface, EGLint * rects, EGLint n_rects), (dpy, surface, rects, n_rects))
GEN_THUNKS_RET(EGLBoolean, eglSwapBuffersWithDamageKHR, (EGLDisplay dpy, EGLSurface surface, EGLint * rects, EGLint n_rects), (dpy, surface, rects, n_rects))
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
    .eglCreateImage = epoxy_eglCreateImage_dispatch_table_rewrite_ptr,
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
    .eglDestroyImage = epoxy_eglDestroyImage_dispatch_table_rewrite_ptr,
    .eglDestroyImageKHR = epoxy_eglDestroyImageKHR_dispatch_table_rewrite_ptr,
    .eglDestroyStreamKHR = epoxy_eglDestroyStreamKHR_dispatch_table_rewrite_ptr,
    .eglDestroySurface = epoxy_eglDestroySurface_dispatch_table_rewrite_ptr,
    .eglDestroySync = epoxy_eglDestroySync_dispatch_table_rewrite_ptr,
    .eglDestroySyncKHR = epoxy_eglDestroySyncKHR_dispatch_table_rewrite_ptr,
    .eglDestroySyncNV = epoxy_eglDestroySyncNV_dispatch_table_rewrite_ptr,
    .eglDupNativeFenceFDANDROID = epoxy_eglDupNativeFenceFDANDROID_dispatch_table_rewrite_ptr,
    .eglExportDMABUFImageMESA = epoxy_eglExportDMABUFImageMESA_dispatch_table_rewrite_ptr,
    .eglExportDMABUFImageQueryMESA = epoxy_eglExportDMABUFImageQueryMESA_dispatch_table_rewrite_ptr,
    .eglExportDRMImageMESA = epoxy_eglExportDRMImageMESA_dispatch_table_rewrite_ptr,
    .eglFenceNV = epoxy_eglFenceNV_dispatch_table_rewrite_ptr,
    .eglGetConfigAttrib = epoxy_eglGetConfigAttrib_dispatch_table_rewrite_ptr,
    .eglGetConfigs = epoxy_eglGetConfigs_dispatch_table_rewrite_ptr,
    .eglGetCurrentContext = epoxy_eglGetCurrentContext_dispatch_table_rewrite_ptr,
    .eglGetCurrentDisplay = epoxy_eglGetCurrentDisplay_dispatch_table_rewrite_ptr,
    .eglGetCurrentSurface = epoxy_eglGetCurrentSurface_dispatch_table_rewrite_ptr,
    .eglGetDisplay = epoxy_eglGetDisplay_dispatch_table_rewrite_ptr,
    .eglGetError = epoxy_eglGetError_dispatch_table_rewrite_ptr,
    .eglGetOutputLayersEXT = epoxy_eglGetOutputLayersEXT_dispatch_table_rewrite_ptr,
    .eglGetOutputPortsEXT = epoxy_eglGetOutputPortsEXT_dispatch_table_rewrite_ptr,
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
    .eglOutputLayerAttribEXT = epoxy_eglOutputLayerAttribEXT_dispatch_table_rewrite_ptr,
    .eglOutputPortAttribEXT = epoxy_eglOutputPortAttribEXT_dispatch_table_rewrite_ptr,
    .eglPostSubBufferNV = epoxy_eglPostSubBufferNV_dispatch_table_rewrite_ptr,
    .eglQueryAPI = epoxy_eglQueryAPI_dispatch_table_rewrite_ptr,
    .eglQueryContext = epoxy_eglQueryContext_dispatch_table_rewrite_ptr,
    .eglQueryDeviceAttribEXT = epoxy_eglQueryDeviceAttribEXT_dispatch_table_rewrite_ptr,
    .eglQueryDeviceStringEXT = epoxy_eglQueryDeviceStringEXT_dispatch_table_rewrite_ptr,
    .eglQueryDevicesEXT = epoxy_eglQueryDevicesEXT_dispatch_table_rewrite_ptr,
    .eglQueryDisplayAttribEXT = epoxy_eglQueryDisplayAttribEXT_dispatch_table_rewrite_ptr,
    .eglQueryNativeDisplayNV = epoxy_eglQueryNativeDisplayNV_dispatch_table_rewrite_ptr,
    .eglQueryNativePixmapNV = epoxy_eglQueryNativePixmapNV_dispatch_table_rewrite_ptr,
    .eglQueryNativeWindowNV = epoxy_eglQueryNativeWindowNV_dispatch_table_rewrite_ptr,
    .eglQueryOutputLayerAttribEXT = epoxy_eglQueryOutputLayerAttribEXT_dispatch_table_rewrite_ptr,
    .eglQueryOutputLayerStringEXT = epoxy_eglQueryOutputLayerStringEXT_dispatch_table_rewrite_ptr,
    .eglQueryOutputPortAttribEXT = epoxy_eglQueryOutputPortAttribEXT_dispatch_table_rewrite_ptr,
    .eglQueryOutputPortStringEXT = epoxy_eglQueryOutputPortStringEXT_dispatch_table_rewrite_ptr,
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
    .eglSetDamageRegionKHR = epoxy_eglSetDamageRegionKHR_dispatch_table_rewrite_ptr,
    .eglSignalSyncKHR = epoxy_eglSignalSyncKHR_dispatch_table_rewrite_ptr,
    .eglSignalSyncNV = epoxy_eglSignalSyncNV_dispatch_table_rewrite_ptr,
    .eglStreamAttribKHR = epoxy_eglStreamAttribKHR_dispatch_table_rewrite_ptr,
    .eglStreamConsumerAcquireKHR = epoxy_eglStreamConsumerAcquireKHR_dispatch_table_rewrite_ptr,
    .eglStreamConsumerGLTextureExternalKHR = epoxy_eglStreamConsumerGLTextureExternalKHR_dispatch_table_rewrite_ptr,
    .eglStreamConsumerOutputEXT = epoxy_eglStreamConsumerOutputEXT_dispatch_table_rewrite_ptr,
    .eglStreamConsumerReleaseKHR = epoxy_eglStreamConsumerReleaseKHR_dispatch_table_rewrite_ptr,
    .eglSurfaceAttrib = epoxy_eglSurfaceAttrib_dispatch_table_rewrite_ptr,
    .eglSwapBuffers = epoxy_eglSwapBuffers_dispatch_table_rewrite_ptr,
    .eglSwapBuffersRegion2NOK = epoxy_eglSwapBuffersRegion2NOK_dispatch_table_rewrite_ptr,
    .eglSwapBuffersRegionNOK = epoxy_eglSwapBuffersRegionNOK_dispatch_table_rewrite_ptr,
    .eglSwapBuffersWithDamageEXT = epoxy_eglSwapBuffersWithDamageEXT_dispatch_table_rewrite_ptr,
    .eglSwapBuffersWithDamageKHR = epoxy_eglSwapBuffersWithDamageKHR_dispatch_table_rewrite_ptr,
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
    epoxy_eglCreateImage = epoxy_eglCreateImage_dispatch_table_thunk;
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
    epoxy_eglDestroyImage = epoxy_eglDestroyImage_dispatch_table_thunk;
    epoxy_eglDestroyImageKHR = epoxy_eglDestroyImageKHR_dispatch_table_thunk;
    epoxy_eglDestroyStreamKHR = epoxy_eglDestroyStreamKHR_dispatch_table_thunk;
    epoxy_eglDestroySurface = epoxy_eglDestroySurface_dispatch_table_thunk;
    epoxy_eglDestroySync = epoxy_eglDestroySync_dispatch_table_thunk;
    epoxy_eglDestroySyncKHR = epoxy_eglDestroySyncKHR_dispatch_table_thunk;
    epoxy_eglDestroySyncNV = epoxy_eglDestroySyncNV_dispatch_table_thunk;
    epoxy_eglDupNativeFenceFDANDROID = epoxy_eglDupNativeFenceFDANDROID_dispatch_table_thunk;
    epoxy_eglExportDMABUFImageMESA = epoxy_eglExportDMABUFImageMESA_dispatch_table_thunk;
    epoxy_eglExportDMABUFImageQueryMESA = epoxy_eglExportDMABUFImageQueryMESA_dispatch_table_thunk;
    epoxy_eglExportDRMImageMESA = epoxy_eglExportDRMImageMESA_dispatch_table_thunk;
    epoxy_eglFenceNV = epoxy_eglFenceNV_dispatch_table_thunk;
    epoxy_eglGetConfigAttrib = epoxy_eglGetConfigAttrib_dispatch_table_thunk;
    epoxy_eglGetConfigs = epoxy_eglGetConfigs_dispatch_table_thunk;
    epoxy_eglGetCurrentContext = epoxy_eglGetCurrentContext_dispatch_table_thunk;
    epoxy_eglGetCurrentDisplay = epoxy_eglGetCurrentDisplay_dispatch_table_thunk;
    epoxy_eglGetCurrentSurface = epoxy_eglGetCurrentSurface_dispatch_table_thunk;
    epoxy_eglGetDisplay = epoxy_eglGetDisplay_dispatch_table_thunk;
    epoxy_eglGetError = epoxy_eglGetError_dispatch_table_thunk;
    epoxy_eglGetOutputLayersEXT = epoxy_eglGetOutputLayersEXT_dispatch_table_thunk;
    epoxy_eglGetOutputPortsEXT = epoxy_eglGetOutputPortsEXT_dispatch_table_thunk;
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
    epoxy_eglOutputLayerAttribEXT = epoxy_eglOutputLayerAttribEXT_dispatch_table_thunk;
    epoxy_eglOutputPortAttribEXT = epoxy_eglOutputPortAttribEXT_dispatch_table_thunk;
    epoxy_eglPostSubBufferNV = epoxy_eglPostSubBufferNV_dispatch_table_thunk;
    epoxy_eglQueryAPI = epoxy_eglQueryAPI_dispatch_table_thunk;
    epoxy_eglQueryContext = epoxy_eglQueryContext_dispatch_table_thunk;
    epoxy_eglQueryDeviceAttribEXT = epoxy_eglQueryDeviceAttribEXT_dispatch_table_thunk;
    epoxy_eglQueryDeviceStringEXT = epoxy_eglQueryDeviceStringEXT_dispatch_table_thunk;
    epoxy_eglQueryDevicesEXT = epoxy_eglQueryDevicesEXT_dispatch_table_thunk;
    epoxy_eglQueryDisplayAttribEXT = epoxy_eglQueryDisplayAttribEXT_dispatch_table_thunk;
    epoxy_eglQueryNativeDisplayNV = epoxy_eglQueryNativeDisplayNV_dispatch_table_thunk;
    epoxy_eglQueryNativePixmapNV = epoxy_eglQueryNativePixmapNV_dispatch_table_thunk;
    epoxy_eglQueryNativeWindowNV = epoxy_eglQueryNativeWindowNV_dispatch_table_thunk;
    epoxy_eglQueryOutputLayerAttribEXT = epoxy_eglQueryOutputLayerAttribEXT_dispatch_table_thunk;
    epoxy_eglQueryOutputLayerStringEXT = epoxy_eglQueryOutputLayerStringEXT_dispatch_table_thunk;
    epoxy_eglQueryOutputPortAttribEXT = epoxy_eglQueryOutputPortAttribEXT_dispatch_table_thunk;
    epoxy_eglQueryOutputPortStringEXT = epoxy_eglQueryOutputPortStringEXT_dispatch_table_thunk;
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
    epoxy_eglSetDamageRegionKHR = epoxy_eglSetDamageRegionKHR_dispatch_table_thunk;
    epoxy_eglSignalSyncKHR = epoxy_eglSignalSyncKHR_dispatch_table_thunk;
    epoxy_eglSignalSyncNV = epoxy_eglSignalSyncNV_dispatch_table_thunk;
    epoxy_eglStreamAttribKHR = epoxy_eglStreamAttribKHR_dispatch_table_thunk;
    epoxy_eglStreamConsumerAcquireKHR = epoxy_eglStreamConsumerAcquireKHR_dispatch_table_thunk;
    epoxy_eglStreamConsumerGLTextureExternalKHR = epoxy_eglStreamConsumerGLTextureExternalKHR_dispatch_table_thunk;
    epoxy_eglStreamConsumerOutputEXT = epoxy_eglStreamConsumerOutputEXT_dispatch_table_thunk;
    epoxy_eglStreamConsumerReleaseKHR = epoxy_eglStreamConsumerReleaseKHR_dispatch_table_thunk;
    epoxy_eglSurfaceAttrib = epoxy_eglSurfaceAttrib_dispatch_table_thunk;
    epoxy_eglSwapBuffers = epoxy_eglSwapBuffers_dispatch_table_thunk;
    epoxy_eglSwapBuffersRegion2NOK = epoxy_eglSwapBuffersRegion2NOK_dispatch_table_thunk;
    epoxy_eglSwapBuffersRegionNOK = epoxy_eglSwapBuffersRegionNOK_dispatch_table_thunk;
    epoxy_eglSwapBuffersWithDamageEXT = epoxy_eglSwapBuffersWithDamageEXT_dispatch_table_thunk;
    epoxy_eglSwapBuffersWithDamageKHR = epoxy_eglSwapBuffersWithDamageKHR_dispatch_table_thunk;
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

PUBLIC PFNEGLCREATEIMAGEPROC epoxy_eglCreateImage = epoxy_eglCreateImage_global_rewrite_ptr;

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

PUBLIC PFNEGLDESTROYIMAGEPROC epoxy_eglDestroyImage = epoxy_eglDestroyImage_global_rewrite_ptr;

PUBLIC PFNEGLDESTROYIMAGEKHRPROC epoxy_eglDestroyImageKHR = epoxy_eglDestroyImageKHR_global_rewrite_ptr;

PUBLIC PFNEGLDESTROYSTREAMKHRPROC epoxy_eglDestroyStreamKHR = epoxy_eglDestroyStreamKHR_global_rewrite_ptr;

PUBLIC PFNEGLDESTROYSURFACEPROC epoxy_eglDestroySurface = epoxy_eglDestroySurface_global_rewrite_ptr;

PUBLIC PFNEGLDESTROYSYNCPROC epoxy_eglDestroySync = epoxy_eglDestroySync_global_rewrite_ptr;

PUBLIC PFNEGLDESTROYSYNCKHRPROC epoxy_eglDestroySyncKHR = epoxy_eglDestroySyncKHR_global_rewrite_ptr;

PUBLIC PFNEGLDESTROYSYNCNVPROC epoxy_eglDestroySyncNV = epoxy_eglDestroySyncNV_global_rewrite_ptr;

PUBLIC PFNEGLDUPNATIVEFENCEFDANDROIDPROC epoxy_eglDupNativeFenceFDANDROID = epoxy_eglDupNativeFenceFDANDROID_global_rewrite_ptr;

PUBLIC PFNEGLEXPORTDMABUFIMAGEMESAPROC epoxy_eglExportDMABUFImageMESA = epoxy_eglExportDMABUFImageMESA_global_rewrite_ptr;

PUBLIC PFNEGLEXPORTDMABUFIMAGEQUERYMESAPROC epoxy_eglExportDMABUFImageQueryMESA = epoxy_eglExportDMABUFImageQueryMESA_global_rewrite_ptr;

PUBLIC PFNEGLEXPORTDRMIMAGEMESAPROC epoxy_eglExportDRMImageMESA = epoxy_eglExportDRMImageMESA_global_rewrite_ptr;

PUBLIC PFNEGLFENCENVPROC epoxy_eglFenceNV = epoxy_eglFenceNV_global_rewrite_ptr;

PUBLIC PFNEGLGETCONFIGATTRIBPROC epoxy_eglGetConfigAttrib = epoxy_eglGetConfigAttrib_global_rewrite_ptr;

PUBLIC PFNEGLGETCONFIGSPROC epoxy_eglGetConfigs = epoxy_eglGetConfigs_global_rewrite_ptr;

PUBLIC PFNEGLGETCURRENTCONTEXTPROC epoxy_eglGetCurrentContext = epoxy_eglGetCurrentContext_global_rewrite_ptr;

PUBLIC PFNEGLGETCURRENTDISPLAYPROC epoxy_eglGetCurrentDisplay = epoxy_eglGetCurrentDisplay_global_rewrite_ptr;

PUBLIC PFNEGLGETCURRENTSURFACEPROC epoxy_eglGetCurrentSurface = epoxy_eglGetCurrentSurface_global_rewrite_ptr;

PUBLIC PFNEGLGETDISPLAYPROC epoxy_eglGetDisplay = epoxy_eglGetDisplay_global_rewrite_ptr;

PUBLIC PFNEGLGETERRORPROC epoxy_eglGetError = epoxy_eglGetError_global_rewrite_ptr;

PUBLIC PFNEGLGETOUTPUTLAYERSEXTPROC epoxy_eglGetOutputLayersEXT = epoxy_eglGetOutputLayersEXT_global_rewrite_ptr;

PUBLIC PFNEGLGETOUTPUTPORTSEXTPROC epoxy_eglGetOutputPortsEXT = epoxy_eglGetOutputPortsEXT_global_rewrite_ptr;

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

PUBLIC PFNEGLOUTPUTLAYERATTRIBEXTPROC epoxy_eglOutputLayerAttribEXT = epoxy_eglOutputLayerAttribEXT_global_rewrite_ptr;

PUBLIC PFNEGLOUTPUTPORTATTRIBEXTPROC epoxy_eglOutputPortAttribEXT = epoxy_eglOutputPortAttribEXT_global_rewrite_ptr;

PUBLIC PFNEGLPOSTSUBBUFFERNVPROC epoxy_eglPostSubBufferNV = epoxy_eglPostSubBufferNV_global_rewrite_ptr;

PUBLIC PFNEGLQUERYAPIPROC epoxy_eglQueryAPI = epoxy_eglQueryAPI_global_rewrite_ptr;

PUBLIC PFNEGLQUERYCONTEXTPROC epoxy_eglQueryContext = epoxy_eglQueryContext_global_rewrite_ptr;

PUBLIC PFNEGLQUERYDEVICEATTRIBEXTPROC epoxy_eglQueryDeviceAttribEXT = epoxy_eglQueryDeviceAttribEXT_global_rewrite_ptr;

PUBLIC PFNEGLQUERYDEVICESTRINGEXTPROC epoxy_eglQueryDeviceStringEXT = epoxy_eglQueryDeviceStringEXT_global_rewrite_ptr;

PUBLIC PFNEGLQUERYDEVICESEXTPROC epoxy_eglQueryDevicesEXT = epoxy_eglQueryDevicesEXT_global_rewrite_ptr;

PUBLIC PFNEGLQUERYDISPLAYATTRIBEXTPROC epoxy_eglQueryDisplayAttribEXT = epoxy_eglQueryDisplayAttribEXT_global_rewrite_ptr;

PUBLIC PFNEGLQUERYNATIVEDISPLAYNVPROC epoxy_eglQueryNativeDisplayNV = epoxy_eglQueryNativeDisplayNV_global_rewrite_ptr;

PUBLIC PFNEGLQUERYNATIVEPIXMAPNVPROC epoxy_eglQueryNativePixmapNV = epoxy_eglQueryNativePixmapNV_global_rewrite_ptr;

PUBLIC PFNEGLQUERYNATIVEWINDOWNVPROC epoxy_eglQueryNativeWindowNV = epoxy_eglQueryNativeWindowNV_global_rewrite_ptr;

PUBLIC PFNEGLQUERYOUTPUTLAYERATTRIBEXTPROC epoxy_eglQueryOutputLayerAttribEXT = epoxy_eglQueryOutputLayerAttribEXT_global_rewrite_ptr;

PUBLIC PFNEGLQUERYOUTPUTLAYERSTRINGEXTPROC epoxy_eglQueryOutputLayerStringEXT = epoxy_eglQueryOutputLayerStringEXT_global_rewrite_ptr;

PUBLIC PFNEGLQUERYOUTPUTPORTATTRIBEXTPROC epoxy_eglQueryOutputPortAttribEXT = epoxy_eglQueryOutputPortAttribEXT_global_rewrite_ptr;

PUBLIC PFNEGLQUERYOUTPUTPORTSTRINGEXTPROC epoxy_eglQueryOutputPortStringEXT = epoxy_eglQueryOutputPortStringEXT_global_rewrite_ptr;

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

PUBLIC PFNEGLSETDAMAGEREGIONKHRPROC epoxy_eglSetDamageRegionKHR = epoxy_eglSetDamageRegionKHR_global_rewrite_ptr;

PUBLIC PFNEGLSIGNALSYNCKHRPROC epoxy_eglSignalSyncKHR = epoxy_eglSignalSyncKHR_global_rewrite_ptr;

PUBLIC PFNEGLSIGNALSYNCNVPROC epoxy_eglSignalSyncNV = epoxy_eglSignalSyncNV_global_rewrite_ptr;

PUBLIC PFNEGLSTREAMATTRIBKHRPROC epoxy_eglStreamAttribKHR = epoxy_eglStreamAttribKHR_global_rewrite_ptr;

PUBLIC PFNEGLSTREAMCONSUMERACQUIREKHRPROC epoxy_eglStreamConsumerAcquireKHR = epoxy_eglStreamConsumerAcquireKHR_global_rewrite_ptr;

PUBLIC PFNEGLSTREAMCONSUMERGLTEXTUREEXTERNALKHRPROC epoxy_eglStreamConsumerGLTextureExternalKHR = epoxy_eglStreamConsumerGLTextureExternalKHR_global_rewrite_ptr;

PUBLIC PFNEGLSTREAMCONSUMEROUTPUTEXTPROC epoxy_eglStreamConsumerOutputEXT = epoxy_eglStreamConsumerOutputEXT_global_rewrite_ptr;

PUBLIC PFNEGLSTREAMCONSUMERRELEASEKHRPROC epoxy_eglStreamConsumerReleaseKHR = epoxy_eglStreamConsumerReleaseKHR_global_rewrite_ptr;

PUBLIC PFNEGLSURFACEATTRIBPROC epoxy_eglSurfaceAttrib = epoxy_eglSurfaceAttrib_global_rewrite_ptr;

PUBLIC PFNEGLSWAPBUFFERSPROC epoxy_eglSwapBuffers = epoxy_eglSwapBuffers_global_rewrite_ptr;

PUBLIC PFNEGLSWAPBUFFERSREGION2NOKPROC epoxy_eglSwapBuffersRegion2NOK = epoxy_eglSwapBuffersRegion2NOK_global_rewrite_ptr;

PUBLIC PFNEGLSWAPBUFFERSREGIONNOKPROC epoxy_eglSwapBuffersRegionNOK = epoxy_eglSwapBuffersRegionNOK_global_rewrite_ptr;

PUBLIC PFNEGLSWAPBUFFERSWITHDAMAGEEXTPROC epoxy_eglSwapBuffersWithDamageEXT = epoxy_eglSwapBuffersWithDamageEXT_global_rewrite_ptr;

PUBLIC PFNEGLSWAPBUFFERSWITHDAMAGEKHRPROC epoxy_eglSwapBuffersWithDamageKHR = epoxy_eglSwapBuffersWithDamageKHR_global_rewrite_ptr;

PUBLIC PFNEGLSWAPINTERVALPROC epoxy_eglSwapInterval = epoxy_eglSwapInterval_global_rewrite_ptr;

PUBLIC PFNEGLTERMINATEPROC epoxy_eglTerminate = epoxy_eglTerminate_global_rewrite_ptr;

PUBLIC PFNEGLUNLOCKSURFACEKHRPROC epoxy_eglUnlockSurfaceKHR = epoxy_eglUnlockSurfaceKHR_global_rewrite_ptr;

PUBLIC PFNEGLWAITCLIENTPROC epoxy_eglWaitClient = epoxy_eglWaitClient_global_rewrite_ptr;

PUBLIC PFNEGLWAITGLPROC epoxy_eglWaitGL = epoxy_eglWaitGL_global_rewrite_ptr;

PUBLIC PFNEGLWAITNATIVEPROC epoxy_eglWaitNative = epoxy_eglWaitNative_global_rewrite_ptr;

PUBLIC PFNEGLWAITSYNCPROC epoxy_eglWaitSync = epoxy_eglWaitSync_global_rewrite_ptr;

PUBLIC PFNEGLWAITSYNCKHRPROC epoxy_eglWaitSyncKHR = epoxy_eglWaitSyncKHR_global_rewrite_ptr;

