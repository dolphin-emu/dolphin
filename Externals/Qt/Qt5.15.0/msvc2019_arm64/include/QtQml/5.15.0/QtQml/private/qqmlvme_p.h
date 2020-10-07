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

#ifndef QQMLVME_P_H
#define QQMLVME_P_H

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

#include "qqmlerror.h"
#include <private/qbitfield_p.h>
#include <private/qrecursionwatcher_p.h>

#include <QtCore/QStack>
#include <QtCore/QString>
#include <QtCore/qelapsedtimer.h>
#include <QtCore/qcoreapplication.h>
#include <QtCore/qtypeinfo.h>

#include <private/qqmlengine_p.h>
#include <private/qfinitestack_p.h>

#include <atomic>

QT_BEGIN_NAMESPACE

class QObject;
class QJSValue;
class QQmlScriptData;
class QQmlContextData;

namespace QQmlVMETypes {
    struct List
    {
        List() : type(0) {}
        List(int t) : type(t) {}

        int type;
        QQmlListProperty<void> qListProperty;
    };
    struct State {
        enum Flag { Deferred = 0x00000001 };

        State() : flags(0), context(nullptr), instructionStream(nullptr) {}
        quint32 flags;
        QQmlContextData *context;
        const char *instructionStream;
        QBitField bindingSkipList;
    };
}
Q_DECLARE_TYPEINFO(QQmlVMETypes::List, Q_PRIMITIVE_TYPE  | Q_MOVABLE_TYPE);
template<>
class QTypeInfo<QQmlVMETypes::State> : public QTypeInfoMerger<QQmlVMETypes::State, QBitField> {}; //Q_DECLARE_TYPEINFO

class QQmlInstantiationInterrupt {
public:
    inline QQmlInstantiationInterrupt();
    // ### Qt 6: remove
    inline QQmlInstantiationInterrupt(volatile bool *runWhile, qint64 nsecs=0);
    inline QQmlInstantiationInterrupt(std::atomic<bool> *runWhile, qint64 nsecs = 0);
    inline QQmlInstantiationInterrupt(qint64 nsecs);

    inline void reset();
    inline bool shouldInterrupt() const;
private:
    enum Mode { None, Time, LegacyFlag, Flag }; // ### Qt 6: remove LegacyFlag
    Mode mode;
    QElapsedTimer timer;
    qint64 nsecs = 0;
    volatile bool *runWhileLegacy = nullptr; // ### Qt 6: remove
    std::atomic<bool> *runWhile = nullptr;
};

class Q_QML_PRIVATE_EXPORT QQmlVME
{
public:
    static void enableComponentComplete();
    static void disableComponentComplete();
    static bool componentCompleteEnabled();

private:
    static bool s_enableComponentComplete;
};

// Used to check that a QQmlVME that is interrupted mid-execution
// is still valid.  Checks all the objects and contexts have not been
// deleted.
//
// VME stands for Virtual Machine Execution. QML files used to
// be compiled to a byte code data structure that a virtual machine executed
// (for constructing the tree of QObjects and setting properties).
class QQmlVMEGuard
{
public:
    QQmlVMEGuard();
    ~QQmlVMEGuard();

    void guard(QQmlObjectCreator *);
    void clear();

    bool isOK() const;

private:
    int m_objectCount;
    QPointer<QObject> *m_objects;
    int m_contextCount;
    QQmlGuardedContextData *m_contexts;
};

QQmlInstantiationInterrupt::QQmlInstantiationInterrupt()
    : mode(None)
{
}

QQmlInstantiationInterrupt::QQmlInstantiationInterrupt(volatile bool *runWhile, qint64 nsecs)
    : mode(LegacyFlag), nsecs(nsecs), runWhileLegacy(runWhile)
{
}

QQmlInstantiationInterrupt::QQmlInstantiationInterrupt(std::atomic<bool> *runWhile, qint64 nsecs)
    : mode(Flag), nsecs(nsecs), runWhile(runWhile)
{
}

QQmlInstantiationInterrupt::QQmlInstantiationInterrupt(qint64 nsecs)
    : mode(Time), nsecs(nsecs)
{
}

void QQmlInstantiationInterrupt::reset()
{
    if (mode == Time || nsecs)
        timer.start();
}

bool QQmlInstantiationInterrupt::shouldInterrupt() const
{
    switch (mode) {
    case None:
        return false;
    case Time:
        return timer.nsecsElapsed() > nsecs;
    case LegacyFlag:
        return !*runWhileLegacy || (nsecs && timer.nsecsElapsed() > nsecs);
    case Flag:
        return !runWhile->load(std::memory_order_acquire) || (nsecs && timer.nsecsElapsed() > nsecs);
    }
    Q_UNREACHABLE();
    return false;
}

QT_END_NAMESPACE

#endif // QQMLVME_P_H
