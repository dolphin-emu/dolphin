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

#ifndef QSGDEFAULTRENDERCONTEXT_H
#define QSGDEFAULTRENDERCONTEXT_H

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

#include <QtQuick/private/qsgcontext_p.h>
#include <QtGui/private/qshader_p.h>

#if QT_CONFIG(opengl)
#include <QtQuick/private/qsgdepthstencilbuffer_p.h>
#endif

QT_BEGIN_NAMESPACE

class QRhi;
class QRhiCommandBuffer;
class QRhiRenderPassDescriptor;
class QOpenGLContext;
class QSGMaterialShader;
class QSGMaterialRhiShader;
class QOpenGLFramebufferObject;
class QSGDepthStencilBufferManager;
class QSGDepthStencilBuffer;

namespace QSGOpenGLAtlasTexture {
    class Manager;
}

namespace QSGRhiAtlasTexture {
    class Manager;
}

class Q_QUICK_PRIVATE_EXPORT QSGDefaultRenderContext : public QSGRenderContext
{
    Q_OBJECT
public:
    QSGDefaultRenderContext(QSGContext *context);

    QRhi *rhi() const override { return m_rhi; }
    QOpenGLContext *openglContext() const { return m_gl; }
    bool isValid() const override { return m_gl || m_rhi; }

    static const int INIT_PARAMS_MAGIC = 0x50E;
    struct InitParams : public QSGRenderContext::InitParams {
        int sType = INIT_PARAMS_MAGIC; // help discovering broken code passing something else as 'context'
        QRhi *rhi = nullptr;
        int sampleCount = 1; // 1, 4, 8, ...
        QOpenGLContext *openGLContext = nullptr; // ### Qt 6: remove
        // only used as a hint f.ex. in the texture atlas init
        QSize initialSurfacePixelSize;
        // The first window that will be used with this rc, if available.
        // Only a hint, to help picking better values for atlases.
        QSurface *maybeSurface = nullptr;
    };

    void initialize(const QSGRenderContext::InitParams *params) override;
    void invalidate() override;

    void prepareSync(qreal devicePixelRatio) override;
    void beginNextFrame(QSGRenderer *renderer,
                        RenderPassCallback mainPassRecordingStart,
                        RenderPassCallback mainPassRecordingEnd,
                        void *callbackUserData) override;
    void renderNextFrame(QSGRenderer *renderer, uint fboId) override;
    void endNextFrame(QSGRenderer *renderer) override;

    void beginNextRhiFrame(QSGRenderer *renderer,
                           QRhiRenderTarget *rt, QRhiRenderPassDescriptor *rp, QRhiCommandBuffer *cb,
                           RenderPassCallback mainPassRecordingStart,
                           RenderPassCallback mainPassRecordingEnd,
                           void *callbackUserData) override;
    void renderNextRhiFrame(QSGRenderer *renderer) override;
    void endNextRhiFrame(QSGRenderer *renderer) override;

    QSGDistanceFieldGlyphCache *distanceFieldGlyphCache(const QRawFont &font) override;

    virtual QSharedPointer<QSGDepthStencilBuffer> depthStencilBufferForFbo(QOpenGLFramebufferObject *fbo);
    QSGDepthStencilBufferManager *depthStencilBufferManager();

    QSGTexture *createTexture(const QImage &image, uint flags) const override;
    QSGRenderer *createRenderer() override;
    QSGTexture *compressedTextureForFactory(const QSGCompressedTextureFactory *factory) const override;

    virtual void compileShader(QSGMaterialShader *shader, QSGMaterial *material, const char *vertexCode = nullptr, const char *fragmentCode = nullptr);
    virtual void initializeShader(QSGMaterialShader *shader);
    virtual void initializeRhiShader(QSGMaterialRhiShader *shader, QShader::Variant shaderVariant);

    void setAttachToGraphicsContext(bool attach) override;

    static QSGDefaultRenderContext *from(QOpenGLContext *context);

    bool hasBrokenIndexBufferObjects() const { return m_brokenIBOs; }
    int maxTextureSize() const override { return m_maxTextureSize; }
    bool separateIndexBuffer() const;

    int msaaSampleCount() const { return m_initParams.sampleCount; }

    QRhiCommandBuffer *currentFrameCommandBuffer() const {
        // may be null if not in an active frame, but returning null is valid then
        return m_currentFrameCommandBuffer;
    }
    QRhiRenderPassDescriptor *currentFrameRenderPass() const {
        // may be null if not in an active frame, but returning null is valid then
        return m_currentFrameRenderPass;
    }

    qreal currentDevicePixelRatio() const
    {
        // Valid starting from QQuickWindow::syncSceneGraph(). This takes the
        // redirections, e.g. QQuickWindow::setRenderTarget(), into account.
        // This calculation logic matches what the renderer does, so this is
        // the same value that gets exposed in RenderState::devicePixelRatio()
        // to material shaders. This getter is useful to perform dpr-related
        // operations in the sync phase (in updatePaintNode()).
        return m_currentDevicePixelRatio;
    }

protected:
    static QString fontKey(const QRawFont &font);

    InitParams m_initParams;
    QRhi *m_rhi;
    QOpenGLContext *m_gl;
    QSGDepthStencilBufferManager *m_depthStencilManager;
    int m_maxTextureSize;
    bool m_brokenIBOs;
    bool m_serializedRender;
    bool m_attachToGLContext;
    QSGOpenGLAtlasTexture::Manager *m_glAtlasManager;
    QSGRhiAtlasTexture::Manager *m_rhiAtlasManager;
    QRhiCommandBuffer *m_currentFrameCommandBuffer;
    QRhiRenderPassDescriptor *m_currentFrameRenderPass;
    qreal m_currentDevicePixelRatio;
};

QT_END_NAMESPACE

#endif // QSGDEFAULTRENDERCONTEXT_H
