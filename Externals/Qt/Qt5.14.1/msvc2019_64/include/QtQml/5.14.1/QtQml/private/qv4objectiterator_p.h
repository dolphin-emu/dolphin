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
#ifndef QV4OBJECTITERATOR_H
#define QV4OBJECTITERATOR_H

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
#include "qv4object_p.h"

QT_BEGIN_NAMESPACE

namespace QV4 {

struct Q_QML_EXPORT ObjectIterator
{
    enum Flags {
        NoFlags = 0,
        EnumerableOnly = 0x1,
        WithSymbols = 0x2
    };

    ExecutionEngine *engine;
    Object *object;
    OwnPropertyKeyIterator *iterator = nullptr;
    uint flags;

    ObjectIterator(Scope &scope, const Object *o, uint flags)
    {
        engine = scope.engine;
        object = static_cast<Object *>(scope.alloc());
        this->flags = flags;
        object->setM(o ? o->m() : nullptr);
        if (o)
            iterator = object->ownPropertyKeys(object);
    }
    ~ObjectIterator()
    {
        delete iterator;
    }

    PropertyKey next(Property *pd = nullptr, PropertyAttributes *attributes = nullptr);
    ReturnedValue nextPropertyName(Value *value);
    ReturnedValue nextPropertyNameAsString(Value *value);
    ReturnedValue nextPropertyNameAsString();
};

namespace Heap {

#define ForInIteratorObjectMembers(class, Member) \
    Member(class, Pointer, Object *, object) \
    Member(class, Pointer, Object *, current) \
    Member(class, Pointer, Object *, target) \
    Member(class, NoMark, OwnPropertyKeyIterator *, iterator)

DECLARE_HEAP_OBJECT(ForInIteratorObject, Object) {
    void init(QV4::Object *o);
    Value workArea[2];

    static void markObjects(Heap::Base *that, MarkStack *markStack);
    void destroy();
};

}

struct ForInIteratorPrototype : Object
{
    V4_PROTOTYPE(iteratorPrototype)
    void init(ExecutionEngine *engine);

    static ReturnedValue method_next(const FunctionObject *b, const Value *thisObject, const Value *argv, int argc);
};

struct ForInIteratorObject: Object {
    V4_OBJECT2(ForInIteratorObject, Object)
    Q_MANAGED_TYPE(ForInIterator)
    V4_PROTOTYPE(forInIteratorPrototype)
    V4_NEEDS_DESTROY

    PropertyKey nextProperty() const;
};

inline
void Heap::ForInIteratorObject::init(QV4::Object *o)
{
    Object::init();
    if (!o)
        return;
    object.set(o->engine(), o->d());
    current.set(o->engine(), o->d());
    Scope scope(o);
    ScopedObject obj(scope);
    iterator = o->ownPropertyKeys(obj.getRef());
    target.set(o->engine(), obj->d());
}


}

QT_END_NAMESPACE

#endif
