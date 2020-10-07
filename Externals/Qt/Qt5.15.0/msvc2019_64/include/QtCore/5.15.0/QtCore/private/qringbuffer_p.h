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

#ifndef QRINGBUFFER_P_H
#define QRINGBUFFER_P_H

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

#include <QtCore/private/qglobal_p.h>
#include <QtCore/qbytearray.h>
#include <QtCore/qvector.h>

QT_BEGIN_NAMESPACE

#ifndef QRINGBUFFER_CHUNKSIZE
#define QRINGBUFFER_CHUNKSIZE 4096
#endif

class QRingChunk
{
public:
    // initialization and cleanup
    inline QRingChunk() noexcept :
        headOffset(0), tailOffset(0)
    {
    }
    inline QRingChunk(const QRingChunk &other) noexcept :
        chunk(other.chunk), headOffset(other.headOffset), tailOffset(other.tailOffset)
    {
    }
    explicit inline QRingChunk(int alloc) :
        chunk(alloc, Qt::Uninitialized), headOffset(0), tailOffset(0)
    {
    }
    explicit inline QRingChunk(const QByteArray &qba) noexcept :
        chunk(qba), headOffset(0), tailOffset(qba.size())
    {
    }

    inline QRingChunk &operator=(const QRingChunk &other) noexcept
    {
        chunk = other.chunk;
        headOffset = other.headOffset;
        tailOffset = other.tailOffset;
        return *this;
    }
    inline QRingChunk(QRingChunk &&other) noexcept :
        chunk(other.chunk), headOffset(other.headOffset), tailOffset(other.tailOffset)
    {
        other.headOffset = other.tailOffset = 0;
    }
    inline QRingChunk &operator=(QRingChunk &&other) noexcept
    {
        swap(other);
        return *this;
    }

    inline void swap(QRingChunk &other) noexcept
    {
        chunk.swap(other.chunk);
        qSwap(headOffset, other.headOffset);
        qSwap(tailOffset, other.tailOffset);
    }

    // allocating and sharing
    void allocate(int alloc);
    inline bool isShared() const
    {
        return !chunk.isDetached();
    }
    Q_CORE_EXPORT void detach();
    QByteArray toByteArray();

    // getters
    inline int head() const
    {
        return headOffset;
    }
    inline int size() const
    {
        return tailOffset - headOffset;
    }
    inline int capacity() const
    {
        return chunk.size();
    }
    inline int available() const
    {
        return chunk.size() - tailOffset;
    }
    inline const char *data() const
    {
        return chunk.constData() + headOffset;
    }
    inline char *data()
    {
        if (isShared())
            detach();
        return chunk.data() + headOffset;
    }

    // array management
    inline void advance(int offset)
    {
        Q_ASSERT(headOffset + offset >= 0);
        Q_ASSERT(size() - offset > 0);

        headOffset += offset;
    }
    inline void grow(int offset)
    {
        Q_ASSERT(size() + offset > 0);
        Q_ASSERT(head() + size() + offset <= capacity());

        tailOffset += offset;
    }
    inline void assign(const QByteArray &qba)
    {
        chunk = qba;
        headOffset = 0;
        tailOffset = qba.size();
    }
    inline void reset()
    {
        headOffset = tailOffset = 0;
    }
    inline void clear()
    {
        assign(QByteArray());
    }

private:
    QByteArray chunk;
    int headOffset, tailOffset;
};

class QRingBuffer
{
public:
    explicit inline QRingBuffer(int growth = QRINGBUFFER_CHUNKSIZE) :
        bufferSize(0), basicBlockSize(growth) { }

    inline void setChunkSize(int size) {
        basicBlockSize = size;
    }

    inline int chunkSize() const {
        return basicBlockSize;
    }

    inline qint64 nextDataBlockSize() const {
        return bufferSize == 0 ? Q_INT64_C(0) : buffers.first().size();
    }

    inline const char *readPointer() const {
        return bufferSize == 0 ? nullptr : buffers.first().data();
    }

    Q_CORE_EXPORT const char *readPointerAtPosition(qint64 pos, qint64 &length) const;
    Q_CORE_EXPORT void free(qint64 bytes);
    Q_CORE_EXPORT char *reserve(qint64 bytes);
    Q_CORE_EXPORT char *reserveFront(qint64 bytes);

    inline void truncate(qint64 pos) {
        Q_ASSERT(pos >= 0 && pos <= size());

        chop(size() - pos);
    }

    Q_CORE_EXPORT void chop(qint64 bytes);

    inline bool isEmpty() const {
        return bufferSize == 0;
    }

    inline int getChar() {
        if (isEmpty())
            return -1;
        char c = *readPointer();
        free(1);
        return int(uchar(c));
    }

    inline void putChar(char c) {
        char *ptr = reserve(1);
        *ptr = c;
    }

    void ungetChar(char c)
    {
        char *ptr = reserveFront(1);
        *ptr = c;
    }


    inline qint64 size() const {
        return bufferSize;
    }

    Q_CORE_EXPORT void clear();
    inline qint64 indexOf(char c) const { return indexOf(c, size()); }
    Q_CORE_EXPORT qint64 indexOf(char c, qint64 maxLength, qint64 pos = 0) const;
    Q_CORE_EXPORT qint64 read(char *data, qint64 maxLength);
    Q_CORE_EXPORT QByteArray read();
    Q_CORE_EXPORT qint64 peek(char *data, qint64 maxLength, qint64 pos = 0) const;
    Q_CORE_EXPORT void append(const char *data, qint64 size);
    Q_CORE_EXPORT void append(const QByteArray &qba);

    inline qint64 skip(qint64 length) {
        qint64 bytesToSkip = qMin(length, bufferSize);

        free(bytesToSkip);
        return bytesToSkip;
    }

    Q_CORE_EXPORT qint64 readLine(char *data, qint64 maxLength);

    inline bool canReadLine() const {
        return indexOf('\n') >= 0;
    }

private:
    QVector<QRingChunk> buffers;
    qint64 bufferSize;
    int basicBlockSize;
};

Q_DECLARE_SHARED(QRingChunk)
Q_DECLARE_TYPEINFO(QRingBuffer, Q_MOVABLE_TYPE);

QT_END_NAMESPACE

#endif // QRINGBUFFER_P_H
