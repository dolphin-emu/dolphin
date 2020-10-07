/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
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
#ifndef QV4STRING_H
#define QV4STRING_H

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

#include <QtCore/qstring.h>
#include "qv4managed_p.h"
#include <QtCore/private/qnumeric_p.h>
#include "qv4enginebase_p.h"
#include <private/qv4stringtoarrayindex_p.h>

QT_BEGIN_NAMESPACE

namespace QV4 {

struct ExecutionEngine;
struct PropertyKey;

namespace Heap {

struct Q_QML_PRIVATE_EXPORT StringOrSymbol : Base
{
    enum StringType {
        StringType_Symbol,
        StringType_Regular,
        StringType_ArrayIndex,
        StringType_Unknown,
        StringType_AddedString,
        StringType_SubString,
        StringType_Complex = StringType_AddedString
    };

    mutable QStringData *text;
    mutable PropertyKey identifier;
    mutable uint subtype;
    mutable uint stringHash;

    static void markObjects(Heap::Base *that, MarkStack *markStack);
    void destroy();

    inline QString toQString() const {
        if (!text)
            return QString();
        QStringDataPtr ptr = { text };
        text->ref.ref();
        return QString(ptr);
    }
    void createHashValue() const;
    inline unsigned hashValue() const {
        if (subtype >= StringType_Unknown)
            createHashValue();
        Q_ASSERT(subtype < StringType_Complex);

        return stringHash;
    }
};

struct Q_QML_PRIVATE_EXPORT String : StringOrSymbol {
    static void markObjects(Heap::Base *that, MarkStack *markStack);

    const VTable *vtable() const {
        return internalClass->vtable;
    }

    void init(const QString &text);
    void simplifyString() const;
    int length() const;
    std::size_t retainedTextSize() const {
        return subtype >= StringType_Complex ? 0 : (std::size_t(text->size) * sizeof(QChar));
    }
    inline QString toQString() const {
        if (subtype >= StringType_Complex)
            simplifyString();
        QStringDataPtr ptr = { text };
        text->ref.ref();
        return QString(ptr);
    }
    inline bool isEqualTo(const String *other) const {
        if (this == other)
            return true;
        if (hashValue() != other->hashValue())
            return false;
        Q_ASSERT(subtype < StringType_Complex);
        if (identifier.isValid() && identifier == other->identifier)
            return true;
        if (subtype == Heap::String::StringType_ArrayIndex && other->subtype == Heap::String::StringType_ArrayIndex)
            return true;

        return toQString() == other->toQString();
    }

    bool startsWithUpper() const;

private:
    static void append(const String *data, QChar *ch);
};
Q_STATIC_ASSERT(std::is_trivial< String >::value);

struct ComplexString : String {
    void init(String *l, String *n);
    void init(String *ref, int from, int len);
    mutable String *left;
    mutable String *right;
    union {
        mutable int largestSubLength;
        int from;
    };
    int len;
};
Q_STATIC_ASSERT(std::is_trivial< ComplexString >::value);

inline
int String::length() const {
    return text ? text->size : static_cast<const ComplexString *>(this)->len;
}

}

struct Q_QML_PRIVATE_EXPORT StringOrSymbol : public Managed {
    V4_MANAGED(StringOrSymbol, Managed)
    V4_NEEDS_DESTROY
    enum {
        IsStringOrSymbol = true
    };

private:
    inline void createPropertyKey() const;
public:
    PropertyKey propertyKey() const { Q_ASSERT(d()->identifier.isValid()); return d()->identifier; }
    PropertyKey toPropertyKey() const;


    inline QString toQString() const {
        return d()->toQString();
    }
};

struct Q_QML_PRIVATE_EXPORT String : public StringOrSymbol {
    V4_MANAGED(String, StringOrSymbol)
    Q_MANAGED_TYPE(String)
    V4_INTERNALCLASS(String)
    enum {
        IsString = true
    };

    uchar subtype() const { return d()->subtype; }
    void setSubtype(uchar subtype) const { d()->subtype = subtype; }

    bool equals(String *other) const {
        return d()->isEqualTo(other->d());
    }
    inline bool isEqualTo(const String *other) const {
        return d()->isEqualTo(other->d());
    }

    inline bool lessThan(const String *other) {
        return toQString() < other->toQString();
    }

    inline QString toQString() const {
        return d()->toQString();
    }

    inline unsigned hashValue() const {
        return d()->hashValue();
    }
    uint toUInt(bool *ok) const;

    // slow path
    Q_NEVER_INLINE void createPropertyKeyImpl() const;

    static uint createHashValue(const QChar *ch, int length, uint *subtype)
    {
        const QChar *end = ch + length;
        return calculateHashValue(ch, end, subtype);
    }

    static uint createHashValue(const char *ch, int length, uint *subtype)
    {
        const char *end = ch + length;
        return calculateHashValue(ch, end, subtype);
    }

    bool startsWithUpper() const { return d()->startsWithUpper(); }

protected:
    static bool virtualIsEqualTo(Managed *that, Managed *o);
    static qint64 virtualGetLength(const Managed *m);

public:
    template <typename T>
    static inline uint calculateHashValue(const T *ch, const T* end, uint *subtype)
    {
        // array indices get their number as hash value
        uint h = stringToArrayIndex(ch, end);
        if (h != UINT_MAX) {
            if (subtype)
                *subtype = Heap::StringOrSymbol::StringType_ArrayIndex;
            return h;
        }

        while (ch < end) {
            h = 31 * h + charToUInt(ch);
            ++ch;
        }

        if (subtype)
            *subtype = (charToUInt(ch) == '@') ? Heap::StringOrSymbol::StringType_Symbol : Heap::StringOrSymbol::StringType_Regular;
        return h;
    }
};

struct ComplexString : String {
    typedef QV4::Heap::ComplexString Data;
    QV4::Heap::ComplexString *d_unchecked() const { return static_cast<QV4::Heap::ComplexString *>(m()); }
    QV4::Heap::ComplexString *d() const {
        QV4::Heap::ComplexString *dptr = d_unchecked();
        dptr->_checkIsInitialized();
        return dptr;
    }
};

inline
void StringOrSymbol::createPropertyKey() const {
    Q_ASSERT(!d()->identifier.isValid());
    Q_ASSERT(isString());
    static_cast<const String *>(this)->createPropertyKeyImpl();
}

inline PropertyKey StringOrSymbol::toPropertyKey() const {
    if (!d()->identifier.isValid())
        createPropertyKey();
    return d()->identifier;
}

template<>
inline const StringOrSymbol *Value::as() const {
    return isManaged() && m()->internalClass->vtable->isStringOrSymbol ? static_cast<const String *>(this) : nullptr;
}

template<>
inline const String *Value::as() const {
    return isManaged() && m()->internalClass->vtable->isString ? static_cast<const String *>(this) : nullptr;
}

template<>
inline ReturnedValue value_convert<String>(ExecutionEngine *e, const Value &v)
{
    return v.toString(e)->asReturnedValue();
}

}

QT_END_NAMESPACE

#endif
