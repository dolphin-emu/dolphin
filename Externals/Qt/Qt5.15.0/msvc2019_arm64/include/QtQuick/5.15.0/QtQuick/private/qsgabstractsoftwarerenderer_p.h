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

#ifndef QSGABSTRACTSOFTWARERENDERER_H
#define QSGABSTRACTSOFTWARERENDERER_H

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

#include <private/qsgrenderer_p.h>

#include <QtCore/QHash>

QT_BEGIN_NAMESPACE

class QSGSimpleRectNode;

class QSGSoftwareRenderableNode;
class QSGSoftwareRenderableNodeUpdater;

class Q_QUICK_PRIVATE_EXPORT QSGAbstractSoftwareRenderer : public QSGRenderer
{
public:
    QSGAbstractSoftwareRenderer(QSGRenderContext *context);
    virtual ~QSGAbstractSoftwareRenderer();

    QSGSoftwareRenderableNode *renderableNode(QSGNode *node) const;
    void addNodeMapping(QSGNode *node, QSGSoftwareRenderableNode *renderableNode);
    void appendRenderableNode(QSGSoftwareRenderableNode *node);

    void nodeChanged(QSGNode *node, QSGNode::DirtyState state) override;

    void markDirty();

protected:
    QRegion renderNodes(QPainter *painter);
    void buildRenderList();
    QRegion optimizeRenderList();

    void setBackgroundColor(const QColor &color);
    void setBackgroundRect(const QRect &rect, qreal devicePixelRatio);
    QColor backgroundColor();
    QRect backgroundRect();
    // only known after calling optimizeRenderList()
    bool isOpaque() const { return m_isOpaque; }

private:
    void nodeAdded(QSGNode *node);
    void nodeRemoved(QSGNode *node);
    void nodeGeometryUpdated(QSGNode *node);
    void nodeMaterialUpdated(QSGNode *node);
    void nodeMatrixUpdated(QSGNode *node);
    void nodeOpacityUpdated(QSGNode *node);

    QHash<QSGNode*, QSGSoftwareRenderableNode*> m_nodes;
    QVector<QSGSoftwareRenderableNode*> m_renderableNodes;

    QSGSimpleRectNode *m_background;

    QRegion m_dirtyRegion;
    QRegion m_obscuredRegion;
    qreal m_devicePixelRatio = 1;
    bool m_isOpaque = false;

    QSGSoftwareRenderableNodeUpdater *m_nodeUpdater;
};

QT_END_NAMESPACE

#endif // QSGABSTRACTSOFTWARERENDERER_H
