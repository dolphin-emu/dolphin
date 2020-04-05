/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
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
#ifndef QV4STATICVALUE_P_H
#define QV4STATICVALUE_P_H

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

#include <QtCore/private/qnumeric_p.h>
#include <cstring>

#ifdef QT_NO_DEBUG
#define QV4_NEARLY_ALWAYS_INLINE Q_ALWAYS_INLINE
#else
#define QV4_NEARLY_ALWAYS_INLINE inline
#endif

QT_BEGIN_NAMESPACE

namespace QV4 {

// ReturnedValue is used to return values from runtime methods
// the type has to be a primitive type (no struct or union), so that the compiler
// will return it in a register on all platforms.
// It will be returned in rax on x64, [eax,edx] on x86 and [r0,r1] on arm
typedef quint64 ReturnedValue;

struct Double {
    quint64 d;

    Double(double dbl) {
        memcpy(&d, &dbl, sizeof(double));
    }

    int sign() const {
        return (d >> 63) ? -1 : 1;
    }

    bool isDenormal() const {
        return static_cast<int>((d << 1) >> 53) == 0;
    }

    int exponent() const {
        return static_cast<int>((d << 1) >> 53) - 1023;
    }

    quint64 significant() const {
        quint64 m = (d << 12) >> 12;
        if (!isDenormal())
            m |= (static_cast<quint64>(1) << 52);
        return m;
    }

    static int toInt32(double d) {
        int i = static_cast<int>(d);
        if (i == d)
            return i;
        return Double(d).toInt32();
    }

    int toInt32() {
        int e = exponent() - 52;
        if (e < 0) {
            if (e <= -53)
                return 0;
            return sign() * static_cast<int>(significant() >> -e);
        } else {
            if (e > 31)
                return 0;
            return sign() * (static_cast<int>(significant()) << e);
        }
    }
};

struct StaticValue
{
    StaticValue() = default;
    constexpr StaticValue(quint64 val) : _val(val) {}

    StaticValue &operator=(ReturnedValue v)
    {
        _val = v;
        return *this;
    }

    template<typename Value>
    StaticValue &operator=(const Value &);

    template<typename Value>
    const Value &asValue() const;

    template<typename Value>
    Value &asValue();

    /*
        We use 8 bytes for a value and a different variant of NaN boxing. A Double
        NaN (actually -qNaN) is indicated by a number that has the top 13 bits set, and for a
        signalling NaN it is the top 14 bits. The other values are usually set to 0 by the
        processor, and are thus free for us to store other data. We keep pointers in there for
        managed objects, and encode the other types using the free space given to use by the unused
        bits for NaN values. This also works for pointers on 64 bit systems, as they all currently
        only have 48 bits of addressable memory. (Note: we do leave the lower 49 bits available for
        pointers.)

        We xor Doubles with (0xffff8000 << 32). That has the effect that no doubles will
        get encoded with bits 63-49 all set to 0. We then use bit 48 to distinguish between
        managed/undefined (0), or Null/Int/Bool/Empty (1). So, storing a 49 bit pointer will leave
        the top 15 bits 0, which is exactly the 'natural' representation of pointers. If bit 49 is
        set, bit 48 indicates Empty (0) or integer-convertible (1). Then the 3 bit below that are
        used to encode Null/Int/Bool.

        Undefined is encoded as a managed pointer with value 0. This is the same as a nullptr.

        Specific bit-sequences:
        0 = always 0
        1 = always 1
        x = stored value
        a,b,c,d = specific bit values, see notes

        32109876 54321098 76543210 98765432 10987654 32109876 54321098 76543210 |
        66665555 55555544 44444444 33333333 33222222 22221111 11111100 00000000 | JS Value
        ------------------------------------------------------------------------+--------------
        00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 | Undefined
        00000000 0000000x xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx | Managed (heap pointer)
        a0000000 0000bc00 00000000 00000000 00000000 00000000 00000000 00000000 | NaN/Inf
        dddddddd ddddddxx xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx | double
        00000000 00000010 00000000 00000000 00000000 00000000 00000000 00000000 | empty (non-sparse array hole)
        00000000 00000010 10000000 00000000 00000000 00000000 00000000 00000000 | Null
        00000000 00000011 00000000 00000000 00000000 00000000 00000000 0000000x | Bool
        00000000 00000011 10000000 00000000 xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx | Int

        Notes:
        - a: xor-ed signbit, always 1 for NaN
        - bc, xor-ed values: 11 = inf, 10 = sNaN, 01 = qNaN, 00 = boxed value
        - d: xor-ed bits, where at least one bit is set, so: (val >> (64-14)) > 0
        - Undefined maps to C++ nullptr, so the "default" initialization is the same for both C++
          and JS
        - Managed has the left 15 bits set to 0, so: (val >> (64-15)) == 0
        - empty, Null, Bool, and Int have the left 14 bits set to 0, and bit 49 set to 1,
          so: (val >> (64-15)) == 1
        - Null, Bool, and Int have bit 48 set, indicating integer-convertible
        - xoring _val with NaNEncodeMask will convert to a double in "natural" representation, where
          any non double results in a NaN
        - on 32bit we can use the fact that addresses are 32bits wide, so the tag part (bits 32 to
          63) are zero. No need to shift.
    */

    quint64 _val;

    QV4_NEARLY_ALWAYS_INLINE Q_DECL_RELAXED_CONSTEXPR quint64 &rawValueRef() { return _val; }
    QV4_NEARLY_ALWAYS_INLINE Q_DECL_RELAXED_CONSTEXPR quint64 rawValue() const { return _val; }
    QV4_NEARLY_ALWAYS_INLINE Q_DECL_RELAXED_CONSTEXPR void setRawValue(quint64 raw) { _val = raw; }

#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
    static inline int valueOffset() { return 0; }
    static inline int tagOffset() { return 4; }
#else // !Q_LITTLE_ENDIAN
    static inline int valueOffset() { return 4; }
    static inline int tagOffset() { return 0; }
#endif
    static inline constexpr quint64 tagValue(quint32 tag, quint32 value) { return quint64(tag) << 32 | value; }
    QV4_NEARLY_ALWAYS_INLINE Q_DECL_RELAXED_CONSTEXPR void setTagValue(quint32 tag, quint32 value) { _val = quint64(tag) << 32 | value; }
    QV4_NEARLY_ALWAYS_INLINE constexpr quint32 value() const { return _val & quint64(~quint32(0)); }
    QV4_NEARLY_ALWAYS_INLINE constexpr quint32 tag() const { return _val >> 32; }
    QV4_NEARLY_ALWAYS_INLINE Q_DECL_RELAXED_CONSTEXPR void setTag(quint32 tag) { setTagValue(tag, value()); }

    QV4_NEARLY_ALWAYS_INLINE constexpr int int_32() const
    {
        return int(value());
    }
    QV4_NEARLY_ALWAYS_INLINE Q_DECL_RELAXED_CONSTEXPR void setInt_32(int i)
    {
        setTagValue(quint32(ValueTypeInternal::Integer), quint32(i));
    }
    QV4_NEARLY_ALWAYS_INLINE uint uint_32() const { return value(); }

    QV4_NEARLY_ALWAYS_INLINE Q_DECL_RELAXED_CONSTEXPR void setEmpty()
    {
        setTagValue(quint32(ValueTypeInternal::Empty), 0);
    }

    // ### Fix for 32 bit (easiest solution is to set highest bit to 1 for mananged/undefined/integercompatible
    // and use negative numbers here
    enum QuickType {
        QT_ManagedOrUndefined = 0,
        QT_ManagedOrUndefined1 = 1,
        QT_ManagedOrUndefined2 = 2,
        QT_ManagedOrUndefined3 = 3,
        QT_Empty = 4,
        QT_Null = 5,
        QT_Bool = 6,
        QT_Int = 7
        // all other values are doubles
    };

    enum Type {
        Undefined_Type = 0,
        Managed_Type = 1,
        Empty_Type = 4,
        Null_Type = 5,
        Boolean_Type = 6,
        Integer_Type = 7,
        Double_Type = 8
    };

    inline Type type() const {
        int t = quickType();
        if (t < QT_Empty)
            return _val ? Managed_Type : Undefined_Type;
        if (t > QT_Int)
            return Double_Type;
        return static_cast<Type>(t);
    }

    // Shared between 32-bit and 64-bit encoding
    enum {
        Tag_Shift = 32
    };

    // Used only by 64-bit encoding
    static const quint64 NaNEncodeMask = 0xfffc000000000000ull;
    enum {
        IsDouble_Shift = 64-14,
        IsManagedOrUndefined_Shift = 64-15,
        IsIntegerConvertible_Shift = 64-15,
        IsIntegerOrBool_Shift = 64-16,
        QuickType_Shift = 64 - 17,
        IsPositiveIntShift = 31
    };

    static const quint64 Immediate_Mask_64 = 0x00020000u; // bit 49

    enum class ValueTypeInternal_64 {
        Empty            = Immediate_Mask_64 | 0,
        Null             = Immediate_Mask_64 | 0x08000u,
        Boolean          = Immediate_Mask_64 | 0x10000u,
        Integer          = Immediate_Mask_64 | 0x18000u
    };

    // Used only by 32-bit encoding
    enum Masks {
        SilentNaNBit           =                  0x00040000,
        NotDouble_Mask         =                  0x7ffa0000,
    };
    static const quint64 Immediate_Mask_32 = NotDouble_Mask | 0x00020000u | SilentNaNBit;

    enum class ValueTypeInternal_32 {
        Empty            = Immediate_Mask_32 | 0,
        Null             = Immediate_Mask_32 | 0x08000u,
        Boolean          = Immediate_Mask_32 | 0x10000u,
        Integer          = Immediate_Mask_32 | 0x18000u
    };

    enum {
        Managed_Type_Internal = 0
    };

    using ValueTypeInternal = ValueTypeInternal_64;

    enum {
        NaN_Mask = 0x7ff80000,
    };

    inline quint64 quickType() const { return (_val >> QuickType_Shift); }

    // used internally in property
    inline bool isEmpty() const { return tag() == quint32(ValueTypeInternal::Empty); }
    inline bool isNull() const { return tag() == quint32(ValueTypeInternal::Null); }
    inline bool isBoolean() const { return tag() == quint32(ValueTypeInternal::Boolean); }
    inline bool isInteger() const { return tag() == quint32(ValueTypeInternal::Integer); }
    inline bool isNullOrUndefined() const { return isNull() || isUndefined(); }
    inline bool isNumber() const { return quickType() >= QT_Int; }

    inline bool isUndefined() const { return _val == 0; }
    inline bool isDouble() const { return (_val >> IsDouble_Shift); }
    inline bool isManaged() const
    {
#if QT_POINTER_SIZE == 4
        return value() && tag() == Managed_Type_Internal;
#else
        return _val && ((_val >> IsManagedOrUndefined_Shift) == 0);
#endif
    }
    inline bool isManagedOrUndefined() const
    {
#if QT_POINTER_SIZE == 4
        return tag() == Managed_Type_Internal;
#else
        return ((_val >> IsManagedOrUndefined_Shift) == 0);
#endif
    }

    inline bool isIntOrBool() const {
        return (_val >> IsIntegerOrBool_Shift) == 3;
    }

    inline bool integerCompatible() const {
        Q_ASSERT(!isEmpty());
        return (_val >> IsIntegerConvertible_Shift) == 1;
    }

    static inline bool integerCompatible(StaticValue a, StaticValue b) {
        return a.integerCompatible() && b.integerCompatible();
    }

    static inline bool bothDouble(StaticValue a, StaticValue b) {
        return a.isDouble() && b.isDouble();
    }

    inline bool isNaN() const
    {
        return (tag() & 0x7ffc0000  ) == 0x00040000;
    }

    inline bool isPositiveInt() const {
#if QT_POINTER_SIZE == 4
        return isInteger() && int_32() >= 0;
#else
        return (_val >> IsPositiveIntShift) == (quint64(ValueTypeInternal::Integer) << 1);
#endif
    }

    QV4_NEARLY_ALWAYS_INLINE double doubleValue() const {
        Q_ASSERT(isDouble());
        double d;
        StaticValue v = *this;
        v._val ^= NaNEncodeMask;
        memcpy(&d, &v._val, 8);
        return d;
    }

    QV4_NEARLY_ALWAYS_INLINE void setDouble(double d) {
        if (qt_is_nan(d))
            d = qt_qnan();
        memcpy(&_val, &d, 8);
        _val ^= NaNEncodeMask;
        Q_ASSERT(isDouble());
    }

    inline bool isInt32() {
        if (tag() == quint32(ValueTypeInternal::Integer))
            return true;
        if (isDouble()) {
            double d = doubleValue();
            if (isInt32(d)) {
                setInt_32(int(d));
                return true;
            }
        }
        return false;
    }

    QV4_NEARLY_ALWAYS_INLINE static bool isInt32(double d) {
        int i = int(d);
        return (i == d && !(d == 0 && std::signbit(d)));
    }

    double asDouble() const {
        if (tag() == quint32(ValueTypeInternal::Integer))
            return int_32();
        return doubleValue();
    }

    bool booleanValue() const {
        return int_32();
    }

    int integerValue() const {
        return int_32();
    }

    inline bool tryIntegerConversion() {
        bool b = integerCompatible();
        if (b)
            setTagValue(quint32(ValueTypeInternal::Integer), value());
        return b;
    }

    bool toBoolean() const {
        if (integerCompatible())
            return static_cast<bool>(int_32());

        if (isManagedOrUndefined())
            return false;

        // double
        const double d = doubleValue();
        return d && !std::isnan(d);
    }

    inline int toInt32() const
    {
        switch (type()) {
        case Null_Type:
        case Boolean_Type:
        case Integer_Type:
            return int_32();
        case Double_Type:
            return Double::toInt32(doubleValue());
        case Empty_Type:
        case Undefined_Type:
        case Managed_Type:
            break;
        }
        return Double::toInt32(std::numeric_limits<double>::quiet_NaN());
    }

    ReturnedValue *data_ptr() { return &_val; }
    constexpr ReturnedValue asReturnedValue() const { return _val; }
    constexpr static StaticValue fromReturnedValue(ReturnedValue val) { return {val}; }

    inline static constexpr StaticValue emptyValue() { return { tagValue(quint32(ValueTypeInternal::Empty), 0) }; }
    static inline constexpr StaticValue fromBoolean(bool b) { return { tagValue(quint32(ValueTypeInternal::Boolean), b) }; }
    static inline constexpr StaticValue fromInt32(int i) { return { tagValue(quint32(ValueTypeInternal::Integer), quint32(i)) }; }
    inline static constexpr StaticValue undefinedValue() { return { 0 }; }
    static inline constexpr StaticValue nullValue() { return { tagValue(quint32(ValueTypeInternal::Null), 0) }; }

    static inline StaticValue fromDouble(double d)
    {
        StaticValue v;
        v.setDouble(d);
        return v;
    }

    static inline StaticValue fromUInt32(uint i)
    {
        StaticValue v;
        if (i < uint(std::numeric_limits<int>::max())) {
            v.setTagValue(quint32(ValueTypeInternal::Integer), i);
        } else {
            v.setDouble(i);
        }
        return v;
    }

    static double toInteger(double d)
    {
        if (std::isnan(d))
            return +0;
        if (!d || std::isinf(d))
            return d;
        return d >= 0 ? std::floor(d) : std::ceil(d);
    }

    static int toInt32(double d)
    {
        return Double::toInt32(d);
    }

    static unsigned int toUInt32(double d)
    {
        return static_cast<uint>(toInt32(d));
    }
};
Q_STATIC_ASSERT(std::is_trivial<StaticValue>::value);

struct Encode {
    static constexpr ReturnedValue undefined() {
        return StaticValue::undefinedValue().asReturnedValue();
    }
    static constexpr ReturnedValue null() {
        return StaticValue::nullValue().asReturnedValue();
    }

    explicit constexpr Encode(bool b)
        : val(StaticValue::fromBoolean(b).asReturnedValue())
    {
    }
    explicit Encode(double d) {
        val = StaticValue::fromDouble(d).asReturnedValue();
    }
    explicit constexpr Encode(int i)
        : val(StaticValue::fromInt32(i).asReturnedValue())
    {
    }
    explicit Encode(uint i) {
        val = StaticValue::fromUInt32(i).asReturnedValue();
    }
    explicit constexpr Encode(ReturnedValue v)
        : val(v)
    {
    }
    constexpr Encode(StaticValue v)
        : val(v.asReturnedValue())
    {
    }

    template<typename HeapBase>
    explicit Encode(HeapBase *o);

    explicit Encode(StaticValue *o) {
        Q_ASSERT(o);
        val = o->asReturnedValue();
    }

    static ReturnedValue smallestNumber(double d) {
        if (StaticValue::isInt32(d))
            return Encode(static_cast<int>(d));
        else
            return Encode(d);
    }

    constexpr operator ReturnedValue() const {
        return val;
    }
    quint64 val;
private:
    explicit Encode(void *);
};

}

QT_END_NAMESPACE

#endif // QV4STATICVALUE_P_H
