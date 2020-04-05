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
#ifndef QMLJS_MATH_H
#define QMLJS_MATH_H

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

#include <qglobal.h>

#include <private/qv4staticvalue_p.h>
#include <QtCore/qnumeric.h>
#include <QtCore/private/qnumeric_p.h>
#include <cmath>

#if defined(Q_CC_GNU)
#define QMLJS_READONLY __attribute((const))
#else
#define QMLJS_READONLY
#endif

QT_BEGIN_NAMESPACE

namespace QV4 {

static inline QMLJS_READONLY ReturnedValue add_int32(int a, int b)
{
    int result;
    if (Q_UNLIKELY(add_overflow(a, b, &result)))
        return StaticValue::fromDouble(static_cast<double>(a) + b).asReturnedValue();
    return StaticValue::fromInt32(result).asReturnedValue();
}

static inline QMLJS_READONLY ReturnedValue sub_int32(int a, int b)
{
    int result;
    if (Q_UNLIKELY(sub_overflow(a, b, &result)))
        return StaticValue::fromDouble(static_cast<double>(a) - b).asReturnedValue();
    return StaticValue::fromInt32(result).asReturnedValue();
}

static inline QMLJS_READONLY ReturnedValue mul_int32(int a, int b)
{
    int result;
    if (Q_UNLIKELY(mul_overflow(a, b, &result)))
        return StaticValue::fromDouble(static_cast<double>(a) * b).asReturnedValue();
    return StaticValue::fromInt32(result).asReturnedValue();
}

}

QT_END_NAMESPACE

#ifdef QMLJS_READONLY
#undef QMLJS_READONLY
#endif

#endif // QMLJS_MATH_H
