/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QEGLSTREAMCONVENIENCE_H
#define QEGLSTREAMCONVENIENCE_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <qglobal.h>
#include <QtEglSupport/private/qt_egl_p.h>

// This provides runtime EGLDevice/Output/Stream support even when eglext.h in
// the sysroot is not up-to-date.

#ifndef EGL_VERSION_1_5
typedef intptr_t EGLAttrib;
#endif

#ifndef EGL_EXT_platform_base
typedef EGLDisplay (EGLAPIENTRYP PFNEGLGETPLATFORMDISPLAYEXTPROC) (EGLenum platform, void *native_display, const EGLint *attrib_list);
#endif

#ifndef EGL_EXT_device_base
typedef void *EGLDeviceEXT;
#define EGL_NO_DEVICE_EXT                 ((EGLDeviceEXT)(0))
typedef EGLBoolean (EGLAPIENTRYP PFNEGLQUERYDEVICESEXTPROC) (EGLint max_devices, EGLDeviceEXT *devices, EGLint *num_devices);
typedef const char *(EGLAPIENTRYP PFNEGLQUERYDEVICESTRINGEXTPROC) (EGLDeviceEXT device, EGLint name);
#endif

#ifndef EGL_EXT_output_base
typedef void *EGLOutputLayerEXT;
typedef void *EGLOutputPortEXT;
#define EGL_NO_OUTPUT_LAYER_EXT           ((EGLOutputLayerEXT)0)
typedef EGLBoolean (EGLAPIENTRYP PFNEGLGETOUTPUTLAYERSEXTPROC) (EGLDisplay dpy, const EGLAttrib *attrib_list, EGLOutputLayerEXT *layers, EGLint max_layers, EGLint *num_layers);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLGETOUTPUTPORTSEXTPROC) (EGLDisplay dpy, const EGLAttrib *attrib_list, EGLOutputPortEXT *ports, EGLint max_ports, EGLint *num_ports);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLOUTPUTLAYERATTRIBEXTPROC) (EGLDisplay dpy, EGLOutputLayerEXT layer, EGLint attribute, EGLAttrib value);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLQUERYOUTPUTLAYERATTRIBEXTPROC) (EGLDisplay dpy, EGLOutputLayerEXT layer, EGLint attribute, EGLAttrib *value);
typedef const char *(EGLAPIENTRYP PFNEGLQUERYOUTPUTLAYERSTRINGEXTPROC) (EGLDisplay dpy, EGLOutputLayerEXT layer, EGLint name);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLQUERYOUTPUTPORTATTRIBEXTPROC) (EGLDisplay dpy, EGLOutputPortEXT port, EGLint attribute, EGLAttrib *value);
typedef const char *(EGLAPIENTRYP PFNEGLQUERYOUTPUTPORTSTRINGEXTPROC) (EGLDisplay dpy, EGLOutputPortEXT port, EGLint name);
#endif

#ifndef EGL_KHR_stream
typedef void *EGLStreamKHR;
typedef quint64 EGLuint64KHR;
#define EGL_NO_STREAM_KHR                 ((EGLStreamKHR)0)
#define EGL_STREAM_STATE_KHR              0x3214
#define EGL_STREAM_STATE_CREATED_KHR      0x3215
#define EGL_STREAM_STATE_CONNECTING_KHR   0x3216
#define EGL_STREAM_STATE_EMPTY_KHR        0x3217
#define EGL_STREAM_STATE_NEW_FRAME_AVAILABLE_KHR 0x3218
#define EGL_STREAM_STATE_OLD_FRAME_AVAILABLE_KHR 0x3219
#define EGL_STREAM_STATE_DISCONNECTED_KHR 0x321A
#define EGL_BAD_STREAM_KHR                0x321B
#define EGL_BAD_STATE_KHR                 0x321C
typedef EGLStreamKHR (EGLAPIENTRYP PFNEGLCREATESTREAMKHRPROC) (EGLDisplay dpy, const EGLint *attrib_list);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLDESTROYSTREAMKHRPROC) (EGLDisplay dpy, EGLStreamKHR stream);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLSTREAMATTRIBKHRPROC) (EGLDisplay dpy, EGLStreamKHR stream, EGLenum attribute, EGLint value);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLQUERYSTREAMKHRPROC) (EGLDisplay dpy, EGLStreamKHR stream, EGLenum attribute, EGLint *value);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLQUERYSTREAMU64KHRPROC) (EGLDisplay dpy, EGLStreamKHR stream, EGLenum attribute, EGLuint64KHR *value);
#endif

#ifndef EGL_KHR_stream_fifo
#define EGL_STREAM_FIFO_LENGTH_KHR        0x31FC
#endif

#ifndef EGL_KHR_stream_producer_eglsurface
#define EGL_STREAM_BIT_KHR                0x0800
typedef EGLSurface (EGLAPIENTRYP PFNEGLCREATESTREAMPRODUCERSURFACEKHRPROC) (EGLDisplay dpy, EGLConfig config, EGLStreamKHR stream, const EGLint *attrib_list);
#endif

#ifndef EGL_KHR_stream_cross_process_fd
typedef int EGLNativeFileDescriptorKHR;
#define EGL_NO_FILE_DESCRIPTOR_KHR        ((EGLNativeFileDescriptorKHR)(-1))
typedef EGLNativeFileDescriptorKHR (EGLAPIENTRYP PFNEGLGETSTREAMFILEDESCRIPTORKHRPROC) (EGLDisplay dpy, EGLStreamKHR stream);
typedef EGLStreamKHR (EGLAPIENTRYP PFNEGLCREATESTREAMFROMFILEDESCRIPTORKHRPROC) (EGLDisplay dpy, EGLNativeFileDescriptorKHR file_descriptor);
#endif

#ifndef EGL_KHR_stream_consumer_gltexture
typedef EGLBoolean (EGLAPIENTRYP PFNEGLSTREAMCONSUMERGLTEXTUREEXTERNALKHRPROC) (EGLDisplay dpy, EGLStreamKHR stream);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLSTREAMCONSUMERACQUIREKHRPROC) (EGLDisplay dpy, EGLStreamKHR stream);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLSTREAMCONSUMERRELEASEKHRPROC) (EGLDisplay dpy, EGLStreamKHR stream);
#endif

#ifndef EGL_EXT_stream_consumer_egloutput
typedef EGLBoolean (EGLAPIENTRYP PFNEGLSTREAMCONSUMEROUTPUTEXTPROC) (EGLDisplay dpy, EGLStreamKHR stream, EGLOutputLayerEXT layer);
#endif

#ifndef EGL_EXT_platform_device
#define EGL_PLATFORM_DEVICE_EXT           0x313F
#endif

#ifndef EGL_EXT_device_drm
#define EGL_DRM_DEVICE_FILE_EXT           0x3233
#endif

#ifndef EGL_EXT_output_drm
#define EGL_DRM_CRTC_EXT                  0x3234
#define EGL_DRM_PLANE_EXT                 0x3235
#endif

#ifndef EGL_PLATFORM_X11_KHR
#define EGL_PLATFORM_X11_KHR              0x31D5
#endif

#ifndef EGL_NV_stream_attrib
typedef EGLStreamKHR (EGLAPIENTRYP PFNEGLCREATESTREAMATTRIBNVPROC) (EGLDisplay dpy, const EGLAttrib *attrib_list);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLSETSTREAMATTRIBNVPROC) (EGLDisplay dpy, EGLStreamKHR stream, EGLenum attribute, EGLAttrib value);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLQUERYSTREAMATTRIBNVPROC) (EGLDisplay dpy, EGLStreamKHR stream, EGLenum attribute, EGLAttrib *value);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLSTREAMCONSUMERACQUIREATTRIBNVPROC) (EGLDisplay dpy, EGLStreamKHR stream, const EGLAttrib *attrib_list);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLSTREAMCONSUMERRELEASEATTRIBNVPROC) (EGLDisplay dpy, EGLStreamKHR stream, const EGLAttrib *attrib_list);
#endif

QT_BEGIN_NAMESPACE

class QEGLStreamConvenience
{
public:
    QEGLStreamConvenience();
    void initialize(EGLDisplay dpy);

    PFNEGLGETPLATFORMDISPLAYEXTPROC get_platform_display;
    PFNEGLQUERYDEVICESEXTPROC query_devices;
    PFNEGLQUERYDEVICESTRINGEXTPROC query_device_string;
    PFNEGLCREATESTREAMKHRPROC create_stream;
    PFNEGLCREATESTREAMATTRIBNVPROC create_stream_attrib_nv;
    PFNEGLSETSTREAMATTRIBNVPROC set_stream_attrib_nv;
    PFNEGLQUERYSTREAMATTRIBNVPROC query_stream_attrib_nv;
    PFNEGLSTREAMCONSUMERACQUIREATTRIBNVPROC acquire_stream_attrib_nv;
    PFNEGLSTREAMCONSUMERRELEASEATTRIBNVPROC release_stream_attrib_nv;
    PFNEGLDESTROYSTREAMKHRPROC destroy_stream;
    PFNEGLSTREAMATTRIBKHRPROC stream_attrib;
    PFNEGLQUERYSTREAMKHRPROC query_stream;
    PFNEGLQUERYSTREAMU64KHRPROC query_stream_u64;
    PFNEGLCREATESTREAMPRODUCERSURFACEKHRPROC create_stream_producer_surface;
    PFNEGLSTREAMCONSUMEROUTPUTEXTPROC stream_consumer_output;
    PFNEGLGETOUTPUTLAYERSEXTPROC get_output_layers;
    PFNEGLGETOUTPUTPORTSEXTPROC get_output_ports;
    PFNEGLOUTPUTLAYERATTRIBEXTPROC output_layer_attrib;
    PFNEGLQUERYOUTPUTLAYERATTRIBEXTPROC query_output_layer_attrib;
    PFNEGLQUERYOUTPUTLAYERSTRINGEXTPROC query_output_layer_string;
    PFNEGLQUERYOUTPUTPORTATTRIBEXTPROC query_output_port_attrib;
    PFNEGLQUERYOUTPUTPORTSTRINGEXTPROC query_output_port_string;
    PFNEGLGETSTREAMFILEDESCRIPTORKHRPROC get_stream_file_descriptor;
    PFNEGLCREATESTREAMFROMFILEDESCRIPTORKHRPROC create_stream_from_file_descriptor;
    PFNEGLSTREAMCONSUMERGLTEXTUREEXTERNALKHRPROC stream_consumer_gltexture;
    PFNEGLSTREAMCONSUMERACQUIREKHRPROC stream_consumer_acquire;
    PFNEGLSTREAMCONSUMERRELEASEKHRPROC stream_consumer_release;

    bool initialized;

    bool has_egl_platform_device;
    bool has_egl_device_base;
    bool has_egl_stream;
    bool has_egl_stream_producer_eglsurface;
    bool has_egl_stream_consumer_egloutput;
    bool has_egl_output_drm;
    bool has_egl_output_base;
    bool has_egl_stream_cross_process_fd;
    bool has_egl_stream_consumer_gltexture;
};

QT_END_NAMESPACE

#endif
