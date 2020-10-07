/****************************************************************************
**
** Copyright (C) 2017 Intel Corporation.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtCore module of the Qt Toolkit.
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

#ifndef QRANDOM_P_H
#define QRANDOM_P_H

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

#include "qglobal_p.h"
#include <private/qsimd_p.h>

QT_BEGIN_NAMESPACE

enum QRandomGeneratorControl {
    UseSystemRNG = 1,
    SkipSystemRNG = 2,
    SkipHWRNG = 4,
    SetRandomData = 8,

    // 28 bits
    RandomDataMask = 0xfffffff0
};

enum RNGType {
    SystemRNG = 0,
    MersenneTwister = 1
};

#if defined(QT_BUILD_INTERNAL) && defined(QT_BUILD_CORE_LIB)
Q_CORE_EXPORT QBasicAtomicInteger<uint> qt_randomdevice_control = Q_BASIC_ATOMIC_INITIALIZER(0U);
#elif defined(QT_BUILD_INTERNAL)
extern Q_CORE_EXPORT QBasicAtomicInteger<uint> qt_randomdevice_control;
#else
static const struct {
    uint loadAcquire() const { return 0; }
} qt_randomdevice_control;
#endif



QT_END_NAMESPACE

#endif // QRANDOM_P_H
