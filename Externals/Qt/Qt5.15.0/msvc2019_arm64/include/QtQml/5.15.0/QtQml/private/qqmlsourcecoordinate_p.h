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

#ifndef QQMLSOURCECOORDINATE_P_H
#define QQMLSOURCECOORDINATE_P_H

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

#include <QtCore/qglobal.h>

#include <limits>

QT_BEGIN_NAMESPACE

// These methods are needed because in some public methods we historically interpret -1 as the
// invalid line or column, even though all the lines and columns are 1-based. Also, the different
// integer ranges may turn certain large values into invalid ones on conversion.

template<typename From, typename To>
To qmlConvertSourceCoordinate(From n);

template<>
inline quint16 qmlConvertSourceCoordinate<int, quint16>(int n)
{
    return (n > 0 && n <= int(std::numeric_limits<quint16>::max())) ? quint16(n) : 0;
}

template<>
inline quint32 qmlConvertSourceCoordinate<int, quint32>(int n)
{
    return n > 0 ? quint32(n) : 0u;
}

// TODO: In Qt6, change behavior and make the invalid coordinate 0 for the following two methods.

template<>
inline int qmlConvertSourceCoordinate<quint16, int>(quint16 n)
{
    return (n == 0u) ? -1 : int(n);
}

template<>
inline int qmlConvertSourceCoordinate<quint32, int>(quint32 n)
{
    return (n == 0u || n > quint32(std::numeric_limits<int>::max())) ? -1 : int(n);
}

QT_END_NAMESPACE

#endif // QQMLSOURCECOORDINATE_P_H
