/****************************************************************************
**
** Copyright (C) 2016 Research In Motion.
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

#ifndef QQUICKFLICKABLEBEHAVIOR_H
#define QQUICKFLICKABLEBEHAVIOR_H

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

/* ### Platform specific flickable mechanics are defined either here, or in
       mkspec files. Long-term (QtQuick 3) Flickable needs to allow such
       mechanic details to be controlled via QML so that platforms can easily
       load custom behavior at QML compile time.
*/

// The maximum number of pixels a flick can overshoot
#ifndef QML_FLICK_OVERSHOOT
#define QML_FLICK_OVERSHOOT 150
#endif

// The number of samples to use in calculating the velocity of a flick
#ifndef QML_FLICK_SAMPLEBUFFER
#define QML_FLICK_SAMPLEBUFFER 3
#endif

// The number of samples to discard when calculating the flick velocity.
// Touch panels often produce inaccurate results as the finger is lifted.
#ifndef QML_FLICK_DISCARDSAMPLES
#define QML_FLICK_DISCARDSAMPLES 0
#endif

// The default maximum velocity of a flick.
#ifndef QML_FLICK_DEFAULTMAXVELOCITY
# define QML_FLICK_DEFAULTMAXVELOCITY 2500
#endif

// The default deceleration of a flick.
#ifndef QML_FLICK_DEFAULTDECELERATION
# define QML_FLICK_DEFAULTDECELERATION 1500
#endif

// How much faster to decelerate when overshooting
#ifndef QML_FLICK_OVERSHOOTFRICTION
#define QML_FLICK_OVERSHOOTFRICTION 8
#endif

// Multiflick acceleration minimum flick velocity threshold
#ifndef QML_FLICK_MULTIFLICK_THRESHOLD
#define QML_FLICK_MULTIFLICK_THRESHOLD 1250
#endif

// If the time (ms) between the last move and the release exceeds this, then velocity will be zero.
#ifndef QML_FLICK_VELOCITY_DECAY_TIME
#define QML_FLICK_VELOCITY_DECAY_TIME 50
#endif

// Multiflick acceleration minimum contentSize/viewSize ratio
#ifndef QML_FLICK_MULTIFLICK_RATIO
#define QML_FLICK_MULTIFLICK_RATIO 10
#endif

// Multiflick acceleration maximum velocity multiplier
#ifndef QML_FLICK_MULTIFLICK_MAXBOOST
#define QML_FLICK_MULTIFLICK_MAXBOOST 3.0
#endif

#endif //QQUICKFLICKABLEBEHAVIOR_H
