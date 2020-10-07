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

#ifndef QV4DEBUGGING_H
#define QV4DEBUGGING_H

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
#include <private/qv4staticvalue_p.h>
#include <QtCore/qobject.h>

QT_BEGIN_NAMESPACE

namespace QV4 {
namespace Debugging {

#if !QT_CONFIG(qml_debug)

class Debugger
{
public:
    bool pauseAtNextOpportunity() const { return false; }
    void maybeBreakAtInstruction() {}
    void enteringFunction() {}
    void leavingFunction(const ReturnedValue &) {}
    void aboutToThrow() {}
};

#else

class Q_QML_EXPORT Debugger : public QObject
{
    Q_OBJECT

public:
    ~Debugger() override {}
    virtual bool pauseAtNextOpportunity() const = 0;
    virtual void maybeBreakAtInstruction() = 0;
    virtual void enteringFunction() = 0;
    virtual void leavingFunction(const ReturnedValue &retVal) = 0;
    virtual void aboutToThrow() = 0;
};

#endif // QT_NO_QML_DEBUGGING

} // namespace Debugging
} // namespace QV4

QT_END_NAMESPACE

#endif // QV4DEBUGGING_H
