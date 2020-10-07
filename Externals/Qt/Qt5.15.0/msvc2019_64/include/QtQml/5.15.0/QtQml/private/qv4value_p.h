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
#ifndef QV4VALUE_P_H
#define QV4VALUE_P_H

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

#include <limits.h>
#include <cmath>

#include <QtCore/QString>
#include "qv4global_p.h"
#include <private/qv4heap_p.h>
#include <private/qv4internalclass_p.h>
#include <private/qv4staticvalue_p.h>

#include <private/qnumeric_p.h>
#include <private/qv4calldata_p.h>

QT_BEGIN_NAMESPACE

namespace QV4 {

namespace Heap {
    struct Base;
}

struct Q_QML_PRIVATE_EXPORT Value : public StaticValue
{
    using HeapBasePtr = Heap::Base *;
    using ManagedPtr = Managed *;

    Value() = default;
    constexpr Value(quint64 val) : StaticValue(val) {}

    static constexpr Value fromStaticValue(StaticValue staticValue)
    {
        return {staticValue._val};
    }

#if QT_POINTER_SIZE == 8
    QML_NEARLY_ALWAYS_INLINE HeapBasePtr m() const
    {
        HeapBasePtr b;
#ifdef __ia64
// Restore bits 49-47 to bits 63-61, undoing the workaround explained in
// setM below.
        quint64 _tmp;

        _tmp = _val & (7L << 47); // 0x3800000000000
        _tmp = (_tmp << 14) | (_val ^ _tmp);
        memcpy(&b, &_tmp, 8);
#else
        memcpy(&b, &_val, 8);
#endif
        return b;
    }
    QML_NEARLY_ALWAYS_INLINE void setM(HeapBasePtr b)
    {
        memcpy(&_val, &b, 8);
#ifdef __ia64
// On ia64, bits 63-61 in a 64-bit pointer are used to store the virtual region
// number.  Since this implementation is not 64-bit clean, we move bits 63-61
// to bits 49-47 and hope for the best.  This is undone in *m(), above.
        _val |= ((_val & (7L << 61)) >> 14);
        _val &= ((1L << 50)-1);
#endif
    }
#elif QT_POINTER_SIZE == 4
    QML_NEARLY_ALWAYS_INLINE HeapBasePtr m() const
    {
        Q_STATIC_ASSERT(sizeof(HeapBasePtr) == sizeof(quint32));
        HeapBasePtr b;
        quint32 v = value();
        memcpy(&b, &v, 4);
        return b;
    }
    QML_NEARLY_ALWAYS_INLINE void setM(HeapBasePtr b)
    {
        quint32 v;
        memcpy(&v, &b, 4);
        setTagValue(Managed_Type_Internal, v);
    }
#else
#  error "unsupported pointer size"
#endif

    inline bool isString() const;
    inline bool isStringOrSymbol() const;
    inline bool isSymbol() const;
    inline bool isObject() const;
    inline bool isFunctionObject() const;

    QML_NEARLY_ALWAYS_INLINE String *stringValue() const {
        if (!isString())
            return nullptr;
        return reinterpret_cast<String *>(const_cast<Value *>(this));
    }
    QML_NEARLY_ALWAYS_INLINE StringOrSymbol *stringOrSymbolValue() const {
        if (!isStringOrSymbol())
            return nullptr;
        return reinterpret_cast<StringOrSymbol *>(const_cast<Value *>(this));
    }
    QML_NEARLY_ALWAYS_INLINE Symbol *symbolValue() const {
        if (!isSymbol())
            return nullptr;
        return reinterpret_cast<Symbol *>(const_cast<Value *>(this));
    }
    QML_NEARLY_ALWAYS_INLINE Object *objectValue() const {
        if (!isObject())
            return nullptr;
        return reinterpret_cast<Object*>(const_cast<Value *>(this));
    }
    QML_NEARLY_ALWAYS_INLINE ManagedPtr managed() const {
        if (!isManaged())
            return nullptr;
        return reinterpret_cast<Managed*>(const_cast<Value *>(this));
    }
    QML_NEARLY_ALWAYS_INLINE Value::HeapBasePtr heapObject() const {
        return isManagedOrUndefined() ? m() : nullptr;
    }

    static inline Value fromHeapObject(HeapBasePtr m)
    {
        Value v;
        v.setM(m);
        return v;
    }

    int toUInt16() const;
    inline int toInt32() const;
    inline unsigned int toUInt32() const;
    qint64 toLength() const;
    inline qint64 toIndex() const;

    bool toBoolean() const {
        if (integerCompatible())
            return static_cast<bool>(int_32());

        return toBooleanImpl(*this);
    }
    static bool toBooleanImpl(Value val);
    double toInteger() const;
    inline ReturnedValue convertedToNumber() const;
    inline double toNumber() const;
    static double toNumberImpl(Value v);
    double toNumberImpl() const { return toNumberImpl(*this); }
    QString toQStringNoThrow() const;
    QString toQString() const;
    Heap::String *toString(ExecutionEngine *e) const {
        if (isString())
            return reinterpret_cast<Heap::String *>(m());
        return toString(e, *this);
    }
    QV4::PropertyKey toPropertyKey(ExecutionEngine *e) const;

    static Heap::String *toString(ExecutionEngine *e, Value val);
    Heap::Object *toObject(ExecutionEngine *e) const {
        if (isObject())
            return reinterpret_cast<Heap::Object *>(m());
        return toObject(e, *this);
    }
    static Heap::Object *toObject(ExecutionEngine *e, Value val);

    inline bool isPrimitive() const;

    template <typename T>
    const T *as() const {
        if (!isManaged())
            return nullptr;

        Q_ASSERT(m()->internalClass->vtable);
#if !defined(QT_NO_QOBJECT_CHECK)
        static_cast<const T *>(this)->qt_check_for_QMANAGED_macro(static_cast<const T *>(this));
#endif
        const VTable *vt = m()->internalClass->vtable;
        while (vt) {
            if (vt == T::staticVTable())
                return static_cast<const T *>(this);
            vt = vt->parent;
        }
        return nullptr;
    }
    template <typename T>
    T *as() {
        if (isManaged())
            return const_cast<T *>(const_cast<const Value *>(this)->as<T>());
        else
            return nullptr;
    }

    template<typename T> inline T *cast() {
        return static_cast<T *>(managed());
    }
    template<typename T> inline const T *cast() const {
        return static_cast<const T *>(managed());
    }

    uint asArrayLength(bool *ok) const;

    static constexpr Value fromReturnedValue(ReturnedValue val)
    {
        return fromStaticValue(StaticValue::fromReturnedValue(val));
    }

    // As per ES specs
    bool sameValue(Value other) const;
    bool sameValueZero(Value other) const;

    inline void mark(MarkStack *markStack);

    static double toInteger(double d) { return StaticValue::toInteger(d); }
    static int toInt32(double d) { return StaticValue::toInt32(d); }
    static unsigned int toUInt32(double d) { return StaticValue::toUInt32(d); }
    inline static constexpr Value emptyValue()
    {
        return fromStaticValue(StaticValue::emptyValue());
    }
    static inline constexpr Value fromBoolean(bool b)
    {
        return fromStaticValue(StaticValue::fromBoolean(b));
    }
    static inline constexpr Value fromInt32(int i)
    {
        return fromStaticValue(StaticValue::fromInt32(i));
    }
    inline static constexpr Value undefinedValue()
    {
        return fromStaticValue(StaticValue::undefinedValue());
    }
    static inline constexpr Value nullValue()
    {
        return fromStaticValue(StaticValue::nullValue());
    }
    static inline Value fromDouble(double d)
    {
        return fromStaticValue(StaticValue::fromDouble(d));
    }
    static inline Value fromUInt32(uint i)
    {
        return fromStaticValue(StaticValue::fromUInt32(i));
    }

    Value &operator =(const ScopedValue &v);
    Value &operator=(ReturnedValue v)
    {
        StaticValue::operator=(v);
        return *this;
    }
    Value &operator=(ManagedPtr m) {
        if (!m) {
            setM(nullptr);
        } else {
            _val = reinterpret_cast<Value *>(m)->_val;
        }
        return *this;
    }
    Value &operator=(HeapBasePtr o) {
        setM(o);
        return *this;
    }

    template<typename T>
    Value &operator=(const Scoped<T> &t);
};
Q_STATIC_ASSERT(std::is_trivial<Value>::value);
Q_STATIC_ASSERT(sizeof(Value) == sizeof(StaticValue));

template<>
inline StaticValue &StaticValue::operator=<Value>(const Value &value)
{
    _val = value._val;
    return *this;
}

template<typename Managed>
inline StaticValue &StaticValue::operator=(const Managed &m)
{
    *static_cast<Value *>(this) = m;
    return *this;
}

template<>
inline Value &StaticValue::asValue<Value>()
{
    return *static_cast<Value *>(this);
}

template<>
inline const Value &StaticValue::asValue<Value>() const
{
    return *static_cast<const Value *>(this);
}

template<>
inline Value *CallData::argValues<Value>()
{
    return static_cast<Value *>(static_cast<StaticValue *>(args));
}

template<>
inline const Value *CallData::argValues<Value>() const
{
    return static_cast<const Value *>(static_cast<const StaticValue *>(args));
}

template<typename HeapBase>
inline Encode::Encode(HeapBase *o)
{
    val = Value::fromHeapObject(o).asReturnedValue();
}

inline void Value::mark(MarkStack *markStack)
{
    HeapBasePtr o = heapObject();
    if (o)
        o->mark(markStack);
}

inline bool Value::isString() const
{
    HeapBasePtr b = heapObject();
    return b && b->internalClass->vtable->isString;
}

bool Value::isStringOrSymbol() const
{
    HeapBasePtr b = heapObject();
    return b && b->internalClass->vtable->isStringOrSymbol;
}

bool Value::isSymbol() const
{
    HeapBasePtr b = heapObject();
    return b && b->internalClass->vtable->isStringOrSymbol && !b->internalClass->vtable->isString;
}

inline bool Value::isObject() const

{
    HeapBasePtr b = heapObject();
    return b && b->internalClass->vtable->isObject;
}

inline bool Value::isFunctionObject() const
{
    HeapBasePtr b = heapObject();
    return b && b->internalClass->vtable->isFunctionObject;
}

inline bool Value::isPrimitive() const
{
    return !isObject();
}

inline double Value::toNumber() const
{
    if (isInteger())
        return int_32();
    if (isDouble())
        return doubleValue();
    return toNumberImpl();
}

inline ReturnedValue Value::convertedToNumber() const
{
    if (isInteger() || isDouble())
        return asReturnedValue();
    Value v;
    v.setDouble(toNumberImpl());
    return v.asReturnedValue();
}

inline
ReturnedValue Heap::Base::asReturnedValue() const
{
    return Value::fromHeapObject(const_cast<Value::HeapBasePtr>(this)).asReturnedValue();
}

// For source compat with older code in other modules
using Primitive = Value;

template<typename T>
ReturnedValue value_convert(ExecutionEngine *e, const Value &v);

inline int Value::toInt32() const
{
    if (Q_LIKELY(integerCompatible()))
        return int_32();

    if (Q_LIKELY(isDouble()))
        return Double::toInt32(doubleValue());

    return Double::toInt32(toNumberImpl());
}

inline unsigned int Value::toUInt32() const
{
    return static_cast<unsigned int>(toInt32());
}

inline qint64 Value::toLength() const
{
    if (Q_LIKELY(integerCompatible()))
        return int_32() < 0 ? 0 : int_32();
    double i = Value::toInteger(isDouble() ? doubleValue() : toNumberImpl());
    if (i <= 0)
        return 0;
    if (i > (static_cast<qint64>(1) << 53) - 1)
        return (static_cast<qint64>(1) << 53) - 1;
    return static_cast<qint64>(i);
}

inline qint64 Value::toIndex() const
{
    qint64 idx;
    if (Q_LIKELY(integerCompatible())) {
        idx = int_32();
    } else {
        idx = static_cast<qint64>(Value::toInteger(isDouble() ? doubleValue() : toNumberImpl()));
    }
    if (idx > (static_cast<qint64>(1) << 53) - 1)
        idx = -1;
    return idx;
}

inline double Value::toInteger() const
{
    if (integerCompatible())
        return int_32();

    return Value::toInteger(isDouble() ? doubleValue() : toNumberImpl());
}


template <size_t o>
struct HeapValue : Value {
    static Q_CONSTEXPR size_t offset = o;
    HeapBasePtr base() {
        HeapBasePtr base = reinterpret_cast<HeapBasePtr>(this) - (offset/sizeof(Heap::Base));
        Q_ASSERT(base->inUse());
        return base;
    }

    void set(EngineBase *e, const Value &newVal) {
        WriteBarrier::write(e, base(), data_ptr(), newVal.asReturnedValue());
    }
    void set(EngineBase *e, HeapBasePtr b) {
        WriteBarrier::write(e, base(), data_ptr(), b->asReturnedValue());
    }
};

template <size_t o>
struct ValueArray {
    static Q_CONSTEXPR size_t offset = o;
    uint size;
    uint alloc;
    Value values[1];

    Value::HeapBasePtr base() {
        Value::HeapBasePtr base = reinterpret_cast<Value::HeapBasePtr>(this)
                - (offset/sizeof(Heap::Base));
        Q_ASSERT(base->inUse());
        return base;
    }

    void set(EngineBase *e, uint index, Value v) {
        WriteBarrier::write(e, base(), values[index].data_ptr(), v.asReturnedValue());
    }
    void set(EngineBase *e, uint index, Value::HeapBasePtr b) {
        WriteBarrier::write(e, base(), values[index].data_ptr(), Value::fromHeapObject(b).asReturnedValue());
    }
    inline const Value &operator[] (uint index) const {
        Q_ASSERT(index < alloc);
        return values[index];
    }
    inline const Value *data() const {
        return values;
    }

    void insertData(EngineBase *e, uint index, Value v) {
        for (uint i = size - 1; i > index; --i) {
            values[i] = values[i - 1];
        }
        set(e, index, v);
    }
    void removeData(EngineBase *e, uint index, int n = 1) {
        Q_UNUSED(e);
        for (uint i = index; i < size - n; ++i) {
            values[i] = values[i + n];
        }
    }

    void mark(MarkStack *markStack) {
        for (Value *v = values, *end = values + alloc; v < end; ++v)
            v->mark(markStack);
    }
};

// It's really important that the offset of values in this structure is
// constant across all architecture,  otherwise JIT cross-compiled code will
// have wrong offsets between host and target.
Q_STATIC_ASSERT(offsetof(ValueArray<0>, values) == 8);

class OptionalReturnedValue {
    ReturnedValue value;
public:

    OptionalReturnedValue() : value(Value::emptyValue().asReturnedValue()) {}
    explicit OptionalReturnedValue(ReturnedValue v)
        : value(v)
    {
        Q_ASSERT(!Value::fromReturnedValue(v).isEmpty());
    }

    ReturnedValue operator->() const { return value; }
    ReturnedValue operator*() const { return value; }
    explicit operator bool() const { return !Value::fromReturnedValue(value).isEmpty(); }
};

}

QT_END_NAMESPACE

#endif // QV4VALUE_DEF_P_H
