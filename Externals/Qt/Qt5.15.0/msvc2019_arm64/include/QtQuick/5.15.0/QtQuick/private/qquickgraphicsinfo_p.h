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

#ifndef QQUICKGRAPHICSINFO_P_H
#define QQUICKGRAPHICSINFO_P_H

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

#include <QtCore/qobject.h>
#include <QtCore/qpointer.h>
#include <QtQml/qqml.h>
#include <QtGui/qsurfaceformat.h>
#include <QtQuick/qsgrendererinterface.h>

QT_BEGIN_NAMESPACE

class QQuickItem;
class QQuickWindow;

class QQuickGraphicsInfo : public QObject
{
    Q_OBJECT
    Q_PROPERTY(GraphicsApi api READ api NOTIFY apiChanged FINAL)
    Q_PROPERTY(ShaderType shaderType READ shaderType NOTIFY shaderTypeChanged FINAL)
    Q_PROPERTY(ShaderCompilationType shaderCompilationType READ shaderCompilationType NOTIFY shaderCompilationTypeChanged FINAL)
    Q_PROPERTY(ShaderSourceType shaderSourceType READ shaderSourceType NOTIFY shaderSourceTypeChanged FINAL)

    Q_PROPERTY(int majorVersion READ majorVersion NOTIFY majorVersionChanged FINAL)
    Q_PROPERTY(int minorVersion READ minorVersion NOTIFY minorVersionChanged FINAL)
    Q_PROPERTY(OpenGLContextProfile profile READ profile NOTIFY profileChanged FINAL)
    Q_PROPERTY(RenderableType renderableType READ renderableType NOTIFY renderableTypeChanged FINAL)

    QML_NAMED_ELEMENT(GraphicsInfo)
    QML_ADDED_IN_MINOR_VERSION(8)
    QML_UNCREATABLE("GraphicsInfo is only available via attached properties.")
    QML_ATTACHED(QQuickGraphicsInfo)

public:
    enum GraphicsApi {
        Unknown = QSGRendererInterface::Unknown,
        Software = QSGRendererInterface::Software,
        OpenGL = QSGRendererInterface::OpenGL,
        Direct3D12 = QSGRendererInterface::Direct3D12,
        OpenVG = QSGRendererInterface::OpenVG,
        OpenGLRhi = QSGRendererInterface::OpenGLRhi,
        Direct3D11Rhi = QSGRendererInterface::Direct3D11Rhi,
        VulkanRhi = QSGRendererInterface::VulkanRhi,
        MetalRhi = QSGRendererInterface::MetalRhi,
        NullRhi = QSGRendererInterface::NullRhi
    };
    Q_ENUM(GraphicsApi)

    enum ShaderType {
        UnknownShadingLanguage = QSGRendererInterface::UnknownShadingLanguage,
        GLSL = QSGRendererInterface::GLSL,
        HLSL = QSGRendererInterface::HLSL,
        RhiShader = QSGRendererInterface::RhiShader
    };
    Q_ENUM(ShaderType)

    enum ShaderCompilationType {
        RuntimeCompilation = QSGRendererInterface::RuntimeCompilation,
        OfflineCompilation = QSGRendererInterface::OfflineCompilation
    };
    Q_ENUM(ShaderCompilationType)

    enum ShaderSourceType {
        ShaderSourceString = QSGRendererInterface::ShaderSourceString,
        ShaderSourceFile = QSGRendererInterface::ShaderSourceFile,
        ShaderByteCode = QSGRendererInterface::ShaderByteCode
    };
    Q_ENUM(ShaderSourceType)

    enum OpenGLContextProfile {
        OpenGLNoProfile = QSurfaceFormat::NoProfile,
        OpenGLCoreProfile = QSurfaceFormat::CoreProfile,
        OpenGLCompatibilityProfile = QSurfaceFormat::CompatibilityProfile
    };
    Q_ENUM(OpenGLContextProfile)

    enum RenderableType {
        SurfaceFormatUnspecified = QSurfaceFormat::DefaultRenderableType,
        SurfaceFormatOpenGL = QSurfaceFormat::OpenGL,
        SurfaceFormatOpenGLES = QSurfaceFormat::OpenGLES
    };
    Q_ENUM(RenderableType)

    QQuickGraphicsInfo(QQuickItem *item = 0);

    static QQuickGraphicsInfo *qmlAttachedProperties(QObject *object);

    GraphicsApi api() const { return m_api; }
    ShaderType shaderType() const { return m_shaderType; }
    ShaderCompilationType shaderCompilationType() const { return m_shaderCompilationType; }
    ShaderSourceType shaderSourceType() const { return m_shaderSourceType; }

    int majorVersion() const { return m_majorVersion; }
    int minorVersion() const { return m_minorVersion; }
    OpenGLContextProfile profile() const { return m_profile; }
    RenderableType renderableType() const { return m_renderableType; }

Q_SIGNALS:
    void apiChanged();
    void shaderTypeChanged();
    void shaderCompilationTypeChanged();
    void shaderSourceTypeChanged();

    void majorVersionChanged();
    void minorVersionChanged();
    void profileChanged();
    void renderableTypeChanged();

private Q_SLOTS:
    void updateInfo();
    void setWindow(QQuickWindow *window);

private:
    QPointer<QQuickWindow> m_window;
    GraphicsApi m_api;
    ShaderType m_shaderType;
    ShaderCompilationType m_shaderCompilationType;
    ShaderSourceType m_shaderSourceType;
    int m_majorVersion;
    int m_minorVersion;
    OpenGLContextProfile m_profile;
    RenderableType m_renderableType;
};

QT_END_NAMESPACE

#endif // QQUICKGRAPHICSINFO_P_H
