/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtNetwork module of the Qt Toolkit.
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

#ifndef QNETWORKCONFIGMANAGER_P_H
#define QNETWORKCONFIGMANAGER_P_H

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

#include <QtNetwork/private/qtnetworkglobal_p.h>
#include "qnetworkconfigmanager.h"
#include "qnetworkconfiguration_p.h"

#include <QtCore/private/qfactoryloader_p.h>
#include <QtCore/qmutex.h>
#include <QtCore/qset.h>

#ifndef QT_NO_BEARERMANAGEMENT

QT_BEGIN_NAMESPACE

class QBearerEngine;
class QTimer;

class Q_NETWORK_EXPORT QNetworkConfigurationManagerPrivate : public QObject
{
    Q_OBJECT

public:
    QNetworkConfigurationManagerPrivate();
    virtual ~QNetworkConfigurationManagerPrivate();

    QNetworkConfiguration defaultConfiguration() const;
    QList<QNetworkConfiguration> allConfigurations(QNetworkConfiguration::StateFlags filter) const;
    QNetworkConfiguration configurationFromIdentifier(const QString &identifier) const;

    bool isOnline() const;

    QNetworkConfigurationManager::Capabilities capabilities() const;

    void performAsyncConfigurationUpdate();

    QList<QBearerEngine *> engines() const;

    void enablePolling();
    void disablePolling();

    void initialize();
    void cleanup();
public Q_SLOTS:
    void updateConfigurations();

    static void addPreAndPostRoutine();

Q_SIGNALS:
    void configurationAdded(const QNetworkConfiguration &config);
    void configurationRemoved(const QNetworkConfiguration &config);
    void configurationChanged(const QNetworkConfiguration &config);
    void configurationUpdateComplete();
    void onlineStateChanged(bool isOnline);

private Q_SLOTS:
    void configurationAdded(QNetworkConfigurationPrivatePointer ptr);
    void configurationRemoved(QNetworkConfigurationPrivatePointer ptr);
    void configurationChanged(QNetworkConfigurationPrivatePointer ptr);

    void pollEngines();


private:
    Q_INVOKABLE void startPolling();
    QTimer *pollTimer;
    QThread *bearerThread;

private:
    mutable QRecursiveMutex mutex;

    QFactoryLoader loader;
    QList<QBearerEngine *> sessionEngines;

    QSet<QString> onlineConfigurations;

    QSet<QBearerEngine *> pollingEngines;
    QSet<QBearerEngine *> updatingEngines;
    int forcedPolling;
    bool updating;

    bool firstUpdate;
};

Q_NETWORK_EXPORT QNetworkConfigurationManagerPrivate *qNetworkConfigurationManagerPrivate();

QT_END_NAMESPACE

#endif // QT_NO_BEARERMANAGEMENT

#endif // QNETWORKCONFMANAGER_P_H
