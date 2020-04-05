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

#ifndef QSGDEFAULTCONTEXT_H
#define QSGDEFAULTCONTEXT_H

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
#include <QtQuick/private/qsgdistancefieldglyphnode_p.h>
#include <qsgrendererinterface.h>

QT_BEGIN_NAMESPACE

class Q_QUICK_PRIVATE_EXPORT QSGDefaultContext : public QSGContext, public QSGRendererInterface
{
public:
    QSGDefaultContext(QObject *parent = nullptr);
    ~QSGDefaultContext();

    void renderContextInitialized(QSGRenderContext *renderContext) override;
    void renderContextInvalidated(QSGRenderContext *) override;
    QSGRenderContext *createRenderContext() override;
    QSGInternalRectangleNode *createInternalRectangleNode() override;
    QSGInternalImageNode *createInternalImageNode(QSGRenderContext *renderContext) override;
    QSGPainterNode *createPainterNode(QQuickPaintedItem *item) override;
    QSGGlyphNode *createGlyphNode(QSGRenderContext *rc, bool preferNativeGlyphNode) override;
    QSGLayer *createLayer(QSGRenderContext *renderContext) override;
    QSurfaceFormat defaultSurfaceFormat() const override;
    QSGRendererInterface *rendererInterface(QSGRenderContext *renderContext) override;
    QSGRectangleNode *createRectangleNode() override;
    QSGImageNode *createImageNode() override;
    QSGNinePatchNode *createNinePatchNode() override;
#if QT_CONFIG(quick_sprite)
    QSGSpriteNode *createSpriteNode() override;
#endif
    QSGGuiThreadShaderEffectManager *createGuiThreadShaderEffectManager() override;
    QSGShaderEffectNode *createShaderEffectNode(QSGRenderContext *renderContext,
                                                QSGGuiThreadShaderEffectManager *mgr) override;

    void setDistanceFieldEnabled(bool enabled);
    bool isDistanceFieldEnabled() const;

    GraphicsApi graphicsApi() const override;
    void *getResource(QQuickWindow *window, Resource resource) const override;
    ShaderType shaderType() const override;
    ShaderCompilationTypes shaderCompilationType() const override;
    ShaderSourceTypes shaderSourceType() const override;

private:
    QMutex m_mutex;
    QSGContext::AntialiasingMethod m_antialiasingMethod;
    bool m_distanceFieldDisabled;
    QSGDistanceFieldGlyphNode::AntialiasingMode m_distanceFieldAntialiasing;
    bool m_distanceFieldAntialiasingDecided;
};

QT_END_NAMESPACE

#endif // QSGDEFAULTCONTEXT_H
