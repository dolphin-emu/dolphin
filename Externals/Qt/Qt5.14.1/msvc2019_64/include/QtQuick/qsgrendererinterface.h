/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQuick module of the Qt Toolkit.
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

#ifndef QSGRENDERERINTERFACE_H
#define QSGRENDERERINTERFACE_H

#include <QtQuick/qsgnode.h>

QT_BEGIN_NAMESPACE

class QQuickWindow;

class Q_QUICK_EXPORT QSGRendererInterface
{
public:
    enum GraphicsApi {
        Unknown,
        Software,
        OpenGL,
        Direct3D12,
        OpenVG,
        OpenGLRhi,
        Direct3D11Rhi,
        VulkanRhi,
        MetalRhi,
        NullRhi,
    };

    enum Resource {
        DeviceResource,
        CommandQueueResource,
        CommandListResource,
        PainterResource,
        RhiResource,
        PhysicalDeviceResource,
        OpenGLContextResource,
        DeviceContextResource,
        CommandEncoderResource,
        VulkanInstanceResource,
        RenderPassResource
    };

    enum ShaderType {
        UnknownShadingLanguage,
        GLSL,
        HLSL,
        RhiShader
    };

    enum ShaderCompilationType {
        RuntimeCompilation = 0x01,
        OfflineCompilation = 0x02
    };
    Q_DECLARE_FLAGS(ShaderCompilationTypes, ShaderCompilationType)

    enum ShaderSourceType {
        ShaderSourceString = 0x01,
        ShaderSourceFile = 0x02,
        ShaderByteCode = 0x04
    };
    Q_DECLARE_FLAGS(ShaderSourceTypes, ShaderSourceType)

    virtual ~QSGRendererInterface();

    virtual GraphicsApi graphicsApi() const = 0;

    virtual void *getResource(QQuickWindow *window, Resource resource) const;
    virtual void *getResource(QQuickWindow *window, const char *resource) const;

    virtual ShaderType shaderType() const = 0;
    virtual ShaderCompilationTypes shaderCompilationType() const = 0;
    virtual ShaderSourceTypes shaderSourceType() const = 0;

    static bool isApiRhiBased(GraphicsApi api);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QSGRendererInterface::ShaderCompilationTypes)
Q_DECLARE_OPERATORS_FOR_FLAGS(QSGRendererInterface::ShaderSourceTypes)

QT_END_NAMESPACE

#endif
