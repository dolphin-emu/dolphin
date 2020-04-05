/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtGui module of the Qt Toolkit.
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

#ifndef QPLATFORMSHAREDGRAPHICSCACHE_H
#define QPLATFORMSHAREDGRAPHICSCACHE_H

//
//  W A R N I N G
//  -------------
//
// This file is part of the QPA API and is not meant to be used
// in applications. Usage of this API may make your code
// source and binary incompatible with future versions of Qt.
//

#include <QtGui/qtguiglobal.h>
#include <QtCore/qobject.h>
#include <QtGui/qimage.h>

QT_BEGIN_NAMESPACE

class Q_GUI_EXPORT QPlatformSharedGraphicsCache: public QObject
{
    Q_OBJECT
public:
    enum PixelFormat
    {
        Alpha8
    };

    enum BufferType
    {
        OpenGLTexture
    };

    explicit QPlatformSharedGraphicsCache(QObject *parent = nullptr) : QObject(parent) {}

    virtual void beginRequestBatch() = 0;
    virtual void ensureCacheInitialized(const QByteArray &cacheId, BufferType bufferType,
                                                    PixelFormat pixelFormat) = 0;
    virtual void requestItems(const QByteArray &cacheId, const QVector<quint32> &itemIds) = 0;
    virtual void insertItems(const QByteArray &cacheId,
                                         const QVector<quint32> &itemIds,
                                         const QVector<QImage> &items) = 0;
    virtual void releaseItems(const QByteArray &cacheId, const QVector<quint32> &itemIds) = 0;
    virtual void endRequestBatch() = 0;

    virtual bool requestBatchStarted() const = 0;

    virtual uint textureIdForBuffer(void *bufferId) = 0;
    virtual void referenceBuffer(void *bufferId) = 0;
    virtual bool dereferenceBuffer(void *bufferId) = 0;
    virtual QSize sizeOfBuffer(void *bufferId) = 0;
    virtual void *eglImageForBuffer(void *bufferId) = 0;

Q_SIGNALS:
    void itemsMissing(const QByteArray &cacheId, const QVector<quint32> &itemIds);
    void itemsAvailable(const QByteArray &cacheId, void *bufferId,
                        const QVector<quint32> &itemIds, const QVector<QPoint> &positionsInBuffer);
    void itemsInvalidated(const QByteArray &cacheId, const QVector<quint32> &itemIds);
    void itemsUpdated(const QByteArray &cacheId, void *bufferId,
                      const QVector<quint32> &itemIds, const QVector<QPoint> &positionsInBuffer);
};

QT_END_NAMESPACE

#endif // QPLATFORMSHAREDGRAPHICSCACHE_H
