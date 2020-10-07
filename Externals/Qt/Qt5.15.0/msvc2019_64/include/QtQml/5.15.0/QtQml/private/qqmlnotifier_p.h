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

#ifndef QQMLNOTIFIER_P_H
#define QQMLNOTIFIER_P_H

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

#include "qqmldata_p.h"
#include <QtCore/qmetaobject.h>
#include <private/qmetaobject_p.h>

QT_BEGIN_NAMESPACE

class QQmlNotifierEndpoint;
class Q_QML_PRIVATE_EXPORT QQmlNotifier
{
public:
    inline QQmlNotifier();
    inline ~QQmlNotifier();
    inline void notify();

    static void notify(QQmlData *ddata, int notifierIndex);

private:
    friend class QQmlData;
    friend class QQmlNotifierEndpoint;
    friend class QQmlThreadNotifierProxyObject;

    static void emitNotify(QQmlNotifierEndpoint *, void **a);
    QQmlNotifierEndpoint *endpoints = nullptr;
};

class QQmlEngine;
class QQmlNotifierEndpoint
{
    QQmlNotifierEndpoint  *next;
    QQmlNotifierEndpoint **prev;
public:
    // QQmlNotifierEndpoint can only invoke one of a set of pre-defined callbacks.
    // To add another callback, extend this enum and add the callback to the top
    // of qqmlnotifier.cpp.  Four bits are reserved for the callback, so there can
    // be up to 15 of them (0 is reserved).
    enum Callback {
        None = 0,
        QQmlBoundSignal = 1,
        QQmlJavaScriptExpressionGuard = 2,
        QQmlVMEMetaObjectEndpoint = 3
    };

    inline QQmlNotifierEndpoint(Callback callback);
    inline ~QQmlNotifierEndpoint();

    inline bool isConnected() const;
    inline bool isConnected(QObject *source, int sourceSignal) const;
    inline bool isConnected(QQmlNotifier *) const;

    void connect(QObject *source, int sourceSignal, QQmlEngine *engine, bool doNotify = true);
    inline void connect(QQmlNotifier *);
    inline void disconnect();

    inline bool isNotifying() const;
    inline void startNotifying(qintptr *originalSenderPtr);
    inline void stopNotifying(qintptr *originalSenderPtr);

    inline void cancelNotify();

    inline int signalIndex() const { return sourceSignal; }

    inline qintptr sender() const;
    inline void setSender(qintptr sender);

    inline QObject *senderAsObject() const;
    inline QQmlNotifier *senderAsNotifier() const;

private:
    friend class QQmlData;
    friend class QQmlNotifier;

    // Contains either the QObject*, or the QQmlNotifier* that this
    // endpoint is connected to.  While the endpoint is notifying, the
    // senderPtr points to another qintptr that contains this value.
    qintptr senderPtr;

    Callback callback:4;
    int needsConnectNotify:1;
    // The index is in the range returned by QObjectPrivate::signalIndex().
    // This is different from QMetaMethod::methodIndex().
    signed int sourceSignal:27;
};

QQmlNotifier::QQmlNotifier()
{
}

QQmlNotifier::~QQmlNotifier()
{
    QQmlNotifierEndpoint *endpoint = endpoints;
    while (endpoint) {
        QQmlNotifierEndpoint *n = endpoint;
        endpoint = n->next;
        n->setSender(0x0);
        n->next = nullptr;
        n->prev = nullptr;
        n->sourceSignal = -1;
    }
    endpoints = nullptr;
}

void QQmlNotifier::notify()
{
    void *args[] = { nullptr };
    if (endpoints) emitNotify(endpoints, args);
}

QQmlNotifierEndpoint::QQmlNotifierEndpoint(Callback callback)
: next(nullptr), prev(nullptr), senderPtr(0), callback(callback), needsConnectNotify(false), sourceSignal(-1)
{
}

QQmlNotifierEndpoint::~QQmlNotifierEndpoint()
{
    disconnect();
}

bool QQmlNotifierEndpoint::isConnected() const
{
    return prev != nullptr;
}

/*! \internal
    \a sourceSignal MUST be in the signal index range (see QObjectPrivate::signalIndex()).
    This is different from QMetaMethod::methodIndex().
*/
bool QQmlNotifierEndpoint::isConnected(QObject *source, int sourceSignal) const
{
    return this->sourceSignal != -1 && senderAsObject() == source &&
           this->sourceSignal == sourceSignal;
}

bool QQmlNotifierEndpoint::isConnected(QQmlNotifier *notifier) const
{
    return sourceSignal == -1 && senderAsNotifier() == notifier;
}

void QQmlNotifierEndpoint::connect(QQmlNotifier *notifier)
{
    disconnect();

    next = notifier->endpoints;
    if (next) { next->prev = &next; }
    notifier->endpoints = this;
    prev = &notifier->endpoints;
    setSender(qintptr(notifier));
}

void QQmlNotifierEndpoint::disconnect()
{
    // Remove from notifier chain before calling disconnectNotify(), so that that
    // QObject::receivers() returns the correct value in there
    if (next) next->prev = prev;
    if (prev) *prev = next;

    if (sourceSignal != -1) {
        QObject * const obj = senderAsObject();
        Q_ASSERT(obj);
        QObjectPrivate * const priv = QObjectPrivate::get(obj);
        if (needsConnectNotify)
            priv->disconnectNotify(QMetaObjectPrivate::signal(obj->metaObject(), sourceSignal));
    }

    setSender(0x0);
    next = nullptr;
    prev = nullptr;
    sourceSignal = -1;
}

/*!
Returns true if a notify is in progress.  This means that the signal or QQmlNotifier
that this endpoing is connected to has been triggered, but this endpoint's callback has not
yet been called.

An in progress notify can be cancelled by calling cancelNotify.
*/
bool QQmlNotifierEndpoint::isNotifying() const
{
    return senderPtr & 0x1;
}

void QQmlNotifierEndpoint::startNotifying(qintptr *originalSenderPtr)
{
    Q_ASSERT(*originalSenderPtr == 0);
    // Set the endpoint to notifying:
    // - Save the original senderPtr,
    *originalSenderPtr = senderPtr;
    // - Take a pointer of it,
    // - And assign that to the senderPtr, including a flag to signify "notifying".
    senderPtr = qintptr(originalSenderPtr) | 0x1;
}

void QQmlNotifierEndpoint::stopNotifying(qintptr *originalSenderPtr)
{
    // End of notifying, restore values
    Q_ASSERT((senderPtr & ~0x1) == qintptr(originalSenderPtr));
    senderPtr = *originalSenderPtr;
    *originalSenderPtr = 0;
}

/*!
Cancel any notifies that are in progress.
*/
void QQmlNotifierEndpoint::cancelNotify()
{
    if (isNotifying()) {
        auto *ptr = (qintptr *)(senderPtr & ~0x1);
        Q_ASSERT(ptr);
        senderPtr = *ptr;
        *ptr = 0;
    }
}

qintptr QQmlNotifierEndpoint::sender() const
{
    return isNotifying() ? *(qintptr *)(senderPtr & ~0x1) : senderPtr;
}

void QQmlNotifierEndpoint::setSender(qintptr sender)
{
    // If we're just notifying, we write through to the originalSenderPtr
    if (isNotifying())
        *(qintptr *)(senderPtr & ~0x1) = sender;
    else
        senderPtr = sender;
}

QObject *QQmlNotifierEndpoint::senderAsObject() const
{
    return (QObject *)(sender());
}

QQmlNotifier *QQmlNotifierEndpoint::senderAsNotifier() const
{
    return (QQmlNotifier *)(sender());
}

QT_END_NAMESPACE

#endif // QQMLNOTIFIER_P_H

