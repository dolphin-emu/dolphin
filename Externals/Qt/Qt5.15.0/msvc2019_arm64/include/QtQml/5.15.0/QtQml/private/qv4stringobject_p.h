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
#ifndef QV4STRINGOBJECT_P_H
#define QV4STRINGOBJECT_P_H

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
#include <QtCore/qnumeric.h>

QT_BEGIN_NAMESPACE

namespace QV4 {

namespace Heap {

#define StringObjectMembers(class, Member) \
    Member(class, Pointer, String *, string)

DECLARE_HEAP_OBJECT(StringObject, Object) {
    DECLARE_MARKOBJECTS(StringObject);

    enum {
        LengthPropertyIndex = 0
    };

    void init(bool /*don't init*/)
    { Object::init(); }
    void init();
    void init(const QV4::String *string);

    Heap::String *getIndex(uint index) const;
    uint length() const;
};

struct StringCtor : FunctionObject {
    void init(QV4::ExecutionContext *scope);
};

}

struct StringObject: Object {
    V4_OBJECT2(StringObject, Object)
    Q_MANAGED_TYPE(StringObject)
    V4_INTERNALCLASS(StringObject)
    V4_PROTOTYPE(stringPrototype)

    Heap::String *getIndex(uint index) const {
        return d()->getIndex(index);
    }
    uint length() const {
        return d()->length();
    }

    using Object::getOwnProperty;
protected:
    static bool virtualDeleteProperty(Managed *m, PropertyKey id);
    static OwnPropertyKeyIterator *virtualOwnPropertyKeys(const Object *m, Value *target);
    static PropertyAttributes virtualGetOwnProperty(const Managed *m, PropertyKey id, Property *p);
};

struct StringCtor: FunctionObject
{
    V4_OBJECT2(StringCtor, FunctionObject)

    static ReturnedValue virtualCallAsConstructor(const FunctionObject *f, const Value *argv, int argc, const Value *);
    static ReturnedValue virtualCall(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);

    static ReturnedValue method_fromCharCode(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_fromCodePoint(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_raw(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
};

struct StringPrototype: StringObject
{
    V4_PROTOTYPE(objectPrototype)
    void init(ExecutionEngine *engine, Object *ctor);

    static ReturnedValue method_toString(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_charAt(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_charCodeAt(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_codePointAt(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_concat(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_endsWith(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_indexOf(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_includes(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_lastIndexOf(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_localeCompare(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_match(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_normalize(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_padEnd(const FunctionObject *f, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_padStart(const FunctionObject *f, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_repeat(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_replace(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_search(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_slice(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_split(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_startsWith(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_substr(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_substring(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_toLowerCase(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_toLocaleLowerCase(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_toUpperCase(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_toLocaleUpperCase(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_trim(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_iterator(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
};

}

QT_END_NAMESPACE

#endif // QV4ECMAOBJECTS_P_H
