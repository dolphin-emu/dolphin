/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
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

#ifndef QV4CALLDATA_P_H
#define QV4CALLDATA_P_H

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

#include <private/qv4staticvalue_p.h>

QT_BEGIN_NAMESPACE

namespace QV4 {

struct CallData
{
    enum Offsets {
        Function = 0,
        Context = 1,
        Accumulator = 2,
        This = 3,
        NewTarget = 4,
        Argc = 5,

        LastOffset = Argc,
        OffsetCount = LastOffset + 1
    };

    StaticValue function;
    StaticValue context;
    StaticValue accumulator;
    StaticValue thisObject;
    StaticValue newTarget;
    StaticValue _argc;

    int argc() const {
        Q_ASSERT(_argc.isInteger());
        return _argc.int_32();
    }

    void setArgc(int argc) {
        Q_ASSERT(argc >= 0);
        _argc.setInt_32(argc);
    }

    inline ReturnedValue argument(int i) const {
        return i < argc() ? args[i].asReturnedValue()
                          : StaticValue::undefinedValue().asReturnedValue();
    }

    StaticValue args[1];

    static Q_DECL_CONSTEXPR int HeaderSize()
    {
        return offsetof(CallData, args) / sizeof(QV4::StaticValue);
    }

    template<typename Value>
    Value *argValues();

    template<typename Value>
    const Value *argValues() const;
};

Q_STATIC_ASSERT(std::is_standard_layout<CallData>::value);
Q_STATIC_ASSERT(offsetof(CallData, function   ) == CallData::Function    * sizeof(StaticValue));
Q_STATIC_ASSERT(offsetof(CallData, context    ) == CallData::Context     * sizeof(StaticValue));
Q_STATIC_ASSERT(offsetof(CallData, accumulator) == CallData::Accumulator * sizeof(StaticValue));
Q_STATIC_ASSERT(offsetof(CallData, thisObject ) == CallData::This        * sizeof(StaticValue));
Q_STATIC_ASSERT(offsetof(CallData, newTarget  ) == CallData::NewTarget   * sizeof(StaticValue));
Q_STATIC_ASSERT(offsetof(CallData, _argc      ) == CallData::Argc        * sizeof(StaticValue));
Q_STATIC_ASSERT(offsetof(CallData, args       ) == 6 * sizeof(StaticValue));

} // namespace QV4

QT_END_NAMESPACE

#endif // QV4CALLDATA_P_H
