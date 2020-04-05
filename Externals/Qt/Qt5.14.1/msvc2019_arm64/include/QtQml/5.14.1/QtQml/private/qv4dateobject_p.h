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
#ifndef QV4DATEOBJECT_P_H
#define QV4DATEOBJECT_P_H

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
#include <QtCore/private/qnumeric_p.h>

QT_BEGIN_NAMESPACE

class QDateTime;

namespace QV4 {

namespace Heap {

struct DateObject : Object {
    void init()
    {
        Object::init();
        date = qt_qnan();
    }

    void init(const Value &date)
    {
        Object::init();
        this->date = date.toNumber();
    }
    void init(const QDateTime &date);
    void init(const QTime &time);

    double date;
};


struct DateCtor : FunctionObject {
    void init(QV4::ExecutionContext *scope);
};

}

struct DateObject: Object {
    V4_OBJECT2(DateObject, Object)
    Q_MANAGED_TYPE(DateObject)
    V4_PROTOTYPE(datePrototype)


    double date() const { return d()->date; }
    void setDate(double date) { d()->date = date; }

    Q_QML_PRIVATE_EXPORT QDateTime toQDateTime() const;
};

template<>
inline const DateObject *Value::as() const {
    return isManaged() && m()->internalClass->vtable->type == Managed::Type_DateObject ? static_cast<const DateObject *>(this) : nullptr;
}

struct DateCtor: FunctionObject
{
    V4_OBJECT2(DateCtor, FunctionObject)

    static ReturnedValue virtualCallAsConstructor(const FunctionObject *, const Value *argv, int argc, const Value *);
    static ReturnedValue virtualCall(const FunctionObject *f, const Value *thisObject, const Value *argv, int);
};

struct DatePrototype: Object
{
    V4_PROTOTYPE(objectPrototype)

    void init(ExecutionEngine *engine, Object *ctor);

    static double getThisDate(ExecutionEngine *v4, const Value *thisObject);

    static ReturnedValue method_parse(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_UTC(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_now(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);

    static ReturnedValue method_toString(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_toDateString(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_toTimeString(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_toLocaleString(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_toLocaleDateString(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_toLocaleTimeString(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_valueOf(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_getTime(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_getYear(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_getFullYear(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_getUTCFullYear(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_getMonth(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_getUTCMonth(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_getDate(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_getUTCDate(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_getDay(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_getUTCDay(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_getHours(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_getUTCHours(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_getMinutes(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_getUTCMinutes(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_getSeconds(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_getUTCSeconds(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_getMilliseconds(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_getUTCMilliseconds(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_getTimezoneOffset(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_setTime(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_setMilliseconds(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_setUTCMilliseconds(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_setSeconds(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_setUTCSeconds(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_setMinutes(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_setUTCMinutes(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_setHours(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_setUTCHours(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_setDate(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_setUTCDate(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_setMonth(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_setUTCMonth(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_setYear(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_setFullYear(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_setUTCFullYear(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_toUTCString(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_toISOString(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_toJSON(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_symbolToPrimitive(const FunctionObject *f, const Value *thisObject, const Value *, int);

    static void timezoneUpdated(ExecutionEngine *e);
};

}

QT_END_NAMESPACE

#endif // QV4ECMAOBJECTS_P_H
