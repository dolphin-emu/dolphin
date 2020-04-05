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

#ifndef QDBUSMENUTYPES_H
#define QDBUSMENUTYPES_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of other Qt classes.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QObject>
#include <QString>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QPixmap>

QT_BEGIN_NAMESPACE

class QDBusPlatformMenu;
class QDBusPlatformMenuItem;
class QDBusMenuItem;
typedef QVector<QDBusMenuItem> QDBusMenuItemList;
typedef QVector<QStringList> QDBusMenuShortcut;

class QDBusMenuItem
{
public:
    QDBusMenuItem() { }
    QDBusMenuItem(const QDBusPlatformMenuItem *item);

    static QDBusMenuItemList items(const QList<int> &ids, const QStringList &propertyNames);
    static QString convertMnemonic(const QString &label);
#ifndef QT_NO_SHORTCUT
    static QDBusMenuShortcut convertKeySequence(const QKeySequence &sequence);
#endif
    static void registerDBusTypes();

    int m_id;
    QVariantMap m_properties;
};
Q_DECLARE_TYPEINFO(QDBusMenuItem, Q_MOVABLE_TYPE);

const QDBusArgument &operator<<(QDBusArgument &arg, const QDBusMenuItem &item);
const QDBusArgument &operator>>(const QDBusArgument &arg, QDBusMenuItem &item);

class QDBusMenuItemKeys
{
public:

    int id;
    QStringList properties;
};
Q_DECLARE_TYPEINFO(QDBusMenuItemKeys, Q_MOVABLE_TYPE);

const QDBusArgument &operator<<(QDBusArgument &arg, const QDBusMenuItemKeys &keys);
const QDBusArgument &operator>>(const QDBusArgument &arg, QDBusMenuItemKeys &keys);

typedef QVector<QDBusMenuItemKeys> QDBusMenuItemKeysList;

class QDBusMenuLayoutItem
{
public:
    uint populate(int id, int depth, const QStringList &propertyNames, const QDBusPlatformMenu *topLevelMenu);
    void populate(const QDBusPlatformMenu *menu, int depth, const QStringList &propertyNames);
    void populate(const QDBusPlatformMenuItem *item, int depth, const QStringList &propertyNames);

    int m_id;
    QVariantMap m_properties;
    QVector<QDBusMenuLayoutItem> m_children;
};
Q_DECLARE_TYPEINFO(QDBusMenuLayoutItem, Q_MOVABLE_TYPE);

const QDBusArgument &operator<<(QDBusArgument &arg, const QDBusMenuLayoutItem &);
const QDBusArgument &operator>>(const QDBusArgument &arg, QDBusMenuLayoutItem &item);

typedef QVector<QDBusMenuLayoutItem> QDBusMenuLayoutItemList;

class QDBusMenuEvent
{
public:
    int m_id;
    QString m_eventId;
    QDBusVariant m_data;
    uint m_timestamp;
};
Q_DECLARE_TYPEINFO(QDBusMenuEvent, Q_MOVABLE_TYPE); // QDBusVariant is movable, even though it cannot
                                                    // be marked as such until Qt 6.

const QDBusArgument &operator<<(QDBusArgument &arg, const QDBusMenuEvent &ev);
const QDBusArgument &operator>>(const QDBusArgument &arg, QDBusMenuEvent &ev);

typedef QVector<QDBusMenuEvent> QDBusMenuEventList;

#ifndef QT_NO_DEBUG_STREAM
QDebug operator<<(QDebug d, const QDBusMenuItem &item);
QDebug operator<<(QDebug d, const QDBusMenuLayoutItem &item);
#endif

QT_END_NAMESPACE

Q_DECLARE_METATYPE(QDBusMenuItem)
Q_DECLARE_METATYPE(QDBusMenuItemList)
Q_DECLARE_METATYPE(QDBusMenuItemKeys)
Q_DECLARE_METATYPE(QDBusMenuItemKeysList)
Q_DECLARE_METATYPE(QDBusMenuLayoutItem)
Q_DECLARE_METATYPE(QDBusMenuLayoutItemList)
Q_DECLARE_METATYPE(QDBusMenuEvent)
Q_DECLARE_METATYPE(QDBusMenuEventList)
Q_DECLARE_METATYPE(QDBusMenuShortcut)

#endif
