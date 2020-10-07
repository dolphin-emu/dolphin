/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
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

#ifndef QSGTEXTURE_H
#define QSGTEXTURE_H

#include <QtQuick/qtquickglobal.h>
#include <QtCore/QObject>
#include <QtGui/QImage>

QT_BEGIN_NAMESPACE

class QSGTexturePrivate;
class QRhi;
class QRhiTexture;
class QRhiResourceUpdateBatch;

class Q_QUICK_EXPORT QSGTexture : public QObject
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QSGTexture)

public:
    QSGTexture();
    ~QSGTexture() override;

    enum WrapMode {
        Repeat,
        ClampToEdge,
        MirroredRepeat
    };

    enum Filtering {
        None,
        Nearest,
        Linear
    };

    enum AnisotropyLevel {
        AnisotropyNone,
        Anisotropy2x,
        Anisotropy4x,
        Anisotropy8x,
        Anisotropy16x
    };

    struct NativeTexture {
        const void *object;
        int layout;
    };

    virtual int textureId() const = 0; // ### Qt 6: remove
    NativeTexture nativeTexture() const;
    virtual QSize textureSize() const = 0;
    virtual bool hasAlphaChannel() const = 0;
    virtual bool hasMipmaps() const = 0;

    virtual QRectF normalizedTextureSubRect() const;

    virtual bool isAtlasTexture() const;

    virtual QSGTexture *removedFromAtlas() const;

    virtual void bind() = 0;
    void updateBindOptions(bool force = false);

    void setMipmapFiltering(Filtering filter);
    QSGTexture::Filtering mipmapFiltering() const;

    void setFiltering(Filtering filter);
    QSGTexture::Filtering filtering() const;

    void setAnisotropyLevel(AnisotropyLevel level);
    QSGTexture::AnisotropyLevel anisotropyLevel() const;

    void setHorizontalWrapMode(WrapMode hwrap);
    QSGTexture::WrapMode horizontalWrapMode() const;

    void setVerticalWrapMode(WrapMode vwrap);
    QSGTexture::WrapMode verticalWrapMode() const;

    inline QRectF convertToNormalizedSourceRect(const QRectF &rect) const;

    // ### Qt 6: make these virtual
    int comparisonKey() const;
    void updateRhiTexture(QRhi *rhi, QRhiResourceUpdateBatch *resourceUpdates);

    // ### Qt 6: make this an argument for removedFromAtlas()
    void setWorkResourceUpdateBatch(QRhiResourceUpdateBatch *resourceUpdates);

protected:
    QSGTexture(QSGTexturePrivate &dd);
};

QRectF QSGTexture::convertToNormalizedSourceRect(const QRectF &rect) const
{
    QSize s = textureSize();
    QRectF r = normalizedTextureSubRect();

    qreal sx = r.width() / s.width();
    qreal sy = r.height() / s.height();

    return QRectF(r.x() + rect.x() * sx,
                  r.y() + rect.y() * sy,
                  rect.width() * sx,
                  rect.height() * sy);
}

class Q_QUICK_EXPORT QSGDynamicTexture : public QSGTexture
{
    Q_OBJECT

public:
    QSGDynamicTexture() = default;
    virtual bool updateTexture() = 0;

protected:
    QSGDynamicTexture(QSGTexturePrivate &dd);
};

QT_END_NAMESPACE

#endif
