/****************************************************************************
**
** Copyright (C) 2018 Intel Corporation.
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

#ifndef QCBORSTREAM_H
#define QCBORSTREAM_H

#include <QtCore/qbytearray.h>
#include <QtCore/qcborcommon.h>
#include <QtCore/qfloat16.h>
#include <QtCore/qscopedpointer.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringview.h>

// See qcborcommon.h for why we check
#if defined(QT_X11_DEFINES_FOUND)
#  undef True
#  undef False
#endif

QT_BEGIN_NAMESPACE

class QIODevice;

enum class QCborNegativeInteger : quint64 {};

class QCborStreamWriterPrivate;
class Q_CORE_EXPORT QCborStreamWriter
{
public:
    explicit QCborStreamWriter(QIODevice *device);
    explicit QCborStreamWriter(QByteArray *data);
    ~QCborStreamWriter();
    Q_DISABLE_COPY(QCborStreamWriter)

    void setDevice(QIODevice *device);
    QIODevice *device() const;

    void append(quint64 u);
    void append(qint64 i);
    void append(QCborNegativeInteger n);
    void append(const QByteArray &ba)       { appendByteString(ba.constData(), ba.size()); }
    void append(QLatin1String str);
    void append(QStringView str);
    void append(QCborTag tag);
    void append(QCborKnownTags tag)         { append(QCborTag(tag)); }
    void append(QCborSimpleType st);
    void append(std::nullptr_t)             { append(QCborSimpleType::Null); }
    void append(qfloat16 f);
    void append(float f);
    void append(double d);

    void appendByteString(const char *data, qsizetype len);
    void appendTextString(const char *utf8, qsizetype len);

    // convenience
    void append(bool b)     { append(b ? QCborSimpleType::True : QCborSimpleType::False); }
    void appendNull()       { append(QCborSimpleType::Null); }
    void appendUndefined()  { append(QCborSimpleType::Undefined); }

#ifndef Q_QDOC
    // overloads to make normal code not complain
    void append(int i)      { append(qint64(i)); }
    void append(uint u)     { append(quint64(u)); }
#endif
#ifndef QT_NO_CAST_FROM_ASCII
    void append(const char *str, qsizetype size = -1)
    { appendTextString(str, (str && size == -1)  ? int(strlen(str)) : size); }
#endif

    void startArray();
    void startArray(quint64 count);
    bool endArray();
    void startMap();
    void startMap(quint64 count);
    bool endMap();

    // no API for encoding chunked strings

private:
    QScopedPointer<QCborStreamWriterPrivate> d;
};

class QCborStreamReaderPrivate;
class Q_CORE_EXPORT QCborStreamReader
{
    Q_GADGET
public:
    enum Type : quint8 {
        UnsignedInteger     = 0x00,
        NegativeInteger     = 0x20,
        ByteString          = 0x40,
        ByteArray           = ByteString,
        TextString          = 0x60,
        String              = TextString,
        Array               = 0x80,
        Map                 = 0xa0,
        Tag                 = 0xc0,
        SimpleType          = 0xe0,
        HalfFloat           = 0xf9,
        Float16             = HalfFloat,
        Float               = 0xfa,
        Double              = 0xfb,

        Invalid             = 0xff
    };
    Q_ENUM(Type)

    enum StringResultCode {
        EndOfString = 0,
        Ok = 1,
        Error = -1
    };
    template <typename Container> struct StringResult {
        Container data;
        StringResultCode status = Error;
    };
    Q_ENUM(StringResultCode)

    QCborStreamReader();
    QCborStreamReader(const char *data, qsizetype len);
    QCborStreamReader(const quint8 *data, qsizetype len);
    explicit QCborStreamReader(const QByteArray &data);
    explicit QCborStreamReader(QIODevice *device);
    ~QCborStreamReader();
    Q_DISABLE_COPY(QCborStreamReader)

    void setDevice(QIODevice *device);
    QIODevice *device() const;
    void addData(const QByteArray &data);
    void addData(const char *data, qsizetype len);
    void addData(const quint8 *data, qsizetype len)
    { addData(reinterpret_cast<const char *>(data), len); }
    void reparse();
    void clear();
    void reset();

    QCborError lastError();

    qint64 currentOffset() const;

    bool isValid() const        { return !isInvalid(); }

    int containerDepth() const;
    QCborStreamReader::Type parentContainerType() const;
    bool hasNext() const noexcept Q_DECL_PURE_FUNCTION;
    bool next(int maxRecursion = 10000);

    Type type() const               { return QCborStreamReader::Type(type_); }
    bool isUnsignedInteger() const  { return type() == UnsignedInteger; }
    bool isNegativeInteger() const  { return type() == NegativeInteger; }
    bool isInteger() const          { return quint8(type()) <= quint8(NegativeInteger); }
    bool isByteArray() const        { return type() == ByteArray; }
    bool isString() const           { return type() == String; }
    bool isArray() const            { return type() == Array; }
    bool isMap() const              { return type() == Map; }
    bool isTag() const              { return type() == Tag; }
    bool isSimpleType() const       { return type() == SimpleType; }
    bool isFloat16() const          { return type() == Float16; }
    bool isFloat() const            { return type() == Float; }
    bool isDouble() const           { return type() == Double; }
    bool isInvalid() const          { return type() == Invalid; }

    bool isSimpleType(QCborSimpleType st) const { return isSimpleType() && toSimpleType() == st; }
    bool isFalse() const            { return isSimpleType(QCborSimpleType::False); }
    bool isTrue() const             { return isSimpleType(QCborSimpleType::True); }
    bool isBool() const             { return isFalse() || isTrue(); }
    bool isNull() const             { return isSimpleType(QCborSimpleType::Null); }
    bool isUndefined() const        { return isSimpleType(QCborSimpleType::Undefined); }

    bool isLengthKnown() const noexcept Q_DECL_PURE_FUNCTION;
    quint64 length() const;

    bool isContainer() const            { return isMap() || isArray(); }
    bool enterContainer()               { Q_ASSERT(isContainer()); return _enterContainer_helper(); }
    bool leaveContainer();

    StringResult<QString> readString()      { Q_ASSERT(isString()); return _readString_helper(); }
    StringResult<QByteArray> readByteArray(){ Q_ASSERT(isByteArray()); return _readByteArray_helper(); }
    qsizetype currentStringChunkSize() const{ Q_ASSERT(isString() || isByteArray()); return _currentStringChunkSize(); }
    StringResult<qsizetype> readStringChunk(char *ptr, qsizetype maxlen);

    bool toBool() const                 { Q_ASSERT(isBool()); return value64 - int(QCborSimpleType::False); }
    QCborTag toTag() const              { Q_ASSERT(isTag()); return QCborTag(value64); }
    quint64 toUnsignedInteger() const   { Q_ASSERT(isUnsignedInteger()); return value64; }
    QCborNegativeInteger toNegativeInteger() const { Q_ASSERT(isNegativeInteger()); return QCborNegativeInteger(value64 + 1); }
    QCborSimpleType toSimpleType() const{ Q_ASSERT(isSimpleType()); return QCborSimpleType(value64); }
    qfloat16 toFloat16() const          { Q_ASSERT(isFloat16()); return _toFloatingPoint<qfloat16>(); }
    float toFloat() const               { Q_ASSERT(isFloat()); return _toFloatingPoint<float>(); }
    double toDouble() const             { Q_ASSERT(isDouble()); return _toFloatingPoint<double>(); }

    qint64 toInteger() const
    {
        Q_ASSERT(isInteger());
        qint64 v = qint64(value64);
        if (isNegativeInteger())
            return -v - 1;
        return v;
    }

private:
    void preparse();
    bool _enterContainer_helper();
    StringResult<QString> _readString_helper();
    StringResult<QByteArray> _readByteArray_helper();
    qsizetype _currentStringChunkSize() const;

    template <typename FP> FP _toFloatingPoint() const noexcept
    {
        using UIntFP = typename QIntegerForSizeof<FP>::Unsigned;
        UIntFP u = UIntFP(value64);
        FP f;
        memcpy(static_cast<void *>(&f), &u, sizeof(f));
        return f;
    }

    friend QCborStreamReaderPrivate;
    friend class QCborContainerPrivate;
    quint64 value64;
    QScopedPointer<QCborStreamReaderPrivate> d;
    quint8 type_;
    quint8 reserved[3] = {};
};

QT_END_NAMESPACE

#if defined(QT_X11_DEFINES_FOUND)
#  define True  1
#  define False 0
#endif

#endif // QCBORSTREAM_H
