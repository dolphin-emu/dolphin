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

#ifndef QCBORVALUE_P_H
#define QCBORVALUE_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.
// This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "qcborvalue.h"

#include <private/qglobal_p.h>
#include <private/qutfcodec_p.h>

#include <math.h>

QT_BEGIN_NAMESPACE

namespace QtCbor {
struct Undefined {};
struct Element
{
    enum ValueFlag : quint32 {
        IsContainer                 = 0x0001,
        HasByteData                 = 0x0002,
        StringIsUtf16               = 0x0004,
        StringIsAscii               = 0x0008
    };
    Q_DECLARE_FLAGS(ValueFlags, ValueFlag)

    union {
        qint64 value;
        QCborContainerPrivate *container;
    };
    QCborValue::Type type;
    ValueFlags flags = {};

    Element(qint64 v = 0, QCborValue::Type t = QCborValue::Undefined, ValueFlags f = {})
        : value(v), type(t), flags(f)
    {}

    Element(QCborContainerPrivate *d, QCborValue::Type t, ValueFlags f = {})
        : container(d), type(t), flags(f | IsContainer)
    {}

    double fpvalue() const
    {
        double d;
        memcpy(&d, &value, sizeof(d));
        return d;
    }
};
Q_DECLARE_OPERATORS_FOR_FLAGS(Element::ValueFlags)
Q_STATIC_ASSERT(sizeof(Element) == 16);

struct ByteData
{
    QByteArray::size_type len;

    const char *byte() const        { return reinterpret_cast<const char *>(this + 1); }
    char *byte()                    { return reinterpret_cast<char *>(this + 1); }
    const QChar *utf16() const      { return reinterpret_cast<const QChar *>(this + 1); }
    QChar *utf16()                  { return reinterpret_cast<QChar *>(this + 1); }

    QByteArray toByteArray() const  { return QByteArray(byte(), len); }
    QString toString() const        { return QString(utf16(), len / 2); }
    QString toUtf8String() const    { return QString::fromUtf8(byte(), len); }

    QByteArray asByteArrayView() const { return QByteArray::fromRawData(byte(), len); }
    QLatin1String asLatin1() const  { return QLatin1String(byte(), len); }
    QStringView asStringView() const{ return QStringView(utf16(), len / 2); }
    QString asQStringRaw() const    { return QString::fromRawData(utf16(), len / 2); }
};
Q_STATIC_ASSERT(std::is_trivial<ByteData>::value);
Q_STATIC_ASSERT(std::is_standard_layout<ByteData>::value);
} // namespace QtCbor

Q_DECLARE_TYPEINFO(QtCbor::Element, Q_PRIMITIVE_TYPE);

class QCborContainerPrivate : public QSharedData
{
    friend class QExplicitlySharedDataPointer<QCborContainerPrivate>;
    ~QCborContainerPrivate();

public:
    enum ContainerDisposition { CopyContainer, MoveContainer };

    QByteArray::size_type usedData = 0;
    QByteArray data;
    QVector<QtCbor::Element> elements;

    void deref() { if (!ref.deref()) delete this; }
    void compact(qsizetype reserved);
    static QCborContainerPrivate *clone(QCborContainerPrivate *d, qsizetype reserved = -1);
    static QCborContainerPrivate *detach(QCborContainerPrivate *d, qsizetype reserved);
    static QCborContainerPrivate *grow(QCborContainerPrivate *d, qsizetype index);

    qptrdiff addByteData(const char *block, qsizetype len)
    {
        // This function does not do overflow checking, since the len parameter
        // is expected to be trusted. There's another version of this function
        // in decodeStringFromCbor(), which checks.

        qptrdiff offset = data.size();

        // align offset
        offset += Q_ALIGNOF(QtCbor::ByteData) - 1;
        offset &= ~(Q_ALIGNOF(QtCbor::ByteData) - 1);

        qptrdiff increment = qptrdiff(sizeof(QtCbor::ByteData)) + len;

        usedData += increment;
        data.resize(offset + increment);

        char *ptr = data.begin() + offset;
        auto b = new (ptr) QtCbor::ByteData;
        b->len = len;
        if (block)
            memcpy(b->byte(), block, len);

        return offset;
    }

    const QtCbor::ByteData *byteData(QtCbor::Element e) const
    {
        if ((e.flags & QtCbor::Element::HasByteData) == 0)
            return nullptr;

        size_t offset = size_t(e.value);
        Q_ASSERT((offset % Q_ALIGNOF(QtCbor::ByteData)) == 0);
        Q_ASSERT(offset + sizeof(QtCbor::ByteData) <= size_t(data.size()));

        auto b = reinterpret_cast<const QtCbor::ByteData *>(data.constData() + offset);
        Q_ASSERT(offset + sizeof(*b) + size_t(b->len) <= size_t(data.size()));
        return b;
    }
    const QtCbor::ByteData *byteData(qsizetype idx) const
    {
        return byteData(elements.at(idx));
    }

    QCborContainerPrivate *containerAt(qsizetype idx, QCborValue::Type type) const
    {
        const QtCbor::Element &e = elements.at(idx);
        if (e.type != type || (e.flags & QtCbor::Element::IsContainer) == 0)
            return nullptr;
        return e.container;
    }

    void replaceAt_complex(QtCbor::Element &e, const QCborValue &value, ContainerDisposition disp);
    void replaceAt_internal(QtCbor::Element &e, const QCborValue &value, ContainerDisposition disp)
    {
        if (value.container)
            return replaceAt_complex(e, value, disp);

        e = { value.value_helper(), value.type() };
        if (value.isContainer())
            e.container = nullptr;
    }
    void replaceAt(qsizetype idx, const QCborValue &value, ContainerDisposition disp = CopyContainer)
    {
        QtCbor::Element &e = elements[idx];
        if (e.flags & QtCbor::Element::IsContainer) {
            e.container->deref();
            e.container = nullptr;
            e.flags = {};
        } else if (auto b = byteData(e)) {
            usedData -= b->len + sizeof(QtCbor::ByteData);
        }
        replaceAt_internal(e, value, disp);
    }
    void insertAt(qsizetype idx, const QCborValue &value, ContainerDisposition disp = CopyContainer)
    {
        replaceAt_internal(*elements.insert(elements.begin() + idx, {}), value, disp);
    }

    void append(QtCbor::Undefined)
    {
        elements.append(QtCbor::Element());
    }
    void append(qint64 value)
    {
        elements.append(QtCbor::Element(value , QCborValue::Integer));
    }
    void append(QCborTag tag)
    {
        elements.append(QtCbor::Element(qint64(tag), QCborValue::Tag));
    }
    void appendByteData(const char *data, qsizetype len, QCborValue::Type type,
                        QtCbor::Element::ValueFlags extraFlags = {})
    {
        elements.append(QtCbor::Element(addByteData(data, len), type,
                                        QtCbor::Element::HasByteData | extraFlags));
    }
    void append(QLatin1String s)
    {
        if (!QtPrivate::isAscii(s))
            return append(QString(s));

        // US-ASCII is a subset of UTF-8, so we can keep in 8-bit
        appendByteData(s.latin1(), s.size(), QCborValue::String,
                       QtCbor::Element::StringIsAscii);
    }
    void appendAsciiString(QStringView s);

#if QT_STRINGVIEW_LEVEL < 2
    void append(const QString &s)
    {
        append(qToStringViewIgnoringNull(s));
    }
#endif

    void append(QStringView s)
    {
        if (QtPrivate::isAscii(s))
            appendAsciiString(s);
        else
            appendByteData(reinterpret_cast<const char *>(s.utf16()), s.size() * 2,
                           QCborValue::String, QtCbor::Element::StringIsUtf16);
    }
    void append(const QCborValue &v)
    {
        insertAt(elements.size(), v);
    }

    QByteArray byteArrayAt(qsizetype idx) const
    {
        const auto &e = elements.at(idx);
        const auto data = byteData(e);
        if (!data)
            return QByteArray();
        return data->toByteArray();
    }
    QString stringAt(qsizetype idx) const
    {
        const auto &e = elements.at(idx);
        const auto data = byteData(e);
        if (!data)
            return QString();
        if (e.flags & QtCbor::Element::StringIsUtf16)
            return data->toString();
        if (e.flags & QtCbor::Element::StringIsAscii)
            return data->asLatin1();
        return data->toUtf8String();
    }

    static void resetValue(QCborValue &v)
    {
        v.container = nullptr;
    }

    static QCborValue makeValue(QCborValue::Type type, qint64 n, QCborContainerPrivate *d = nullptr,
                                ContainerDisposition disp = CopyContainer)
    {
        QCborValue result(type);
        result.n = n;
        result.container = d;
        if (d && disp == CopyContainer)
            d->ref.ref();
        return result;
    }

    QCborValue valueAt(qsizetype idx) const
    {
        const auto &e = elements.at(idx);

        if (e.flags & QtCbor::Element::IsContainer) {
            if (e.type == QCborValue::Tag && e.container->elements.size() != 2) {
                // invalid tags can be created due to incomplete parsing
                return makeValue(QCborValue::Invalid, 0, nullptr);
            }
            return makeValue(e.type, -1, e.container);
        } else if (e.flags & QtCbor::Element::HasByteData) {
            return makeValue(e.type, idx, const_cast<QCborContainerPrivate *>(this));
        }
        return makeValue(e.type, e.value);
    }
    QCborValue extractAt_complex(QtCbor::Element e);
    QCborValue extractAt(qsizetype idx)
    {
        QtCbor::Element e;
        qSwap(e, elements[idx]);

        if (e.flags & QtCbor::Element::IsContainer) {
            if (e.type == QCborValue::Tag && e.container->elements.size() != 2) {
                // invalid tags can be created due to incomplete parsing
                e.container->deref();
                return makeValue(QCborValue::Invalid, 0, nullptr);
            }
            return makeValue(e.type, -1, e.container, MoveContainer);
        } else if (e.flags & QtCbor::Element::HasByteData) {
            return extractAt_complex(e);
        }
        return makeValue(e.type, e.value);
    }

    static QtCbor::Element elementFromValue(const QCborValue &value)
    {
        if (value.n >= 0 && value.container)
            return value.container->elements.at(value.n);

        QtCbor::Element e;
        e.value = value.n;
        e.type = value.t;
        if (value.container) {
            e.container = value.container;
            e.flags = QtCbor::Element::IsContainer;
        }
        return e;
    }

    static int compareUtf8(const QtCbor::ByteData *b, const QLatin1String &s)
    {
        return QUtf8::compareUtf8(b->byte(), b->len, s);
    }

    static int compareUtf8(const QtCbor::ByteData *b, QStringView s)
    {
        return QUtf8::compareUtf8(b->byte(), b->len, s.data(), s.size());
    }

    template<typename String>
    int stringCompareElement(const QtCbor::Element &e, String s) const
    {
        if (e.type != QCborValue::String)
            return int(e.type) - int(QCborValue::String);

        const QtCbor::ByteData *b = byteData(e);
        if (!b)
            return s.isEmpty() ? 0 : -1;

        if (e.flags & QtCbor::Element::StringIsUtf16)
            return QtPrivate::compareStrings(b->asStringView(), s);
        return compareUtf8(b, s);
    }

    template<typename String>
    bool stringEqualsElement(const QtCbor::Element &e, String s) const
    {
        return stringCompareElement(e, s) == 0;
    }

    template<typename String>
    bool stringEqualsElement(qsizetype idx, String s) const
    {
        return stringEqualsElement(elements.at(idx), s);
    }

    static int compareElement_helper(const QCborContainerPrivate *c1, QtCbor::Element e1,
                                     const QCborContainerPrivate *c2, QtCbor::Element e2);
    int compareElement(qsizetype idx, const QCborValue &value) const
    {
        auto &e1 = elements.at(idx);
        auto e2 = elementFromValue(value);
        return compareElement_helper(this, e1, value.container, e2);
    }

    void removeAt(qsizetype idx)
    {
        replaceAt(idx, {});
        elements.remove(idx);
    }

    void decodeValueFromCbor(QCborStreamReader &reader, int remainiingStackDepth);
    void decodeStringFromCbor(QCborStreamReader &reader);
    static inline void setErrorInReader(QCborStreamReader &reader, QCborError error);
};

QT_END_NAMESPACE

#endif // QCBORVALUE_P_H
