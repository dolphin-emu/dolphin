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

#ifndef QSGIMAGENODE_H
#define QSGIMAGENODE_H

#include <QtQuick/qsgnode.h>
#include <QtQuick/qsgtexture.h>

QT_BEGIN_NAMESPACE

class Q_QUICK_EXPORT QSGImageNode : public QSGGeometryNode
{
public:
    ~QSGImageNode() override = default;

    virtual void setRect(const QRectF &rect) = 0;
    inline void setRect(qreal x, qreal y, qreal w, qreal h) { setRect(QRectF(x, y, w, h)); }
    virtual QRectF rect() const = 0;

    virtual void setSourceRect(const QRectF &r) = 0;
    inline void setSourceRect(qreal x, qreal y, qreal w, qreal h) { setSourceRect(QRectF(x, y, w, h)); }
    virtual QRectF sourceRect() const = 0;

    virtual void setTexture(QSGTexture *texture) = 0;
    virtual QSGTexture *texture() const = 0;

    virtual void setFiltering(QSGTexture::Filtering filtering) = 0;
    virtual QSGTexture::Filtering filtering() const = 0;

    virtual void setMipmapFiltering(QSGTexture::Filtering filtering) = 0;
    virtual QSGTexture::Filtering mipmapFiltering() const = 0;

    // ### Qt6: Add anisotropy support here, and possibly a virtual hook or another mean to extend this class.

    enum TextureCoordinatesTransformFlag {
        NoTransform        = 0x00,
        MirrorHorizontally = 0x01,
        MirrorVertically   = 0x02
    };
    Q_DECLARE_FLAGS(TextureCoordinatesTransformMode, TextureCoordinatesTransformFlag)

    virtual void setTextureCoordinatesTransform(TextureCoordinatesTransformMode mode) = 0;
    virtual TextureCoordinatesTransformMode textureCoordinatesTransform() const = 0;

    virtual void setOwnsTexture(bool owns) = 0;
    virtual bool ownsTexture() const = 0;

    static void rebuildGeometry(QSGGeometry *g,
                                QSGTexture *texture,
                                const QRectF &rect,
                                QRectF sourceRect,
                                TextureCoordinatesTransformMode texCoordMode);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QSGImageNode::TextureCoordinatesTransformMode)

QT_END_NAMESPACE

#endif // QSGIMAGENODE_H
