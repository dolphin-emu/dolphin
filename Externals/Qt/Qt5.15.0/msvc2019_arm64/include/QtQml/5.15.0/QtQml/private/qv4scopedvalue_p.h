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
#ifndef QV4SCOPEDVALUE_P_H
#define QV4SCOPEDVALUE_P_H

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

#include "qv4engine_p.h"
#include "qv4value_p.h"
#include "qv4property_p.h"
#include "qv4propertykey_p.h"

#ifdef V4_USE_VALGRIND
#include <valgrind/memcheck.h>
#endif

QT_BEGIN_NAMESPACE

#define SAVE_JS_STACK(ctx) Value *__jsStack = ctx->engine->jsStackTop
#define CHECK_JS_STACK(ctx) Q_ASSERT(__jsStack == ctx->engine->jsStackTop)

namespace QV4 {

struct ScopedValue;

#define CHECK_EXCEPTION() \
    do { \
        if (scope.hasException() || scope.engine->isInterrupted.loadAcquire()) { \
            return QV4::Encode::undefined(); \
        } \
    } while (false)

#define RETURN_UNDEFINED() \
    return QV4::Encode::undefined()

#define RETURN_RESULT(r) \
    return QV4::Encode(r)

#define THROW_TYPE_ERROR() \
    return scope.engine->throwTypeError()

#define THROW_GENERIC_ERROR(str) \
    return scope.engine->throwError(QString::fromUtf8(str))

struct Scope {
    explicit Scope(ExecutionContext *ctx)
        : engine(ctx->engine())
        , mark(engine->jsStackTop)
    {
    }

    explicit Scope(ExecutionEngine *e)
        : engine(e)
        , mark(engine->jsStackTop)
    {
    }

    explicit Scope(const Managed *m)
        : engine(m->engine())
        , mark(engine->jsStackTop)
    {
    }

    ~Scope() {
#ifndef QT_NO_DEBUG
        Q_ASSERT(engine->jsStackTop >= mark);
//        Q_ASSERT(engine->currentContext < mark);
        memset(mark, 0, (engine->jsStackTop - mark)*sizeof(Value));
#endif
#ifdef V4_USE_VALGRIND
        VALGRIND_MAKE_MEM_UNDEFINED(mark, (engine->jsStackLimit - mark) * sizeof(Value));
#endif
        engine->jsStackTop = mark;
    }

    enum AllocMode {
        Undefined,
        Empty,
        /* Be careful when using Uninitialized, the stack has to be fully initialized before calling into the memory manager again */
        Uninitialized
    };
    template <AllocMode mode = Undefined>
    QML_NEARLY_ALWAYS_INLINE Value *alloc(int nValues) const
    {
        Value *ptr = engine->jsAlloca(nValues);
        switch (mode) {
        case Undefined:
            for (int i = 0; i < nValues; ++i)
                ptr[i] = Value::undefinedValue();
            break;
        case Empty:
            for (int i = 0; i < nValues; ++i)
                ptr[i] = Value::emptyValue();
            break;
        case Uninitialized:
            break;
        }
        return ptr;
    }
    template <AllocMode mode = Undefined>
    QML_NEARLY_ALWAYS_INLINE Value *alloc() const
    {
        Value *ptr = engine->jsAlloca(1);
        switch (mode) {
        case Undefined:
            *ptr = Value::undefinedValue();
            break;
        case Empty:
            *ptr = Value::emptyValue();
            break;
        case Uninitialized:
            break;
        }
        return ptr;
    }

    bool hasException() const {
        return engine->hasException;
    }

    ExecutionEngine *engine;
    Value *mark;

private:
    Q_DISABLE_COPY(Scope)
};

struct ScopedValue
{
    ScopedValue(const Scope &scope)
    {
        ptr = scope.alloc<Scope::Uninitialized>();
        ptr->setRawValue(0);
    }

    ScopedValue(const Scope &scope, const Value &v)
    {
        ptr = scope.alloc<Scope::Uninitialized>();
        *ptr = v;
    }

    ScopedValue(const Scope &scope, Heap::Base *o)
    {
        ptr = scope.alloc<Scope::Uninitialized>();
        ptr->setM(o);
    }

    ScopedValue(const Scope &scope, Managed *m)
    {
        ptr = scope.alloc<Scope::Uninitialized>();
        ptr->setRawValue(m->asReturnedValue());
    }

    ScopedValue(const Scope &scope, const ReturnedValue &v)
    {
        ptr = scope.alloc<Scope::Uninitialized>();
        ptr->setRawValue(v);
    }

    ScopedValue &operator=(const Value &v) {
        *ptr = v;
        return *this;
    }

    ScopedValue &operator=(Heap::Base *o) {
        ptr->setM(o);
        return *this;
    }

    ScopedValue &operator=(Managed *m) {
        *ptr = *m;
        return *this;
    }

    ScopedValue &operator=(const ReturnedValue &v) {
        ptr->setRawValue(v);
        return *this;
    }

    ScopedValue &operator=(const ScopedValue &other) {
        *ptr = *other.ptr;
        return *this;
    }

    Value *operator->() {
        return ptr;
    }

    const Value *operator->() const {
        return ptr;
    }

    operator Value *() { return ptr; }
    operator const Value &() const { return *ptr; }

    Value *ptr;
};


struct ScopedPropertyKey
{
    ScopedPropertyKey(const Scope &scope)
    {
        ptr = reinterpret_cast<PropertyKey *>(scope.alloc<Scope::Uninitialized>());
        *ptr = PropertyKey::invalid();
    }

    ScopedPropertyKey(const Scope &scope, const PropertyKey &v)
    {
        ptr = reinterpret_cast<PropertyKey *>(scope.alloc<Scope::Uninitialized>());
        *ptr = v;
    }

    ScopedPropertyKey &operator=(const PropertyKey &other) {
        *ptr = other;
        return *this;
    }

    PropertyKey *operator->() {
        return ptr;
    }
    operator PropertyKey() const {
        return *ptr;
    }

    bool operator==(const PropertyKey &other) const {
        return *ptr == other;
    }
    bool operator==(const ScopedPropertyKey &other) const {
        return *ptr == *other.ptr;
    }
    bool operator!=(const PropertyKey &other) const {
        return *ptr != other;
    }
    bool operator!=(const ScopedPropertyKey &other) const {
        return *ptr != *other.ptr;
    }

    PropertyKey *ptr;
};


template<typename T>
struct Scoped
{
    enum ConvertType { Convert };

    QML_NEARLY_ALWAYS_INLINE void setPointer(const Managed *p) {
        ptr->setM(p ? p->m() : nullptr);
    }

    QML_NEARLY_ALWAYS_INLINE Scoped(const Scope &scope)
    {
        ptr = scope.alloc<Scope::Undefined>();
    }

    QML_NEARLY_ALWAYS_INLINE Scoped(const Scope &scope, const Value &v)
    {
        ptr = scope.alloc<Scope::Uninitialized>();
        setPointer(v.as<T>());
    }
    QML_NEARLY_ALWAYS_INLINE Scoped(const Scope &scope, Heap::Base *o)
    {
        Value v;
        v = o;
        ptr = scope.alloc<Scope::Uninitialized>();
        setPointer(v.as<T>());
    }
    QML_NEARLY_ALWAYS_INLINE Scoped(const Scope &scope, const ScopedValue &v)
    {
        ptr = scope.alloc<Scope::Uninitialized>();
        setPointer(v.ptr->as<T>());
    }

    QML_NEARLY_ALWAYS_INLINE Scoped(const Scope &scope, const Value &v, ConvertType)
    {
        ptr = scope.alloc<Scope::Uninitialized>();
        ptr->setRawValue(value_convert<T>(scope.engine, v));
    }

    QML_NEARLY_ALWAYS_INLINE Scoped(const Scope &scope, const Value *v)
    {
        ptr = scope.alloc<Scope::Uninitialized>();
        setPointer(v ? v->as<T>() : nullptr);
    }

    QML_NEARLY_ALWAYS_INLINE Scoped(const Scope &scope, T *t)
    {
        ptr = scope.alloc<Scope::Uninitialized>();
        setPointer(t);
    }

    QML_NEARLY_ALWAYS_INLINE Scoped(const Scope &scope, const T *t)
    {
        ptr = scope.alloc<Scope::Uninitialized>();
        setPointer(t);
    }

    QML_NEARLY_ALWAYS_INLINE Scoped(const Scope &scope, typename T::Data *t)
    {
        ptr = scope.alloc<Scope::Uninitialized>();
        *ptr = t;
    }

    QML_NEARLY_ALWAYS_INLINE Scoped(const Scope &scope, const ReturnedValue &v)
    {
        ptr = scope.alloc<Scope::Uninitialized>();
        setPointer(QV4::Value::fromReturnedValue(v).as<T>());
    }

    QML_NEARLY_ALWAYS_INLINE Scoped(const Scope &scope, const ReturnedValue &v, ConvertType)
    {
        ptr = scope.alloc<Scope::Uninitialized>();
        ptr->setRawValue(value_convert<T>(scope.engine, QV4::Value::fromReturnedValue(v)));
    }

    Scoped<T> &operator=(Heap::Base *o) {
        setPointer(Value::fromHeapObject(o).as<T>());
        return *this;
    }
    Scoped<T> &operator=(typename T::Data *t) {
        *ptr = t;
        return *this;
    }
    Scoped<T> &operator=(const Value &v) {
        setPointer(v.as<T>());
        return *this;
    }
    Scoped<T> &operator=(Value *v) {
        setPointer(v ? v->as<T>() : nullptr);
        return *this;
    }

    Scoped<T> &operator=(const ReturnedValue &v) {
        setPointer(QV4::Value::fromReturnedValue(v).as<T>());
        return *this;
    }

    Scoped<T> &operator=(const Scoped<T> &other) {
        *ptr = *other.ptr;
        return *this;
    }

    Scoped<T> &operator=(T *t) {
        setPointer(t);
        return *this;
    }

    operator T *() {
        return static_cast<T *>(ptr->managed());
    }
    operator const Value &() const {
        return *ptr;
    }

    T *operator->() {
        return getPointer();
    }

    const T *operator->() const {
        return getPointer();
    }

    explicit operator bool() const {
        return ptr->m();
    }

    T *getPointer() {
        return reinterpret_cast<T *>(ptr);
    }

    const T *getPointer() const {
        return reinterpret_cast<T *>(ptr);
    }

    Value *getRef() {
        return ptr;
    }

    QML_NEARLY_ALWAYS_INLINE ReturnedValue asReturnedValue() const {
        return ptr->rawValue();
    }

    Value *ptr;
};

inline Value &Value::operator =(const ScopedValue &v)
{
    _val = v.ptr->rawValue();
    return *this;
}

template<typename T>
inline Value &Value::operator=(const Scoped<T> &t)
{
    _val = t.ptr->rawValue();
    return *this;
}

struct ScopedProperty
{
    ScopedProperty(Scope &scope)
    {
        property = reinterpret_cast<Property*>(scope.alloc(sizeof(Property) / sizeof(Value)));
    }

    Property *operator->() { return property; }
    operator const Property *() const { return property; }
    operator Property *() { return property; }

    Property *property;
};

}

QT_END_NAMESPACE

#endif
