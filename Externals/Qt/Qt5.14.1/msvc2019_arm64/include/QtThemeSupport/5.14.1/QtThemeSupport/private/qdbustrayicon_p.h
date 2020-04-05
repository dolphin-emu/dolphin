/****************************************************************************
**
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


#ifndef QDBUSTRAYICON_H
#define QDBUSTRAYICON_H

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

#include <QtGui/private/qtguiglobal_p.h>

QT_REQUIRE_CONFIG(systemtrayicon);

#include <QIcon>
#include <QTemporaryFile>
#include <QTimer>
#include "QtGui/qpa/qplatformsystemtrayicon.h"
#include "private/qdbusmenuconnection_p.h"

QT_BEGIN_NAMESPACE

class QStatusNotifierItemAdaptor;
class QDBusMenuAdaptor;
class QDBusPlatformMenu;
class QXdgNotificationInterface;

class QDBusTrayIcon: public QPlatformSystemTrayIcon
{
    Q_OBJECT
    Q_PROPERTY(QString category READ category NOTIFY categoryChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString tooltip READ tooltip NOTIFY tooltipChanged)
    Q_PROPERTY(QString iconName READ iconName NOTIFY iconChanged)
    Q_PROPERTY(QIcon icon READ icon NOTIFY iconChanged)
    Q_PROPERTY(bool isRequestingAttention READ isRequestingAttention NOTIFY attention)
    Q_PROPERTY(QString attentionTitle READ attentionTitle NOTIFY attention)
    Q_PROPERTY(QString attentionMessage READ attentionMessage NOTIFY attention)
    Q_PROPERTY(QString attentionIconName READ attentionIconName NOTIFY attention)
    Q_PROPERTY(QIcon attentionIcon READ attentionIcon NOTIFY attention)
    Q_PROPERTY(QDBusPlatformMenu *menu READ menu NOTIFY menuChanged)

public:
    QDBusTrayIcon();

    virtual ~QDBusTrayIcon();

    QDBusMenuConnection * dBusConnection();

    void init() override;
    void cleanup() override;
    void updateIcon(const QIcon &icon) override;
    void updateToolTip(const QString &tooltip) override;
    void updateMenu(QPlatformMenu *menu) override;
    QPlatformMenu *createMenu() const override;
    void showMessage(const QString &title, const QString &msg,
                     const QIcon &icon, MessageIcon iconType, int msecs) override;

    bool isSystemTrayAvailable() const override;
    bool supportsMessages() const override { return true; }
    QRect geometry() const override { return QRect(); }

    QString category() const { return m_category; }
    QString status() const { return m_status; }
    QString tooltip() const { return m_tooltip; }

    QString iconName() const { return m_iconName; }
    const QIcon & icon() const { return m_icon; }

    bool isRequestingAttention() const { return m_attentionTimer.isActive(); }
    QString attentionTitle() const { return m_messageTitle; }
    QString attentionMessage() const { return m_message; }
    QString attentionIconName() const { return m_attentionIconName; }
    const QIcon & attentionIcon() const { return m_attentionIcon; }

    QString instanceId() const { return m_instanceId; }

    QDBusPlatformMenu *menu() { return m_menu; }

signals:
    void categoryChanged();
    void statusChanged(QString arg);
    void tooltipChanged();
    void iconChanged();
    void attention();
    void menuChanged();

private Q_SLOTS:
    void attentionTimerExpired();
    void actionInvoked(uint id, const QString &action);
    void notificationClosed(uint id, uint reason);
    void watcherServiceRegistered(const QString &serviceName);

private:
    void setStatus(const QString &status);
    QTemporaryFile *tempIcon(const QIcon &icon);

private:
    QDBusMenuConnection* m_dbusConnection;
    QStatusNotifierItemAdaptor *m_adaptor;
    QDBusMenuAdaptor *m_menuAdaptor;
    QDBusPlatformMenu *m_menu;
    QXdgNotificationInterface *m_notifier;
    QString m_instanceId;
    QString m_category;
    QString m_defaultStatus;
    QString m_status;
    QString m_tooltip;
    QString m_messageTitle;
    QString m_message;
    QIcon m_icon;
    QTemporaryFile *m_tempIcon;
    QString m_iconName;
    QIcon m_attentionIcon;
    QTemporaryFile *m_tempAttentionIcon;
    QString m_attentionIconName;
    QTimer m_attentionTimer;
    bool m_registered;
};

QT_END_NAMESPACE

#endif // QDBUSTRAYICON_H
