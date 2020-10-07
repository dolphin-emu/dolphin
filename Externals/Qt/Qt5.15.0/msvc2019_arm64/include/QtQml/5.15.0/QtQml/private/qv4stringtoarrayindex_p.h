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

#ifndef QV4STRINGTOARRAYINDEX_P_H
#define QV4STRINGTOARRAYINDEX_P_H

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

#include <QtCore/private/qnumeric_p.h>
#include <QtCore/qstring.h>
#include <limits>

QT_BEGIN_NAMESPACE

namespace QV4 {

inline uint charToUInt(const QChar *ch) { return ch->unicode(); }
inline uint charToUInt(const char *ch) { return static_cast<unsigned char>(*ch); }

template <typename T>
uint stringToArrayIndex(const T *ch, const T *end)
{
    uint i = charToUInt(ch) - '0';
    if (i > 9)
        return std::numeric_limits<uint>::max();
    ++ch;
    // reject "01", "001", ...
    if (i == 0 && ch != end)
        return std::numeric_limits<uint>::max();

    while (ch < end) {
        uint x = charToUInt(ch) - '0';
        if (x > 9)
            return std::numeric_limits<uint>::max();
        if (mul_overflow(i, uint(10), &i) || add_overflow(i, x, &i)) // i = i * 10 + x
            return std::numeric_limits<uint>::max();
        ++ch;
    }
    return i;
}

inline uint stringToArrayIndex(const QString &str)
{
    return stringToArrayIndex(str.constData(), str.constData() + str.length());
}

} // namespace QV4

QT_END_NAMESPACE

#endif // QV4STRINGTOARRAYINDEX_P_H
