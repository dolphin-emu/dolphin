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

#ifndef QQMLDEBUGSERVICE_H
#define QQMLDEBUGSERVICE_H

#include <QtCore/qobject.h>
#include <QtCore/qhash.h>

#include <private/qtqmlglobal_p.h>

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

QT_REQUIRE_CONFIG(qml_debug);

QT_BEGIN_NAMESPACE

class QJSEngine;

class QQmlDebugServicePrivate;
class Q_QML_PRIVATE_EXPORT QQmlDebugService : public QObject
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QQmlDebugService)

public:
    ~QQmlDebugService() override;

    const QString &name() const;
    float version() const;

    enum State { NotConnected, Unavailable, Enabled };
    State state() const;
    void setState(State newState);

    virtual void stateAboutToBeChanged(State) {}
    virtual void stateChanged(State) {}
    virtual void messageReceived(const QByteArray &) {}

    virtual void engineAboutToBeAdded(QJSEngine *engine) { emit attachedToEngine(engine); }
    virtual void engineAboutToBeRemoved(QJSEngine *engine) { emit detachedFromEngine(engine); }

    virtual void engineAdded(QJSEngine *) {}
    virtual void engineRemoved(QJSEngine *) {}

    static const QHash<int, QObject *> &objectsForIds();
    static int idForObject(QObject *);
    static QObject *objectForId(int id) { return objectsForIds().value(id); }

protected:
    explicit QQmlDebugService(const QString &, float version, QObject *parent = nullptr);

signals:
    void attachedToEngine(QJSEngine *);
    void detachedFromEngine(QJSEngine *);

    void messageToClient(const QString &name, const QByteArray &message);
    void messagesToClient(const QString &name, const QList<QByteArray> &messages);
};

QT_END_NAMESPACE

#endif // QQMLDEBUGSERVICE_H

