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
#ifndef QV4_OBJECT_H
#define QV4_OBJECT_H

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

#include "qv4managed_p.h"
#include "qv4memberdata_p.h"
#include "qv4arraydata_p.h"
#include "qv4engine_p.h"
#include "qv4scopedvalue_p.h"
#include "qv4value_p.h"
#include "qv4internalclass_p.h"
#include "qv4string_p.h"

QT_BEGIN_NAMESPACE


namespace QV4 {

namespace Heap {

#define ObjectMembers(class, Member) \
    Member(class, Pointer, MemberData *, memberData) \
    Member(class, Pointer, ArrayData *, arrayData)

DECLARE_EXPORTED_HEAP_OBJECT(Object, Base) {
    static void markObjects(Heap::Base *base, MarkStack *stack);
    void init() { Base::init(); }

    const VTable *vtable() const {
        return internalClass->vtable;
    }

    const Value *inlinePropertyDataWithOffset(uint indexWithOffset) const {
        Q_ASSERT(indexWithOffset >= vtable()->inlinePropertyOffset && indexWithOffset < uint(vtable()->inlinePropertyOffset + vtable()->nInlineProperties));
        return reinterpret_cast<const Value *>(this) + indexWithOffset;
    }
    const Value *inlinePropertyData(uint index) const {
        Q_ASSERT(index < vtable()->nInlineProperties);
        return reinterpret_cast<const Value *>(this) + vtable()->inlinePropertyOffset + index;
    }
    void setInlinePropertyWithOffset(ExecutionEngine *e, uint indexWithOffset, Value v) {
        Q_ASSERT(indexWithOffset >= vtable()->inlinePropertyOffset && indexWithOffset < uint(vtable()->inlinePropertyOffset + vtable()->nInlineProperties));
        Value *prop = reinterpret_cast<Value *>(this) + indexWithOffset;
        WriteBarrier::write(e, this, prop->data_ptr(), v.asReturnedValue());
    }
    void setInlinePropertyWithOffset(ExecutionEngine *e, uint indexWithOffset, Heap::Base *b) {
        Q_ASSERT(indexWithOffset >= vtable()->inlinePropertyOffset && indexWithOffset < uint(vtable()->inlinePropertyOffset + vtable()->nInlineProperties));
        Value *prop = reinterpret_cast<Value *>(this) + indexWithOffset;
        WriteBarrier::write(e, this, prop->data_ptr(), Value::fromHeapObject(b).asReturnedValue());
    }

    PropertyIndex writablePropertyData(uint index) {
        uint nInline = vtable()->nInlineProperties;
        if (index < nInline)
            return PropertyIndex{ this, reinterpret_cast<Value *>(this) + vtable()->inlinePropertyOffset + index};
        index -= nInline;
        return PropertyIndex{ memberData, memberData->values.values + index };
    }

    const Value *propertyData(uint index) const {
        uint nInline = vtable()->nInlineProperties;
        if (index < nInline)
            return reinterpret_cast<const Value *>(this) + vtable()->inlinePropertyOffset + index;
        index -= nInline;
        return memberData->values.data() + index;
    }
    void setProperty(ExecutionEngine *e, uint index, Value v) {
        uint nInline = vtable()->nInlineProperties;
        if (index < nInline) {
            setInlinePropertyWithOffset(e, index + vtable()->inlinePropertyOffset, v);
            return;
        }
        index -= nInline;
        memberData->values.set(e, index, v);
    }
    void setProperty(ExecutionEngine *e, uint index, Heap::Base *b) {
        uint nInline = vtable()->nInlineProperties;
        if (index < nInline) {
            setInlinePropertyWithOffset(e, index + vtable()->inlinePropertyOffset, b);
            return;
        }
        index -= nInline;
        memberData->values.set(e, index, b);
    }

    void setUsedAsProto();

    Heap::Object *prototype() const { return internalClass->prototype; }
};

}

struct Q_QML_EXPORT Object: Managed {
    V4_OBJECT2(Object, Object)
    Q_MANAGED_TYPE(Object)
    V4_INTERNALCLASS(Object)
    V4_PROTOTYPE(objectPrototype)

    enum { NInlineProperties = 2 };

    enum {
        IsObject = true,
        GetterOffset = 0,
        SetterOffset = 1
    };

    void setInternalClass(Heap::InternalClass *ic);

    const Value *propertyData(uint index) const { return d()->propertyData(index); }

    Heap::ArrayData *arrayData() const { return d()->arrayData; }
    void setArrayData(ArrayData *a) { d()->arrayData.set(engine(), a ? a->d() : nullptr); }

    void getProperty(const InternalClassEntry &entry, Property *p) const;
    void setProperty(const InternalClassEntry &entry, const Property *p);
    void setProperty(uint index, Value v) const { d()->setProperty(engine(), index, v); }
    void setProperty(uint index, Heap::Base *b) const { d()->setProperty(engine(), index, b); }
    void setProperty(ExecutionEngine *engine, uint index, Value v) const { d()->setProperty(engine, index, v); }
    void setProperty(ExecutionEngine *engine, uint index, Heap::Base *b) const { d()->setProperty(engine, index, b); }

    const VTable *vtable() const { return d()->vtable(); }

    PropertyAttributes getOwnProperty(PropertyKey id, Property *p = nullptr) const {
        return vtable()->getOwnProperty(this, id, p);
    }

    PropertyIndex getValueOrSetter(PropertyKey id, PropertyAttributes *attrs);

    bool hasProperty(PropertyKey id) const {
        return vtable()->hasProperty(this, id);
    }

    bool defineOwnProperty(PropertyKey id, const Property *p, PropertyAttributes attrs) {
        return vtable()->defineOwnProperty(this, id, p, attrs);
    }

    //
    // helpers
    //
    static ReturnedValue getValue(const Value *thisObject, const Value &v, PropertyAttributes attrs) {
        if (attrs.isData())
            return v.asReturnedValue();
        return getValueAccessor(thisObject, v, attrs);
    }
    ReturnedValue getValue(const Value &v, PropertyAttributes attrs) const {
        return getValue(this, v, attrs);
    }
    ReturnedValue getValueByIndex(uint propertyIndex) const {
        PropertyAttributes attrs = internalClass()->propertyData.at(propertyIndex);
        const Value *v = propertyData(propertyIndex);
        if (!attrs.isAccessor())
            return v->asReturnedValue();
        return getValueAccessor(this, *v, attrs);
    }
    static ReturnedValue getValueAccessor(const Value *thisObject, const Value &v, PropertyAttributes attrs);

    bool putValue(uint memberIndex, PropertyAttributes attrs, const Value &value);

    /* The spec default: Writable: true, Enumerable: false, Configurable: true */
    void defineDefaultProperty(StringOrSymbol *name, const Value &value, PropertyAttributes attributes = Attr_Data|Attr_NotEnumerable) {
        insertMember(name, value, attributes);
    }
    void defineDefaultProperty(const QString &name, const Value &value, PropertyAttributes attributes = Attr_Data|Attr_NotEnumerable);
    void defineDefaultProperty(const QString &name, VTable::Call code,
                               int argumentCount = 0, PropertyAttributes attributes = Attr_Data|Attr_NotEnumerable);
    void defineDefaultProperty(StringOrSymbol *name, VTable::Call code,
                               int argumentCount = 0, PropertyAttributes attributes = Attr_Data|Attr_NotEnumerable);
    void defineAccessorProperty(const QString &name, VTable::Call getter, VTable::Call setter);
    void defineAccessorProperty(StringOrSymbol *name, VTable::Call getter, VTable::Call setter);
    /* Fixed: Writable: false, Enumerable: false, Configurable: false */
    void defineReadonlyProperty(const QString &name, const Value &value);
    void defineReadonlyProperty(String *name, const Value &value);

    /* Fixed: Writable: false, Enumerable: false, Configurable: true */
    void defineReadonlyConfigurableProperty(const QString &name, const Value &value);
    void defineReadonlyConfigurableProperty(StringOrSymbol *name, const Value &value);

    void addSymbolSpecies();

    void insertMember(StringOrSymbol *s, const Value &v, PropertyAttributes attributes = Attr_Data) {
        Scope scope(engine());
        ScopedProperty p(scope);
        p->value = v;
        insertMember(s, p, attributes);
    }
    void insertMember(StringOrSymbol *s, const Property *p, PropertyAttributes attributes);

    bool isExtensible() const { return vtable()->isExtensible(this); }
    bool preventExtensions() { return vtable()->preventExtensions(this); }
    Heap::Object *getPrototypeOf() const { return vtable()->getPrototypeOf(this); }
    bool setPrototypeOf(const Object *p) { return vtable()->setPrototypeOf(this, p); }
    void setPrototypeUnchecked(const Object *p);

    // Array handling

public:
    void copyArrayData(Object *other);

    bool setArrayLength(uint newLen);
    void setArrayLengthUnchecked(uint l);

    void arraySet(uint index, const Property *p, PropertyAttributes attributes = Attr_Data);
    void arraySet(uint index, const Value &value);

    bool arrayPut(uint index, const Value &value) {
        return arrayData()->vtable()->put(this, index, value);
    }
    bool arrayPut(uint index, const Value *values, uint n) {
        return arrayData()->vtable()->putArray(this, index, values, n);
    }
    void setArrayAttributes(uint i, PropertyAttributes a) {
        Q_ASSERT(arrayData());
        if (d()->arrayData->attrs || a != Attr_Data) {
            ArrayData::ensureAttributes(this);
            a.resolve();
            arrayData()->vtable()->setAttribute(this, i, a);
        }
    }

    void push_back(const Value &v);

    ArrayData::Type arrayType() const {
        return arrayData() ? static_cast<ArrayData::Type>(d()->arrayData->type) : Heap::ArrayData::Simple;
    }
    // ### remove me
    void setArrayType(ArrayData::Type t) {
        Q_ASSERT(t != Heap::ArrayData::Simple && t != Heap::ArrayData::Sparse);
        arrayCreate();
        d()->arrayData->type = t;
    }

    inline void arrayReserve(uint n) {
        ArrayData::realloc(this, Heap::ArrayData::Simple, n, false);
    }

    void arrayCreate() {
        if (!arrayData())
            ArrayData::realloc(this, Heap::ArrayData::Simple, 0, false);
#ifdef CHECK_SPARSE_ARRAYS
        initSparseArray();
#endif
    }

    void initSparseArray();
    SparseArrayNode *sparseBegin() const { return arrayType() == Heap::ArrayData::Sparse ? d()->arrayData->sparse->begin() : nullptr; }
    SparseArrayNode *sparseEnd() const { return arrayType() == Heap::ArrayData::Sparse ? d()->arrayData->sparse->end() : nullptr; }

    inline bool protoHasArray() {
        Scope scope(engine());
        ScopedObject p(scope, this);

        while ((p = p->getPrototypeOf()))
            if (p->arrayData())
                return true;

        return false;
    }

    inline ReturnedValue get(StringOrSymbol *name, bool *hasProperty = nullptr, const Value *receiver = nullptr) const
    { if (!receiver) receiver = this; return vtable()->get(this, name->toPropertyKey(), receiver, hasProperty); }
    inline ReturnedValue get(uint idx, bool *hasProperty = nullptr, const Value *receiver = nullptr) const
    { if (!receiver) receiver = this; return vtable()->get(this, PropertyKey::fromArrayIndex(idx), receiver, hasProperty); }
    QT_DEPRECATED inline ReturnedValue getIndexed(uint idx, bool *hasProperty = nullptr) const
    { return get(idx, hasProperty); }
    inline ReturnedValue get(PropertyKey id, const Value *receiver = nullptr, bool *hasProperty = nullptr) const
    { if (!receiver) receiver = this; return vtable()->get(this, id, receiver, hasProperty); }

    // use the set variants instead, to customize throw behavior
    inline bool put(StringOrSymbol *name, const Value &v, Value *receiver = nullptr)
    { if (!receiver) receiver = this; return vtable()->put(this, name->toPropertyKey(), v, receiver); }
    inline bool put(uint idx, const Value &v, Value *receiver = nullptr)
    { if (!receiver) receiver = this; return vtable()->put(this, PropertyKey::fromArrayIndex(idx), v, receiver); }
    QT_DEPRECATED inline bool putIndexed(uint idx, const Value &v)
    { return put(idx, v); }
    inline bool put(PropertyKey id, const Value &v, Value *receiver = nullptr)
    { if (!receiver) receiver = this; return vtable()->put(this, id, v, receiver); }

    enum ThrowOnFailure {
        DoThrowOnRejection,
        DoNotThrow
    };

    // This is the same as set(), but it doesn't require creating a string key,
    // which is much more efficient for the array case.
    inline bool setIndexed(uint idx, const Value &v, ThrowOnFailure shouldThrow)
    {
        bool ret = vtable()->put(this, PropertyKey::fromArrayIndex(idx), v, this);
        // ES6: 7.3.3, 6: If success is false and Throw is true, throw a TypeError exception.
        if (!ret && shouldThrow == ThrowOnFailure::DoThrowOnRejection) {
            ExecutionEngine *e = engine();
            if (!e->hasException) { // allow a custom set impl to throw itself
                QString message = QLatin1String("Cannot assign to read-only property \"") +
                        QString::number(idx) + QLatin1Char('\"');
                e->throwTypeError(message);
            }
        }
        return ret;
    }

    // ES6: 7.3.3 Set (O, P, V, Throw)
    inline bool set(StringOrSymbol *name, const Value &v, ThrowOnFailure shouldThrow)
    {
        bool ret = vtable()->put(this, name->toPropertyKey(), v, this);
        // ES6: 7.3.3, 6: If success is false and Throw is true, throw a TypeError exception.
        if (!ret && shouldThrow == ThrowOnFailure::DoThrowOnRejection) {
            ExecutionEngine *e = engine();
            if (!e->hasException) { // allow a custom set impl to throw itself
                QString message = QLatin1String("Cannot assign to read-only property \"") +
                        name->toQString() + QLatin1Char('\"');
                e->throwTypeError(message);
            }
        }
        return ret;
    }

    bool deleteProperty(PropertyKey id)
    { return vtable()->deleteProperty(this, id); }
    OwnPropertyKeyIterator *ownPropertyKeys(Value *target) const
    { return vtable()->ownPropertyKeys(this, target); }
    qint64 getLength() const { return vtable()->getLength(this); }
    ReturnedValue instanceOf(const Value &var) const
    { return vtable()->instanceOf(this, var); }

    bool isConcatSpreadable() const;
    bool isArray() const;
    const FunctionObject *speciesConstructor(Scope &scope, const FunctionObject *defaultConstructor) const;

    bool setProtoFromNewTarget(const Value *newTarget);

    ReturnedValue resolveLookupGetter(ExecutionEngine *engine, Lookup *lookup) const
    { return vtable()->resolveLookupGetter(this, engine, lookup); }
    ReturnedValue resolveLookupSetter(ExecutionEngine *engine, Lookup *lookup, const Value &value)
    { return vtable()->resolveLookupSetter(this, engine, lookup, value); }

protected:
    static ReturnedValue virtualGet(const Managed *m, PropertyKey id, const Value *receiver,bool *hasProperty);
    static bool virtualPut(Managed *m, PropertyKey id, const Value &value, Value *receiver);
    static bool virtualDeleteProperty(Managed *m, PropertyKey id);
    static bool virtualHasProperty(const Managed *m, PropertyKey id);
    static PropertyAttributes virtualGetOwnProperty(const Managed *m, PropertyKey id, Property *p);
    static bool virtualDefineOwnProperty(Managed *m, PropertyKey id, const Property *p, PropertyAttributes attrs);
    static bool virtualIsExtensible(const Managed *m);
    static bool virtualPreventExtensions(Managed *);
    static Heap::Object *virtualGetPrototypeOf(const Managed *);
    static bool virtualSetPrototypeOf(Managed *, const Object *);
    static OwnPropertyKeyIterator *virtualOwnPropertyKeys(const Object *m, Value *target);
    static qint64 virtualGetLength(const Managed *m);
    static ReturnedValue virtualInstanceOf(const Object *typeObject, const Value &var);
    static ReturnedValue virtualResolveLookupGetter(const Object *object, ExecutionEngine *engine, Lookup *lookup);
    static bool virtualResolveLookupSetter(Object *object, ExecutionEngine *engine, Lookup *lookup, const Value &value);
public:
    // qv4runtime uses this directly
    static ReturnedValue checkedInstanceOf(ExecutionEngine *engine, const FunctionObject *typeObject, const Value &var);

private:
    bool internalDefineOwnProperty(ExecutionEngine *engine, uint index, const InternalClassEntry *memberEntry, const Property *p, PropertyAttributes attrs);
    ReturnedValue internalGet(PropertyKey id, const Value *receiver, bool *hasProperty) const;
    bool internalPut(PropertyKey id, const Value &value, Value *receiver);
    bool internalDeleteProperty(PropertyKey id);

    friend struct ObjectIterator;
    friend struct ObjectPrototype;
};

struct Q_QML_PRIVATE_EXPORT ObjectOwnPropertyKeyIterator : OwnPropertyKeyIterator
{
    uint arrayIndex = 0;
    uint memberIndex = 0;
    bool iterateOverSymbols = false;
    ~ObjectOwnPropertyKeyIterator() override = default;
    PropertyKey next(const Object *o, Property *pd = nullptr, PropertyAttributes *attrs = nullptr) override;

};

namespace Heap {

struct BooleanObject : Object {
    void init() { Object::init(); }
    void init(bool b) {
        Object::init();
        this->b = b;
    }

    bool b;
};

struct NumberObject : Object {
    void init() { Object::init(); }
    void init(double val) {
        Object::init();
        value = val;
    }

    double value;
};

struct ArrayObject : Object {
    enum {
        LengthPropertyIndex = 0
    };

    void init() {
        Object::init();
        commonInit();
    }

    void init(const QStringList &list);

private:
    void commonInit()
    { setProperty(internalClass->engine, LengthPropertyIndex, Value::fromInt32(0)); }
};

}

struct BooleanObject: Object {
    V4_OBJECT2(BooleanObject, Object)
    Q_MANAGED_TYPE(BooleanObject)
    V4_PROTOTYPE(booleanPrototype)

    bool value() const { return d()->b; }

};

struct NumberObject: Object {
    V4_OBJECT2(NumberObject, Object)
    Q_MANAGED_TYPE(NumberObject)
    V4_PROTOTYPE(numberPrototype)

    double value() const { return d()->value; }
};

struct ArrayObject: Object {
    V4_OBJECT2(ArrayObject, Object)
    Q_MANAGED_TYPE(ArrayObject)
    V4_INTERNALCLASS(ArrayObject)
    V4_PROTOTYPE(arrayPrototype)

    void init(ExecutionEngine *engine);

    static qint64 virtualGetLength(const Managed *m);

    QStringList toQStringList() const;
protected:
    static bool virtualDefineOwnProperty(Managed *m, PropertyKey id, const Property *p, PropertyAttributes attrs);

};

inline void Object::setArrayLengthUnchecked(uint l)
{
    if (isArrayObject())
        setProperty(Heap::ArrayObject::LengthPropertyIndex, Value::fromUInt32(l));
}

inline void Object::push_back(const Value &v)
{
    arrayCreate();

    uint idx = getLength();
    arrayReserve(idx + 1);
    arrayPut(idx, v);
    setArrayLengthUnchecked(idx + 1);
}

inline void Object::arraySet(uint index, const Property *p, PropertyAttributes attributes)
{
    // ### Clean up
    arrayCreate();
    if (attributes.isAccessor() || (index > 0x1000 && index > 2*d()->arrayData->values.alloc)) {
        initSparseArray();
    } else {
        arrayData()->vtable()->reallocate(this, index + 1, false);
    }
    setArrayAttributes(index, attributes);
    ArrayData::insert(this, index, &p->value, attributes.isAccessor());
    if (isArrayObject() && index >= getLength())
        setArrayLengthUnchecked(index + 1);
}


inline void Object::arraySet(uint index, const Value &value)
{
    arrayCreate();
    if (index > 0x1000 && index > 2*d()->arrayData->values.alloc) {
        initSparseArray();
    }
    ArrayData::insert(this, index, &value);
    if (isArrayObject() && index >= getLength())
        setArrayLengthUnchecked(index + 1);
}


template<>
inline const ArrayObject *Value::as() const {
    return isManaged() && m()->internalClass->vtable->type == Managed::Type_ArrayObject ? static_cast<const ArrayObject *>(this) : nullptr;
}

template<>
inline ReturnedValue value_convert<Object>(ExecutionEngine *e, const Value &v)
{
    return v.toObject(e)->asReturnedValue();
}

}

QT_END_NAMESPACE

#endif // QMLJS_OBJECTS_H
