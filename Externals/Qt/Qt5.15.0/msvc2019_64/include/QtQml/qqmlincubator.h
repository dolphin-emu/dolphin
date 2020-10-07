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

#ifndef QQMLINCUBATOR_H
#define QQMLINCUBATOR_H

#include <QtQml/qtqmlglobal.h>
#include <QtQml/qqmlerror.h>
#include <atomic>

QT_BEGIN_NAMESPACE


class QQmlEngine;
class QQmlPropertyData;
class QVariant;
using QVariantMap = QMap<QString, QVariant>;

class QQmlIncubatorPrivate;
class Q_QML_EXPORT QQmlIncubator
{
    Q_DISABLE_COPY(QQmlIncubator)
public:
    enum IncubationMode {
        Asynchronous,
        AsynchronousIfNested,
        Synchronous
    };
    enum Status {
        Null,
        Ready,
        Loading,
        Error
    };

    QQmlIncubator(IncubationMode = Asynchronous);
    virtual ~QQmlIncubator();

    void clear();
    void forceCompletion();

    bool isNull() const;
    bool isReady() const;
    bool isError() const;
    bool isLoading() const;

    QList<QQmlError> errors() const;

    IncubationMode incubationMode() const;

    Status status() const;

    QObject *object() const;

    void setInitialProperties(const QVariantMap &initialProperties);

protected:
    virtual void statusChanged(Status);
    virtual void setInitialState(QObject *);

private:
    friend class QQmlComponent;
    friend class QQmlEnginePrivate;
    friend class QQmlIncubatorPrivate;
    QQmlIncubatorPrivate *d;
};

class QQmlEnginePrivate;
class Q_QML_EXPORT QQmlIncubationController
{
    Q_DISABLE_COPY(QQmlIncubationController)
public:
    QQmlIncubationController();
    virtual ~QQmlIncubationController();

    QQmlEngine *engine() const;
    int incubatingObjectCount() const;

    void incubateFor(int msecs);
#if QT_DEPRECATED_SINCE(5, 15)
    QT_DEPRECATED_VERSION_X(5, 15, "Use the overload that takes a std::atomic<bool>")
    void incubateWhile(volatile bool *flag, int msecs=0);
#endif
    void incubateWhile(std::atomic<bool> *flag, int msecs = 0);

protected:
    virtual void incubatingObjectCountChanged(int);

private:
    friend class QQmlEngine;
    friend class QQmlEnginePrivate;
    friend class QQmlIncubatorPrivate;
    QQmlEnginePrivate *d;
};

QT_END_NAMESPACE

#endif // QQMLINCUBATOR_H
