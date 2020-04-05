/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtCore module of the Qt Toolkit.
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

#ifndef QNONCONTIGUOUSBYTEDEVICE_P_H
#define QNONCONTIGUOUSBYTEDEVICE_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of a number of Qt sources files.  This header file may change from
// version to version without notice, or even be removed.
//
// We mean it.
//

#include <QtCore/qobject.h>
#include <QtCore/qbytearray.h>
#include <QtCore/qbuffer.h>
#include <QtCore/qiodevice.h>
#include <QtCore/QSharedPointer>
#include "private/qringbuffer_p.h"

QT_BEGIN_NAMESPACE

class Q_CORE_EXPORT QNonContiguousByteDevice : public QObject
{
    Q_OBJECT
public:
    virtual const char* readPointer(qint64 maximumLength, qint64 &len) = 0;
    virtual bool advanceReadPointer(qint64 amount) = 0;
    virtual bool atEnd() const = 0;
    virtual qint64 pos() const { return -1; }
    virtual bool reset() = 0;
    virtual qint64 size() const = 0;

    virtual ~QNonContiguousByteDevice();

protected:
    QNonContiguousByteDevice();


Q_SIGNALS:
    void readyRead();
    void readProgress(qint64 current, qint64 total);
};

class Q_CORE_EXPORT QNonContiguousByteDeviceFactory
{
public:
    static QNonContiguousByteDevice* create(QIODevice *device);
    static QSharedPointer<QNonContiguousByteDevice> createShared(QIODevice *device);

    static QNonContiguousByteDevice* create(QByteArray *byteArray);
    static QSharedPointer<QNonContiguousByteDevice> createShared(QByteArray *byteArray);

    static QNonContiguousByteDevice* create(QSharedPointer<QRingBuffer> ringBuffer);
    static QSharedPointer<QNonContiguousByteDevice> createShared(QSharedPointer<QRingBuffer> ringBuffer);

    static QIODevice* wrap(QNonContiguousByteDevice* byteDevice);
};

// the actual implementations
//

class QNonContiguousByteDeviceByteArrayImpl : public QNonContiguousByteDevice
{
public:
    QNonContiguousByteDeviceByteArrayImpl(QByteArray *ba);
    ~QNonContiguousByteDeviceByteArrayImpl();
    const char* readPointer(qint64 maximumLength, qint64 &len) override;
    bool advanceReadPointer(qint64 amount) override;
    bool atEnd() const override;
    bool reset() override;
    qint64 size() const override;
    qint64 pos() const override;
protected:
    QByteArray* byteArray;
    qint64 currentPosition;
};

class QNonContiguousByteDeviceRingBufferImpl : public QNonContiguousByteDevice
{
public:
    QNonContiguousByteDeviceRingBufferImpl(QSharedPointer<QRingBuffer> rb);
    ~QNonContiguousByteDeviceRingBufferImpl();
    const char* readPointer(qint64 maximumLength, qint64 &len) override;
    bool advanceReadPointer(qint64 amount) override;
    bool atEnd() const override;
    bool reset() override;
    qint64 size() const override;
    qint64 pos() const override;
protected:
    QSharedPointer<QRingBuffer> ringBuffer;
    qint64 currentPosition;
};


class QNonContiguousByteDeviceIoDeviceImpl : public QNonContiguousByteDevice
{
    Q_OBJECT
public:
    QNonContiguousByteDeviceIoDeviceImpl(QIODevice *d);
    ~QNonContiguousByteDeviceIoDeviceImpl();
    const char* readPointer(qint64 maximumLength, qint64 &len) override;
    bool advanceReadPointer(qint64 amount) override;
    bool atEnd() const override;
    bool reset() override;
    qint64 size() const override;
    qint64 pos() const override;
protected:
    QIODevice* device;
    QByteArray* currentReadBuffer;
    qint64 currentReadBufferSize;
    qint64 currentReadBufferAmount;
    qint64 currentReadBufferPosition;
    qint64 totalAdvancements;
    bool eof;
    qint64 initialPosition;
};

class QNonContiguousByteDeviceBufferImpl : public QNonContiguousByteDevice
{
    Q_OBJECT
public:
    QNonContiguousByteDeviceBufferImpl(QBuffer *b);
    ~QNonContiguousByteDeviceBufferImpl();
    const char* readPointer(qint64 maximumLength, qint64 &len) override;
    bool advanceReadPointer(qint64 amount) override;
    bool atEnd() const override;
    bool reset() override;
    qint64 size() const override;
protected:
    QBuffer* buffer;
    QByteArray byteArray;
    QNonContiguousByteDeviceByteArrayImpl* arrayImpl;
};

// ... and the reverse thing
class QByteDeviceWrappingIoDevice : public QIODevice
{
public:
    QByteDeviceWrappingIoDevice (QNonContiguousByteDevice *bd);
    ~QByteDeviceWrappingIoDevice ();
    bool isSequential() const override;
    bool atEnd() const override;
    bool reset() override;
    qint64 size() const override;
protected:
    qint64 readData(char *data, qint64 maxSize) override;
    qint64 writeData(const char *data, qint64 maxSize) override;

     QNonContiguousByteDevice *byteDevice;
};

QT_END_NAMESPACE

#endif
