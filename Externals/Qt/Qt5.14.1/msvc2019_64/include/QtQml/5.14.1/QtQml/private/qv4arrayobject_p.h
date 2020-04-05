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
#ifndef QV4ARRAYOBJECT_H
#define QV4ARRAYOBJECT_H

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

struct ArrayCtor : FunctionObject {
    void init(QV4::ExecutionContext *scope);
};

}

struct ArrayCtor: FunctionObject
{
    V4_OBJECT2(ArrayCtor, FunctionObject)

    static ReturnedValue virtualCallAsConstructor(const FunctionObject *f, const Value *argv, int argc, const Value *newTarget);
    static ReturnedValue virtualCall(const FunctionObject *f, const Value *thisObject, const Value *argv, int argc);
};

struct ArrayPrototype: ArrayObject
{
    void init(ExecutionEngine *engine, Object *ctor);

    static ReturnedValue method_isArray(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_from(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_of(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_toString(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_toLocaleString(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_concat(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_copyWithin(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_entries(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_find(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_findIndex(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_join(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_pop(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_push(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_reverse(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_shift(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_slice(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_sort(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_splice(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_unshift(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_includes(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_indexOf(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_keys(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_lastIndexOf(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_every(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_fill(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_some(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_forEach(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_map(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_filter(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_reduce(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_reduceRight(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_values(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);

    // while this function is implemented here, it's the same for many other JS classes, so the corresponding JS function
    // is instantiated in the engine, and it can be added to any JS object through Object::addSymbolSpecies()
    static ReturnedValue method_get_species(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
};


} // namespace QV4

QT_END_NAMESPACE

#endif // QV4ECMAOBJECTS_P_H
