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
#ifndef QV4LOOKUP_H
#define QV4LOOKUP_H

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
#include "qv4runtime_p.h"
#include "qv4engine_p.h"
#include "qv4context_p.h"
#include "qv4object_p.h"
#include "qv4internalclass_p.h"

QT_BEGIN_NAMESPACE

namespace QV4 {

struct Q_QML_PRIVATE_EXPORT Lookup {
    union {
        ReturnedValue (*getter)(Lookup *l, ExecutionEngine *engine, const Value &object);
        ReturnedValue (*globalGetter)(Lookup *l, ExecutionEngine *engine);
        ReturnedValue (*qmlContextPropertyGetter)(Lookup *l, ExecutionEngine *engine, Value *thisObject);
        bool (*setter)(Lookup *l, ExecutionEngine *engine, Value &object, const Value &v);
    };
    // NOTE: gc assumes the first two entries in the struct are pointers to heap objects or null
    union {
        struct {
            Heap::Base *h1;
            Heap::Base *h2;
            quintptr unused;
            quintptr unused2;
        } markDef;
        struct {
            Heap::InternalClass *ic;
            quintptr unused;
            uint index;
            uint offset;
        } objectLookup;
        struct {
            quintptr protoId;
            quintptr _unused;
            const Value *data;
        } protoLookup;
        struct {
            Heap::InternalClass *ic;
            Heap::InternalClass *ic2;
            uint offset;
            uint offset2;
        } objectLookupTwoClasses;
        struct {
            quintptr protoId;
            quintptr protoId2;
            const Value *data;
            const Value *data2;
        } protoLookupTwoClasses;
        struct {
            // Make sure the next two values are in sync with protoLookup
            quintptr protoId;
            Heap::Object *proto;
            const Value *data;
            quintptr type;
        } primitiveLookup;
        struct {
            Heap::InternalClass *newClass;
            quintptr protoId;
            uint offset;
            uint unused;
        } insertionLookup;
        struct {
            quintptr _unused;
            quintptr _unused2;
            uint index;
            uint unused;
        } indexedLookup;
        struct {
            Heap::InternalClass *ic;
            Heap::InternalClass *qmlTypeIc; // only used when lookup goes through QQmlTypeWrapper
            QQmlPropertyCache *propertyCache;
            QQmlPropertyData *propertyData;
        } qobjectLookup;
        struct {
            Heap::InternalClass *ic;
            quintptr unused;
            QQmlPropertyCache *propertyCache;
            QQmlPropertyData *propertyData;
        } qgadgetLookup;
        struct {
            quintptr unused1;
            quintptr unused2;
            int scriptIndex;
        } qmlContextScriptLookup;
        struct {
            Heap::Object *singleton;
            quintptr unused;
        } qmlContextSingletonLookup;
        struct {
            quintptr unused1;
            quintptr unused2;
            int objectId;
        } qmlContextIdObjectLookup;
        struct {
            // Same as protoLookup, as used for global lookups
            quintptr reserved1;
            quintptr reserved2;
            quintptr reserved3;
            ReturnedValue (*getterTrampoline)(Lookup *l, ExecutionEngine *engine);
        } qmlContextGlobalLookup;
        struct {
            Heap::Object *qmlTypeWrapper;
            quintptr unused2;
        } qmlTypeLookup;
        struct {
            Heap::InternalClass *ic;
            quintptr unused;
            ReturnedValue encodedEnumValue;
        } qmlEnumValueLookup;
        struct {
            Heap::InternalClass *ic;
            Heap::Object *qmlScopedEnumWrapper;
        } qmlScopedEnumWrapperLookup;
    };
    uint nameIndex;

    ReturnedValue resolveGetter(ExecutionEngine *engine, const Object *object);
    ReturnedValue resolvePrimitiveGetter(ExecutionEngine *engine, const Value &object);
    ReturnedValue resolveGlobalGetter(ExecutionEngine *engine);
    void resolveProtoGetter(PropertyKey name, const Heap::Object *proto);

    static ReturnedValue getterGeneric(Lookup *l, ExecutionEngine *engine, const Value &object);
    static ReturnedValue getterTwoClasses(Lookup *l, ExecutionEngine *engine, const Value &object);
    static ReturnedValue getterFallback(Lookup *l, ExecutionEngine *engine, const Value &object);

    static ReturnedValue getter0MemberData(Lookup *l, ExecutionEngine *engine, const Value &object);
    static ReturnedValue getter0Inline(Lookup *l, ExecutionEngine *engine, const Value &object);
    static ReturnedValue getterProto(Lookup *l, ExecutionEngine *engine, const Value &object);
    static ReturnedValue getter0Inlinegetter0Inline(Lookup *l, ExecutionEngine *engine, const Value &object);
    static ReturnedValue getter0Inlinegetter0MemberData(Lookup *l, ExecutionEngine *engine, const Value &object);
    static ReturnedValue getter0MemberDatagetter0MemberData(Lookup *l, ExecutionEngine *engine, const Value &object);
    static ReturnedValue getterProtoTwoClasses(Lookup *l, ExecutionEngine *engine, const Value &object);
    static ReturnedValue getterAccessor(Lookup *l, ExecutionEngine *engine, const Value &object);
    static ReturnedValue getterProtoAccessor(Lookup *l, ExecutionEngine *engine, const Value &object);
    static ReturnedValue getterProtoAccessorTwoClasses(Lookup *l, ExecutionEngine *engine, const Value &object);
    static ReturnedValue getterIndexed(Lookup *l, ExecutionEngine *engine, const Value &object);

    static ReturnedValue primitiveGetterProto(Lookup *l, ExecutionEngine *engine, const Value &object);
    static ReturnedValue primitiveGetterAccessor(Lookup *l, ExecutionEngine *engine, const Value &object);
    static ReturnedValue stringLengthGetter(Lookup *l, ExecutionEngine *engine, const Value &object);

    static ReturnedValue globalGetterGeneric(Lookup *l, ExecutionEngine *engine);
    static ReturnedValue globalGetterProto(Lookup *l, ExecutionEngine *engine);
    static ReturnedValue globalGetterProtoAccessor(Lookup *l, ExecutionEngine *engine);

    bool resolveSetter(ExecutionEngine *engine, Object *object, const Value &value);
    static bool setterGeneric(Lookup *l, ExecutionEngine *engine, Value &object, const Value &value);
    Q_NEVER_INLINE static bool setterTwoClasses(Lookup *l, ExecutionEngine *engine, Value &object, const Value &value);
    static bool setterFallback(Lookup *l, ExecutionEngine *engine, Value &object, const Value &value);
    static bool setter0MemberData(Lookup *l, ExecutionEngine *engine, Value &object, const Value &value);
    static bool setter0Inline(Lookup *l, ExecutionEngine *engine, Value &object, const Value &value);
    static bool setter0setter0(Lookup *l, ExecutionEngine *engine, Value &object, const Value &value);
    static bool setterInsert(Lookup *l, ExecutionEngine *engine, Value &object, const Value &value);
    static bool arrayLengthSetter(Lookup *l, ExecutionEngine *engine, Value &object, const Value &value);

    void markObjects(MarkStack *stack) {
        if (markDef.h1 && !(reinterpret_cast<quintptr>(markDef.h1) & 1))
            markDef.h1->mark(stack);
        if (markDef.h2 && !(reinterpret_cast<quintptr>(markDef.h2) & 1))
            markDef.h2->mark(stack);
    }

    void clear() {
        memset(&markDef, 0, sizeof(markDef));
    }
};

Q_STATIC_ASSERT(std::is_standard_layout<Lookup>::value);
// Ensure that these offsets are always at this point to keep generated code compatible
// across 32-bit and 64-bit (matters when cross-compiling).
Q_STATIC_ASSERT(offsetof(Lookup, getter) == 0);

}

QT_END_NAMESPACE

#endif
