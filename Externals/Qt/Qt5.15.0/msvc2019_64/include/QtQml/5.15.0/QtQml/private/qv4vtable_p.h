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
#ifndef QV4VTABLE_P_H
#define QV4VTABLE_P_H

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

#include "qv4global_p.h"

QT_BEGIN_NAMESPACE

namespace QV4 {

struct Lookup;

struct Q_QML_PRIVATE_EXPORT OwnPropertyKeyIterator {
    virtual ~OwnPropertyKeyIterator() = 0;
    virtual PropertyKey next(const Object *o, Property *p = nullptr, PropertyAttributes *attrs = nullptr) = 0;
};

struct VTable
{
    typedef void (*Destroy)(Heap::Base *);
    typedef void (*MarkObjects)(Heap::Base *, MarkStack *markStack);
    typedef bool (*IsEqualTo)(Managed *m, Managed *other);

    typedef ReturnedValue (*Get)(const Managed *, PropertyKey id, const Value *receiver, bool *hasProperty);
    typedef bool (*Put)(Managed *, PropertyKey id, const Value &value, Value *receiver);
    typedef bool (*DeleteProperty)(Managed *m, PropertyKey id);
    typedef bool (*HasProperty)(const Managed *m, PropertyKey id);
    typedef PropertyAttributes (*GetOwnProperty)(const Managed *m, PropertyKey id, Property *p);
    typedef bool (*DefineOwnProperty)(Managed *m, PropertyKey id, const Property *p, PropertyAttributes attrs);
    typedef bool (*IsExtensible)(const Managed *);
    typedef bool (*PreventExtensions)(Managed *);
    typedef Heap::Object *(*GetPrototypeOf)(const Managed *);
    typedef bool (*SetPrototypeOf)(Managed *, const Object *);
    typedef qint64 (*GetLength)(const Managed *m);
    typedef OwnPropertyKeyIterator *(*OwnPropertyKeys)(const Object *m, Value *target);
    typedef ReturnedValue (*InstanceOf)(const Object *typeObject, const Value &var);

    typedef ReturnedValue (*Call)(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    typedef ReturnedValue (*CallAsConstructor)(const FunctionObject *, const Value *argv, int argc, const Value *newTarget);

    typedef ReturnedValue (*ResolveLookupGetter)(const Object *, ExecutionEngine *, Lookup *);
    typedef bool (*ResolveLookupSetter)(Object *, ExecutionEngine *, Lookup *, const Value &);

    const VTable * const parent;
    quint16 inlinePropertyOffset;
    quint16 nInlineProperties;
    quint8 isExecutionContext;
    quint8 isString;
    quint8 isObject;
    quint8 isFunctionObject;
    quint8 isErrorObject;
    quint8 isArrayData;
    quint8 isStringOrSymbol;
    quint8 type;
    quint8 unused[4];
    const char *className;

    Destroy destroy;
    MarkObjects markObjects;
    IsEqualTo isEqualTo;

    Get get;
    Put put;
    DeleteProperty deleteProperty;
    HasProperty hasProperty;
    GetOwnProperty getOwnProperty;
    DefineOwnProperty defineOwnProperty;
    IsExtensible isExtensible;
    PreventExtensions preventExtensions;
    GetPrototypeOf getPrototypeOf;
    SetPrototypeOf setPrototypeOf;
    GetLength getLength;
    OwnPropertyKeys ownPropertyKeys;
    InstanceOf instanceOf;

    Call call;
    CallAsConstructor callAsConstructor;

    ResolveLookupGetter resolveLookupGetter;
    ResolveLookupSetter resolveLookupSetter;
};


struct VTableBase {
protected:
    static constexpr VTable::Destroy virtualDestroy = nullptr;
    static constexpr VTable::IsEqualTo virtualIsEqualTo = nullptr;

    static constexpr VTable::Get virtualGet = nullptr;
    static constexpr VTable::Put virtualPut = nullptr;
    static constexpr VTable::DeleteProperty virtualDeleteProperty = nullptr;
    static constexpr VTable::HasProperty virtualHasProperty = nullptr;
    static constexpr VTable::GetOwnProperty virtualGetOwnProperty = nullptr;
    static constexpr VTable::DefineOwnProperty virtualDefineOwnProperty = nullptr;
    static constexpr VTable::IsExtensible virtualIsExtensible = nullptr;
    static constexpr VTable::PreventExtensions virtualPreventExtensions = nullptr;
    static constexpr VTable::GetPrototypeOf virtualGetPrototypeOf = nullptr;
    static constexpr VTable::SetPrototypeOf virtualSetPrototypeOf = nullptr;
    static constexpr VTable::GetLength virtualGetLength = nullptr;
    static constexpr VTable::OwnPropertyKeys virtualOwnPropertyKeys = nullptr;
    static constexpr VTable::InstanceOf virtualInstanceOf = nullptr;

    static constexpr VTable::Call virtualCall = nullptr;
    static constexpr VTable::CallAsConstructor virtualCallAsConstructor = nullptr;

    static constexpr VTable::ResolveLookupGetter virtualResolveLookupGetter = nullptr;
    static constexpr VTable::ResolveLookupSetter virtualResolveLookupSetter = nullptr;
};

#define DEFINE_MANAGED_VTABLE_INT(classname, parentVTable) \
{     \
    parentVTable, \
    (sizeof(classname::Data) + sizeof(QV4::Value) - 1)/sizeof(QV4::Value), \
    (sizeof(classname::Data) + (classname::NInlineProperties*sizeof(QV4::Value)) + QV4::Chunk::SlotSize - 1)/QV4::Chunk::SlotSize*QV4::Chunk::SlotSize/sizeof(QV4::Value) \
        - (sizeof(classname::Data) + sizeof(QV4::Value) - 1)/sizeof(QV4::Value), \
    classname::IsExecutionContext,          \
    classname::IsString,                    \
    classname::IsObject,                    \
    classname::IsFunctionObject,            \
    classname::IsErrorObject,               \
    classname::IsArrayData,                 \
    classname::IsStringOrSymbol,            \
    classname::MyType,                      \
    { 0, 0, 0, 0 },                         \
    #classname, \
    \
    classname::virtualDestroy,              \
    classname::Data::markObjects,           \
    classname::virtualIsEqualTo,            \
    \
    classname::virtualGet,                  \
    classname::virtualPut,                  \
    classname::virtualDeleteProperty,       \
    classname::virtualHasProperty,          \
    classname::virtualGetOwnProperty,       \
    classname::virtualDefineOwnProperty,    \
    classname::virtualIsExtensible,         \
    classname::virtualPreventExtensions,    \
    classname::virtualGetPrototypeOf,       \
    classname::virtualSetPrototypeOf,       \
    classname::virtualGetLength,            \
    classname::virtualOwnPropertyKeys,      \
    classname::virtualInstanceOf,           \
    \
    classname::virtualCall,                 \
    classname::virtualCallAsConstructor,    \
    \
    classname::virtualResolveLookupGetter,  \
    classname::virtualResolveLookupSetter   \
}

#define DEFINE_MANAGED_VTABLE(classname) \
const QV4::VTable classname::static_vtbl = DEFINE_MANAGED_VTABLE_INT(classname, 0)

#define V4_OBJECT2(DataClass, superClass) \
    private: \
        DataClass() Q_DECL_EQ_DELETE; \
        Q_DISABLE_COPY(DataClass) \
    public: \
        Q_MANAGED_CHECK \
        typedef QV4::Heap::DataClass Data; \
        typedef superClass SuperClass; \
        static const QV4::VTable static_vtbl; \
        static inline const QV4::VTable *staticVTable() { return &static_vtbl; } \
        V4_MANAGED_SIZE_TEST \
        QV4::Heap::DataClass *d_unchecked() const { return static_cast<QV4::Heap::DataClass *>(m()); } \
        QV4::Heap::DataClass *d() const { \
            QV4::Heap::DataClass *dptr = d_unchecked(); \
            dptr->_checkIsInitialized(); \
            return dptr; \
        } \
        Q_STATIC_ASSERT(std::is_trivial< QV4::Heap::DataClass >::value);

#define V4_PROTOTYPE(p) \
    static QV4::Object *defaultPrototype(QV4::ExecutionEngine *e) \
    { return e->p(); }


#define DEFINE_OBJECT_VTABLE_BASE(classname) \
    const QV4::VTable classname::static_vtbl = DEFINE_MANAGED_VTABLE_INT(classname, (std::is_same<classname::SuperClass, Object>::value) ? nullptr : &classname::SuperClass::static_vtbl)

#define DEFINE_OBJECT_VTABLE(classname) \
DEFINE_OBJECT_VTABLE_BASE(classname)

#define DEFINE_OBJECT_TEMPLATE_VTABLE(classname) \
template<> DEFINE_OBJECT_VTABLE_BASE(classname)

}

QT_END_NAMESPACE

#endif
