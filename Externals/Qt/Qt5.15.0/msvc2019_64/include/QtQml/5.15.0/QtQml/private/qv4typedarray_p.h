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
#ifndef QV4TYPEDARRAY_H
#define QV4TYPEDARRAY_H

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

#include "qv4object_p.h"
#include "qv4functionobject_p.h"
#include "qv4arraybuffer_p.h"

QT_BEGIN_NAMESPACE

namespace QV4 {

struct ArrayBuffer;

enum TypedArrayType {
    Int8Array,
    UInt8Array,
    Int16Array,
    UInt16Array,
    Int32Array,
    UInt32Array,
    UInt8ClampedArray,
    Float32Array,
    Float64Array,
    NTypedArrayTypes
};

enum AtomicModifyOps {
    AtomicAdd,
    AtomicAnd,
    AtomicExchange,
    AtomicOr,
    AtomicSub,
    AtomicXor,
    NAtomicModifyOps
};

struct TypedArrayOperations {
    typedef ReturnedValue (*Read)(const char *data);
    typedef void (*Write)(char *data, Value value);
    typedef ReturnedValue (*AtomicModify)(char *data, Value value);
    typedef ReturnedValue (*AtomicCompareExchange)(char *data, Value expected, Value v);
    typedef ReturnedValue (*AtomicLoad)(char *data);
    typedef ReturnedValue (*AtomicStore)(char *data, Value value);

    template<typename T>
    static constexpr TypedArrayOperations create(const char *name);
    template<typename T>
    static constexpr TypedArrayOperations createWithAtomics(const char *name);

    int bytesPerElement;
    const char *name;
    Read read;
    Write write;
    AtomicModify atomicModifyOps[AtomicModifyOps::NAtomicModifyOps];
    AtomicCompareExchange atomicCompareExchange;
    AtomicLoad atomicLoad;
    AtomicStore atomicStore;
};

namespace Heap {

#define TypedArrayMembers(class, Member) \
    Member(class, Pointer, ArrayBuffer *, buffer) \
    Member(class, NoMark, const TypedArrayOperations *, type) \
    Member(class, NoMark, uint, byteLength) \
    Member(class, NoMark, uint, byteOffset) \
    Member(class, NoMark, uint, arrayType)

DECLARE_HEAP_OBJECT(TypedArray, Object) {
    DECLARE_MARKOBJECTS(TypedArray);
    using Type = TypedArrayType;

    void init(Type t);
};

struct IntrinsicTypedArrayCtor : FunctionObject {
};

struct TypedArrayCtor : FunctionObject {
    void init(QV4::ExecutionContext *scope, TypedArray::Type t);

    TypedArray::Type type;
};

struct IntrinsicTypedArrayPrototype : Object {
};

struct TypedArrayPrototype : Object {
    inline void init(TypedArray::Type t);
    TypedArray::Type type;
};


}

struct Q_QML_PRIVATE_EXPORT TypedArray : Object
{
    V4_OBJECT2(TypedArray, Object)

    static Heap::TypedArray *create(QV4::ExecutionEngine *e, Heap::TypedArray::Type t);

    uint byteLength() const {
        return d()->byteLength;
    }

    uint length() const {
        return d()->byteLength/d()->type->bytesPerElement;
    }

    QTypedArrayData<char> *arrayData() {
        return d()->buffer->data;
    }

    Heap::TypedArray::Type arrayType() const {
        return static_cast<Heap::TypedArray::Type>(d()->arrayType);
    }
    using Object::get;

    static ReturnedValue virtualGet(const Managed *m, PropertyKey id, const Value *receiver, bool *hasProperty);
    static bool virtualHasProperty(const Managed *m, PropertyKey id);
    static PropertyAttributes virtualGetOwnProperty(const Managed *m, PropertyKey id, Property *p);
    static bool virtualPut(Managed *m, PropertyKey id, const Value &value, Value *receiver);
    static bool virtualDefineOwnProperty(Managed *m, PropertyKey id, const Property *p, PropertyAttributes attrs);
    static OwnPropertyKeyIterator *virtualOwnPropertyKeys(const Object *m, Value *target);

};

struct IntrinsicTypedArrayCtor: FunctionObject
{
    V4_OBJECT2(IntrinsicTypedArrayCtor, FunctionObject)

    static constexpr VTable::Call virtualCall = nullptr;

    static ReturnedValue method_of(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_from(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
};

struct TypedArrayCtor: FunctionObject
{
    V4_OBJECT2(TypedArrayCtor, FunctionObject)

    static ReturnedValue virtualCallAsConstructor(const FunctionObject *f, const Value *argv, int argc, const Value *);
    static ReturnedValue virtualCall(const FunctionObject *f, const Value *thisObject, const Value *argv, int argc);
};

struct IntrinsicTypedArrayPrototype : Object
{
    V4_OBJECT2(IntrinsicTypedArrayPrototype, Object)
    V4_PROTOTYPE(objectPrototype)

    void init(ExecutionEngine *engine, IntrinsicTypedArrayCtor *ctor);

    static ReturnedValue method_get_buffer(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_get_byteLength(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_get_byteOffset(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_get_length(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);

    static ReturnedValue method_copyWithin(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_entries(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_every(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_fill(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_filter(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_find(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_findIndex(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_forEach(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_includes(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_indexOf(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_join(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_keys(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_lastIndexOf(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_map(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_reduce(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_reduceRight(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_reverse(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_some(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_values(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_set(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_slice(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_subarray(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_toLocaleString(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);

    static ReturnedValue method_get_toStringTag(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);

};

struct TypedArrayPrototype : Object
{
    V4_OBJECT2(TypedArrayPrototype, Object)
    V4_PROTOTYPE(objectPrototype)

    void init(ExecutionEngine *engine, TypedArrayCtor *ctor);
};

inline void
Heap::TypedArrayPrototype::init(TypedArray::Type t)
{
    Object::init();
    type = t;
}

} // namespace QV4

QT_END_NAMESPACE

#endif
