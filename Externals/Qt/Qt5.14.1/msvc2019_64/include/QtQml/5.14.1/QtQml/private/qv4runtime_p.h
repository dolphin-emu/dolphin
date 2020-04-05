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
#ifndef QMLJS_RUNTIME_H
#define QMLJS_RUNTIME_H

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
#include "qv4value_p.h"
#include "qv4math_p.h"
#include "qv4runtimeapi_p.h"
#include <QtCore/qnumeric.h>

QT_BEGIN_NAMESPACE

#undef QV4_COUNT_RUNTIME_FUNCTIONS

namespace QV4 {

#ifdef QV4_COUNT_RUNTIME_FUNCTIONS
class RuntimeCounters
{
public:
    RuntimeCounters();
    ~RuntimeCounters();

    static RuntimeCounters *instance;

    void count(const char *func);
    void count(const char *func, uint tag);
    void count(const char *func, uint tag1, uint tag2);

private:
    struct Data;
    Data *d;
};

#  define TRACE0() RuntimeCounters::instance->count(Q_FUNC_INFO);
#  define TRACE1(x) RuntimeCounters::instance->count(Q_FUNC_INFO, x.type());
#  define TRACE2(x, y) RuntimeCounters::instance->count(Q_FUNC_INFO, x.type(), y.type());
#else
#  define TRACE0()
#  define TRACE1(x)
#  define TRACE2(x, y)
#endif // QV4_COUNT_RUNTIME_FUNCTIONS

enum TypeHint {
    PREFERREDTYPE_HINT,
    NUMBER_HINT,
    STRING_HINT
};

struct Q_QML_PRIVATE_EXPORT RuntimeHelpers {
    static ReturnedValue objectDefaultValue(const Object *object, int typeHint);
    static ReturnedValue toPrimitive(const Value &value, TypeHint typeHint);
    static ReturnedValue ordinaryToPrimitive(ExecutionEngine *engine, const Object *object, String *typeHint);

    static double stringToNumber(const QString &s);
    static Heap::String *stringFromNumber(ExecutionEngine *engine, double number);
    static double toNumber(const Value &value);
    static void numberToString(QString *result, double num, int radix = 10);

    static Heap::String *convertToString(ExecutionEngine *engine, Value value, TypeHint = STRING_HINT);
    static Heap::Object *convertToObject(ExecutionEngine *engine, const Value &value);

    static Bool equalHelper(const Value &x, const Value &y);
    static Bool strictEqual(const Value &x, const Value &y);

    static ReturnedValue addHelper(ExecutionEngine *engine, const Value &left, const Value &right);
};


// type conversion and testing
inline ReturnedValue RuntimeHelpers::toPrimitive(const Value &value, TypeHint typeHint)
{
    if (!value.isObject())
        return value.asReturnedValue();
    return RuntimeHelpers::objectDefaultValue(&reinterpret_cast<const Object &>(value), typeHint);
}

inline double RuntimeHelpers::toNumber(const Value &value)
{
    return value.toNumber();
}
} // namespace QV4

QT_END_NAMESPACE

#endif // QMLJS_RUNTIME_H
