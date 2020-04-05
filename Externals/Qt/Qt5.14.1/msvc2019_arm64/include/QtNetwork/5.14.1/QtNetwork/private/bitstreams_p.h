/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtNetwork module of the Qt Toolkit.
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

#ifndef BITSTREAMS_P_H
#define BITSTREAMS_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of other Qt classes.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtCore/qglobal.h>
#include <QtCore/qdebug.h>

#include <type_traits>
#include <algorithm>
#include <vector>

QT_BEGIN_NAMESPACE

class QByteArray;

namespace HPack
{

// BitOStream works with an external buffer,
// for example, HEADERS frame.
class Q_AUTOTEST_EXPORT BitOStream
{
public:
    BitOStream(std::vector<uchar> &buffer);

    // Write 'bitLength' bits from the least significant
    // bits in 'bits' to bitstream:
    void writeBits(uchar bits, quint8 bitLength);
    // HPACK data format, we support:
    // * 32-bit integers
    // * strings
    void write(quint32 src);
    void write(const QByteArray &src, bool compressed);

    quint64 bitLength() const;
    quint64 byteLength() const;
    const uchar *begin() const;
    const uchar *end() const;

    void clear();

private:
    Q_DISABLE_COPY_MOVE(BitOStream);

    std::vector<uchar> &buffer;
    quint64 bitsSet;
};

class Q_AUTOTEST_EXPORT BitIStream
{
public:
    // Error is set by 'read' functions.
    // 'peek' does not set the error,
    // since it just peeks some bits
    // without the notion of wrong/right.
    // 'read' functions only change 'streamOffset'
    // on success.
    enum class Error
    {
        NoError,
        NotEnoughData,
        CompressionError,
        InvalidInteger
    };

    BitIStream();
    BitIStream(const uchar *f, const uchar *l);

    quint64 bitLength() const;
    bool hasMoreBits() const;

    // peekBits tries to read 'length' bits from the bitstream into
    // 'dst' ('length' must be <= sizeof(dst) * 8), packing them
    // starting from the most significant bit of the most significant
    // byte. It's a template so that we can use it with different
    // integer types. Returns the number of bits actually read.
    // Does not change stream's offset.

    template<class T>
    quint64 peekBits(quint64 from, quint64 length, T *dstPtr) const
    {
        static_assert(std::is_unsigned<T>::value, "peekBits: unsigned integer type expected");

        Q_ASSERT(dstPtr);
        Q_ASSERT(length <= sizeof(T) * 8);

        if (from >= bitLength() || !length)
            return 0;

        T &dst = *dstPtr;
        dst = T();
        length = std::min(length, bitLength() - from);

        const uchar *srcByte = first + from / 8;
        auto bitsToRead = length + from % 8;

        while (bitsToRead > 8) {
            dst = (dst << 8) | *srcByte;
            bitsToRead -= 8;
            ++srcByte;
        }

        dst <<= bitsToRead;
        dst |= *srcByte >> (8 - bitsToRead);
        dst <<= sizeof(T) * 8 - length;

        return length;
    }

    quint64 streamOffset() const
    {
        return offset;
    }

    bool skipBits(quint64 nBits);
    bool rewindOffset(quint64 nBits);

    bool read(quint32 *dstPtr);
    bool read(QByteArray *dstPtr);

    Error error() const;

private:
    void setError(Error newState);

    const uchar *first;
    const uchar *last;
    quint64 offset;
    Error streamError;
};

} // namespace HPack

QT_END_NAMESPACE

#endif
