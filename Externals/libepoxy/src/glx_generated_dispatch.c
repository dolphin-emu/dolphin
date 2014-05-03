/* GL dispatch code.
 * This is code-generated from the GL API XML files from Khronos.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "dispatch_common.h"
#include "epoxy/glx.h"

struct dispatch_table {
    PFNGLXBINDCHANNELTOWINDOWSGIXPROC epoxy_glXBindChannelToWindowSGIX;
    PFNGLXBINDHYPERPIPESGIXPROC epoxy_glXBindHyperpipeSGIX;
    PFNGLXBINDSWAPBARRIERNVPROC epoxy_glXBindSwapBarrierNV;
    PFNGLXBINDSWAPBARRIERSGIXPROC epoxy_glXBindSwapBarrierSGIX;
    PFNGLXBINDTEXIMAGEEXTPROC epoxy_glXBindTexImageEXT;
    PFNGLXBINDVIDEOCAPTUREDEVICENVPROC epoxy_glXBindVideoCaptureDeviceNV;
    PFNGLXBINDVIDEODEVICENVPROC epoxy_glXBindVideoDeviceNV;
    PFNGLXBINDVIDEOIMAGENVPROC epoxy_glXBindVideoImageNV;
    PFNGLXCHANNELRECTSGIXPROC epoxy_glXChannelRectSGIX;
    PFNGLXCHANNELRECTSYNCSGIXPROC epoxy_glXChannelRectSyncSGIX;
    PFNGLXCHOOSEFBCONFIGPROC epoxy_glXChooseFBConfig;
    PFNGLXCHOOSEFBCONFIGSGIXPROC epoxy_glXChooseFBConfigSGIX;
    PFNGLXCHOOSEVISUALPROC epoxy_glXChooseVisual;
    PFNGLXCOPYCONTEXTPROC epoxy_glXCopyContext;
    PFNGLXCOPYIMAGESUBDATANVPROC epoxy_glXCopyImageSubDataNV;
    PFNGLXCOPYSUBBUFFERMESAPROC epoxy_glXCopySubBufferMESA;
    PFNGLXCREATECONTEXTPROC epoxy_glXCreateContext;
    PFNGLXCREATECONTEXTATTRIBSARBPROC epoxy_glXCreateContextAttribsARB;
    PFNGLXCREATECONTEXTWITHCONFIGSGIXPROC epoxy_glXCreateContextWithConfigSGIX;
    PFNGLXCREATEGLXPBUFFERSGIXPROC epoxy_glXCreateGLXPbufferSGIX;
    PFNGLXCREATEGLXPIXMAPPROC epoxy_glXCreateGLXPixmap;
    PFNGLXCREATEGLXPIXMAPMESAPROC epoxy_glXCreateGLXPixmapMESA;
    PFNGLXCREATEGLXPIXMAPWITHCONFIGSGIXPROC epoxy_glXCreateGLXPixmapWithConfigSGIX;
    PFNGLXCREATENEWCONTEXTPROC epoxy_glXCreateNewContext;
    PFNGLXCREATEPBUFFERPROC epoxy_glXCreatePbuffer;
    PFNGLXCREATEPIXMAPPROC epoxy_glXCreatePixmap;
    PFNGLXCREATEWINDOWPROC epoxy_glXCreateWindow;
    PFNGLXCUSHIONSGIPROC epoxy_glXCushionSGI;
    PFNGLXDELAYBEFORESWAPNVPROC epoxy_glXDelayBeforeSwapNV;
    PFNGLXDESTROYCONTEXTPROC epoxy_glXDestroyContext;
    PFNGLXDESTROYGLXPBUFFERSGIXPROC epoxy_glXDestroyGLXPbufferSGIX;
    PFNGLXDESTROYGLXPIXMAPPROC epoxy_glXDestroyGLXPixmap;
    PFNGLXDESTROYGLXVIDEOSOURCESGIXPROC epoxy_glXDestroyGLXVideoSourceSGIX;
    PFNGLXDESTROYHYPERPIPECONFIGSGIXPROC epoxy_glXDestroyHyperpipeConfigSGIX;
    PFNGLXDESTROYPBUFFERPROC epoxy_glXDestroyPbuffer;
    PFNGLXDESTROYPIXMAPPROC epoxy_glXDestroyPixmap;
    PFNGLXDESTROYWINDOWPROC epoxy_glXDestroyWindow;
    PFNGLXENUMERATEVIDEOCAPTUREDEVICESNVPROC epoxy_glXEnumerateVideoCaptureDevicesNV;
    PFNGLXENUMERATEVIDEODEVICESNVPROC epoxy_glXEnumerateVideoDevicesNV;
    PFNGLXFREECONTEXTEXTPROC epoxy_glXFreeContextEXT;
    PFNGLXGETAGPOFFSETMESAPROC epoxy_glXGetAGPOffsetMESA;
    PFNGLXGETCLIENTSTRINGPROC epoxy_glXGetClientString;
    PFNGLXGETCONFIGPROC epoxy_glXGetConfig;
    PFNGLXGETCONTEXTIDEXTPROC epoxy_glXGetContextIDEXT;
    PFNGLXGETCURRENTCONTEXTPROC epoxy_glXGetCurrentContext;
    PFNGLXGETCURRENTDISPLAYPROC epoxy_glXGetCurrentDisplay;
    PFNGLXGETCURRENTDISPLAYEXTPROC epoxy_glXGetCurrentDisplayEXT;
    PFNGLXGETCURRENTDRAWABLEPROC epoxy_glXGetCurrentDrawable;
    PFNGLXGETCURRENTREADDRAWABLEPROC epoxy_glXGetCurrentReadDrawable;
    PFNGLXGETCURRENTREADDRAWABLESGIPROC epoxy_glXGetCurrentReadDrawableSGI;
    PFNGLXGETFBCONFIGATTRIBPROC epoxy_glXGetFBConfigAttrib;
    PFNGLXGETFBCONFIGATTRIBSGIXPROC epoxy_glXGetFBConfigAttribSGIX;
    PFNGLXGETFBCONFIGFROMVISUALSGIXPROC epoxy_glXGetFBConfigFromVisualSGIX;
    PFNGLXGETFBCONFIGSPROC epoxy_glXGetFBConfigs;
    PFNGLXGETMSCRATEOMLPROC epoxy_glXGetMscRateOML;
    PFNGLXGETPROCADDRESSPROC epoxy_glXGetProcAddress;
    PFNGLXGETPROCADDRESSARBPROC epoxy_glXGetProcAddressARB;
    PFNGLXGETSELECTEDEVENTPROC epoxy_glXGetSelectedEvent;
    PFNGLXGETSELECTEDEVENTSGIXPROC epoxy_glXGetSelectedEventSGIX;
    PFNGLXGETSYNCVALUESOMLPROC epoxy_glXGetSyncValuesOML;
    PFNGLXGETTRANSPARENTINDEXSUNPROC epoxy_glXGetTransparentIndexSUN;
    PFNGLXGETVIDEODEVICENVPROC epoxy_glXGetVideoDeviceNV;
    PFNGLXGETVIDEOINFONVPROC epoxy_glXGetVideoInfoNV;
    PFNGLXGETVIDEOSYNCSGIPROC epoxy_glXGetVideoSyncSGI;
    PFNGLXGETVISUALFROMFBCONFIGPROC epoxy_glXGetVisualFromFBConfig;
    PFNGLXGETVISUALFROMFBCONFIGSGIXPROC epoxy_glXGetVisualFromFBConfigSGIX;
    PFNGLXHYPERPIPEATTRIBSGIXPROC epoxy_glXHyperpipeAttribSGIX;
    PFNGLXHYPERPIPECONFIGSGIXPROC epoxy_glXHyperpipeConfigSGIX;
    PFNGLXIMPORTCONTEXTEXTPROC epoxy_glXImportContextEXT;
    PFNGLXISDIRECTPROC epoxy_glXIsDirect;
    PFNGLXJOINSWAPGROUPNVPROC epoxy_glXJoinSwapGroupNV;
    PFNGLXJOINSWAPGROUPSGIXPROC epoxy_glXJoinSwapGroupSGIX;
    PFNGLXLOCKVIDEOCAPTUREDEVICENVPROC epoxy_glXLockVideoCaptureDeviceNV;
    PFNGLXMAKECONTEXTCURRENTPROC epoxy_glXMakeContextCurrent;
    PFNGLXMAKECURRENTPROC epoxy_glXMakeCurrent;
    PFNGLXMAKECURRENTREADSGIPROC epoxy_glXMakeCurrentReadSGI;
    PFNGLXQUERYCHANNELDELTASSGIXPROC epoxy_glXQueryChannelDeltasSGIX;
    PFNGLXQUERYCHANNELRECTSGIXPROC epoxy_glXQueryChannelRectSGIX;
    PFNGLXQUERYCONTEXTPROC epoxy_glXQueryContext;
    PFNGLXQUERYCONTEXTINFOEXTPROC epoxy_glXQueryContextInfoEXT;
    PFNGLXQUERYCURRENTRENDERERINTEGERMESAPROC epoxy_glXQueryCurrentRendererIntegerMESA;
    PFNGLXQUERYCURRENTRENDERERSTRINGMESAPROC epoxy_glXQueryCurrentRendererStringMESA;
    PFNGLXQUERYDRAWABLEPROC epoxy_glXQueryDrawable;
    PFNGLXQUERYEXTENSIONPROC epoxy_glXQueryExtension;
    PFNGLXQUERYEXTENSIONSSTRINGPROC epoxy_glXQueryExtensionsString;
    PFNGLXQUERYFRAMECOUNTNVPROC epoxy_glXQueryFrameCountNV;
    PFNGLXQUERYGLXPBUFFERSGIXPROC epoxy_glXQueryGLXPbufferSGIX;
    PFNGLXQUERYHYPERPIPEATTRIBSGIXPROC epoxy_glXQueryHyperpipeAttribSGIX;
    PFNGLXQUERYHYPERPIPEBESTATTRIBSGIXPROC epoxy_glXQueryHyperpipeBestAttribSGIX;
    PFNGLXQUERYHYPERPIPECONFIGSGIXPROC epoxy_glXQueryHyperpipeConfigSGIX;
    PFNGLXQUERYHYPERPIPENETWORKSGIXPROC epoxy_glXQueryHyperpipeNetworkSGIX;
    PFNGLXQUERYMAXSWAPBARRIERSSGIXPROC epoxy_glXQueryMaxSwapBarriersSGIX;
    PFNGLXQUERYMAXSWAPGROUPSNVPROC epoxy_glXQueryMaxSwapGroupsNV;
    PFNGLXQUERYRENDERERINTEGERMESAPROC epoxy_glXQueryRendererIntegerMESA;
    PFNGLXQUERYRENDERERSTRINGMESAPROC epoxy_glXQueryRendererStringMESA;
    PFNGLXQUERYSERVERSTRINGPROC epoxy_glXQueryServerString;
    PFNGLXQUERYSWAPGROUPNVPROC epoxy_glXQuerySwapGroupNV;
    PFNGLXQUERYVERSIONPROC epoxy_glXQueryVersion;
    PFNGLXQUERYVIDEOCAPTUREDEVICENVPROC epoxy_glXQueryVideoCaptureDeviceNV;
    PFNGLXRELEASEBUFFERSMESAPROC epoxy_glXReleaseBuffersMESA;
    PFNGLXRELEASETEXIMAGEEXTPROC epoxy_glXReleaseTexImageEXT;
    PFNGLXRELEASEVIDEOCAPTUREDEVICENVPROC epoxy_glXReleaseVideoCaptureDeviceNV;
    PFNGLXRELEASEVIDEODEVICENVPROC epoxy_glXReleaseVideoDeviceNV;
    PFNGLXRELEASEVIDEOIMAGENVPROC epoxy_glXReleaseVideoImageNV;
    PFNGLXRESETFRAMECOUNTNVPROC epoxy_glXResetFrameCountNV;
    PFNGLXSELECTEVENTPROC epoxy_glXSelectEvent;
    PFNGLXSELECTEVENTSGIXPROC epoxy_glXSelectEventSGIX;
    PFNGLXSENDPBUFFERTOVIDEONVPROC epoxy_glXSendPbufferToVideoNV;
    PFNGLXSET3DFXMODEMESAPROC epoxy_glXSet3DfxModeMESA;
    PFNGLXSWAPBUFFERSPROC epoxy_glXSwapBuffers;
    PFNGLXSWAPBUFFERSMSCOMLPROC epoxy_glXSwapBuffersMscOML;
    PFNGLXSWAPINTERVALEXTPROC epoxy_glXSwapIntervalEXT;
    PFNGLXSWAPINTERVALSGIPROC epoxy_glXSwapIntervalSGI;
    PFNGLXUSEXFONTPROC epoxy_glXUseXFont;
    PFNGLXWAITFORMSCOMLPROC epoxy_glXWaitForMscOML;
    PFNGLXWAITFORSBCOMLPROC epoxy_glXWaitForSbcOML;
    PFNGLXWAITGLPROC epoxy_glXWaitGL;
    PFNGLXWAITVIDEOSYNCSGIPROC epoxy_glXWaitVideoSyncSGI;
    PFNGLXWAITXPROC epoxy_glXWaitX;
};

#if USING_DISPATCH_TABLE
static inline struct dispatch_table *
get_dispatch_table(void);

#endif
enum glx_provider {
    glx_provider_terminator = 0,
    GLX_10,
    GLX_11,
    GLX_12,
    GLX_13,
    GLX_extension_GLX_ARB_create_context,
    GLX_extension_GLX_ARB_get_proc_address,
    GLX_extension_GLX_EXT_import_context,
    GLX_extension_GLX_EXT_swap_control,
    GLX_extension_GLX_EXT_texture_from_pixmap,
    GLX_extension_GLX_MESA_agp_offset,
    GLX_extension_GLX_MESA_copy_sub_buffer,
    GLX_extension_GLX_MESA_pixmap_colormap,
    GLX_extension_GLX_MESA_query_renderer,
    GLX_extension_GLX_MESA_release_buffers,
    GLX_extension_GLX_MESA_set_3dfx_mode,
    GLX_extension_GLX_NV_copy_image,
    GLX_extension_GLX_NV_delay_before_swap,
    GLX_extension_GLX_NV_present_video,
    GLX_extension_GLX_NV_swap_group,
    GLX_extension_GLX_NV_video_capture,
    GLX_extension_GLX_NV_video_output,
    GLX_extension_GLX_OML_sync_control,
    GLX_extension_GLX_SGIX_fbconfig,
    GLX_extension_GLX_SGIX_hyperpipe,
    GLX_extension_GLX_SGIX_pbuffer,
    GLX_extension_GLX_SGIX_swap_barrier,
    GLX_extension_GLX_SGIX_swap_group,
    GLX_extension_GLX_SGIX_video_resize,
    GLX_extension_GLX_SGIX_video_source,
    GLX_extension_GLX_SGI_cushion,
    GLX_extension_GLX_SGI_make_current_read,
    GLX_extension_GLX_SGI_swap_control,
    GLX_extension_GLX_SGI_video_sync,
    GLX_extension_GLX_SUN_get_transparent_index,
    always_present,
};

static const char *enum_strings[] = {
    [GLX_10] = "GLX 10",
    [GLX_11] = "GLX 11",
    [GLX_12] = "GLX 12",
    [GLX_13] = "GLX 13",
    [GLX_extension_GLX_ARB_create_context] = "GLX extension \"GLX_ARB_create_context\"",
    [GLX_extension_GLX_ARB_get_proc_address] = "GLX extension \"GLX_ARB_get_proc_address\"",
    [GLX_extension_GLX_EXT_import_context] = "GLX extension \"GLX_EXT_import_context\"",
    [GLX_extension_GLX_EXT_swap_control] = "GLX extension \"GLX_EXT_swap_control\"",
    [GLX_extension_GLX_EXT_texture_from_pixmap] = "GLX extension \"GLX_EXT_texture_from_pixmap\"",
    [GLX_extension_GLX_MESA_agp_offset] = "GLX extension \"GLX_MESA_agp_offset\"",
    [GLX_extension_GLX_MESA_copy_sub_buffer] = "GLX extension \"GLX_MESA_copy_sub_buffer\"",
    [GLX_extension_GLX_MESA_pixmap_colormap] = "GLX extension \"GLX_MESA_pixmap_colormap\"",
    [GLX_extension_GLX_MESA_query_renderer] = "GLX extension \"GLX_MESA_query_renderer\"",
    [GLX_extension_GLX_MESA_release_buffers] = "GLX extension \"GLX_MESA_release_buffers\"",
    [GLX_extension_GLX_MESA_set_3dfx_mode] = "GLX extension \"GLX_MESA_set_3dfx_mode\"",
    [GLX_extension_GLX_NV_copy_image] = "GLX extension \"GLX_NV_copy_image\"",
    [GLX_extension_GLX_NV_delay_before_swap] = "GLX extension \"GLX_NV_delay_before_swap\"",
    [GLX_extension_GLX_NV_present_video] = "GLX extension \"GLX_NV_present_video\"",
    [GLX_extension_GLX_NV_swap_group] = "GLX extension \"GLX_NV_swap_group\"",
    [GLX_extension_GLX_NV_video_capture] = "GLX extension \"GLX_NV_video_capture\"",
    [GLX_extension_GLX_NV_video_output] = "GLX extension \"GLX_NV_video_output\"",
    [GLX_extension_GLX_OML_sync_control] = "GLX extension \"GLX_OML_sync_control\"",
    [GLX_extension_GLX_SGIX_fbconfig] = "GLX extension \"GLX_SGIX_fbconfig\"",
    [GLX_extension_GLX_SGIX_hyperpipe] = "GLX extension \"GLX_SGIX_hyperpipe\"",
    [GLX_extension_GLX_SGIX_pbuffer] = "GLX extension \"GLX_SGIX_pbuffer\"",
    [GLX_extension_GLX_SGIX_swap_barrier] = "GLX extension \"GLX_SGIX_swap_barrier\"",
    [GLX_extension_GLX_SGIX_swap_group] = "GLX extension \"GLX_SGIX_swap_group\"",
    [GLX_extension_GLX_SGIX_video_resize] = "GLX extension \"GLX_SGIX_video_resize\"",
    [GLX_extension_GLX_SGIX_video_source] = "GLX extension \"GLX_SGIX_video_source\"",
    [GLX_extension_GLX_SGI_cushion] = "GLX extension \"GLX_SGI_cushion\"",
    [GLX_extension_GLX_SGI_make_current_read] = "GLX extension \"GLX_SGI_make_current_read\"",
    [GLX_extension_GLX_SGI_swap_control] = "GLX extension \"GLX_SGI_swap_control\"",
    [GLX_extension_GLX_SGI_video_sync] = "GLX extension \"GLX_SGI_video_sync\"",
    [GLX_extension_GLX_SUN_get_transparent_index] = "GLX extension \"GLX_SUN_get_transparent_index\"",
    [always_present] = "always present",
};

static const char entrypoint_strings[] = 
   "glXBindChannelToWindowSGIX\0"
   "glXBindHyperpipeSGIX\0"
   "glXBindSwapBarrierNV\0"
   "glXBindSwapBarrierSGIX\0"
   "glXBindTexImageEXT\0"
   "glXBindVideoCaptureDeviceNV\0"
   "glXBindVideoDeviceNV\0"
   "glXBindVideoImageNV\0"
   "glXChannelRectSGIX\0"
   "glXChannelRectSyncSGIX\0"
   "glXChooseFBConfig\0"
   "glXChooseFBConfigSGIX\0"
   "glXChooseVisual\0"
   "glXCopyContext\0"
   "glXCopyImageSubDataNV\0"
   "glXCopySubBufferMESA\0"
   "glXCreateContext\0"
   "glXCreateContextAttribsARB\0"
   "glXCreateContextWithConfigSGIX\0"
   "glXCreateGLXPbufferSGIX\0"
   "glXCreateGLXPixmap\0"
   "glXCreateGLXPixmapMESA\0"
   "glXCreateGLXPixmapWithConfigSGIX\0"
   "glXCreateNewContext\0"
   "glXCreatePbuffer\0"
   "glXCreatePixmap\0"
   "glXCreateWindow\0"
   "glXCushionSGI\0"
   "glXDelayBeforeSwapNV\0"
   "glXDestroyContext\0"
   "glXDestroyGLXPbufferSGIX\0"
   "glXDestroyGLXPixmap\0"
   "glXDestroyGLXVideoSourceSGIX\0"
   "glXDestroyHyperpipeConfigSGIX\0"
   "glXDestroyPbuffer\0"
   "glXDestroyPixmap\0"
   "glXDestroyWindow\0"
   "glXEnumerateVideoCaptureDevicesNV\0"
   "glXEnumerateVideoDevicesNV\0"
   "glXFreeContextEXT\0"
   "glXGetAGPOffsetMESA\0"
   "glXGetClientString\0"
   "glXGetConfig\0"
   "glXGetContextIDEXT\0"
   "glXGetCurrentContext\0"
   "glXGetCurrentDisplay\0"
   "glXGetCurrentDisplayEXT\0"
   "glXGetCurrentDrawable\0"
   "glXGetCurrentReadDrawable\0"
   "glXGetCurrentReadDrawableSGI\0"
   "glXGetFBConfigAttrib\0"
   "glXGetFBConfigAttribSGIX\0"
   "glXGetFBConfigFromVisualSGIX\0"
   "glXGetFBConfigs\0"
   "glXGetMscRateOML\0"
   "glXGetProcAddress\0"
   "glXGetProcAddressARB\0"
   "glXGetSelectedEvent\0"
   "glXGetSelectedEventSGIX\0"
   "glXGetSyncValuesOML\0"
   "glXGetTransparentIndexSUN\0"
   "glXGetVideoDeviceNV\0"
   "glXGetVideoInfoNV\0"
   "glXGetVideoSyncSGI\0"
   "glXGetVisualFromFBConfig\0"
   "glXGetVisualFromFBConfigSGIX\0"
   "glXHyperpipeAttribSGIX\0"
   "glXHyperpipeConfigSGIX\0"
   "glXImportContextEXT\0"
   "glXIsDirect\0"
   "glXJoinSwapGroupNV\0"
   "glXJoinSwapGroupSGIX\0"
   "glXLockVideoCaptureDeviceNV\0"
   "glXMakeContextCurrent\0"
   "glXMakeCurrent\0"
   "glXMakeCurrentReadSGI\0"
   "glXQueryChannelDeltasSGIX\0"
   "glXQueryChannelRectSGIX\0"
   "glXQueryContext\0"
   "glXQueryContextInfoEXT\0"
   "glXQueryCurrentRendererIntegerMESA\0"
   "glXQueryCurrentRendererStringMESA\0"
   "glXQueryDrawable\0"
   "glXQueryExtension\0"
   "glXQueryExtensionsString\0"
   "glXQueryFrameCountNV\0"
   "glXQueryGLXPbufferSGIX\0"
   "glXQueryHyperpipeAttribSGIX\0"
   "glXQueryHyperpipeBestAttribSGIX\0"
   "glXQueryHyperpipeConfigSGIX\0"
   "glXQueryHyperpipeNetworkSGIX\0"
   "glXQueryMaxSwapBarriersSGIX\0"
   "glXQueryMaxSwapGroupsNV\0"
   "glXQueryRendererIntegerMESA\0"
   "glXQueryRendererStringMESA\0"
   "glXQueryServerString\0"
   "glXQuerySwapGroupNV\0"
   "glXQueryVersion\0"
   "glXQueryVideoCaptureDeviceNV\0"
   "glXReleaseBuffersMESA\0"
   "glXReleaseTexImageEXT\0"
   "glXReleaseVideoCaptureDeviceNV\0"
   "glXReleaseVideoDeviceNV\0"
   "glXReleaseVideoImageNV\0"
   "glXResetFrameCountNV\0"
   "glXSelectEvent\0"
   "glXSelectEventSGIX\0"
   "glXSendPbufferToVideoNV\0"
   "glXSet3DfxModeMESA\0"
   "glXSwapBuffers\0"
   "glXSwapBuffersMscOML\0"
   "glXSwapIntervalEXT\0"
   "glXSwapIntervalSGI\0"
   "glXUseXFont\0"
   "glXWaitForMscOML\0"
   "glXWaitForSbcOML\0"
   "glXWaitGL\0"
   "glXWaitVideoSyncSGI\0"
   "glXWaitX\0"
    ;

static void *glx_provider_resolver(const char *name,
                                   const enum glx_provider *providers,
                                   const uint16_t *entrypoints)
{
    int i;
    for (i = 0; providers[i] != glx_provider_terminator; i++) {
        switch (providers[i]) {
        case GLX_10:
            if (true)
                return epoxy_glx_dlsym(entrypoint_strings + entrypoints[i]);
            break;
        case GLX_11:
            if (true)
                return epoxy_glx_dlsym(entrypoint_strings + entrypoints[i]);
            break;
        case GLX_12:
            if (true)
                return epoxy_glx_dlsym(entrypoint_strings + entrypoints[i]);
            break;
        case GLX_13:
            if (true)
                return epoxy_glx_dlsym(entrypoint_strings + entrypoints[i]);
            break;
        case GLX_extension_GLX_ARB_create_context:
            if (epoxy_conservative_has_glx_extension("GLX_ARB_create_context"))
                return glXGetProcAddress((const GLubyte *)entrypoint_strings + entrypoints[i]);
            break;
        case GLX_extension_GLX_ARB_get_proc_address:
            if (epoxy_conservative_has_glx_extension("GLX_ARB_get_proc_address"))
                return glXGetProcAddress((const GLubyte *)entrypoint_strings + entrypoints[i]);
            break;
        case GLX_extension_GLX_EXT_import_context:
            if (epoxy_conservative_has_glx_extension("GLX_EXT_import_context"))
                return glXGetProcAddress((const GLubyte *)entrypoint_strings + entrypoints[i]);
            break;
        case GLX_extension_GLX_EXT_swap_control:
            if (epoxy_conservative_has_glx_extension("GLX_EXT_swap_control"))
                return glXGetProcAddress((const GLubyte *)entrypoint_strings + entrypoints[i]);
            break;
        case GLX_extension_GLX_EXT_texture_from_pixmap:
            if (epoxy_conservative_has_glx_extension("GLX_EXT_texture_from_pixmap"))
                return glXGetProcAddress((const GLubyte *)entrypoint_strings + entrypoints[i]);
            break;
        case GLX_extension_GLX_MESA_agp_offset:
            if (epoxy_conservative_has_glx_extension("GLX_MESA_agp_offset"))
                return glXGetProcAddress((const GLubyte *)entrypoint_strings + entrypoints[i]);
            break;
        case GLX_extension_GLX_MESA_copy_sub_buffer:
            if (epoxy_conservative_has_glx_extension("GLX_MESA_copy_sub_buffer"))
                return glXGetProcAddress((const GLubyte *)entrypoint_strings + entrypoints[i]);
            break;
        case GLX_extension_GLX_MESA_pixmap_colormap:
            if (epoxy_conservative_has_glx_extension("GLX_MESA_pixmap_colormap"))
                return glXGetProcAddress((const GLubyte *)entrypoint_strings + entrypoints[i]);
            break;
        case GLX_extension_GLX_MESA_query_renderer:
            if (epoxy_conservative_has_glx_extension("GLX_MESA_query_renderer"))
                return glXGetProcAddress((const GLubyte *)entrypoint_strings + entrypoints[i]);
            break;
        case GLX_extension_GLX_MESA_release_buffers:
            if (epoxy_conservative_has_glx_extension("GLX_MESA_release_buffers"))
                return glXGetProcAddress((const GLubyte *)entrypoint_strings + entrypoints[i]);
            break;
        case GLX_extension_GLX_MESA_set_3dfx_mode:
            if (epoxy_conservative_has_glx_extension("GLX_MESA_set_3dfx_mode"))
                return glXGetProcAddress((const GLubyte *)entrypoint_strings + entrypoints[i]);
            break;
        case GLX_extension_GLX_NV_copy_image:
            if (epoxy_conservative_has_glx_extension("GLX_NV_copy_image"))
                return glXGetProcAddress((const GLubyte *)entrypoint_strings + entrypoints[i]);
            break;
        case GLX_extension_GLX_NV_delay_before_swap:
            if (epoxy_conservative_has_glx_extension("GLX_NV_delay_before_swap"))
                return glXGetProcAddress((const GLubyte *)entrypoint_strings + entrypoints[i]);
            break;
        case GLX_extension_GLX_NV_present_video:
            if (epoxy_conservative_has_glx_extension("GLX_NV_present_video"))
                return glXGetProcAddress((const GLubyte *)entrypoint_strings + entrypoints[i]);
            break;
        case GLX_extension_GLX_NV_swap_group:
            if (epoxy_conservative_has_glx_extension("GLX_NV_swap_group"))
                return glXGetProcAddress((const GLubyte *)entrypoint_strings + entrypoints[i]);
            break;
        case GLX_extension_GLX_NV_video_capture:
            if (epoxy_conservative_has_glx_extension("GLX_NV_video_capture"))
                return glXGetProcAddress((const GLubyte *)entrypoint_strings + entrypoints[i]);
            break;
        case GLX_extension_GLX_NV_video_output:
            if (epoxy_conservative_has_glx_extension("GLX_NV_video_output"))
                return glXGetProcAddress((const GLubyte *)entrypoint_strings + entrypoints[i]);
            break;
        case GLX_extension_GLX_OML_sync_control:
            if (epoxy_conservative_has_glx_extension("GLX_OML_sync_control"))
                return glXGetProcAddress((const GLubyte *)entrypoint_strings + entrypoints[i]);
            break;
        case GLX_extension_GLX_SGIX_fbconfig:
            if (epoxy_conservative_has_glx_extension("GLX_SGIX_fbconfig"))
                return glXGetProcAddress((const GLubyte *)entrypoint_strings + entrypoints[i]);
            break;
        case GLX_extension_GLX_SGIX_hyperpipe:
            if (epoxy_conservative_has_glx_extension("GLX_SGIX_hyperpipe"))
                return glXGetProcAddress((const GLubyte *)entrypoint_strings + entrypoints[i]);
            break;
        case GLX_extension_GLX_SGIX_pbuffer:
            if (epoxy_conservative_has_glx_extension("GLX_SGIX_pbuffer"))
                return glXGetProcAddress((const GLubyte *)entrypoint_strings + entrypoints[i]);
            break;
        case GLX_extension_GLX_SGIX_swap_barrier:
            if (epoxy_conservative_has_glx_extension("GLX_SGIX_swap_barrier"))
                return glXGetProcAddress((const GLubyte *)entrypoint_strings + entrypoints[i]);
            break;
        case GLX_extension_GLX_SGIX_swap_group:
            if (epoxy_conservative_has_glx_extension("GLX_SGIX_swap_group"))
                return glXGetProcAddress((const GLubyte *)entrypoint_strings + entrypoints[i]);
            break;
        case GLX_extension_GLX_SGIX_video_resize:
            if (epoxy_conservative_has_glx_extension("GLX_SGIX_video_resize"))
                return glXGetProcAddress((const GLubyte *)entrypoint_strings + entrypoints[i]);
            break;
        case GLX_extension_GLX_SGIX_video_source:
            if (epoxy_conservative_has_glx_extension("GLX_SGIX_video_source"))
                return glXGetProcAddress((const GLubyte *)entrypoint_strings + entrypoints[i]);
            break;
        case GLX_extension_GLX_SGI_cushion:
            if (epoxy_conservative_has_glx_extension("GLX_SGI_cushion"))
                return glXGetProcAddress((const GLubyte *)entrypoint_strings + entrypoints[i]);
            break;
        case GLX_extension_GLX_SGI_make_current_read:
            if (epoxy_conservative_has_glx_extension("GLX_SGI_make_current_read"))
                return glXGetProcAddress((const GLubyte *)entrypoint_strings + entrypoints[i]);
            break;
        case GLX_extension_GLX_SGI_swap_control:
            if (epoxy_conservative_has_glx_extension("GLX_SGI_swap_control"))
                return glXGetProcAddress((const GLubyte *)entrypoint_strings + entrypoints[i]);
            break;
        case GLX_extension_GLX_SGI_video_sync:
            if (epoxy_conservative_has_glx_extension("GLX_SGI_video_sync"))
                return glXGetProcAddress((const GLubyte *)entrypoint_strings + entrypoints[i]);
            break;
        case GLX_extension_GLX_SUN_get_transparent_index:
            if (epoxy_conservative_has_glx_extension("GLX_SUN_get_transparent_index"))
                return glXGetProcAddress((const GLubyte *)entrypoint_strings + entrypoints[i]);
            break;
        case always_present:
            if (true)
                return epoxy_glx_dlsym(entrypoint_strings + entrypoints[i]);
            break;
        case glx_provider_terminator:
            abort(); /* Not reached */
        }
    }

    epoxy_print_failure_reasons(name, enum_strings, (const int *)providers);
    abort();
}

static void *
glx_single_resolver(enum glx_provider provider, uint16_t entrypoint_offset) __attribute__((noinline));

static void *
glx_single_resolver(enum glx_provider provider, uint16_t entrypoint_offset)
{
    enum glx_provider providers[] = {
        provider,
        glx_provider_terminator
    };
    return glx_provider_resolver(entrypoint_strings + entrypoint_offset,
                                providers, &entrypoint_offset);
}

static PFNGLXBINDCHANNELTOWINDOWSGIXPROC
epoxy_glXBindChannelToWindowSGIX_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_SGIX_video_resize, 0 /* glXBindChannelToWindowSGIX */);
}

static PFNGLXBINDHYPERPIPESGIXPROC
epoxy_glXBindHyperpipeSGIX_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_SGIX_hyperpipe, 27 /* glXBindHyperpipeSGIX */);
}

static PFNGLXBINDSWAPBARRIERNVPROC
epoxy_glXBindSwapBarrierNV_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_NV_swap_group, 48 /* glXBindSwapBarrierNV */);
}

static PFNGLXBINDSWAPBARRIERSGIXPROC
epoxy_glXBindSwapBarrierSGIX_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_SGIX_swap_barrier, 69 /* glXBindSwapBarrierSGIX */);
}

static PFNGLXBINDTEXIMAGEEXTPROC
epoxy_glXBindTexImageEXT_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_EXT_texture_from_pixmap, 92 /* glXBindTexImageEXT */);
}

static PFNGLXBINDVIDEOCAPTUREDEVICENVPROC
epoxy_glXBindVideoCaptureDeviceNV_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_NV_video_capture, 111 /* glXBindVideoCaptureDeviceNV */);
}

static PFNGLXBINDVIDEODEVICENVPROC
epoxy_glXBindVideoDeviceNV_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_NV_present_video, 139 /* glXBindVideoDeviceNV */);
}

static PFNGLXBINDVIDEOIMAGENVPROC
epoxy_glXBindVideoImageNV_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_NV_video_output, 160 /* glXBindVideoImageNV */);
}

static PFNGLXCHANNELRECTSGIXPROC
epoxy_glXChannelRectSGIX_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_SGIX_video_resize, 180 /* glXChannelRectSGIX */);
}

static PFNGLXCHANNELRECTSYNCSGIXPROC
epoxy_glXChannelRectSyncSGIX_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_SGIX_video_resize, 199 /* glXChannelRectSyncSGIX */);
}

static PFNGLXCHOOSEFBCONFIGPROC
epoxy_glXChooseFBConfig_resolver(void)
{
    return glx_single_resolver(GLX_13, 222 /* glXChooseFBConfig */);
}

static PFNGLXCHOOSEFBCONFIGSGIXPROC
epoxy_glXChooseFBConfigSGIX_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_SGIX_fbconfig, 240 /* glXChooseFBConfigSGIX */);
}

static PFNGLXCHOOSEVISUALPROC
epoxy_glXChooseVisual_resolver(void)
{
    return glx_single_resolver(GLX_10, 262 /* glXChooseVisual */);
}

static PFNGLXCOPYCONTEXTPROC
epoxy_glXCopyContext_resolver(void)
{
    return glx_single_resolver(GLX_10, 278 /* glXCopyContext */);
}

static PFNGLXCOPYIMAGESUBDATANVPROC
epoxy_glXCopyImageSubDataNV_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_NV_copy_image, 293 /* glXCopyImageSubDataNV */);
}

static PFNGLXCOPYSUBBUFFERMESAPROC
epoxy_glXCopySubBufferMESA_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_MESA_copy_sub_buffer, 315 /* glXCopySubBufferMESA */);
}

static PFNGLXCREATECONTEXTPROC
epoxy_glXCreateContext_resolver(void)
{
    return glx_single_resolver(GLX_10, 336 /* glXCreateContext */);
}

static PFNGLXCREATECONTEXTATTRIBSARBPROC
epoxy_glXCreateContextAttribsARB_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_ARB_create_context, 353 /* glXCreateContextAttribsARB */);
}

static PFNGLXCREATECONTEXTWITHCONFIGSGIXPROC
epoxy_glXCreateContextWithConfigSGIX_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_SGIX_fbconfig, 380 /* glXCreateContextWithConfigSGIX */);
}

static PFNGLXCREATEGLXPBUFFERSGIXPROC
epoxy_glXCreateGLXPbufferSGIX_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_SGIX_pbuffer, 411 /* glXCreateGLXPbufferSGIX */);
}

static PFNGLXCREATEGLXPIXMAPPROC
epoxy_glXCreateGLXPixmap_resolver(void)
{
    return glx_single_resolver(GLX_10, 435 /* glXCreateGLXPixmap */);
}

static PFNGLXCREATEGLXPIXMAPMESAPROC
epoxy_glXCreateGLXPixmapMESA_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_MESA_pixmap_colormap, 454 /* glXCreateGLXPixmapMESA */);
}

static PFNGLXCREATEGLXPIXMAPWITHCONFIGSGIXPROC
epoxy_glXCreateGLXPixmapWithConfigSGIX_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_SGIX_fbconfig, 477 /* glXCreateGLXPixmapWithConfigSGIX */);
}

static PFNGLXCREATENEWCONTEXTPROC
epoxy_glXCreateNewContext_resolver(void)
{
    return glx_single_resolver(GLX_13, 510 /* glXCreateNewContext */);
}

static PFNGLXCREATEPBUFFERPROC
epoxy_glXCreatePbuffer_resolver(void)
{
    return glx_single_resolver(GLX_13, 530 /* glXCreatePbuffer */);
}

static PFNGLXCREATEPIXMAPPROC
epoxy_glXCreatePixmap_resolver(void)
{
    return glx_single_resolver(GLX_13, 547 /* glXCreatePixmap */);
}

static PFNGLXCREATEWINDOWPROC
epoxy_glXCreateWindow_resolver(void)
{
    return glx_single_resolver(GLX_13, 563 /* glXCreateWindow */);
}

static PFNGLXCUSHIONSGIPROC
epoxy_glXCushionSGI_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_SGI_cushion, 579 /* glXCushionSGI */);
}

static PFNGLXDELAYBEFORESWAPNVPROC
epoxy_glXDelayBeforeSwapNV_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_NV_delay_before_swap, 593 /* glXDelayBeforeSwapNV */);
}

static PFNGLXDESTROYCONTEXTPROC
epoxy_glXDestroyContext_resolver(void)
{
    return glx_single_resolver(GLX_10, 614 /* glXDestroyContext */);
}

static PFNGLXDESTROYGLXPBUFFERSGIXPROC
epoxy_glXDestroyGLXPbufferSGIX_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_SGIX_pbuffer, 632 /* glXDestroyGLXPbufferSGIX */);
}

static PFNGLXDESTROYGLXPIXMAPPROC
epoxy_glXDestroyGLXPixmap_resolver(void)
{
    return glx_single_resolver(GLX_10, 657 /* glXDestroyGLXPixmap */);
}

static PFNGLXDESTROYGLXVIDEOSOURCESGIXPROC
epoxy_glXDestroyGLXVideoSourceSGIX_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_SGIX_video_source, 677 /* glXDestroyGLXVideoSourceSGIX */);
}

static PFNGLXDESTROYHYPERPIPECONFIGSGIXPROC
epoxy_glXDestroyHyperpipeConfigSGIX_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_SGIX_hyperpipe, 706 /* glXDestroyHyperpipeConfigSGIX */);
}

static PFNGLXDESTROYPBUFFERPROC
epoxy_glXDestroyPbuffer_resolver(void)
{
    return glx_single_resolver(GLX_13, 736 /* glXDestroyPbuffer */);
}

static PFNGLXDESTROYPIXMAPPROC
epoxy_glXDestroyPixmap_resolver(void)
{
    return glx_single_resolver(GLX_13, 754 /* glXDestroyPixmap */);
}

static PFNGLXDESTROYWINDOWPROC
epoxy_glXDestroyWindow_resolver(void)
{
    return glx_single_resolver(GLX_13, 771 /* glXDestroyWindow */);
}

static PFNGLXENUMERATEVIDEOCAPTUREDEVICESNVPROC
epoxy_glXEnumerateVideoCaptureDevicesNV_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_NV_video_capture, 788 /* glXEnumerateVideoCaptureDevicesNV */);
}

static PFNGLXENUMERATEVIDEODEVICESNVPROC
epoxy_glXEnumerateVideoDevicesNV_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_NV_present_video, 822 /* glXEnumerateVideoDevicesNV */);
}

static PFNGLXFREECONTEXTEXTPROC
epoxy_glXFreeContextEXT_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_EXT_import_context, 849 /* glXFreeContextEXT */);
}

static PFNGLXGETAGPOFFSETMESAPROC
epoxy_glXGetAGPOffsetMESA_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_MESA_agp_offset, 867 /* glXGetAGPOffsetMESA */);
}

static PFNGLXGETCLIENTSTRINGPROC
epoxy_glXGetClientString_resolver(void)
{
    return glx_single_resolver(GLX_11, 887 /* glXGetClientString */);
}

static PFNGLXGETCONFIGPROC
epoxy_glXGetConfig_resolver(void)
{
    return glx_single_resolver(GLX_10, 906 /* glXGetConfig */);
}

static PFNGLXGETCONTEXTIDEXTPROC
epoxy_glXGetContextIDEXT_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_EXT_import_context, 919 /* glXGetContextIDEXT */);
}

static PFNGLXGETCURRENTCONTEXTPROC
epoxy_glXGetCurrentContext_resolver(void)
{
    return glx_single_resolver(GLX_10, 938 /* glXGetCurrentContext */);
}

static PFNGLXGETCURRENTDISPLAYPROC
epoxy_glXGetCurrentDisplay_resolver(void)
{
    return glx_single_resolver(GLX_12, 959 /* glXGetCurrentDisplay */);
}

static PFNGLXGETCURRENTDISPLAYEXTPROC
epoxy_glXGetCurrentDisplayEXT_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_EXT_import_context, 980 /* glXGetCurrentDisplayEXT */);
}

static PFNGLXGETCURRENTDRAWABLEPROC
epoxy_glXGetCurrentDrawable_resolver(void)
{
    return glx_single_resolver(GLX_10, 1004 /* glXGetCurrentDrawable */);
}

static PFNGLXGETCURRENTREADDRAWABLEPROC
epoxy_glXGetCurrentReadDrawable_resolver(void)
{
    return glx_single_resolver(GLX_13, 1026 /* glXGetCurrentReadDrawable */);
}

static PFNGLXGETCURRENTREADDRAWABLESGIPROC
epoxy_glXGetCurrentReadDrawableSGI_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_SGI_make_current_read, 1052 /* glXGetCurrentReadDrawableSGI */);
}

static PFNGLXGETFBCONFIGATTRIBPROC
epoxy_glXGetFBConfigAttrib_resolver(void)
{
    return glx_single_resolver(GLX_13, 1081 /* glXGetFBConfigAttrib */);
}

static PFNGLXGETFBCONFIGATTRIBSGIXPROC
epoxy_glXGetFBConfigAttribSGIX_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_SGIX_fbconfig, 1102 /* glXGetFBConfigAttribSGIX */);
}

static PFNGLXGETFBCONFIGFROMVISUALSGIXPROC
epoxy_glXGetFBConfigFromVisualSGIX_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_SGIX_fbconfig, 1127 /* glXGetFBConfigFromVisualSGIX */);
}

static PFNGLXGETFBCONFIGSPROC
epoxy_glXGetFBConfigs_resolver(void)
{
    return glx_single_resolver(GLX_13, 1156 /* glXGetFBConfigs */);
}

static PFNGLXGETMSCRATEOMLPROC
epoxy_glXGetMscRateOML_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_OML_sync_control, 1172 /* glXGetMscRateOML */);
}

static PFNGLXGETPROCADDRESSPROC
epoxy_glXGetProcAddress_resolver(void)
{
    return glx_single_resolver(always_present, 1189 /* glXGetProcAddress */);
}

static PFNGLXGETPROCADDRESSARBPROC
epoxy_glXGetProcAddressARB_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_ARB_get_proc_address, 1207 /* glXGetProcAddressARB */);
}

static PFNGLXGETSELECTEDEVENTPROC
epoxy_glXGetSelectedEvent_resolver(void)
{
    return glx_single_resolver(GLX_13, 1228 /* glXGetSelectedEvent */);
}

static PFNGLXGETSELECTEDEVENTSGIXPROC
epoxy_glXGetSelectedEventSGIX_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_SGIX_pbuffer, 1248 /* glXGetSelectedEventSGIX */);
}

static PFNGLXGETSYNCVALUESOMLPROC
epoxy_glXGetSyncValuesOML_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_OML_sync_control, 1272 /* glXGetSyncValuesOML */);
}

static PFNGLXGETTRANSPARENTINDEXSUNPROC
epoxy_glXGetTransparentIndexSUN_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_SUN_get_transparent_index, 1292 /* glXGetTransparentIndexSUN */);
}

static PFNGLXGETVIDEODEVICENVPROC
epoxy_glXGetVideoDeviceNV_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_NV_video_output, 1318 /* glXGetVideoDeviceNV */);
}

static PFNGLXGETVIDEOINFONVPROC
epoxy_glXGetVideoInfoNV_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_NV_video_output, 1338 /* glXGetVideoInfoNV */);
}

static PFNGLXGETVIDEOSYNCSGIPROC
epoxy_glXGetVideoSyncSGI_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_SGI_video_sync, 1356 /* glXGetVideoSyncSGI */);
}

static PFNGLXGETVISUALFROMFBCONFIGPROC
epoxy_glXGetVisualFromFBConfig_resolver(void)
{
    return glx_single_resolver(GLX_13, 1375 /* glXGetVisualFromFBConfig */);
}

static PFNGLXGETVISUALFROMFBCONFIGSGIXPROC
epoxy_glXGetVisualFromFBConfigSGIX_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_SGIX_fbconfig, 1400 /* glXGetVisualFromFBConfigSGIX */);
}

static PFNGLXHYPERPIPEATTRIBSGIXPROC
epoxy_glXHyperpipeAttribSGIX_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_SGIX_hyperpipe, 1429 /* glXHyperpipeAttribSGIX */);
}

static PFNGLXHYPERPIPECONFIGSGIXPROC
epoxy_glXHyperpipeConfigSGIX_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_SGIX_hyperpipe, 1452 /* glXHyperpipeConfigSGIX */);
}

static PFNGLXIMPORTCONTEXTEXTPROC
epoxy_glXImportContextEXT_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_EXT_import_context, 1475 /* glXImportContextEXT */);
}

static PFNGLXISDIRECTPROC
epoxy_glXIsDirect_resolver(void)
{
    return glx_single_resolver(GLX_10, 1495 /* glXIsDirect */);
}

static PFNGLXJOINSWAPGROUPNVPROC
epoxy_glXJoinSwapGroupNV_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_NV_swap_group, 1507 /* glXJoinSwapGroupNV */);
}

static PFNGLXJOINSWAPGROUPSGIXPROC
epoxy_glXJoinSwapGroupSGIX_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_SGIX_swap_group, 1526 /* glXJoinSwapGroupSGIX */);
}

static PFNGLXLOCKVIDEOCAPTUREDEVICENVPROC
epoxy_glXLockVideoCaptureDeviceNV_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_NV_video_capture, 1547 /* glXLockVideoCaptureDeviceNV */);
}

static PFNGLXMAKECONTEXTCURRENTPROC
epoxy_glXMakeContextCurrent_resolver(void)
{
    return glx_single_resolver(GLX_13, 1575 /* glXMakeContextCurrent */);
}

static PFNGLXMAKECURRENTPROC
epoxy_glXMakeCurrent_resolver(void)
{
    return glx_single_resolver(GLX_10, 1597 /* glXMakeCurrent */);
}

static PFNGLXMAKECURRENTREADSGIPROC
epoxy_glXMakeCurrentReadSGI_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_SGI_make_current_read, 1612 /* glXMakeCurrentReadSGI */);
}

static PFNGLXQUERYCHANNELDELTASSGIXPROC
epoxy_glXQueryChannelDeltasSGIX_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_SGIX_video_resize, 1634 /* glXQueryChannelDeltasSGIX */);
}

static PFNGLXQUERYCHANNELRECTSGIXPROC
epoxy_glXQueryChannelRectSGIX_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_SGIX_video_resize, 1660 /* glXQueryChannelRectSGIX */);
}

static PFNGLXQUERYCONTEXTPROC
epoxy_glXQueryContext_resolver(void)
{
    return glx_single_resolver(GLX_13, 1684 /* glXQueryContext */);
}

static PFNGLXQUERYCONTEXTINFOEXTPROC
epoxy_glXQueryContextInfoEXT_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_EXT_import_context, 1700 /* glXQueryContextInfoEXT */);
}

static PFNGLXQUERYCURRENTRENDERERINTEGERMESAPROC
epoxy_glXQueryCurrentRendererIntegerMESA_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_MESA_query_renderer, 1723 /* glXQueryCurrentRendererIntegerMESA */);
}

static PFNGLXQUERYCURRENTRENDERERSTRINGMESAPROC
epoxy_glXQueryCurrentRendererStringMESA_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_MESA_query_renderer, 1758 /* glXQueryCurrentRendererStringMESA */);
}

static PFNGLXQUERYDRAWABLEPROC
epoxy_glXQueryDrawable_resolver(void)
{
    return glx_single_resolver(GLX_13, 1792 /* glXQueryDrawable */);
}

static PFNGLXQUERYEXTENSIONPROC
epoxy_glXQueryExtension_resolver(void)
{
    return glx_single_resolver(GLX_10, 1809 /* glXQueryExtension */);
}

static PFNGLXQUERYEXTENSIONSSTRINGPROC
epoxy_glXQueryExtensionsString_resolver(void)
{
    return glx_single_resolver(GLX_11, 1827 /* glXQueryExtensionsString */);
}

static PFNGLXQUERYFRAMECOUNTNVPROC
epoxy_glXQueryFrameCountNV_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_NV_swap_group, 1852 /* glXQueryFrameCountNV */);
}

static PFNGLXQUERYGLXPBUFFERSGIXPROC
epoxy_glXQueryGLXPbufferSGIX_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_SGIX_pbuffer, 1873 /* glXQueryGLXPbufferSGIX */);
}

static PFNGLXQUERYHYPERPIPEATTRIBSGIXPROC
epoxy_glXQueryHyperpipeAttribSGIX_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_SGIX_hyperpipe, 1896 /* glXQueryHyperpipeAttribSGIX */);
}

static PFNGLXQUERYHYPERPIPEBESTATTRIBSGIXPROC
epoxy_glXQueryHyperpipeBestAttribSGIX_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_SGIX_hyperpipe, 1924 /* glXQueryHyperpipeBestAttribSGIX */);
}

static PFNGLXQUERYHYPERPIPECONFIGSGIXPROC
epoxy_glXQueryHyperpipeConfigSGIX_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_SGIX_hyperpipe, 1956 /* glXQueryHyperpipeConfigSGIX */);
}

static PFNGLXQUERYHYPERPIPENETWORKSGIXPROC
epoxy_glXQueryHyperpipeNetworkSGIX_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_SGIX_hyperpipe, 1984 /* glXQueryHyperpipeNetworkSGIX */);
}

static PFNGLXQUERYMAXSWAPBARRIERSSGIXPROC
epoxy_glXQueryMaxSwapBarriersSGIX_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_SGIX_swap_barrier, 2013 /* glXQueryMaxSwapBarriersSGIX */);
}

static PFNGLXQUERYMAXSWAPGROUPSNVPROC
epoxy_glXQueryMaxSwapGroupsNV_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_NV_swap_group, 2041 /* glXQueryMaxSwapGroupsNV */);
}

static PFNGLXQUERYRENDERERINTEGERMESAPROC
epoxy_glXQueryRendererIntegerMESA_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_MESA_query_renderer, 2065 /* glXQueryRendererIntegerMESA */);
}

static PFNGLXQUERYRENDERERSTRINGMESAPROC
epoxy_glXQueryRendererStringMESA_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_MESA_query_renderer, 2093 /* glXQueryRendererStringMESA */);
}

static PFNGLXQUERYSERVERSTRINGPROC
epoxy_glXQueryServerString_resolver(void)
{
    return glx_single_resolver(GLX_11, 2120 /* glXQueryServerString */);
}

static PFNGLXQUERYSWAPGROUPNVPROC
epoxy_glXQuerySwapGroupNV_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_NV_swap_group, 2141 /* glXQuerySwapGroupNV */);
}

static PFNGLXQUERYVERSIONPROC
epoxy_glXQueryVersion_resolver(void)
{
    return glx_single_resolver(GLX_10, 2161 /* glXQueryVersion */);
}

static PFNGLXQUERYVIDEOCAPTUREDEVICENVPROC
epoxy_glXQueryVideoCaptureDeviceNV_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_NV_video_capture, 2177 /* glXQueryVideoCaptureDeviceNV */);
}

static PFNGLXRELEASEBUFFERSMESAPROC
epoxy_glXReleaseBuffersMESA_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_MESA_release_buffers, 2206 /* glXReleaseBuffersMESA */);
}

static PFNGLXRELEASETEXIMAGEEXTPROC
epoxy_glXReleaseTexImageEXT_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_EXT_texture_from_pixmap, 2228 /* glXReleaseTexImageEXT */);
}

static PFNGLXRELEASEVIDEOCAPTUREDEVICENVPROC
epoxy_glXReleaseVideoCaptureDeviceNV_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_NV_video_capture, 2250 /* glXReleaseVideoCaptureDeviceNV */);
}

static PFNGLXRELEASEVIDEODEVICENVPROC
epoxy_glXReleaseVideoDeviceNV_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_NV_video_output, 2281 /* glXReleaseVideoDeviceNV */);
}

static PFNGLXRELEASEVIDEOIMAGENVPROC
epoxy_glXReleaseVideoImageNV_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_NV_video_output, 2305 /* glXReleaseVideoImageNV */);
}

static PFNGLXRESETFRAMECOUNTNVPROC
epoxy_glXResetFrameCountNV_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_NV_swap_group, 2328 /* glXResetFrameCountNV */);
}

static PFNGLXSELECTEVENTPROC
epoxy_glXSelectEvent_resolver(void)
{
    return glx_single_resolver(GLX_13, 2349 /* glXSelectEvent */);
}

static PFNGLXSELECTEVENTSGIXPROC
epoxy_glXSelectEventSGIX_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_SGIX_pbuffer, 2364 /* glXSelectEventSGIX */);
}

static PFNGLXSENDPBUFFERTOVIDEONVPROC
epoxy_glXSendPbufferToVideoNV_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_NV_video_output, 2383 /* glXSendPbufferToVideoNV */);
}

static PFNGLXSET3DFXMODEMESAPROC
epoxy_glXSet3DfxModeMESA_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_MESA_set_3dfx_mode, 2407 /* glXSet3DfxModeMESA */);
}

static PFNGLXSWAPBUFFERSPROC
epoxy_glXSwapBuffers_resolver(void)
{
    return glx_single_resolver(GLX_10, 2426 /* glXSwapBuffers */);
}

static PFNGLXSWAPBUFFERSMSCOMLPROC
epoxy_glXSwapBuffersMscOML_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_OML_sync_control, 2441 /* glXSwapBuffersMscOML */);
}

static PFNGLXSWAPINTERVALEXTPROC
epoxy_glXSwapIntervalEXT_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_EXT_swap_control, 2462 /* glXSwapIntervalEXT */);
}

static PFNGLXSWAPINTERVALSGIPROC
epoxy_glXSwapIntervalSGI_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_SGI_swap_control, 2481 /* glXSwapIntervalSGI */);
}

static PFNGLXUSEXFONTPROC
epoxy_glXUseXFont_resolver(void)
{
    return glx_single_resolver(GLX_10, 2500 /* glXUseXFont */);
}

static PFNGLXWAITFORMSCOMLPROC
epoxy_glXWaitForMscOML_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_OML_sync_control, 2512 /* glXWaitForMscOML */);
}

static PFNGLXWAITFORSBCOMLPROC
epoxy_glXWaitForSbcOML_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_OML_sync_control, 2529 /* glXWaitForSbcOML */);
}

static PFNGLXWAITGLPROC
epoxy_glXWaitGL_resolver(void)
{
    return glx_single_resolver(GLX_10, 2546 /* glXWaitGL */);
}

static PFNGLXWAITVIDEOSYNCSGIPROC
epoxy_glXWaitVideoSyncSGI_resolver(void)
{
    return glx_single_resolver(GLX_extension_GLX_SGI_video_sync, 2556 /* glXWaitVideoSyncSGI */);
}

static PFNGLXWAITXPROC
epoxy_glXWaitX_resolver(void)
{
    return glx_single_resolver(GLX_10, 2576 /* glXWaitX */);
}

GEN_THUNKS_RET(int, glXBindChannelToWindowSGIX, (Display * display, int screen, int channel, Window window), (display, screen, channel, window))
GEN_THUNKS_RET(int, glXBindHyperpipeSGIX, (Display * dpy, int hpId), (dpy, hpId))
GEN_THUNKS_RET(Bool, glXBindSwapBarrierNV, (Display * dpy, GLuint group, GLuint barrier), (dpy, group, barrier))
GEN_THUNKS(glXBindSwapBarrierSGIX, (Display * dpy, GLXDrawable drawable, int barrier), (dpy, drawable, barrier))
GEN_THUNKS(glXBindTexImageEXT, (Display * dpy, GLXDrawable drawable, int buffer, const int * attrib_list), (dpy, drawable, buffer, attrib_list))
GEN_THUNKS_RET(int, glXBindVideoCaptureDeviceNV, (Display * dpy, unsigned int video_capture_slot, GLXVideoCaptureDeviceNV device), (dpy, video_capture_slot, device))
GEN_THUNKS_RET(int, glXBindVideoDeviceNV, (Display * dpy, unsigned int video_slot, unsigned int video_device, const int * attrib_list), (dpy, video_slot, video_device, attrib_list))
GEN_THUNKS_RET(int, glXBindVideoImageNV, (Display * dpy, GLXVideoDeviceNV VideoDevice, GLXPbuffer pbuf, int iVideoBuffer), (dpy, VideoDevice, pbuf, iVideoBuffer))
GEN_THUNKS_RET(int, glXChannelRectSGIX, (Display * display, int screen, int channel, int x, int y, int w, int h), (display, screen, channel, x, y, w, h))
GEN_THUNKS_RET(int, glXChannelRectSyncSGIX, (Display * display, int screen, int channel, GLenum synctype), (display, screen, channel, synctype))
GEN_THUNKS_RET(GLXFBConfig *, glXChooseFBConfig, (Display * dpy, int screen, const int * attrib_list, int * nelements), (dpy, screen, attrib_list, nelements))
GEN_THUNKS_RET(GLXFBConfigSGIX *, glXChooseFBConfigSGIX, (Display * dpy, int screen, int * attrib_list, int * nelements), (dpy, screen, attrib_list, nelements))
GEN_THUNKS_RET(XVisualInfo *, glXChooseVisual, (Display * dpy, int screen, int * attribList), (dpy, screen, attribList))
GEN_THUNKS(glXCopyContext, (Display * dpy, GLXContext src, GLXContext dst, unsigned long mask), (dpy, src, dst, mask))
GEN_THUNKS(glXCopyImageSubDataNV, (Display * dpy, GLXContext srcCtx, GLuint srcName, GLenum srcTarget, GLint srcLevel, GLint srcX, GLint srcY, GLint srcZ, GLXContext dstCtx, GLuint dstName, GLenum dstTarget, GLint dstLevel, GLint dstX, GLint dstY, GLint dstZ, GLsizei width, GLsizei height, GLsizei depth), (dpy, srcCtx, srcName, srcTarget, srcLevel, srcX, srcY, srcZ, dstCtx, dstName, dstTarget, dstLevel, dstX, dstY, dstZ, width, height, depth))
GEN_THUNKS(glXCopySubBufferMESA, (Display * dpy, GLXDrawable drawable, int x, int y, int width, int height), (dpy, drawable, x, y, width, height))
GEN_THUNKS_RET(GLXContext, glXCreateContext, (Display * dpy, XVisualInfo * vis, GLXContext shareList, Bool direct), (dpy, vis, shareList, direct))
GEN_THUNKS_RET(GLXContext, glXCreateContextAttribsARB, (Display * dpy, GLXFBConfig config, GLXContext share_context, Bool direct, const int * attrib_list), (dpy, config, share_context, direct, attrib_list))
GEN_THUNKS_RET(GLXContext, glXCreateContextWithConfigSGIX, (Display * dpy, GLXFBConfigSGIX config, int render_type, GLXContext share_list, Bool direct), (dpy, config, render_type, share_list, direct))
GEN_THUNKS_RET(GLXPbufferSGIX, glXCreateGLXPbufferSGIX, (Display * dpy, GLXFBConfigSGIX config, unsigned int width, unsigned int height, int * attrib_list), (dpy, config, width, height, attrib_list))
GEN_THUNKS_RET(GLXPixmap, glXCreateGLXPixmap, (Display * dpy, XVisualInfo * visual, Pixmap pixmap), (dpy, visual, pixmap))
GEN_THUNKS_RET(GLXPixmap, glXCreateGLXPixmapMESA, (Display * dpy, XVisualInfo * visual, Pixmap pixmap, Colormap cmap), (dpy, visual, pixmap, cmap))
GEN_THUNKS_RET(GLXPixmap, glXCreateGLXPixmapWithConfigSGIX, (Display * dpy, GLXFBConfigSGIX config, Pixmap pixmap), (dpy, config, pixmap))
GEN_THUNKS_RET(GLXContext, glXCreateNewContext, (Display * dpy, GLXFBConfig config, int render_type, GLXContext share_list, Bool direct), (dpy, config, render_type, share_list, direct))
GEN_THUNKS_RET(GLXPbuffer, glXCreatePbuffer, (Display * dpy, GLXFBConfig config, const int * attrib_list), (dpy, config, attrib_list))
GEN_THUNKS_RET(GLXPixmap, glXCreatePixmap, (Display * dpy, GLXFBConfig config, Pixmap pixmap, const int * attrib_list), (dpy, config, pixmap, attrib_list))
GEN_THUNKS_RET(GLXWindow, glXCreateWindow, (Display * dpy, GLXFBConfig config, Window win, const int * attrib_list), (dpy, config, win, attrib_list))
GEN_THUNKS(glXCushionSGI, (Display * dpy, Window window, float cushion), (dpy, window, cushion))
GEN_THUNKS_RET(Bool, glXDelayBeforeSwapNV, (Display * dpy, GLXDrawable drawable, GLfloat seconds), (dpy, drawable, seconds))
GEN_THUNKS(glXDestroyContext, (Display * dpy, GLXContext ctx), (dpy, ctx))
GEN_THUNKS(glXDestroyGLXPbufferSGIX, (Display * dpy, GLXPbufferSGIX pbuf), (dpy, pbuf))
GEN_THUNKS(glXDestroyGLXPixmap, (Display * dpy, GLXPixmap pixmap), (dpy, pixmap))
GEN_THUNKS(glXDestroyGLXVideoSourceSGIX, (Display * dpy, GLXVideoSourceSGIX glxvideosource), (dpy, glxvideosource))
GEN_THUNKS_RET(int, glXDestroyHyperpipeConfigSGIX, (Display * dpy, int hpId), (dpy, hpId))
GEN_THUNKS(glXDestroyPbuffer, (Display * dpy, GLXPbuffer pbuf), (dpy, pbuf))
GEN_THUNKS(glXDestroyPixmap, (Display * dpy, GLXPixmap pixmap), (dpy, pixmap))
GEN_THUNKS(glXDestroyWindow, (Display * dpy, GLXWindow win), (dpy, win))
GEN_THUNKS_RET(GLXVideoCaptureDeviceNV *, glXEnumerateVideoCaptureDevicesNV, (Display * dpy, int screen, int * nelements), (dpy, screen, nelements))
GEN_THUNKS_RET(unsigned int *, glXEnumerateVideoDevicesNV, (Display * dpy, int screen, int * nelements), (dpy, screen, nelements))
GEN_THUNKS(glXFreeContextEXT, (Display * dpy, GLXContext context), (dpy, context))
GEN_THUNKS_RET(unsigned int, glXGetAGPOffsetMESA, (const void * pointer), (pointer))
GEN_THUNKS_RET(const char *, glXGetClientString, (Display * dpy, int name), (dpy, name))
GEN_THUNKS_RET(int, glXGetConfig, (Display * dpy, XVisualInfo * visual, int attrib, int * value), (dpy, visual, attrib, value))
GEN_THUNKS_RET(GLXContextID, glXGetContextIDEXT, (const GLXContext context), (context))
GEN_THUNKS_RET(GLXContext, glXGetCurrentContext, (void), ())
GEN_THUNKS_RET(Display *, glXGetCurrentDisplay, (void), ())
GEN_THUNKS_RET(Display *, glXGetCurrentDisplayEXT, (void), ())
GEN_THUNKS_RET(GLXDrawable, glXGetCurrentDrawable, (void), ())
GEN_THUNKS_RET(GLXDrawable, glXGetCurrentReadDrawable, (void), ())
GEN_THUNKS_RET(GLXDrawable, glXGetCurrentReadDrawableSGI, (void), ())
GEN_THUNKS_RET(int, glXGetFBConfigAttrib, (Display * dpy, GLXFBConfig config, int attribute, int * value), (dpy, config, attribute, value))
GEN_THUNKS_RET(int, glXGetFBConfigAttribSGIX, (Display * dpy, GLXFBConfigSGIX config, int attribute, int * value), (dpy, config, attribute, value))
GEN_THUNKS_RET(GLXFBConfigSGIX, glXGetFBConfigFromVisualSGIX, (Display * dpy, XVisualInfo * vis), (dpy, vis))
GEN_THUNKS_RET(GLXFBConfig *, glXGetFBConfigs, (Display * dpy, int screen, int * nelements), (dpy, screen, nelements))
GEN_THUNKS_RET(Bool, glXGetMscRateOML, (Display * dpy, GLXDrawable drawable, int32_t * numerator, int32_t * denominator), (dpy, drawable, numerator, denominator))
GEN_THUNKS_RET(__GLXextFuncPtr, glXGetProcAddress, (const GLubyte * procName), (procName))
GEN_THUNKS_RET(__GLXextFuncPtr, glXGetProcAddressARB, (const GLubyte * procName), (procName))
GEN_THUNKS(glXGetSelectedEvent, (Display * dpy, GLXDrawable draw, unsigned long * event_mask), (dpy, draw, event_mask))
GEN_THUNKS(glXGetSelectedEventSGIX, (Display * dpy, GLXDrawable drawable, unsigned long * mask), (dpy, drawable, mask))
GEN_THUNKS_RET(Bool, glXGetSyncValuesOML, (Display * dpy, GLXDrawable drawable, int64_t * ust, int64_t * msc, int64_t * sbc), (dpy, drawable, ust, msc, sbc))
GEN_THUNKS_RET(Status, glXGetTransparentIndexSUN, (Display * dpy, Window overlay, Window underlay, long * pTransparentIndex), (dpy, overlay, underlay, pTransparentIndex))
GEN_THUNKS_RET(int, glXGetVideoDeviceNV, (Display * dpy, int screen, int numVideoDevices, GLXVideoDeviceNV * pVideoDevice), (dpy, screen, numVideoDevices, pVideoDevice))
GEN_THUNKS_RET(int, glXGetVideoInfoNV, (Display * dpy, int screen, GLXVideoDeviceNV VideoDevice, unsigned long * pulCounterOutputPbuffer, unsigned long * pulCounterOutputVideo), (dpy, screen, VideoDevice, pulCounterOutputPbuffer, pulCounterOutputVideo))
GEN_THUNKS_RET(int, glXGetVideoSyncSGI, (unsigned int * count), (count))
GEN_THUNKS_RET(XVisualInfo *, glXGetVisualFromFBConfig, (Display * dpy, GLXFBConfig config), (dpy, config))
GEN_THUNKS_RET(XVisualInfo *, glXGetVisualFromFBConfigSGIX, (Display * dpy, GLXFBConfigSGIX config), (dpy, config))
GEN_THUNKS_RET(int, glXHyperpipeAttribSGIX, (Display * dpy, int timeSlice, int attrib, int size, void * attribList), (dpy, timeSlice, attrib, size, attribList))
GEN_THUNKS_RET(int, glXHyperpipeConfigSGIX, (Display * dpy, int networkId, int npipes, GLXHyperpipeConfigSGIX * cfg, int * hpId), (dpy, networkId, npipes, cfg, hpId))
GEN_THUNKS_RET(GLXContext, glXImportContextEXT, (Display * dpy, GLXContextID contextID), (dpy, contextID))
GEN_THUNKS_RET(Bool, glXIsDirect, (Display * dpy, GLXContext ctx), (dpy, ctx))
GEN_THUNKS_RET(Bool, glXJoinSwapGroupNV, (Display * dpy, GLXDrawable drawable, GLuint group), (dpy, drawable, group))
GEN_THUNKS(glXJoinSwapGroupSGIX, (Display * dpy, GLXDrawable drawable, GLXDrawable member), (dpy, drawable, member))
GEN_THUNKS(glXLockVideoCaptureDeviceNV, (Display * dpy, GLXVideoCaptureDeviceNV device), (dpy, device))
GEN_THUNKS_RET(Bool, glXMakeContextCurrent, (Display * dpy, GLXDrawable draw, GLXDrawable read, GLXContext ctx), (dpy, draw, read, ctx))
GEN_THUNKS_RET(Bool, glXMakeCurrent, (Display * dpy, GLXDrawable drawable, GLXContext ctx), (dpy, drawable, ctx))
GEN_THUNKS_RET(Bool, glXMakeCurrentReadSGI, (Display * dpy, GLXDrawable draw, GLXDrawable read, GLXContext ctx), (dpy, draw, read, ctx))
GEN_THUNKS_RET(int, glXQueryChannelDeltasSGIX, (Display * display, int screen, int channel, int * x, int * y, int * w, int * h), (display, screen, channel, x, y, w, h))
GEN_THUNKS_RET(int, glXQueryChannelRectSGIX, (Display * display, int screen, int channel, int * dx, int * dy, int * dw, int * dh), (display, screen, channel, dx, dy, dw, dh))
GEN_THUNKS_RET(int, glXQueryContext, (Display * dpy, GLXContext ctx, int attribute, int * value), (dpy, ctx, attribute, value))
GEN_THUNKS_RET(int, glXQueryContextInfoEXT, (Display * dpy, GLXContext context, int attribute, int * value), (dpy, context, attribute, value))
GEN_THUNKS_RET(Bool, glXQueryCurrentRendererIntegerMESA, (int attribute, unsigned int * value), (attribute, value))
GEN_THUNKS_RET(const char *, glXQueryCurrentRendererStringMESA, (int attribute), (attribute))
GEN_THUNKS(glXQueryDrawable, (Display * dpy, GLXDrawable draw, int attribute, unsigned int * value), (dpy, draw, attribute, value))
GEN_THUNKS_RET(Bool, glXQueryExtension, (Display * dpy, int * errorb, int * event), (dpy, errorb, event))
GEN_THUNKS_RET(const char *, glXQueryExtensionsString, (Display * dpy, int screen), (dpy, screen))
GEN_THUNKS_RET(Bool, glXQueryFrameCountNV, (Display * dpy, int screen, GLuint * count), (dpy, screen, count))
GEN_THUNKS_RET(int, glXQueryGLXPbufferSGIX, (Display * dpy, GLXPbufferSGIX pbuf, int attribute, unsigned int * value), (dpy, pbuf, attribute, value))
GEN_THUNKS_RET(int, glXQueryHyperpipeAttribSGIX, (Display * dpy, int timeSlice, int attrib, int size, void * returnAttribList), (dpy, timeSlice, attrib, size, returnAttribList))
GEN_THUNKS_RET(int, glXQueryHyperpipeBestAttribSGIX, (Display * dpy, int timeSlice, int attrib, int size, void * attribList, void * returnAttribList), (dpy, timeSlice, attrib, size, attribList, returnAttribList))
GEN_THUNKS_RET(GLXHyperpipeConfigSGIX *, glXQueryHyperpipeConfigSGIX, (Display * dpy, int hpId, int * npipes), (dpy, hpId, npipes))
GEN_THUNKS_RET(GLXHyperpipeNetworkSGIX *, glXQueryHyperpipeNetworkSGIX, (Display * dpy, int * npipes), (dpy, npipes))
GEN_THUNKS_RET(Bool, glXQueryMaxSwapBarriersSGIX, (Display * dpy, int screen, int * max), (dpy, screen, max))
GEN_THUNKS_RET(Bool, glXQueryMaxSwapGroupsNV, (Display * dpy, int screen, GLuint * maxGroups, GLuint * maxBarriers), (dpy, screen, maxGroups, maxBarriers))
GEN_THUNKS_RET(Bool, glXQueryRendererIntegerMESA, (Display * dpy, int screen, int renderer, int attribute, unsigned int * value), (dpy, screen, renderer, attribute, value))
GEN_THUNKS_RET(const char *, glXQueryRendererStringMESA, (Display * dpy, int screen, int renderer, int attribute), (dpy, screen, renderer, attribute))
GEN_THUNKS_RET(const char *, glXQueryServerString, (Display * dpy, int screen, int name), (dpy, screen, name))
GEN_THUNKS_RET(Bool, glXQuerySwapGroupNV, (Display * dpy, GLXDrawable drawable, GLuint * group, GLuint * barrier), (dpy, drawable, group, barrier))
GEN_THUNKS_RET(Bool, glXQueryVersion, (Display * dpy, int * maj, int * min), (dpy, maj, min))
GEN_THUNKS_RET(int, glXQueryVideoCaptureDeviceNV, (Display * dpy, GLXVideoCaptureDeviceNV device, int attribute, int * value), (dpy, device, attribute, value))
GEN_THUNKS_RET(Bool, glXReleaseBuffersMESA, (Display * dpy, GLXDrawable drawable), (dpy, drawable))
GEN_THUNKS(glXReleaseTexImageEXT, (Display * dpy, GLXDrawable drawable, int buffer), (dpy, drawable, buffer))
GEN_THUNKS(glXReleaseVideoCaptureDeviceNV, (Display * dpy, GLXVideoCaptureDeviceNV device), (dpy, device))
GEN_THUNKS_RET(int, glXReleaseVideoDeviceNV, (Display * dpy, int screen, GLXVideoDeviceNV VideoDevice), (dpy, screen, VideoDevice))
GEN_THUNKS_RET(int, glXReleaseVideoImageNV, (Display * dpy, GLXPbuffer pbuf), (dpy, pbuf))
GEN_THUNKS_RET(Bool, glXResetFrameCountNV, (Display * dpy, int screen), (dpy, screen))
GEN_THUNKS(glXSelectEvent, (Display * dpy, GLXDrawable draw, unsigned long event_mask), (dpy, draw, event_mask))
GEN_THUNKS(glXSelectEventSGIX, (Display * dpy, GLXDrawable drawable, unsigned long mask), (dpy, drawable, mask))
GEN_THUNKS_RET(int, glXSendPbufferToVideoNV, (Display * dpy, GLXPbuffer pbuf, int iBufferType, unsigned long * pulCounterPbuffer, GLboolean bBlock), (dpy, pbuf, iBufferType, pulCounterPbuffer, bBlock))
GEN_THUNKS_RET(Bool, glXSet3DfxModeMESA, (int mode), (mode))
GEN_THUNKS(glXSwapBuffers, (Display * dpy, GLXDrawable drawable), (dpy, drawable))
GEN_THUNKS_RET(int64_t, glXSwapBuffersMscOML, (Display * dpy, GLXDrawable drawable, int64_t target_msc, int64_t divisor, int64_t remainder), (dpy, drawable, target_msc, divisor, remainder))
GEN_THUNKS(glXSwapIntervalEXT, (Display * dpy, GLXDrawable drawable, int interval), (dpy, drawable, interval))
GEN_THUNKS_RET(int, glXSwapIntervalSGI, (int interval), (interval))
GEN_THUNKS(glXUseXFont, (Font font, int first, int count, int list), (font, first, count, list))
GEN_THUNKS_RET(Bool, glXWaitForMscOML, (Display * dpy, GLXDrawable drawable, int64_t target_msc, int64_t divisor, int64_t remainder, int64_t * ust, int64_t * msc, int64_t * sbc), (dpy, drawable, target_msc, divisor, remainder, ust, msc, sbc))
GEN_THUNKS_RET(Bool, glXWaitForSbcOML, (Display * dpy, GLXDrawable drawable, int64_t target_sbc, int64_t * ust, int64_t * msc, int64_t * sbc), (dpy, drawable, target_sbc, ust, msc, sbc))
GEN_THUNKS(glXWaitGL, (void), ())
GEN_THUNKS_RET(int, glXWaitVideoSyncSGI, (int divisor, int remainder, unsigned int * count), (divisor, remainder, count))
GEN_THUNKS(glXWaitX, (void), ())
#if USING_DISPATCH_TABLE
static struct dispatch_table resolver_table = {
    .glXBindChannelToWindowSGIX = epoxy_glXBindChannelToWindowSGIX_dispatch_table_rewrite_ptr,
    .glXBindHyperpipeSGIX = epoxy_glXBindHyperpipeSGIX_dispatch_table_rewrite_ptr,
    .glXBindSwapBarrierNV = epoxy_glXBindSwapBarrierNV_dispatch_table_rewrite_ptr,
    .glXBindSwapBarrierSGIX = epoxy_glXBindSwapBarrierSGIX_dispatch_table_rewrite_ptr,
    .glXBindTexImageEXT = epoxy_glXBindTexImageEXT_dispatch_table_rewrite_ptr,
    .glXBindVideoCaptureDeviceNV = epoxy_glXBindVideoCaptureDeviceNV_dispatch_table_rewrite_ptr,
    .glXBindVideoDeviceNV = epoxy_glXBindVideoDeviceNV_dispatch_table_rewrite_ptr,
    .glXBindVideoImageNV = epoxy_glXBindVideoImageNV_dispatch_table_rewrite_ptr,
    .glXChannelRectSGIX = epoxy_glXChannelRectSGIX_dispatch_table_rewrite_ptr,
    .glXChannelRectSyncSGIX = epoxy_glXChannelRectSyncSGIX_dispatch_table_rewrite_ptr,
    .glXChooseFBConfig = epoxy_glXChooseFBConfig_dispatch_table_rewrite_ptr,
    .glXChooseFBConfigSGIX = epoxy_glXChooseFBConfigSGIX_dispatch_table_rewrite_ptr,
    .glXChooseVisual = epoxy_glXChooseVisual_dispatch_table_rewrite_ptr,
    .glXCopyContext = epoxy_glXCopyContext_dispatch_table_rewrite_ptr,
    .glXCopyImageSubDataNV = epoxy_glXCopyImageSubDataNV_dispatch_table_rewrite_ptr,
    .glXCopySubBufferMESA = epoxy_glXCopySubBufferMESA_dispatch_table_rewrite_ptr,
    .glXCreateContext = epoxy_glXCreateContext_dispatch_table_rewrite_ptr,
    .glXCreateContextAttribsARB = epoxy_glXCreateContextAttribsARB_dispatch_table_rewrite_ptr,
    .glXCreateContextWithConfigSGIX = epoxy_glXCreateContextWithConfigSGIX_dispatch_table_rewrite_ptr,
    .glXCreateGLXPbufferSGIX = epoxy_glXCreateGLXPbufferSGIX_dispatch_table_rewrite_ptr,
    .glXCreateGLXPixmap = epoxy_glXCreateGLXPixmap_dispatch_table_rewrite_ptr,
    .glXCreateGLXPixmapMESA = epoxy_glXCreateGLXPixmapMESA_dispatch_table_rewrite_ptr,
    .glXCreateGLXPixmapWithConfigSGIX = epoxy_glXCreateGLXPixmapWithConfigSGIX_dispatch_table_rewrite_ptr,
    .glXCreateNewContext = epoxy_glXCreateNewContext_dispatch_table_rewrite_ptr,
    .glXCreatePbuffer = epoxy_glXCreatePbuffer_dispatch_table_rewrite_ptr,
    .glXCreatePixmap = epoxy_glXCreatePixmap_dispatch_table_rewrite_ptr,
    .glXCreateWindow = epoxy_glXCreateWindow_dispatch_table_rewrite_ptr,
    .glXCushionSGI = epoxy_glXCushionSGI_dispatch_table_rewrite_ptr,
    .glXDelayBeforeSwapNV = epoxy_glXDelayBeforeSwapNV_dispatch_table_rewrite_ptr,
    .glXDestroyContext = epoxy_glXDestroyContext_dispatch_table_rewrite_ptr,
    .glXDestroyGLXPbufferSGIX = epoxy_glXDestroyGLXPbufferSGIX_dispatch_table_rewrite_ptr,
    .glXDestroyGLXPixmap = epoxy_glXDestroyGLXPixmap_dispatch_table_rewrite_ptr,
    .glXDestroyGLXVideoSourceSGIX = epoxy_glXDestroyGLXVideoSourceSGIX_dispatch_table_rewrite_ptr,
    .glXDestroyHyperpipeConfigSGIX = epoxy_glXDestroyHyperpipeConfigSGIX_dispatch_table_rewrite_ptr,
    .glXDestroyPbuffer = epoxy_glXDestroyPbuffer_dispatch_table_rewrite_ptr,
    .glXDestroyPixmap = epoxy_glXDestroyPixmap_dispatch_table_rewrite_ptr,
    .glXDestroyWindow = epoxy_glXDestroyWindow_dispatch_table_rewrite_ptr,
    .glXEnumerateVideoCaptureDevicesNV = epoxy_glXEnumerateVideoCaptureDevicesNV_dispatch_table_rewrite_ptr,
    .glXEnumerateVideoDevicesNV = epoxy_glXEnumerateVideoDevicesNV_dispatch_table_rewrite_ptr,
    .glXFreeContextEXT = epoxy_glXFreeContextEXT_dispatch_table_rewrite_ptr,
    .glXGetAGPOffsetMESA = epoxy_glXGetAGPOffsetMESA_dispatch_table_rewrite_ptr,
    .glXGetClientString = epoxy_glXGetClientString_dispatch_table_rewrite_ptr,
    .glXGetConfig = epoxy_glXGetConfig_dispatch_table_rewrite_ptr,
    .glXGetContextIDEXT = epoxy_glXGetContextIDEXT_dispatch_table_rewrite_ptr,
    .glXGetCurrentContext = epoxy_glXGetCurrentContext_dispatch_table_rewrite_ptr,
    .glXGetCurrentDisplay = epoxy_glXGetCurrentDisplay_dispatch_table_rewrite_ptr,
    .glXGetCurrentDisplayEXT = epoxy_glXGetCurrentDisplayEXT_dispatch_table_rewrite_ptr,
    .glXGetCurrentDrawable = epoxy_glXGetCurrentDrawable_dispatch_table_rewrite_ptr,
    .glXGetCurrentReadDrawable = epoxy_glXGetCurrentReadDrawable_dispatch_table_rewrite_ptr,
    .glXGetCurrentReadDrawableSGI = epoxy_glXGetCurrentReadDrawableSGI_dispatch_table_rewrite_ptr,
    .glXGetFBConfigAttrib = epoxy_glXGetFBConfigAttrib_dispatch_table_rewrite_ptr,
    .glXGetFBConfigAttribSGIX = epoxy_glXGetFBConfigAttribSGIX_dispatch_table_rewrite_ptr,
    .glXGetFBConfigFromVisualSGIX = epoxy_glXGetFBConfigFromVisualSGIX_dispatch_table_rewrite_ptr,
    .glXGetFBConfigs = epoxy_glXGetFBConfigs_dispatch_table_rewrite_ptr,
    .glXGetMscRateOML = epoxy_glXGetMscRateOML_dispatch_table_rewrite_ptr,
    .glXGetProcAddress = epoxy_glXGetProcAddress_dispatch_table_rewrite_ptr,
    .glXGetProcAddressARB = epoxy_glXGetProcAddressARB_dispatch_table_rewrite_ptr,
    .glXGetSelectedEvent = epoxy_glXGetSelectedEvent_dispatch_table_rewrite_ptr,
    .glXGetSelectedEventSGIX = epoxy_glXGetSelectedEventSGIX_dispatch_table_rewrite_ptr,
    .glXGetSyncValuesOML = epoxy_glXGetSyncValuesOML_dispatch_table_rewrite_ptr,
    .glXGetTransparentIndexSUN = epoxy_glXGetTransparentIndexSUN_dispatch_table_rewrite_ptr,
    .glXGetVideoDeviceNV = epoxy_glXGetVideoDeviceNV_dispatch_table_rewrite_ptr,
    .glXGetVideoInfoNV = epoxy_glXGetVideoInfoNV_dispatch_table_rewrite_ptr,
    .glXGetVideoSyncSGI = epoxy_glXGetVideoSyncSGI_dispatch_table_rewrite_ptr,
    .glXGetVisualFromFBConfig = epoxy_glXGetVisualFromFBConfig_dispatch_table_rewrite_ptr,
    .glXGetVisualFromFBConfigSGIX = epoxy_glXGetVisualFromFBConfigSGIX_dispatch_table_rewrite_ptr,
    .glXHyperpipeAttribSGIX = epoxy_glXHyperpipeAttribSGIX_dispatch_table_rewrite_ptr,
    .glXHyperpipeConfigSGIX = epoxy_glXHyperpipeConfigSGIX_dispatch_table_rewrite_ptr,
    .glXImportContextEXT = epoxy_glXImportContextEXT_dispatch_table_rewrite_ptr,
    .glXIsDirect = epoxy_glXIsDirect_dispatch_table_rewrite_ptr,
    .glXJoinSwapGroupNV = epoxy_glXJoinSwapGroupNV_dispatch_table_rewrite_ptr,
    .glXJoinSwapGroupSGIX = epoxy_glXJoinSwapGroupSGIX_dispatch_table_rewrite_ptr,
    .glXLockVideoCaptureDeviceNV = epoxy_glXLockVideoCaptureDeviceNV_dispatch_table_rewrite_ptr,
    .glXMakeContextCurrent = epoxy_glXMakeContextCurrent_dispatch_table_rewrite_ptr,
    .glXMakeCurrent = epoxy_glXMakeCurrent_dispatch_table_rewrite_ptr,
    .glXMakeCurrentReadSGI = epoxy_glXMakeCurrentReadSGI_dispatch_table_rewrite_ptr,
    .glXQueryChannelDeltasSGIX = epoxy_glXQueryChannelDeltasSGIX_dispatch_table_rewrite_ptr,
    .glXQueryChannelRectSGIX = epoxy_glXQueryChannelRectSGIX_dispatch_table_rewrite_ptr,
    .glXQueryContext = epoxy_glXQueryContext_dispatch_table_rewrite_ptr,
    .glXQueryContextInfoEXT = epoxy_glXQueryContextInfoEXT_dispatch_table_rewrite_ptr,
    .glXQueryCurrentRendererIntegerMESA = epoxy_glXQueryCurrentRendererIntegerMESA_dispatch_table_rewrite_ptr,
    .glXQueryCurrentRendererStringMESA = epoxy_glXQueryCurrentRendererStringMESA_dispatch_table_rewrite_ptr,
    .glXQueryDrawable = epoxy_glXQueryDrawable_dispatch_table_rewrite_ptr,
    .glXQueryExtension = epoxy_glXQueryExtension_dispatch_table_rewrite_ptr,
    .glXQueryExtensionsString = epoxy_glXQueryExtensionsString_dispatch_table_rewrite_ptr,
    .glXQueryFrameCountNV = epoxy_glXQueryFrameCountNV_dispatch_table_rewrite_ptr,
    .glXQueryGLXPbufferSGIX = epoxy_glXQueryGLXPbufferSGIX_dispatch_table_rewrite_ptr,
    .glXQueryHyperpipeAttribSGIX = epoxy_glXQueryHyperpipeAttribSGIX_dispatch_table_rewrite_ptr,
    .glXQueryHyperpipeBestAttribSGIX = epoxy_glXQueryHyperpipeBestAttribSGIX_dispatch_table_rewrite_ptr,
    .glXQueryHyperpipeConfigSGIX = epoxy_glXQueryHyperpipeConfigSGIX_dispatch_table_rewrite_ptr,
    .glXQueryHyperpipeNetworkSGIX = epoxy_glXQueryHyperpipeNetworkSGIX_dispatch_table_rewrite_ptr,
    .glXQueryMaxSwapBarriersSGIX = epoxy_glXQueryMaxSwapBarriersSGIX_dispatch_table_rewrite_ptr,
    .glXQueryMaxSwapGroupsNV = epoxy_glXQueryMaxSwapGroupsNV_dispatch_table_rewrite_ptr,
    .glXQueryRendererIntegerMESA = epoxy_glXQueryRendererIntegerMESA_dispatch_table_rewrite_ptr,
    .glXQueryRendererStringMESA = epoxy_glXQueryRendererStringMESA_dispatch_table_rewrite_ptr,
    .glXQueryServerString = epoxy_glXQueryServerString_dispatch_table_rewrite_ptr,
    .glXQuerySwapGroupNV = epoxy_glXQuerySwapGroupNV_dispatch_table_rewrite_ptr,
    .glXQueryVersion = epoxy_glXQueryVersion_dispatch_table_rewrite_ptr,
    .glXQueryVideoCaptureDeviceNV = epoxy_glXQueryVideoCaptureDeviceNV_dispatch_table_rewrite_ptr,
    .glXReleaseBuffersMESA = epoxy_glXReleaseBuffersMESA_dispatch_table_rewrite_ptr,
    .glXReleaseTexImageEXT = epoxy_glXReleaseTexImageEXT_dispatch_table_rewrite_ptr,
    .glXReleaseVideoCaptureDeviceNV = epoxy_glXReleaseVideoCaptureDeviceNV_dispatch_table_rewrite_ptr,
    .glXReleaseVideoDeviceNV = epoxy_glXReleaseVideoDeviceNV_dispatch_table_rewrite_ptr,
    .glXReleaseVideoImageNV = epoxy_glXReleaseVideoImageNV_dispatch_table_rewrite_ptr,
    .glXResetFrameCountNV = epoxy_glXResetFrameCountNV_dispatch_table_rewrite_ptr,
    .glXSelectEvent = epoxy_glXSelectEvent_dispatch_table_rewrite_ptr,
    .glXSelectEventSGIX = epoxy_glXSelectEventSGIX_dispatch_table_rewrite_ptr,
    .glXSendPbufferToVideoNV = epoxy_glXSendPbufferToVideoNV_dispatch_table_rewrite_ptr,
    .glXSet3DfxModeMESA = epoxy_glXSet3DfxModeMESA_dispatch_table_rewrite_ptr,
    .glXSwapBuffers = epoxy_glXSwapBuffers_dispatch_table_rewrite_ptr,
    .glXSwapBuffersMscOML = epoxy_glXSwapBuffersMscOML_dispatch_table_rewrite_ptr,
    .glXSwapIntervalEXT = epoxy_glXSwapIntervalEXT_dispatch_table_rewrite_ptr,
    .glXSwapIntervalSGI = epoxy_glXSwapIntervalSGI_dispatch_table_rewrite_ptr,
    .glXUseXFont = epoxy_glXUseXFont_dispatch_table_rewrite_ptr,
    .glXWaitForMscOML = epoxy_glXWaitForMscOML_dispatch_table_rewrite_ptr,
    .glXWaitForSbcOML = epoxy_glXWaitForSbcOML_dispatch_table_rewrite_ptr,
    .glXWaitGL = epoxy_glXWaitGL_dispatch_table_rewrite_ptr,
    .glXWaitVideoSyncSGI = epoxy_glXWaitVideoSyncSGI_dispatch_table_rewrite_ptr,
    .glXWaitX = epoxy_glXWaitX_dispatch_table_rewrite_ptr,
};

uint32_t glx_tls_index;
uint32_t glx_tls_size = sizeof(struct dispatch_table);

static inline struct dispatch_table *
get_dispatch_table(void)
{
	return TlsGetValue(glx_tls_index);
}

void
glx_init_dispatch_table(void)
{
    struct dispatch_table *dispatch_table = get_dispatch_table();
    memcpy(dispatch_table, &resolver_table, sizeof(resolver_table));
}

void
glx_switch_to_dispatch_table(void)
{
    epoxy_glXBindChannelToWindowSGIX = epoxy_glXBindChannelToWindowSGIX_dispatch_table_thunk;
    epoxy_glXBindHyperpipeSGIX = epoxy_glXBindHyperpipeSGIX_dispatch_table_thunk;
    epoxy_glXBindSwapBarrierNV = epoxy_glXBindSwapBarrierNV_dispatch_table_thunk;
    epoxy_glXBindSwapBarrierSGIX = epoxy_glXBindSwapBarrierSGIX_dispatch_table_thunk;
    epoxy_glXBindTexImageEXT = epoxy_glXBindTexImageEXT_dispatch_table_thunk;
    epoxy_glXBindVideoCaptureDeviceNV = epoxy_glXBindVideoCaptureDeviceNV_dispatch_table_thunk;
    epoxy_glXBindVideoDeviceNV = epoxy_glXBindVideoDeviceNV_dispatch_table_thunk;
    epoxy_glXBindVideoImageNV = epoxy_glXBindVideoImageNV_dispatch_table_thunk;
    epoxy_glXChannelRectSGIX = epoxy_glXChannelRectSGIX_dispatch_table_thunk;
    epoxy_glXChannelRectSyncSGIX = epoxy_glXChannelRectSyncSGIX_dispatch_table_thunk;
    epoxy_glXChooseFBConfig = epoxy_glXChooseFBConfig_dispatch_table_thunk;
    epoxy_glXChooseFBConfigSGIX = epoxy_glXChooseFBConfigSGIX_dispatch_table_thunk;
    epoxy_glXChooseVisual = epoxy_glXChooseVisual_dispatch_table_thunk;
    epoxy_glXCopyContext = epoxy_glXCopyContext_dispatch_table_thunk;
    epoxy_glXCopyImageSubDataNV = epoxy_glXCopyImageSubDataNV_dispatch_table_thunk;
    epoxy_glXCopySubBufferMESA = epoxy_glXCopySubBufferMESA_dispatch_table_thunk;
    epoxy_glXCreateContext = epoxy_glXCreateContext_dispatch_table_thunk;
    epoxy_glXCreateContextAttribsARB = epoxy_glXCreateContextAttribsARB_dispatch_table_thunk;
    epoxy_glXCreateContextWithConfigSGIX = epoxy_glXCreateContextWithConfigSGIX_dispatch_table_thunk;
    epoxy_glXCreateGLXPbufferSGIX = epoxy_glXCreateGLXPbufferSGIX_dispatch_table_thunk;
    epoxy_glXCreateGLXPixmap = epoxy_glXCreateGLXPixmap_dispatch_table_thunk;
    epoxy_glXCreateGLXPixmapMESA = epoxy_glXCreateGLXPixmapMESA_dispatch_table_thunk;
    epoxy_glXCreateGLXPixmapWithConfigSGIX = epoxy_glXCreateGLXPixmapWithConfigSGIX_dispatch_table_thunk;
    epoxy_glXCreateNewContext = epoxy_glXCreateNewContext_dispatch_table_thunk;
    epoxy_glXCreatePbuffer = epoxy_glXCreatePbuffer_dispatch_table_thunk;
    epoxy_glXCreatePixmap = epoxy_glXCreatePixmap_dispatch_table_thunk;
    epoxy_glXCreateWindow = epoxy_glXCreateWindow_dispatch_table_thunk;
    epoxy_glXCushionSGI = epoxy_glXCushionSGI_dispatch_table_thunk;
    epoxy_glXDelayBeforeSwapNV = epoxy_glXDelayBeforeSwapNV_dispatch_table_thunk;
    epoxy_glXDestroyContext = epoxy_glXDestroyContext_dispatch_table_thunk;
    epoxy_glXDestroyGLXPbufferSGIX = epoxy_glXDestroyGLXPbufferSGIX_dispatch_table_thunk;
    epoxy_glXDestroyGLXPixmap = epoxy_glXDestroyGLXPixmap_dispatch_table_thunk;
    epoxy_glXDestroyGLXVideoSourceSGIX = epoxy_glXDestroyGLXVideoSourceSGIX_dispatch_table_thunk;
    epoxy_glXDestroyHyperpipeConfigSGIX = epoxy_glXDestroyHyperpipeConfigSGIX_dispatch_table_thunk;
    epoxy_glXDestroyPbuffer = epoxy_glXDestroyPbuffer_dispatch_table_thunk;
    epoxy_glXDestroyPixmap = epoxy_glXDestroyPixmap_dispatch_table_thunk;
    epoxy_glXDestroyWindow = epoxy_glXDestroyWindow_dispatch_table_thunk;
    epoxy_glXEnumerateVideoCaptureDevicesNV = epoxy_glXEnumerateVideoCaptureDevicesNV_dispatch_table_thunk;
    epoxy_glXEnumerateVideoDevicesNV = epoxy_glXEnumerateVideoDevicesNV_dispatch_table_thunk;
    epoxy_glXFreeContextEXT = epoxy_glXFreeContextEXT_dispatch_table_thunk;
    epoxy_glXGetAGPOffsetMESA = epoxy_glXGetAGPOffsetMESA_dispatch_table_thunk;
    epoxy_glXGetClientString = epoxy_glXGetClientString_dispatch_table_thunk;
    epoxy_glXGetConfig = epoxy_glXGetConfig_dispatch_table_thunk;
    epoxy_glXGetContextIDEXT = epoxy_glXGetContextIDEXT_dispatch_table_thunk;
    epoxy_glXGetCurrentContext = epoxy_glXGetCurrentContext_dispatch_table_thunk;
    epoxy_glXGetCurrentDisplay = epoxy_glXGetCurrentDisplay_dispatch_table_thunk;
    epoxy_glXGetCurrentDisplayEXT = epoxy_glXGetCurrentDisplayEXT_dispatch_table_thunk;
    epoxy_glXGetCurrentDrawable = epoxy_glXGetCurrentDrawable_dispatch_table_thunk;
    epoxy_glXGetCurrentReadDrawable = epoxy_glXGetCurrentReadDrawable_dispatch_table_thunk;
    epoxy_glXGetCurrentReadDrawableSGI = epoxy_glXGetCurrentReadDrawableSGI_dispatch_table_thunk;
    epoxy_glXGetFBConfigAttrib = epoxy_glXGetFBConfigAttrib_dispatch_table_thunk;
    epoxy_glXGetFBConfigAttribSGIX = epoxy_glXGetFBConfigAttribSGIX_dispatch_table_thunk;
    epoxy_glXGetFBConfigFromVisualSGIX = epoxy_glXGetFBConfigFromVisualSGIX_dispatch_table_thunk;
    epoxy_glXGetFBConfigs = epoxy_glXGetFBConfigs_dispatch_table_thunk;
    epoxy_glXGetMscRateOML = epoxy_glXGetMscRateOML_dispatch_table_thunk;
    epoxy_glXGetProcAddress = epoxy_glXGetProcAddress_dispatch_table_thunk;
    epoxy_glXGetProcAddressARB = epoxy_glXGetProcAddressARB_dispatch_table_thunk;
    epoxy_glXGetSelectedEvent = epoxy_glXGetSelectedEvent_dispatch_table_thunk;
    epoxy_glXGetSelectedEventSGIX = epoxy_glXGetSelectedEventSGIX_dispatch_table_thunk;
    epoxy_glXGetSyncValuesOML = epoxy_glXGetSyncValuesOML_dispatch_table_thunk;
    epoxy_glXGetTransparentIndexSUN = epoxy_glXGetTransparentIndexSUN_dispatch_table_thunk;
    epoxy_glXGetVideoDeviceNV = epoxy_glXGetVideoDeviceNV_dispatch_table_thunk;
    epoxy_glXGetVideoInfoNV = epoxy_glXGetVideoInfoNV_dispatch_table_thunk;
    epoxy_glXGetVideoSyncSGI = epoxy_glXGetVideoSyncSGI_dispatch_table_thunk;
    epoxy_glXGetVisualFromFBConfig = epoxy_glXGetVisualFromFBConfig_dispatch_table_thunk;
    epoxy_glXGetVisualFromFBConfigSGIX = epoxy_glXGetVisualFromFBConfigSGIX_dispatch_table_thunk;
    epoxy_glXHyperpipeAttribSGIX = epoxy_glXHyperpipeAttribSGIX_dispatch_table_thunk;
    epoxy_glXHyperpipeConfigSGIX = epoxy_glXHyperpipeConfigSGIX_dispatch_table_thunk;
    epoxy_glXImportContextEXT = epoxy_glXImportContextEXT_dispatch_table_thunk;
    epoxy_glXIsDirect = epoxy_glXIsDirect_dispatch_table_thunk;
    epoxy_glXJoinSwapGroupNV = epoxy_glXJoinSwapGroupNV_dispatch_table_thunk;
    epoxy_glXJoinSwapGroupSGIX = epoxy_glXJoinSwapGroupSGIX_dispatch_table_thunk;
    epoxy_glXLockVideoCaptureDeviceNV = epoxy_glXLockVideoCaptureDeviceNV_dispatch_table_thunk;
    epoxy_glXMakeContextCurrent = epoxy_glXMakeContextCurrent_dispatch_table_thunk;
    epoxy_glXMakeCurrent = epoxy_glXMakeCurrent_dispatch_table_thunk;
    epoxy_glXMakeCurrentReadSGI = epoxy_glXMakeCurrentReadSGI_dispatch_table_thunk;
    epoxy_glXQueryChannelDeltasSGIX = epoxy_glXQueryChannelDeltasSGIX_dispatch_table_thunk;
    epoxy_glXQueryChannelRectSGIX = epoxy_glXQueryChannelRectSGIX_dispatch_table_thunk;
    epoxy_glXQueryContext = epoxy_glXQueryContext_dispatch_table_thunk;
    epoxy_glXQueryContextInfoEXT = epoxy_glXQueryContextInfoEXT_dispatch_table_thunk;
    epoxy_glXQueryCurrentRendererIntegerMESA = epoxy_glXQueryCurrentRendererIntegerMESA_dispatch_table_thunk;
    epoxy_glXQueryCurrentRendererStringMESA = epoxy_glXQueryCurrentRendererStringMESA_dispatch_table_thunk;
    epoxy_glXQueryDrawable = epoxy_glXQueryDrawable_dispatch_table_thunk;
    epoxy_glXQueryExtension = epoxy_glXQueryExtension_dispatch_table_thunk;
    epoxy_glXQueryExtensionsString = epoxy_glXQueryExtensionsString_dispatch_table_thunk;
    epoxy_glXQueryFrameCountNV = epoxy_glXQueryFrameCountNV_dispatch_table_thunk;
    epoxy_glXQueryGLXPbufferSGIX = epoxy_glXQueryGLXPbufferSGIX_dispatch_table_thunk;
    epoxy_glXQueryHyperpipeAttribSGIX = epoxy_glXQueryHyperpipeAttribSGIX_dispatch_table_thunk;
    epoxy_glXQueryHyperpipeBestAttribSGIX = epoxy_glXQueryHyperpipeBestAttribSGIX_dispatch_table_thunk;
    epoxy_glXQueryHyperpipeConfigSGIX = epoxy_glXQueryHyperpipeConfigSGIX_dispatch_table_thunk;
    epoxy_glXQueryHyperpipeNetworkSGIX = epoxy_glXQueryHyperpipeNetworkSGIX_dispatch_table_thunk;
    epoxy_glXQueryMaxSwapBarriersSGIX = epoxy_glXQueryMaxSwapBarriersSGIX_dispatch_table_thunk;
    epoxy_glXQueryMaxSwapGroupsNV = epoxy_glXQueryMaxSwapGroupsNV_dispatch_table_thunk;
    epoxy_glXQueryRendererIntegerMESA = epoxy_glXQueryRendererIntegerMESA_dispatch_table_thunk;
    epoxy_glXQueryRendererStringMESA = epoxy_glXQueryRendererStringMESA_dispatch_table_thunk;
    epoxy_glXQueryServerString = epoxy_glXQueryServerString_dispatch_table_thunk;
    epoxy_glXQuerySwapGroupNV = epoxy_glXQuerySwapGroupNV_dispatch_table_thunk;
    epoxy_glXQueryVersion = epoxy_glXQueryVersion_dispatch_table_thunk;
    epoxy_glXQueryVideoCaptureDeviceNV = epoxy_glXQueryVideoCaptureDeviceNV_dispatch_table_thunk;
    epoxy_glXReleaseBuffersMESA = epoxy_glXReleaseBuffersMESA_dispatch_table_thunk;
    epoxy_glXReleaseTexImageEXT = epoxy_glXReleaseTexImageEXT_dispatch_table_thunk;
    epoxy_glXReleaseVideoCaptureDeviceNV = epoxy_glXReleaseVideoCaptureDeviceNV_dispatch_table_thunk;
    epoxy_glXReleaseVideoDeviceNV = epoxy_glXReleaseVideoDeviceNV_dispatch_table_thunk;
    epoxy_glXReleaseVideoImageNV = epoxy_glXReleaseVideoImageNV_dispatch_table_thunk;
    epoxy_glXResetFrameCountNV = epoxy_glXResetFrameCountNV_dispatch_table_thunk;
    epoxy_glXSelectEvent = epoxy_glXSelectEvent_dispatch_table_thunk;
    epoxy_glXSelectEventSGIX = epoxy_glXSelectEventSGIX_dispatch_table_thunk;
    epoxy_glXSendPbufferToVideoNV = epoxy_glXSendPbufferToVideoNV_dispatch_table_thunk;
    epoxy_glXSet3DfxModeMESA = epoxy_glXSet3DfxModeMESA_dispatch_table_thunk;
    epoxy_glXSwapBuffers = epoxy_glXSwapBuffers_dispatch_table_thunk;
    epoxy_glXSwapBuffersMscOML = epoxy_glXSwapBuffersMscOML_dispatch_table_thunk;
    epoxy_glXSwapIntervalEXT = epoxy_glXSwapIntervalEXT_dispatch_table_thunk;
    epoxy_glXSwapIntervalSGI = epoxy_glXSwapIntervalSGI_dispatch_table_thunk;
    epoxy_glXUseXFont = epoxy_glXUseXFont_dispatch_table_thunk;
    epoxy_glXWaitForMscOML = epoxy_glXWaitForMscOML_dispatch_table_thunk;
    epoxy_glXWaitForSbcOML = epoxy_glXWaitForSbcOML_dispatch_table_thunk;
    epoxy_glXWaitGL = epoxy_glXWaitGL_dispatch_table_thunk;
    epoxy_glXWaitVideoSyncSGI = epoxy_glXWaitVideoSyncSGI_dispatch_table_thunk;
    epoxy_glXWaitX = epoxy_glXWaitX_dispatch_table_thunk;
}

#endif /* !USING_DISPATCH_TABLE */
PUBLIC PFNGLXBINDCHANNELTOWINDOWSGIXPROC epoxy_glXBindChannelToWindowSGIX = epoxy_glXBindChannelToWindowSGIX_global_rewrite_ptr;

PUBLIC PFNGLXBINDHYPERPIPESGIXPROC epoxy_glXBindHyperpipeSGIX = epoxy_glXBindHyperpipeSGIX_global_rewrite_ptr;

PUBLIC PFNGLXBINDSWAPBARRIERNVPROC epoxy_glXBindSwapBarrierNV = epoxy_glXBindSwapBarrierNV_global_rewrite_ptr;

PUBLIC PFNGLXBINDSWAPBARRIERSGIXPROC epoxy_glXBindSwapBarrierSGIX = epoxy_glXBindSwapBarrierSGIX_global_rewrite_ptr;

PUBLIC PFNGLXBINDTEXIMAGEEXTPROC epoxy_glXBindTexImageEXT = epoxy_glXBindTexImageEXT_global_rewrite_ptr;

PUBLIC PFNGLXBINDVIDEOCAPTUREDEVICENVPROC epoxy_glXBindVideoCaptureDeviceNV = epoxy_glXBindVideoCaptureDeviceNV_global_rewrite_ptr;

PUBLIC PFNGLXBINDVIDEODEVICENVPROC epoxy_glXBindVideoDeviceNV = epoxy_glXBindVideoDeviceNV_global_rewrite_ptr;

PUBLIC PFNGLXBINDVIDEOIMAGENVPROC epoxy_glXBindVideoImageNV = epoxy_glXBindVideoImageNV_global_rewrite_ptr;

PUBLIC PFNGLXCHANNELRECTSGIXPROC epoxy_glXChannelRectSGIX = epoxy_glXChannelRectSGIX_global_rewrite_ptr;

PUBLIC PFNGLXCHANNELRECTSYNCSGIXPROC epoxy_glXChannelRectSyncSGIX = epoxy_glXChannelRectSyncSGIX_global_rewrite_ptr;

PUBLIC PFNGLXCHOOSEFBCONFIGPROC epoxy_glXChooseFBConfig = epoxy_glXChooseFBConfig_global_rewrite_ptr;

PUBLIC PFNGLXCHOOSEFBCONFIGSGIXPROC epoxy_glXChooseFBConfigSGIX = epoxy_glXChooseFBConfigSGIX_global_rewrite_ptr;

PUBLIC PFNGLXCHOOSEVISUALPROC epoxy_glXChooseVisual = epoxy_glXChooseVisual_global_rewrite_ptr;

PUBLIC PFNGLXCOPYCONTEXTPROC epoxy_glXCopyContext = epoxy_glXCopyContext_global_rewrite_ptr;

PUBLIC PFNGLXCOPYIMAGESUBDATANVPROC epoxy_glXCopyImageSubDataNV = epoxy_glXCopyImageSubDataNV_global_rewrite_ptr;

PUBLIC PFNGLXCOPYSUBBUFFERMESAPROC epoxy_glXCopySubBufferMESA = epoxy_glXCopySubBufferMESA_global_rewrite_ptr;

PUBLIC PFNGLXCREATECONTEXTPROC epoxy_glXCreateContext = epoxy_glXCreateContext_global_rewrite_ptr;

PUBLIC PFNGLXCREATECONTEXTATTRIBSARBPROC epoxy_glXCreateContextAttribsARB = epoxy_glXCreateContextAttribsARB_global_rewrite_ptr;

PUBLIC PFNGLXCREATECONTEXTWITHCONFIGSGIXPROC epoxy_glXCreateContextWithConfigSGIX = epoxy_glXCreateContextWithConfigSGIX_global_rewrite_ptr;

PUBLIC PFNGLXCREATEGLXPBUFFERSGIXPROC epoxy_glXCreateGLXPbufferSGIX = epoxy_glXCreateGLXPbufferSGIX_global_rewrite_ptr;

PUBLIC PFNGLXCREATEGLXPIXMAPPROC epoxy_glXCreateGLXPixmap = epoxy_glXCreateGLXPixmap_global_rewrite_ptr;

PUBLIC PFNGLXCREATEGLXPIXMAPMESAPROC epoxy_glXCreateGLXPixmapMESA = epoxy_glXCreateGLXPixmapMESA_global_rewrite_ptr;

PUBLIC PFNGLXCREATEGLXPIXMAPWITHCONFIGSGIXPROC epoxy_glXCreateGLXPixmapWithConfigSGIX = epoxy_glXCreateGLXPixmapWithConfigSGIX_global_rewrite_ptr;

PUBLIC PFNGLXCREATENEWCONTEXTPROC epoxy_glXCreateNewContext = epoxy_glXCreateNewContext_global_rewrite_ptr;

PUBLIC PFNGLXCREATEPBUFFERPROC epoxy_glXCreatePbuffer = epoxy_glXCreatePbuffer_global_rewrite_ptr;

PUBLIC PFNGLXCREATEPIXMAPPROC epoxy_glXCreatePixmap = epoxy_glXCreatePixmap_global_rewrite_ptr;

PUBLIC PFNGLXCREATEWINDOWPROC epoxy_glXCreateWindow = epoxy_glXCreateWindow_global_rewrite_ptr;

PUBLIC PFNGLXCUSHIONSGIPROC epoxy_glXCushionSGI = epoxy_glXCushionSGI_global_rewrite_ptr;

PUBLIC PFNGLXDELAYBEFORESWAPNVPROC epoxy_glXDelayBeforeSwapNV = epoxy_glXDelayBeforeSwapNV_global_rewrite_ptr;

PUBLIC PFNGLXDESTROYCONTEXTPROC epoxy_glXDestroyContext = epoxy_glXDestroyContext_global_rewrite_ptr;

PUBLIC PFNGLXDESTROYGLXPBUFFERSGIXPROC epoxy_glXDestroyGLXPbufferSGIX = epoxy_glXDestroyGLXPbufferSGIX_global_rewrite_ptr;

PUBLIC PFNGLXDESTROYGLXPIXMAPPROC epoxy_glXDestroyGLXPixmap = epoxy_glXDestroyGLXPixmap_global_rewrite_ptr;

PUBLIC PFNGLXDESTROYGLXVIDEOSOURCESGIXPROC epoxy_glXDestroyGLXVideoSourceSGIX = epoxy_glXDestroyGLXVideoSourceSGIX_global_rewrite_ptr;

PUBLIC PFNGLXDESTROYHYPERPIPECONFIGSGIXPROC epoxy_glXDestroyHyperpipeConfigSGIX = epoxy_glXDestroyHyperpipeConfigSGIX_global_rewrite_ptr;

PUBLIC PFNGLXDESTROYPBUFFERPROC epoxy_glXDestroyPbuffer = epoxy_glXDestroyPbuffer_global_rewrite_ptr;

PUBLIC PFNGLXDESTROYPIXMAPPROC epoxy_glXDestroyPixmap = epoxy_glXDestroyPixmap_global_rewrite_ptr;

PUBLIC PFNGLXDESTROYWINDOWPROC epoxy_glXDestroyWindow = epoxy_glXDestroyWindow_global_rewrite_ptr;

PUBLIC PFNGLXENUMERATEVIDEOCAPTUREDEVICESNVPROC epoxy_glXEnumerateVideoCaptureDevicesNV = epoxy_glXEnumerateVideoCaptureDevicesNV_global_rewrite_ptr;

PUBLIC PFNGLXENUMERATEVIDEODEVICESNVPROC epoxy_glXEnumerateVideoDevicesNV = epoxy_glXEnumerateVideoDevicesNV_global_rewrite_ptr;

PUBLIC PFNGLXFREECONTEXTEXTPROC epoxy_glXFreeContextEXT = epoxy_glXFreeContextEXT_global_rewrite_ptr;

PUBLIC PFNGLXGETAGPOFFSETMESAPROC epoxy_glXGetAGPOffsetMESA = epoxy_glXGetAGPOffsetMESA_global_rewrite_ptr;

PUBLIC PFNGLXGETCLIENTSTRINGPROC epoxy_glXGetClientString = epoxy_glXGetClientString_global_rewrite_ptr;

PUBLIC PFNGLXGETCONFIGPROC epoxy_glXGetConfig = epoxy_glXGetConfig_global_rewrite_ptr;

PUBLIC PFNGLXGETCONTEXTIDEXTPROC epoxy_glXGetContextIDEXT = epoxy_glXGetContextIDEXT_global_rewrite_ptr;

PUBLIC PFNGLXGETCURRENTCONTEXTPROC epoxy_glXGetCurrentContext = epoxy_glXGetCurrentContext_global_rewrite_ptr;

PUBLIC PFNGLXGETCURRENTDISPLAYPROC epoxy_glXGetCurrentDisplay = epoxy_glXGetCurrentDisplay_global_rewrite_ptr;

PUBLIC PFNGLXGETCURRENTDISPLAYEXTPROC epoxy_glXGetCurrentDisplayEXT = epoxy_glXGetCurrentDisplayEXT_global_rewrite_ptr;

PUBLIC PFNGLXGETCURRENTDRAWABLEPROC epoxy_glXGetCurrentDrawable = epoxy_glXGetCurrentDrawable_global_rewrite_ptr;

PUBLIC PFNGLXGETCURRENTREADDRAWABLEPROC epoxy_glXGetCurrentReadDrawable = epoxy_glXGetCurrentReadDrawable_global_rewrite_ptr;

PUBLIC PFNGLXGETCURRENTREADDRAWABLESGIPROC epoxy_glXGetCurrentReadDrawableSGI = epoxy_glXGetCurrentReadDrawableSGI_global_rewrite_ptr;

PUBLIC PFNGLXGETFBCONFIGATTRIBPROC epoxy_glXGetFBConfigAttrib = epoxy_glXGetFBConfigAttrib_global_rewrite_ptr;

PUBLIC PFNGLXGETFBCONFIGATTRIBSGIXPROC epoxy_glXGetFBConfigAttribSGIX = epoxy_glXGetFBConfigAttribSGIX_global_rewrite_ptr;

PUBLIC PFNGLXGETFBCONFIGFROMVISUALSGIXPROC epoxy_glXGetFBConfigFromVisualSGIX = epoxy_glXGetFBConfigFromVisualSGIX_global_rewrite_ptr;

PUBLIC PFNGLXGETFBCONFIGSPROC epoxy_glXGetFBConfigs = epoxy_glXGetFBConfigs_global_rewrite_ptr;

PUBLIC PFNGLXGETMSCRATEOMLPROC epoxy_glXGetMscRateOML = epoxy_glXGetMscRateOML_global_rewrite_ptr;

PUBLIC PFNGLXGETPROCADDRESSPROC epoxy_glXGetProcAddress = epoxy_glXGetProcAddress_global_rewrite_ptr;

PUBLIC PFNGLXGETPROCADDRESSARBPROC epoxy_glXGetProcAddressARB = epoxy_glXGetProcAddressARB_global_rewrite_ptr;

PUBLIC PFNGLXGETSELECTEDEVENTPROC epoxy_glXGetSelectedEvent = epoxy_glXGetSelectedEvent_global_rewrite_ptr;

PUBLIC PFNGLXGETSELECTEDEVENTSGIXPROC epoxy_glXGetSelectedEventSGIX = epoxy_glXGetSelectedEventSGIX_global_rewrite_ptr;

PUBLIC PFNGLXGETSYNCVALUESOMLPROC epoxy_glXGetSyncValuesOML = epoxy_glXGetSyncValuesOML_global_rewrite_ptr;

PUBLIC PFNGLXGETTRANSPARENTINDEXSUNPROC epoxy_glXGetTransparentIndexSUN = epoxy_glXGetTransparentIndexSUN_global_rewrite_ptr;

PUBLIC PFNGLXGETVIDEODEVICENVPROC epoxy_glXGetVideoDeviceNV = epoxy_glXGetVideoDeviceNV_global_rewrite_ptr;

PUBLIC PFNGLXGETVIDEOINFONVPROC epoxy_glXGetVideoInfoNV = epoxy_glXGetVideoInfoNV_global_rewrite_ptr;

PUBLIC PFNGLXGETVIDEOSYNCSGIPROC epoxy_glXGetVideoSyncSGI = epoxy_glXGetVideoSyncSGI_global_rewrite_ptr;

PUBLIC PFNGLXGETVISUALFROMFBCONFIGPROC epoxy_glXGetVisualFromFBConfig = epoxy_glXGetVisualFromFBConfig_global_rewrite_ptr;

PUBLIC PFNGLXGETVISUALFROMFBCONFIGSGIXPROC epoxy_glXGetVisualFromFBConfigSGIX = epoxy_glXGetVisualFromFBConfigSGIX_global_rewrite_ptr;

PUBLIC PFNGLXHYPERPIPEATTRIBSGIXPROC epoxy_glXHyperpipeAttribSGIX = epoxy_glXHyperpipeAttribSGIX_global_rewrite_ptr;

PUBLIC PFNGLXHYPERPIPECONFIGSGIXPROC epoxy_glXHyperpipeConfigSGIX = epoxy_glXHyperpipeConfigSGIX_global_rewrite_ptr;

PUBLIC PFNGLXIMPORTCONTEXTEXTPROC epoxy_glXImportContextEXT = epoxy_glXImportContextEXT_global_rewrite_ptr;

PUBLIC PFNGLXISDIRECTPROC epoxy_glXIsDirect = epoxy_glXIsDirect_global_rewrite_ptr;

PUBLIC PFNGLXJOINSWAPGROUPNVPROC epoxy_glXJoinSwapGroupNV = epoxy_glXJoinSwapGroupNV_global_rewrite_ptr;

PUBLIC PFNGLXJOINSWAPGROUPSGIXPROC epoxy_glXJoinSwapGroupSGIX = epoxy_glXJoinSwapGroupSGIX_global_rewrite_ptr;

PUBLIC PFNGLXLOCKVIDEOCAPTUREDEVICENVPROC epoxy_glXLockVideoCaptureDeviceNV = epoxy_glXLockVideoCaptureDeviceNV_global_rewrite_ptr;

PUBLIC PFNGLXMAKECONTEXTCURRENTPROC epoxy_glXMakeContextCurrent = epoxy_glXMakeContextCurrent_global_rewrite_ptr;

PUBLIC PFNGLXMAKECURRENTPROC epoxy_glXMakeCurrent = epoxy_glXMakeCurrent_global_rewrite_ptr;

PUBLIC PFNGLXMAKECURRENTREADSGIPROC epoxy_glXMakeCurrentReadSGI = epoxy_glXMakeCurrentReadSGI_global_rewrite_ptr;

PUBLIC PFNGLXQUERYCHANNELDELTASSGIXPROC epoxy_glXQueryChannelDeltasSGIX = epoxy_glXQueryChannelDeltasSGIX_global_rewrite_ptr;

PUBLIC PFNGLXQUERYCHANNELRECTSGIXPROC epoxy_glXQueryChannelRectSGIX = epoxy_glXQueryChannelRectSGIX_global_rewrite_ptr;

PUBLIC PFNGLXQUERYCONTEXTPROC epoxy_glXQueryContext = epoxy_glXQueryContext_global_rewrite_ptr;

PUBLIC PFNGLXQUERYCONTEXTINFOEXTPROC epoxy_glXQueryContextInfoEXT = epoxy_glXQueryContextInfoEXT_global_rewrite_ptr;

PUBLIC PFNGLXQUERYCURRENTRENDERERINTEGERMESAPROC epoxy_glXQueryCurrentRendererIntegerMESA = epoxy_glXQueryCurrentRendererIntegerMESA_global_rewrite_ptr;

PUBLIC PFNGLXQUERYCURRENTRENDERERSTRINGMESAPROC epoxy_glXQueryCurrentRendererStringMESA = epoxy_glXQueryCurrentRendererStringMESA_global_rewrite_ptr;

PUBLIC PFNGLXQUERYDRAWABLEPROC epoxy_glXQueryDrawable = epoxy_glXQueryDrawable_global_rewrite_ptr;

PUBLIC PFNGLXQUERYEXTENSIONPROC epoxy_glXQueryExtension = epoxy_glXQueryExtension_global_rewrite_ptr;

PUBLIC PFNGLXQUERYEXTENSIONSSTRINGPROC epoxy_glXQueryExtensionsString = epoxy_glXQueryExtensionsString_global_rewrite_ptr;

PUBLIC PFNGLXQUERYFRAMECOUNTNVPROC epoxy_glXQueryFrameCountNV = epoxy_glXQueryFrameCountNV_global_rewrite_ptr;

PUBLIC PFNGLXQUERYGLXPBUFFERSGIXPROC epoxy_glXQueryGLXPbufferSGIX = epoxy_glXQueryGLXPbufferSGIX_global_rewrite_ptr;

PUBLIC PFNGLXQUERYHYPERPIPEATTRIBSGIXPROC epoxy_glXQueryHyperpipeAttribSGIX = epoxy_glXQueryHyperpipeAttribSGIX_global_rewrite_ptr;

PUBLIC PFNGLXQUERYHYPERPIPEBESTATTRIBSGIXPROC epoxy_glXQueryHyperpipeBestAttribSGIX = epoxy_glXQueryHyperpipeBestAttribSGIX_global_rewrite_ptr;

PUBLIC PFNGLXQUERYHYPERPIPECONFIGSGIXPROC epoxy_glXQueryHyperpipeConfigSGIX = epoxy_glXQueryHyperpipeConfigSGIX_global_rewrite_ptr;

PUBLIC PFNGLXQUERYHYPERPIPENETWORKSGIXPROC epoxy_glXQueryHyperpipeNetworkSGIX = epoxy_glXQueryHyperpipeNetworkSGIX_global_rewrite_ptr;

PUBLIC PFNGLXQUERYMAXSWAPBARRIERSSGIXPROC epoxy_glXQueryMaxSwapBarriersSGIX = epoxy_glXQueryMaxSwapBarriersSGIX_global_rewrite_ptr;

PUBLIC PFNGLXQUERYMAXSWAPGROUPSNVPROC epoxy_glXQueryMaxSwapGroupsNV = epoxy_glXQueryMaxSwapGroupsNV_global_rewrite_ptr;

PUBLIC PFNGLXQUERYRENDERERINTEGERMESAPROC epoxy_glXQueryRendererIntegerMESA = epoxy_glXQueryRendererIntegerMESA_global_rewrite_ptr;

PUBLIC PFNGLXQUERYRENDERERSTRINGMESAPROC epoxy_glXQueryRendererStringMESA = epoxy_glXQueryRendererStringMESA_global_rewrite_ptr;

PUBLIC PFNGLXQUERYSERVERSTRINGPROC epoxy_glXQueryServerString = epoxy_glXQueryServerString_global_rewrite_ptr;

PUBLIC PFNGLXQUERYSWAPGROUPNVPROC epoxy_glXQuerySwapGroupNV = epoxy_glXQuerySwapGroupNV_global_rewrite_ptr;

PUBLIC PFNGLXQUERYVERSIONPROC epoxy_glXQueryVersion = epoxy_glXQueryVersion_global_rewrite_ptr;

PUBLIC PFNGLXQUERYVIDEOCAPTUREDEVICENVPROC epoxy_glXQueryVideoCaptureDeviceNV = epoxy_glXQueryVideoCaptureDeviceNV_global_rewrite_ptr;

PUBLIC PFNGLXRELEASEBUFFERSMESAPROC epoxy_glXReleaseBuffersMESA = epoxy_glXReleaseBuffersMESA_global_rewrite_ptr;

PUBLIC PFNGLXRELEASETEXIMAGEEXTPROC epoxy_glXReleaseTexImageEXT = epoxy_glXReleaseTexImageEXT_global_rewrite_ptr;

PUBLIC PFNGLXRELEASEVIDEOCAPTUREDEVICENVPROC epoxy_glXReleaseVideoCaptureDeviceNV = epoxy_glXReleaseVideoCaptureDeviceNV_global_rewrite_ptr;

PUBLIC PFNGLXRELEASEVIDEODEVICENVPROC epoxy_glXReleaseVideoDeviceNV = epoxy_glXReleaseVideoDeviceNV_global_rewrite_ptr;

PUBLIC PFNGLXRELEASEVIDEOIMAGENVPROC epoxy_glXReleaseVideoImageNV = epoxy_glXReleaseVideoImageNV_global_rewrite_ptr;

PUBLIC PFNGLXRESETFRAMECOUNTNVPROC epoxy_glXResetFrameCountNV = epoxy_glXResetFrameCountNV_global_rewrite_ptr;

PUBLIC PFNGLXSELECTEVENTPROC epoxy_glXSelectEvent = epoxy_glXSelectEvent_global_rewrite_ptr;

PUBLIC PFNGLXSELECTEVENTSGIXPROC epoxy_glXSelectEventSGIX = epoxy_glXSelectEventSGIX_global_rewrite_ptr;

PUBLIC PFNGLXSENDPBUFFERTOVIDEONVPROC epoxy_glXSendPbufferToVideoNV = epoxy_glXSendPbufferToVideoNV_global_rewrite_ptr;

PUBLIC PFNGLXSET3DFXMODEMESAPROC epoxy_glXSet3DfxModeMESA = epoxy_glXSet3DfxModeMESA_global_rewrite_ptr;

PUBLIC PFNGLXSWAPBUFFERSPROC epoxy_glXSwapBuffers = epoxy_glXSwapBuffers_global_rewrite_ptr;

PUBLIC PFNGLXSWAPBUFFERSMSCOMLPROC epoxy_glXSwapBuffersMscOML = epoxy_glXSwapBuffersMscOML_global_rewrite_ptr;

PUBLIC PFNGLXSWAPINTERVALEXTPROC epoxy_glXSwapIntervalEXT = epoxy_glXSwapIntervalEXT_global_rewrite_ptr;

PUBLIC PFNGLXSWAPINTERVALSGIPROC epoxy_glXSwapIntervalSGI = epoxy_glXSwapIntervalSGI_global_rewrite_ptr;

PUBLIC PFNGLXUSEXFONTPROC epoxy_glXUseXFont = epoxy_glXUseXFont_global_rewrite_ptr;

PUBLIC PFNGLXWAITFORMSCOMLPROC epoxy_glXWaitForMscOML = epoxy_glXWaitForMscOML_global_rewrite_ptr;

PUBLIC PFNGLXWAITFORSBCOMLPROC epoxy_glXWaitForSbcOML = epoxy_glXWaitForSbcOML_global_rewrite_ptr;

PUBLIC PFNGLXWAITGLPROC epoxy_glXWaitGL = epoxy_glXWaitGL_global_rewrite_ptr;

PUBLIC PFNGLXWAITVIDEOSYNCSGIPROC epoxy_glXWaitVideoSyncSGI = epoxy_glXWaitVideoSyncSGI_global_rewrite_ptr;

PUBLIC PFNGLXWAITXPROC epoxy_glXWaitX = epoxy_glXWaitX_global_rewrite_ptr;

