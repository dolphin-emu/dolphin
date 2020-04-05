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

#ifndef QANIMATIONJOBUTIL_P_H
#define QANIMATIONJOBUTIL_P_H

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

#include <type_traits>

QT_REQUIRE_CONFIG(qml_animation);

// SelfDeletable is used for self-destruction detection along with
// ACTION_IF_DELETED and RETURN_IF_DELETED macros. While using, the objects
// under test should have a member m_selfDeletable of type SelfDeletable
struct SelfDeletable {
    ~SelfDeletable() {
        if (m_wasDeleted)
            *m_wasDeleted = true;
    }
    bool *m_wasDeleted = nullptr;
};

// \param p pointer to object under test, which should have a member m_selfDeletable of type SelfDeletable
// \param func statements or functions that to be executed under test.
// \param action post process if p was deleted under test.
#define ACTION_IF_DELETED(p, func, action) \
{ \
    static_assert(std::is_same<decltype((p)->m_selfDeletable), SelfDeletable>::value, "m_selfDeletable must be SelfDeletable");\
    bool *prevWasDeleted = (p)->m_selfDeletable.m_wasDeleted; \
    bool wasDeleted = false; \
    (p)->m_selfDeletable.m_wasDeleted = &wasDeleted; \
    {func;} \
    if (wasDeleted) { \
        if (prevWasDeleted) \
            *prevWasDeleted = true; \
        {action;} \
    } \
    (p)->m_selfDeletable.m_wasDeleted = prevWasDeleted; \
}

#define RETURN_IF_DELETED(func) \
ACTION_IF_DELETED(this, func, return)

#endif // QANIMATIONJOBUTIL_P_H
