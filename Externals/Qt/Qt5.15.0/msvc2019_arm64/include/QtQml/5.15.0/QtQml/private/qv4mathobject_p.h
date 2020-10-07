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
#ifndef QV4MATHOBJECT_H
#define QV4MATHOBJECT_H

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

QT_BEGIN_NAMESPACE

namespace QV4 {

namespace Heap {

struct MathObject : Object {
    void init();
};

}

struct MathObject: Object
{
    V4_OBJECT2(MathObject, Object)
    Q_MANAGED_TYPE(MathObject)

    static ReturnedValue method_abs(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_acos(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_acosh(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_asin(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_asinh(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_atan(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_atanh(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_atan2(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_cbrt(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_ceil(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_clz32(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_cos(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_cosh(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_exp(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_expm1(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_floor(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_fround(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_hypot(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_imul(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_log(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_log10(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_log1p(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_log2(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_max(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_min(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_pow(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_random(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_round(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_sign(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_sin(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_sinh(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_sqrt(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_tan(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_tanh(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_trunc(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
};

}

QT_END_NAMESPACE

#endif // QMLJS_OBJECTS_H
