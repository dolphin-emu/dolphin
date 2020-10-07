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

#ifndef QQUICKIMAGEBASE_P_H
#define QQUICKIMAGEBASE_P_H

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

#include "qquickimplicitsizeitem_p.h"
#include <private/qtquickglobal_p.h>
#include <QtGui/qcolorspace.h>

QT_BEGIN_NAMESPACE

class QQuickImageBasePrivate;
class Q_QUICK_PRIVATE_EXPORT QQuickImageBase : public QQuickImplicitSizeItem
{
    Q_OBJECT

    Q_PROPERTY(Status status READ status NOTIFY statusChanged)
    Q_PROPERTY(QUrl source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(qreal progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(bool asynchronous READ asynchronous WRITE setAsynchronous NOTIFY asynchronousChanged)
    Q_PROPERTY(bool cache READ cache WRITE setCache NOTIFY cacheChanged)
    Q_PROPERTY(QSize sourceSize READ sourceSize WRITE setSourceSize RESET resetSourceSize NOTIFY sourceSizeChanged)
    Q_PROPERTY(bool mirror READ mirror WRITE setMirror NOTIFY mirrorChanged)
    Q_PROPERTY(int currentFrame READ currentFrame WRITE setCurrentFrame NOTIFY currentFrameChanged REVISION 14)
    Q_PROPERTY(int frameCount READ frameCount NOTIFY frameCountChanged REVISION 14)
    Q_PROPERTY(QColorSpace colorSpace READ colorSpace WRITE setColorSpace NOTIFY colorSpaceChanged REVISION 15)

    QML_NAMED_ELEMENT(ImageBase);
    QML_ADDED_IN_MINOR_VERSION(14)
    QML_UNCREATABLE("ImageBase is an abstract base class.")

public:
    enum LoadPixmapOption {
        NoOption            = 0x0000,
        HandleDPR           = 0x0001,
        UseProviderOptions  = 0x0002
    };

    Q_DECLARE_FLAGS(LoadPixmapOptions, LoadPixmapOption)
    Q_FLAG(LoadPixmapOptions)

    QQuickImageBase(QQuickItem *parent=nullptr);
    ~QQuickImageBase();
    enum Status { Null, Ready, Loading, Error };
    Q_ENUM(Status)
    Status status() const;
    qreal progress() const;

    QUrl source() const;
    virtual void setSource(const QUrl &url);

    bool asynchronous() const;
    void setAsynchronous(bool);

    bool cache() const;
    void setCache(bool);

    QImage image() const;

    virtual void setSourceSize(const QSize&);
    QSize sourceSize() const;
    void resetSourceSize();

    QRectF sourceClipRect() const;
    void setSourceClipRect(const QRectF &r);
    void resetSourceClipRect();

    virtual void setMirror(bool mirror);
    bool mirror() const;

    virtual void setCurrentFrame(int frame);
    virtual int currentFrame() const;

    virtual int frameCount() const;

    virtual void setAutoTransform(bool transform);
    bool autoTransform() const;

    QColorSpace colorSpace() const;
    virtual void setColorSpace(const QColorSpace &colorSpace);

    static void resolve2xLocalFile(const QUrl &url, qreal targetDevicePixelRatio, QUrl *sourceUrl, qreal *sourceDevicePixelRatio);

    // Use a virtual rather than a signal->signal to avoid the huge
    // connect/conneciton overhead for this rare case.
    virtual void emitAutoTransformBaseChanged() { }

Q_SIGNALS:
    void sourceChanged(const QUrl &);
    void sourceSizeChanged();
    void statusChanged(QQuickImageBase::Status);
    void progressChanged(qreal progress);
    void asynchronousChanged();
    void cacheChanged();
    void mirrorChanged();
    Q_REVISION(14) void currentFrameChanged();
    Q_REVISION(14) void frameCountChanged();
    Q_REVISION(15) void sourceClipRectChanged();
    Q_REVISION(15) void colorSpaceChanged();

protected:
    void loadEmptyUrl();
    void loadPixmap(const QUrl &url, LoadPixmapOptions loadOptions = NoOption);
    virtual void load();
    void componentComplete() override;
    virtual void pixmapChange();
    void itemChange(ItemChange change, const ItemChangeData &value) override;
    QQuickImageBase(QQuickImageBasePrivate &dd, QQuickItem *parent);

private Q_SLOTS:
    virtual void requestFinished();
    void requestProgress(qint64,qint64);

private:
    Q_DISABLE_COPY(QQuickImageBase)
    Q_DECLARE_PRIVATE(QQuickImageBase)
};

QT_END_NAMESPACE

#endif // QQUICKIMAGEBASE_P_H
