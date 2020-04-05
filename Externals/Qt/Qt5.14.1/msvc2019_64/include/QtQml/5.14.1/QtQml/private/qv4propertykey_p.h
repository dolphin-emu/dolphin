/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
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
#ifndef QV4PROPERTYKEY_H
#define QV4PROPERTYKEY_H

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

#include <private/qv4global_p.h>

QT_BEGIN_NAMESPACE

class QString;

namespace QV4 {

struct PropertyKey
{
private:
    // Property keys are Strings, Symbols or unsigned integers.
    // For convenience we derive them from Values, allowing us to store them
    // on the JS stack
    //
    // They do however behave somewhat different than a Value:
    // * If the key is a String, the pointer to the string is stored in the identifier
    // table and thus unique.
    // * If the key is a Symbol it simply points to the referenced symbol object
    // * if the key is an array index (a uint < UINT_MAX), it's encoded as an
    // integer value
    quint64 val;

    // Important: Always keep this in sync with the definitions for Integers and heap objects in Value
    static const quint64 ArrayIndexMask = 0x3800000000000ull;
    enum {
        IsManagedOrUndefined_Shift = 64-15,
    };
    inline bool isManaged() const { return (val >> IsManagedOrUndefined_Shift) == 0; }
    inline quint32 value() const { return val & quint64(~quint32(0)); }

#if QT_POINTER_SIZE == 8
    QML_NEARLY_ALWAYS_INLINE Heap::StringOrSymbol *m() const
    {
        Heap::StringOrSymbol *b;
        memcpy(&b, &val, 8);
        return b;
    }
    QML_NEARLY_ALWAYS_INLINE void setM(Heap::StringOrSymbol *b)
    {
        memcpy(&val, &b, 8);
    }
#elif QT_POINTER_SIZE == 4
    QML_NEARLY_ALWAYS_INLINE Heap::StringOrSymbol *m() const
    {
        Q_STATIC_ASSERT(sizeof(Heap::StringOrSymbol*) == sizeof(quint32));
        Heap::StringOrSymbol *b;
        quint32 v = value();
        memcpy(&b, &v, 4);
        return b;
    }
    QML_NEARLY_ALWAYS_INLINE void setM(Heap::StringOrSymbol *b)
    {
        quint32 v;
        memcpy(&v, &b, 4);
        val = v;
    }
#endif

public:
    static PropertyKey invalid() { PropertyKey key; key.val = 0; return key; }
    static PropertyKey fromArrayIndex(uint idx) { PropertyKey key; key.val = ArrayIndexMask | static_cast<quint64>(idx); return key; }
    bool isStringOrSymbol() const { return isManaged() && val != 0; }
    uint asArrayIndex() const { Q_ASSERT(isArrayIndex()); return static_cast<uint>(val & 0xffffffff); }
    uint isArrayIndex() const { return !isManaged() && val != 0; }
    bool isValid() const { return val != 0; }
    static PropertyKey fromStringOrSymbol(Heap::StringOrSymbol *b)
    { PropertyKey key; key.setM(b); return key; }
    Heap::StringOrSymbol *asStringOrSymbol() const {
        if (!isManaged())
            return nullptr;
        return m();
    }

    Q_QML_EXPORT bool isString() const;
    bool isSymbol() const;
    bool isCanonicalNumericIndexString() const;

    Q_QML_EXPORT QString toQString() const;
    Heap::StringOrSymbol *toStringOrSymbol(ExecutionEngine *e);
    quint64 id() const { return val; }
    static PropertyKey fromId(quint64 id) {
        PropertyKey key; key.val = id; return key;
    }

    enum FunctionNamePrefix {
        None,
        Getter,
        Setter
    };
    Heap::String *asFunctionName(ExecutionEngine *e, FunctionNamePrefix prefix) const;

    bool operator ==(const PropertyKey &other) const { return val == other.val; }
    bool operator !=(const PropertyKey &other) const { return val != other.val; }
    bool operator <(const PropertyKey &other) const { return val < other.val; }
};

}

QT_END_NAMESPACE

#endif
