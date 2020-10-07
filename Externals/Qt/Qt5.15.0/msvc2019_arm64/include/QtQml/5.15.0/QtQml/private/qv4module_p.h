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
#ifndef QV4MODULE
#define QV4MODULE

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
#include "qv4context_p.h"

QT_BEGIN_NAMESPACE

namespace QV4 {

namespace Heap {

#define ModuleMembers(class, Member) \
    Member(class, NoMark, ExecutableCompilationUnit *, unit) \
    Member(class, Pointer, CallContext *, scope) \
    Member(class, HeapValue, HeapValue, self) \
    Member(class, NoMark, bool, evaluated)

DECLARE_EXPORTED_HEAP_OBJECT(Module, Object) {
    DECLARE_MARKOBJECTS(Module)

    void init(ExecutionEngine *engine, ExecutableCompilationUnit *moduleUnit);
};

}

struct Q_QML_EXPORT Module : public Object {
    V4_OBJECT2(Module, Object)

    void evaluate();
    const Value *resolveExport(PropertyKey key) const;

    static ReturnedValue virtualGet(const Managed *m, PropertyKey id, const Value *receiver, bool *hasProperty);
    static PropertyAttributes virtualGetOwnProperty(const Managed *m, PropertyKey id, Property *p);
    static bool virtualHasProperty(const Managed *m, PropertyKey id);
    static bool virtualPreventExtensions(Managed *);
    static bool virtualDefineOwnProperty(Managed *, PropertyKey, const Property *, PropertyAttributes);
    static bool virtualPut(Managed *, PropertyKey, const Value &, Value *);
    static bool virtualDeleteProperty(Managed *m, PropertyKey id);
    static OwnPropertyKeyIterator *virtualOwnPropertyKeys(const Object *m, Value *target);
    static Heap::Object *virtualGetPrototypeOf(const Managed *);
    static bool virtualSetPrototypeOf(Managed *, const Object *proto);
    static bool virtualIsExtensible(const Managed *);
};

}

QT_END_NAMESPACE

#endif // QV4MODULE
