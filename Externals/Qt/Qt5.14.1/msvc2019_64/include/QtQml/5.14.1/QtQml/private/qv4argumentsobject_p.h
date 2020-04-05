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
#ifndef QV4ARGUMENTSOBJECTS_H
#define QV4ARGUMENTSOBJECTS_H

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

QT_BEGIN_NAMESPACE

namespace QV4 {

namespace Heap {

#define ArgumentsObjectMembers(class, Member) \
    Member(class, Pointer, CallContext *, context) \
    Member(class, NoMark, bool, fullyCreated) \
    Member(class, NoMark, uint, argCount) \
    Member(class, NoMark, quint64, mapped)

DECLARE_HEAP_OBJECT(ArgumentsObject, Object) {
    DECLARE_MARKOBJECTS(ArgumentsObject);
    enum {
        LengthPropertyIndex = 0,
        SymbolIteratorPropertyIndex = 1,
        CalleePropertyIndex = 2
    };
    void init(CppStackFrame *frame);
};

#define StrictArgumentsObjectMembers(class, Member)

DECLARE_HEAP_OBJECT(StrictArgumentsObject, Object) {
    enum {
        LengthPropertyIndex = 0,
        SymbolIteratorPropertyIndex = 1,
        CalleePropertyIndex = 2,
        CalleeSetterPropertyIndex = 3
    };
    void init(CppStackFrame *frame);
};

}

struct ArgumentsObject: Object {
    V4_OBJECT2(ArgumentsObject, Object)
    Q_MANAGED_TYPE(ArgumentsObject)

    Heap::CallContext *context() const { return d()->context; }
    bool fullyCreated() const { return d()->fullyCreated; }

    static bool isNonStrictArgumentsObject(Managed *m) {
        return m->vtable() == staticVTable();
    }

    static bool virtualDefineOwnProperty(Managed *m, PropertyKey id, const Property *desc, PropertyAttributes attrs);
    static ReturnedValue virtualGet(const Managed *m, PropertyKey id, const Value *receiver, bool *hasProperty);
    static bool virtualPut(Managed *m, PropertyKey id, const Value &value, Value *receiver);
    static bool virtualDeleteProperty(Managed *m, PropertyKey id);
    static PropertyAttributes virtualGetOwnProperty(const Managed *m, PropertyKey id, Property *p);
    static qint64 virtualGetLength(const Managed *m);
    static OwnPropertyKeyIterator *virtualOwnPropertyKeys(const Object *m, Value *target);

    void fullyCreate();

    // There's a slight hack here, as this limits the amount of mapped arguments to 64, but that should be
    // more than enough for all practical uses of arguments
    bool isMapped(uint arg) const {
        return arg < 64 && (d()->mapped & (1ull << arg));
    }

    void removeMapping(uint arg) {
        if (arg < 64)
            (d()->mapped &= ~(1ull << arg));
    }

};

struct StrictArgumentsObject : Object {
    V4_OBJECT2(StrictArgumentsObject, Object)
    Q_MANAGED_TYPE(ArgumentsObject)
};

}

QT_END_NAMESPACE

#endif

