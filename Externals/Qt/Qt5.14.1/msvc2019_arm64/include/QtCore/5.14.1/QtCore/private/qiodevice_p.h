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

#ifndef QIODEVICE_P_H
#define QIODEVICE_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of QIODevice. This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "QtCore/qiodevice.h"
#include "QtCore/qbytearray.h"
#include "QtCore/qobjectdefs.h"
#include "QtCore/qstring.h"
#include "private/qringbuffer_p.h"
#include "QtCore/qvector.h"
#ifndef QT_NO_QOBJECT
#include "private/qobject_p.h"
#endif

QT_BEGIN_NAMESPACE

#ifndef QIODEVICE_BUFFERSIZE
#define QIODEVICE_BUFFERSIZE 16384
#endif

Q_CORE_EXPORT int qt_subtract_from_timeout(int timeout, int elapsed);

class Q_CORE_EXPORT QIODevicePrivate
#ifndef QT_NO_QOBJECT
    : public QObjectPrivate
#endif
{
    Q_DECLARE_PUBLIC(QIODevice)

public:
    QIODevicePrivate();
    virtual ~QIODevicePrivate();

    QIODevice::OpenMode openMode;
    QString errorString;

    QVector<QRingBuffer> readBuffers;
    QVector<QRingBuffer> writeBuffers;

    class QRingBufferRef {
        QRingBuffer *m_buf;
        inline QRingBufferRef() : m_buf(nullptr) { }
        friend class QIODevicePrivate;
    public:
        // wrap functions from QRingBuffer
        inline void setChunkSize(int size) { Q_ASSERT(m_buf); m_buf->setChunkSize(size); }
        inline int chunkSize() const { Q_ASSERT(m_buf); return m_buf->chunkSize(); }
        inline qint64 nextDataBlockSize() const { return (m_buf ? m_buf->nextDataBlockSize() : Q_INT64_C(0)); }
        inline const char *readPointer() const { return (m_buf ? m_buf->readPointer() : nullptr); }
        inline const char *readPointerAtPosition(qint64 pos, qint64 &length) const { Q_ASSERT(m_buf); return m_buf->readPointerAtPosition(pos, length); }
        inline void free(qint64 bytes) { Q_ASSERT(m_buf); m_buf->free(bytes); }
        inline char *reserve(qint64 bytes) { Q_ASSERT(m_buf); return m_buf->reserve(bytes); }
        inline char *reserveFront(qint64 bytes) { Q_ASSERT(m_buf); return m_buf->reserveFront(bytes); }
        inline void truncate(qint64 pos) { Q_ASSERT(m_buf); m_buf->truncate(pos); }
        inline void chop(qint64 bytes) { Q_ASSERT(m_buf); m_buf->chop(bytes); }
        inline bool isEmpty() const { return !m_buf || m_buf->isEmpty(); }
        inline int getChar() { return (m_buf ? m_buf->getChar() : -1); }
        inline void putChar(char c) { Q_ASSERT(m_buf); m_buf->putChar(c); }
        inline void ungetChar(char c) { Q_ASSERT(m_buf); m_buf->ungetChar(c); }
        inline qint64 size() const { return (m_buf ? m_buf->size() : Q_INT64_C(0)); }
        inline void clear() { if (m_buf) m_buf->clear(); }
        inline qint64 indexOf(char c) const { return (m_buf ? m_buf->indexOf(c, m_buf->size()) : Q_INT64_C(-1)); }
        inline qint64 indexOf(char c, qint64 maxLength, qint64 pos = 0) const { return (m_buf ? m_buf->indexOf(c, maxLength, pos) : Q_INT64_C(-1)); }
        inline qint64 read(char *data, qint64 maxLength) { return (m_buf ? m_buf->read(data, maxLength) : Q_INT64_C(0)); }
        inline QByteArray read() { return (m_buf ? m_buf->read() : QByteArray()); }
        inline qint64 peek(char *data, qint64 maxLength, qint64 pos = 0) const { return (m_buf ? m_buf->peek(data, maxLength, pos) : Q_INT64_C(0)); }
        inline void append(const char *data, qint64 size) { Q_ASSERT(m_buf); m_buf->append(data, size); }
        inline void append(const QByteArray &qba) { Q_ASSERT(m_buf); m_buf->append(qba); }
        inline qint64 skip(qint64 length) { return (m_buf ? m_buf->skip(length) : Q_INT64_C(0)); }
        inline qint64 readLine(char *data, qint64 maxLength) { return (m_buf ? m_buf->readLine(data, maxLength) : Q_INT64_C(-1)); }
        inline bool canReadLine() const { return m_buf && m_buf->canReadLine(); }
    };

    QRingBufferRef buffer;
    QRingBufferRef writeBuffer;
    qint64 pos;
    qint64 devicePos;
    int readChannelCount;
    int writeChannelCount;
    int currentReadChannel;
    int currentWriteChannel;
    int readBufferChunkSize;
    int writeBufferChunkSize;
    qint64 transactionPos;
    bool transactionStarted;
    bool baseReadLineDataCalled;

    virtual bool putCharHelper(char c);

    enum AccessMode {
        Unset,
        Sequential,
        RandomAccess
    };
    mutable AccessMode accessMode;
    inline bool isSequential() const
    {
        if (accessMode == Unset)
            accessMode = q_func()->isSequential() ? Sequential : RandomAccess;
        return accessMode == Sequential;
    }

    inline bool isBufferEmpty() const
    {
        return buffer.isEmpty() || (transactionStarted && isSequential()
                                    && transactionPos == buffer.size());
    }
    bool allWriteBuffersEmpty() const;

    void seekBuffer(qint64 newPos);

    inline void setCurrentReadChannel(int channel)
    {
        buffer.m_buf = (channel < readBuffers.size() ? &readBuffers[channel] : nullptr);
        currentReadChannel = channel;
    }
    inline void setCurrentWriteChannel(int channel)
    {
        writeBuffer.m_buf = (channel < writeBuffers.size() ? &writeBuffers[channel] : nullptr);
        currentWriteChannel = channel;
    }
    void setReadChannelCount(int count);
    void setWriteChannelCount(int count);

    qint64 read(char *data, qint64 maxSize, bool peeking = false);
    virtual qint64 peek(char *data, qint64 maxSize);
    virtual QByteArray peek(qint64 maxSize);
    qint64 skipByReading(qint64 maxSize);
    // ### Qt6: consider replacing with a protected virtual QIODevice::skipData().
    virtual qint64 skip(qint64 maxSize);

#ifdef QT_NO_QOBJECT
    QIODevice *q_ptr;
#endif
};

QT_END_NAMESPACE

#endif // QIODEVICE_P_H
