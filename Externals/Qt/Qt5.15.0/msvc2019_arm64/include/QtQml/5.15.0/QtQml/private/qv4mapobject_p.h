/****************************************************************************
**
** Copyright (C) 2018 Crimson AS <info@crimson.no>
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

#ifndef QV4MAPOBJECT_P_H
#define QV4MAPOBJECT_P_H

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
#include "qv4objectproto_p.h"
#include "qv4functionobject_p.h"
#include "qv4string_p.h"

QT_BEGIN_NAMESPACE

namespace QV4 {

class ESTable;

namespace Heap {

struct WeakMapCtor : FunctionObject {
    void init(QV4::ExecutionContext *scope);
};

struct MapCtor : WeakMapCtor {
    void init(QV4::ExecutionContext *scope);
};

struct MapObject : Object {
    static void markObjects(Heap::Base *that, MarkStack *markStack);
    void init();
    void destroy();
    void removeUnmarkedKeys();

    MapObject *nextWeakMap;
    ESTable *esTable;
    bool isWeakMap;
};

}

struct WeakMapCtor: FunctionObject
{
    V4_OBJECT2(WeakMapCtor, FunctionObject)

    static ReturnedValue construct(const FunctionObject *f, const Value *argv, int argc, const Value *, bool weakMap);

    static ReturnedValue virtualCallAsConstructor(const FunctionObject *f, const Value *argv, int argc, const Value *);
    static ReturnedValue virtualCall(const FunctionObject *f, const Value *thisObject, const Value *argv, int argc);
};

struct MapCtor : WeakMapCtor
{
    V4_OBJECT2(MapCtor, WeakMapCtor)

    static ReturnedValue virtualCallAsConstructor(const FunctionObject *f, const Value *argv, int argc, const Value *);
};

struct MapObject : Object
{
    V4_OBJECT2(MapObject, Object)
    V4_PROTOTYPE(mapPrototype)
    V4_NEEDS_DESTROY
};

struct WeakMapPrototype : Object
{
    void init(ExecutionEngine *engine, Object *ctor);

    static ReturnedValue method_delete(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_get(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_has(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_set(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
};

struct MapPrototype : WeakMapPrototype
{
    void init(ExecutionEngine *engine, Object *ctor);

    static ReturnedValue method_clear(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_delete(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_entries(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_forEach(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_get(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_has(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_keys(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_set(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_get_size(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_values(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
};


} // namespace QV4


QT_END_NAMESPACE

#endif // QV4MAPOBJECT_P_H

