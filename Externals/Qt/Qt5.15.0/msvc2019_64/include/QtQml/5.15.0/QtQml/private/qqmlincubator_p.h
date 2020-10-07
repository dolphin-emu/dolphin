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

#ifndef QQMLINCUBATOR_P_H
#define QQMLINCUBATOR_P_H

#include "qqmlincubator.h"

#include <private/qintrusivelist_p.h>
#include <private/qqmlvme_p.h>
#include <private/qrecursionwatcher_p.h>
#include <private/qqmlengine_p.h>
#include <private/qqmlcontext_p.h>

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

QT_BEGIN_NAMESPACE

class RequiredProperties;

class QQmlIncubator;
class Q_QML_PRIVATE_EXPORT QQmlIncubatorPrivate : public QQmlEnginePrivate::Incubator
{
public:
    QQmlIncubatorPrivate(QQmlIncubator *q, QQmlIncubator::IncubationMode m);
    ~QQmlIncubatorPrivate();

    inline static QQmlIncubatorPrivate *get(QQmlIncubator *incubator) { return incubator->d; }

    QQmlIncubator *q;

    QQmlIncubator::Status calculateStatus() const;
    void changeStatus(QQmlIncubator::Status);
    QQmlIncubator::Status status;

    QQmlIncubator::IncubationMode mode;
    bool isAsynchronous;

    QList<QQmlError> errors;

    enum Progress { Execute, Completing, Completed };
    Progress progress;

    QPointer<QObject> result;
    QQmlGuardedContextData rootContext;
    QQmlEnginePrivate *enginePriv;
    QQmlRefPointer<QV4::ExecutableCompilationUnit> compilationUnit;
    QScopedPointer<QQmlObjectCreator> creator;
    int subComponentToCreate;
    QQmlVMEGuard vmeGuard;

    QExplicitlySharedDataPointer<QQmlIncubatorPrivate> waitingOnMe;
    typedef QQmlEnginePrivate::Incubator QIPBase;
    QIntrusiveList<QIPBase, &QIPBase::nextWaitingFor> waitingFor;

    QRecursionNode recursion;
    QVariantMap initialProperties;

    void clear();

    void forceCompletion(QQmlInstantiationInterrupt &i);
    void incubate(QQmlInstantiationInterrupt &i);
    RequiredProperties &requiredProperties();
    bool hadRequiredProperties() const;
};

QT_END_NAMESPACE

#endif // QQMLINCUBATOR_P_H

