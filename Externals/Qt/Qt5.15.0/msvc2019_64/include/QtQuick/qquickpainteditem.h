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

#ifndef QQUICKPAINTEDITEM_P_H
#define QQUICKPAINTEDITEM_P_H

#include <QtQuick/qquickitem.h>
#include <QtGui/qcolor.h>

QT_BEGIN_NAMESPACE

class QQuickPaintedItemPrivate;
class Q_QUICK_EXPORT QQuickPaintedItem : public QQuickItem
{
    Q_OBJECT

    Q_PROPERTY(QSize contentsSize READ contentsSize WRITE setContentsSize NOTIFY contentsSizeChanged)
    Q_PROPERTY(QColor fillColor READ fillColor WRITE setFillColor NOTIFY fillColorChanged)
    Q_PROPERTY(qreal contentsScale READ contentsScale WRITE setContentsScale NOTIFY contentsScaleChanged)
    Q_PROPERTY(RenderTarget renderTarget READ renderTarget WRITE setRenderTarget NOTIFY renderTargetChanged)
    Q_PROPERTY(QSize textureSize READ textureSize WRITE setTextureSize NOTIFY textureSizeChanged)

    QML_NAMED_ELEMENT(PaintedItem)
    QML_UNCREATABLE("Cannot create instance of abstract class PaintedItem.")

public:
    explicit QQuickPaintedItem(QQuickItem *parent = nullptr);
    ~QQuickPaintedItem() override;

    enum RenderTarget {
        Image,
        FramebufferObject,
        InvertedYFramebufferObject
    };
    Q_ENUM(RenderTarget)

    enum PerformanceHint {
        FastFBOResizing = 0x1
    };
    Q_DECLARE_FLAGS(PerformanceHints, PerformanceHint)
    Q_FLAG(PerformanceHints)

    void update(const QRect &rect = QRect());

    bool opaquePainting() const;
    void setOpaquePainting(bool opaque);

    bool antialiasing() const;
    void setAntialiasing(bool enable);

    bool mipmap() const;
    void setMipmap(bool enable);

    PerformanceHints performanceHints() const;
    void setPerformanceHint(PerformanceHint hint, bool enabled = true);
    void setPerformanceHints(PerformanceHints hints);

    QRectF contentsBoundingRect() const;

    QSize contentsSize() const;
    void setContentsSize(const QSize &);
    void resetContentsSize();

    qreal contentsScale() const;
    void setContentsScale(qreal);

    QSize textureSize() const;
    void setTextureSize(const QSize &size);

    QColor fillColor() const;
    void setFillColor(const QColor&);

    RenderTarget renderTarget() const;
    void setRenderTarget(RenderTarget target);

    virtual void paint(QPainter *painter) = 0;

    bool isTextureProvider() const override;
    QSGTextureProvider *textureProvider() const override;

Q_SIGNALS:
    void fillColorChanged();
    void contentsSizeChanged();
    void contentsScaleChanged();
    void renderTargetChanged();
    void textureSizeChanged();

protected:
    QQuickPaintedItem(QQuickPaintedItemPrivate &dd, QQuickItem *parent = nullptr);
    QSGNode *updatePaintNode(QSGNode *, UpdatePaintNodeData *) override;
    void releaseResources() override;
    void itemChange(ItemChange, const ItemChangeData &) override;

private Q_SLOTS:
    void invalidateSceneGraph();

private:
    Q_DISABLE_COPY(QQuickPaintedItem)
    Q_DECLARE_PRIVATE(QQuickPaintedItem)
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QQuickPaintedItem::PerformanceHints)

QT_END_NAMESPACE

#endif // QQUICKPAINTEDITEM_P_H
