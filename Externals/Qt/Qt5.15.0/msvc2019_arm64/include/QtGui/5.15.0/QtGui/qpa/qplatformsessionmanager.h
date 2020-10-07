/****************************************************************************
**
** Copyright (C) 2013 Samuel Gaist <samuel.gaist@edeltech.ch>
** Copyright (C) 2013 Teo Mrnjavac <teo@kde.org>
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtGui module of the Qt Toolkit.
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

#ifndef QPLATFORMSESSIONMANAGER_H
#define QPLATFORMSESSIONMANAGER_H

//
//  W A R N I N G
//  -------------
//
// This file is part of the QPA API and is not meant to be used
// in applications. Usage of this API may make your code
// source and binary incompatible with future versions of Qt.
//

#include <QtGui/qtguiglobal.h>
#include <QtCore/qmetatype.h>
#include <QtCore/qnamespace.h>

#include <QtGui/qsessionmanager.h>

#ifndef QT_NO_SESSIONMANAGER

QT_BEGIN_NAMESPACE

class Q_GUI_EXPORT QPlatformSessionManager
{
public:
    Q_DISABLE_COPY_MOVE(QPlatformSessionManager)

    explicit QPlatformSessionManager(const QString &id, const QString &key);
    virtual ~QPlatformSessionManager();

    virtual QString sessionId() const;
    virtual QString sessionKey() const;

    virtual bool allowsInteraction();
    virtual bool allowsErrorInteraction();
    virtual void release();

    virtual void cancel();

    virtual void setRestartHint(QSessionManager::RestartHint restartHint);
    virtual QSessionManager::RestartHint restartHint() const;

    virtual void setRestartCommand(const QStringList &command);
    virtual QStringList restartCommand() const;
    virtual void setDiscardCommand(const QStringList &command);
    virtual QStringList discardCommand() const;

    virtual void setManagerProperty(const QString &name, const QString &value);
    virtual void setManagerProperty(const QString &name, const QStringList &value);

    virtual bool isPhase2() const;
    virtual void requestPhase2();

    void appCommitData();
    void appSaveState();

protected:
    QString m_sessionId;
    QString m_sessionKey;

private:
    QStringList m_restartCommand;
    QStringList m_discardCommand;
    QSessionManager::RestartHint m_restartHint;
};

QT_END_NAMESPACE

#endif // QT_NO_SESSIONMANAGER

#endif // QPLATFORMSESSIONMANAGER_H
