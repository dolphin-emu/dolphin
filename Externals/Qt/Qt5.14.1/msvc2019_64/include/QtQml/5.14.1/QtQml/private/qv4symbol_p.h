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
#ifndef QV4_SYMBOL_H
#define QV4_SYMBOL_H

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

#include "qv4string_p.h"
#include "qv4functionobject_p.h"

QT_BEGIN_NAMESPACE


namespace QV4 {

namespace Heap {

struct SymbolCtor : FunctionObject {
    void init(QV4::ExecutionContext *scope);
};

struct Symbol : StringOrSymbol {
    void init(const QString &s);
};

#define SymbolObjectMembers(class, Member) \
    Member(class, Pointer, Symbol *, symbol)

DECLARE_HEAP_OBJECT(SymbolObject, Object) {
    DECLARE_MARKOBJECTS(SymbolObject);
    void init(const QV4::Symbol *s);
};

}

struct SymbolCtor : FunctionObject
{
    V4_OBJECT2(SymbolCtor, FunctionObject)

    static ReturnedValue virtualCall(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue virtualCallAsConstructor(const FunctionObject *, const Value *argv, int argc, const Value *newTarget);
    static ReturnedValue method_for(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_keyFor(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
};

struct SymbolPrototype : Object
{
    V4_PROTOTYPE(objectPrototype)
    void init(ExecutionEngine *engine, Object *ctor);

    static ReturnedValue method_toString(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_valueOf(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);

    static ReturnedValue method_symbolToPrimitive(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
};

struct Symbol : StringOrSymbol
{
    V4_MANAGED(Symbol, StringOrSymbol)
    Q_MANAGED_TYPE(Symbol)
    V4_INTERNALCLASS(Symbol)
    V4_NEEDS_DESTROY

    static Heap::Symbol *create(ExecutionEngine *e, const QString &s);

    QString descriptiveString() const;
};

struct SymbolObject : Object
{
    V4_OBJECT2(SymbolObject, Object)
    Q_MANAGED_TYPE(SymbolObject)
    V4_INTERNALCLASS(SymbolObject)
    V4_PROTOTYPE(symbolPrototype)
};

}

QT_END_NAMESPACE

#endif
