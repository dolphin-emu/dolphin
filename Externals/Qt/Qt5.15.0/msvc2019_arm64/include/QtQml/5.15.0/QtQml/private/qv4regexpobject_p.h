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
#ifndef QV4REGEXPOBJECT_H
#define QV4REGEXPOBJECT_H

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

#include "qv4runtime_p.h"
#include "qv4engine_p.h"
#include "qv4context_p.h"
#include "qv4functionobject_p.h"
#include "qv4string_p.h"
#include "qv4managed_p.h"
#include "qv4property_p.h"
#include "qv4objectiterator_p.h"
#include "qv4regexp_p.h"

#include <QtCore/QString>
#include <QtCore/QHash>
#include <QtCore/QScopedPointer>
#include <cstdio>
#include <cassert>

QT_BEGIN_NAMESPACE

namespace QV4 {

namespace Heap {

#define RegExpObjectMembers(class, Member) \
    Member(class, Pointer, RegExp *, value)

DECLARE_HEAP_OBJECT(RegExpObject, Object) {
    DECLARE_MARKOBJECTS(RegExpObject)

    void init();
    void init(QV4::RegExp *value);
    void init(const QRegExp &re);
#if QT_CONFIG(regularexpression)
    void init(const QRegularExpression &re);
#endif
};

#define RegExpCtorMembers(class, Member) \
    Member(class, HeapValue, HeapValue, lastMatch) \
    Member(class, Pointer, String *, lastInput) \
    Member(class, NoMark, int, lastMatchStart) \
    Member(class, NoMark, int, lastMatchEnd)

DECLARE_HEAP_OBJECT(RegExpCtor, FunctionObject) {
    DECLARE_MARKOBJECTS(RegExpCtor);

    void init(QV4::ExecutionContext *scope);
    void clearLastMatch();
};

}

struct Q_QML_PRIVATE_EXPORT RegExpObject: Object {
    V4_OBJECT2(RegExpObject, Object)
    Q_MANAGED_TYPE(RegExpObject)
    V4_INTERNALCLASS(RegExpObject)
    V4_PROTOTYPE(regExpPrototype)

    // needs to be compatible with the flags in qv4compileddata_p.h
    enum Flags {
        RegExp_Global     = 0x01,
        RegExp_IgnoreCase = 0x02,
        RegExp_Multiline  = 0x04,
        RegExp_Unicode    = 0x08,
        RegExp_Sticky     = 0x10
    };

    enum {
        Index_LastIndex = 0,
        Index_ArrayIndex = Heap::ArrayObject::LengthPropertyIndex + 1,
        Index_ArrayInput = Index_ArrayIndex + 1
    };

    enum { NInlineProperties = 5 };


    void initProperties();

    int lastIndex() const {
        Q_ASSERT(internalClass()->verifyIndex(engine()->id_lastIndex()->propertyKey(), Index_LastIndex));
        return propertyData(Index_LastIndex)->toInt32();
    }
    void setLastIndex(int index) {
        Q_ASSERT(internalClass()->verifyIndex(engine()->id_lastIndex()->propertyKey(), Index_LastIndex));
        if (!internalClass()->propertyData[Index_LastIndex].isWritable()) {
            engine()->throwTypeError();
            return;
        }
        return setProperty(Index_LastIndex, Value::fromInt32(index));
    }

    QRegExp toQRegExp() const;
#if QT_CONFIG(regularexpression)
    QRegularExpression toQRegularExpression() const;
#endif
    QString toString() const;
    QString source() const
    {
        Scope scope(engine());
        ScopedValue s(scope, get(scope.engine->id_source()));
        return s->toQString();
    }

    Heap::RegExp *value() const { return d()->value; }
    uint flags() const { return d()->value->flags; }
    bool global() const { return d()->value->global(); }
    bool sticky() const { return d()->value->sticky(); }
    bool unicode() const { return d()->value->unicode(); }

    ReturnedValue builtinExec(ExecutionEngine *engine, const String *s);
};

struct RegExpCtor: FunctionObject
{
    V4_OBJECT2(RegExpCtor, FunctionObject)

    Value lastMatch() { return d()->lastMatch; }
    Heap::String *lastInput() { return d()->lastInput; }
    int lastMatchStart() { return d()->lastMatchStart; }
    int lastMatchEnd() { return d()->lastMatchEnd; }

    static ReturnedValue virtualCallAsConstructor(const FunctionObject *f, const Value *argv, int argc, const Value *);
    static ReturnedValue virtualCall(const FunctionObject *f, const Value *thisObject, const Value *argv, int argc);
};

struct RegExpPrototype: Object
{
    void init(ExecutionEngine *engine, Object *ctor);

    static ReturnedValue method_exec(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_get_flags(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_get_global(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_get_ignoreCase(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_match(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_get_multiline(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_replace(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_search(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_get_source(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_split(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_get_sticky(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_test(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_toString(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_get_unicode(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);

    // Web extension
    static ReturnedValue method_compile(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);

    // properties on the constructor, web extensions
    template <uint index>
    static ReturnedValue method_get_lastMatch_n(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_get_lastParen(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_get_input(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_get_leftContext(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_get_rightContext(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);

    static ReturnedValue execFirstMatch(const FunctionObject *b, const Value *thisObject, const Value *argv, int argc);

    static ReturnedValue exec(ExecutionEngine *engine, const Object *o, const String *s);
};

}

QT_END_NAMESPACE

#endif // QMLJS_OBJECTS_H
