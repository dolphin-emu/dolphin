/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtGui module of the Qt Toolkit.
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

#ifndef QCOLORTRC_P_H
#define QCOLORTRC_P_H

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

#include <QtGui/private/qtguiglobal_p.h>
#include "qcolortransferfunction_p.h"
#include "qcolortransfertable_p.h"

QT_BEGIN_NAMESPACE


// Defines an ICC TRC (Tone Reproduction Curve)
class Q_GUI_EXPORT QColorTrc
{
public:
    QColorTrc() noexcept : m_type(Type::Uninitialized)
    { }
    QColorTrc(const QColorTransferFunction &fun) : m_type(Type::Function), m_fun(fun)
    { }
    QColorTrc(const QColorTransferTable &table) : m_type(Type::Table), m_table(table)
    { }

    enum class Type {
        Uninitialized,
        Function,
        Table
    };

    bool isLinear() const
    {
        return m_type == Type::Uninitialized || (m_type == Type::Function && m_fun.isLinear());
    }
    bool isValid() const
    {
        return m_type != Type::Uninitialized;
    }
    float apply(float x) const
    {
        if (m_type == Type::Table)
            return m_table.apply(x);
        if (m_type == Type::Function)
            return m_fun.apply(x);
        return x;
    }
    float applyExtended(float x) const
    {
        if (x >= 0.0f && x <= 1.0f)
            return apply(x);
        if (m_type == Type::Function)
            return std::copysign(m_fun.apply(std::abs(x)), x);
        if (m_type == Type::Table)
            return x < 0.0f ? 0.0f : 1.0f;
        return x;
    }
    float applyInverse(float x) const
    {
        if (m_type == Type::Table)
            return m_table.applyInverse(x);
        if (m_type == Type::Function)
            return m_fun.inverted().apply(x);
        return x;
    }
    float applyInverseExtended(float x) const
    {
        if (x >= 0.0f && x <= 1.0f)
            return applyInverse(x);
        if (m_type == Type::Function)
            return std::copysign(applyInverse(x), x);
        if (m_type == Type::Table)
            return x < 0.0f ? 0.0f : 1.0f;
        return x;
    }

    friend inline bool operator!=(const QColorTrc &o1, const QColorTrc &o2);
    friend inline bool operator==(const QColorTrc &o1, const QColorTrc &o2);

    Type m_type;
    QColorTransferFunction m_fun;
    QColorTransferTable m_table;
};

inline bool operator!=(const QColorTrc &o1, const QColorTrc &o2)
{
    if (o1.m_type != o2.m_type)
        return true;
    if (o1.m_type == QColorTrc::Type::Function)
        return o1.m_fun != o2.m_fun;
    if (o1.m_type == QColorTrc::Type::Table)
        return o1.m_table != o2.m_table;
    return false;
}
inline bool operator==(const QColorTrc &o1, const QColorTrc &o2)
{
    return !(o1 != o2);
}

QT_END_NAMESPACE

#endif // QCOLORTRC
