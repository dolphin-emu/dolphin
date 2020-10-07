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

#ifndef QQUICKIMAGEPROVIDER_H
#define QQUICKIMAGEPROVIDER_H

#include <QtQuick/qtquickglobal.h>
#include <QtGui/qimage.h>
#include <QtGui/qpixmap.h>
#include <QtQml/qqmlengine.h>

QT_BEGIN_NAMESPACE


class QQuickImageProviderPrivate;
class QQuickAsyncImageProviderPrivate;
class QQuickImageProviderOptionsPrivate;
class QSGTexture;
class QQuickWindow;

class Q_QUICK_EXPORT QQuickTextureFactory : public QObject
{
    Q_OBJECT
public:
    QQuickTextureFactory();
    ~QQuickTextureFactory() override;

    virtual QSGTexture *createTexture(QQuickWindow *window) const = 0;
    virtual QSize textureSize() const = 0;
    virtual int textureByteCount() const = 0;
    virtual QImage image() const;

    static QQuickTextureFactory *textureFactoryForImage(const QImage &image);
};

class QQuickImageResponsePrivate;

class Q_QUICK_EXPORT QQuickImageResponse : public QObject
{
Q_OBJECT
public:
    QQuickImageResponse();
    ~QQuickImageResponse() override;

    virtual QQuickTextureFactory *textureFactory() const = 0;
    virtual QString errorString() const;

public Q_SLOTS:
    virtual void cancel();

Q_SIGNALS:
    void finished();

private:
    Q_DECLARE_PRIVATE(QQuickImageResponse)
    Q_PRIVATE_SLOT(d_func(), void _q_finished())
};

class Q_QUICK_EXPORT QQuickImageProvider : public QQmlImageProviderBase
{
    friend class QQuickImageProviderWithOptions; // ### Qt 6 Remove
public:
    QQuickImageProvider(ImageType type, Flags flags = Flags());
    ~QQuickImageProvider() override;

    ImageType imageType() const override;
    Flags flags() const override;

#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
    virtual QImage requestImage(const QString &id, QSize *size, const QSize& requestedSize, const QQuickImageProviderOptions &options);
    virtual QPixmap requestPixmap(const QString &id, QSize *size, const QSize& requestedSize, const QQuickImageProviderOptions &options);
    virtual QQuickTextureFactory *requestTexture(const QString &id, QSize *size, const QSize &requestedSize, const QQuickImageProviderOptions &options);
#else
    virtual QImage requestImage(const QString &id, QSize *size, const QSize& requestedSize);
    virtual QPixmap requestPixmap(const QString &id, QSize *size, const QSize& requestedSize);
    virtual QQuickTextureFactory *requestTexture(const QString &id, QSize *size, const QSize &requestedSize);
#endif

private:
    QQuickImageProviderPrivate *d;
};

class Q_QUICK_EXPORT QQuickAsyncImageProvider : public QQuickImageProvider
{
public:
    QQuickAsyncImageProvider();
    ~QQuickAsyncImageProvider() override;

#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
    virtual QQuickImageResponse *requestImageResponse(const QString &id, const QSize &requestedSize, const QQuickImageProviderOptions &options) = 0;
#else
    virtual QQuickImageResponse *requestImageResponse(const QString &id, const QSize &requestedSize) = 0;
#endif

private:
    QQuickAsyncImageProviderPrivate *d;
};

QT_END_NAMESPACE

#endif // QQUICKIMAGEPROVIDER_H
