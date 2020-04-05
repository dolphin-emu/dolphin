/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtDBus module of the Qt Toolkit.
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

/*
    This file was originally created by qdbusxml2cpp version 0.8
    Command line was:
    qdbusxml2cpp -a dbusmenu ../../3rdparty/dbus-ifaces/dbus-menu.xml

    However it is maintained manually.

    It is also not part of the public API. This header file may change from
    version to version without notice, or even be removed.
*/

#ifndef DBUSMENUADAPTOR_H
#define DBUSMENUADAPTOR_H

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

#include <QtCore/QObject>
#include <QtDBus/QtDBus>
#include "qdbusmenutypes_p.h"

QT_BEGIN_NAMESPACE

/*
 * Adaptor class for interface com.canonical.dbusmenu
 */
class QDBusMenuAdaptor: public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.canonical.dbusmenu")
    Q_CLASSINFO("D-Bus Introspection", ""
"  <interface name=\"com.canonical.dbusmenu\">\n"
"    <property access=\"read\" type=\"u\" name=\"Version\">\n"
"    </property>\n"
"    <property access=\"read\" type=\"s\" name=\"TextDirection\">\n"
"    </property>\n"
"    <property access=\"read\" type=\"s\" name=\"Status\">\n"
"    </property>\n"
"    <property access=\"read\" type=\"as\" name=\"IconThemePath\">\n"
"    </property>\n"
"    <method name=\"GetLayout\">\n"
"      <annotation value=\"QDBusMenuLayoutItem\" name=\"org.qtproject.QtDBus.QtTypeName.Out1\"/>\n"
"      <arg direction=\"in\" type=\"i\" name=\"parentId\"/>\n"
"      <arg direction=\"in\" type=\"i\" name=\"recursionDepth\"/>\n"
"      <arg direction=\"in\" type=\"as\" name=\"propertyNames\"/>\n"
"      <arg direction=\"out\" type=\"u\" name=\"revision\"/>\n"
"      <arg direction=\"out\" type=\"(ia{sv}av)\" name=\"layout\"/>\n"
"    </method>\n"
"    <method name=\"GetGroupProperties\">\n"
"      <annotation value=\"QList&lt;int&gt;\" name=\"org.qtproject.QtDBus.QtTypeName.In0\"/>\n"
"      <annotation value=\"QDBusMenuItemList\" name=\"org.qtproject.QtDBus.QtTypeName.Out0\"/>\n"
"      <arg direction=\"in\" type=\"ai\" name=\"ids\"/>\n"
"      <arg direction=\"in\" type=\"as\" name=\"propertyNames\"/>\n"
"      <arg direction=\"out\" type=\"a(ia{sv})\" name=\"properties\"/>\n"
"    </method>\n"
"    <method name=\"GetProperty\">\n"
"      <arg direction=\"in\" type=\"i\" name=\"id\"/>\n"
"      <arg direction=\"in\" type=\"s\" name=\"name\"/>\n"
"      <arg direction=\"out\" type=\"v\" name=\"value\"/>\n"
"    </method>\n"
"    <method name=\"Event\">\n"
"      <arg direction=\"in\" type=\"i\" name=\"id\"/>\n"
"      <arg direction=\"in\" type=\"s\" name=\"eventId\"/>\n"
"      <arg direction=\"in\" type=\"v\" name=\"data\"/>\n"
"      <arg direction=\"in\" type=\"u\" name=\"timestamp\"/>\n"
"    </method>\n"
"    <method name=\"EventGroup\">\n"
"      <annotation value=\"QList&lt;QDBusMenuEvent&gt;\" name=\"org.qtproject.QtDBus.QtTypeName.In0\"/>\n"
"      <annotation value=\"QList&lt;int&gt;\" name=\"org.qtproject.QtDBus.QtTypeName.Out0\"/>\n"
"      <arg direction=\"in\" type=\"a(isvu)\" name=\"events\"/>\n"
"      <arg direction=\"out\" type=\"ai\" name=\"idErrors\"/>\n"
"    </method>\n"
"    <method name=\"AboutToShow\">\n"
"      <arg direction=\"in\" type=\"i\" name=\"id\"/>\n"
"      <arg direction=\"out\" type=\"b\" name=\"needUpdate\"/>\n"
"    </method>\n"
"    <method name=\"AboutToShowGroup\">\n"
"      <annotation value=\"QList&lt;int&gt;\" name=\"org.qtproject.QtDBus.QtTypeName.In0\"/>\n"
"      <annotation value=\"QList&lt;int&gt;\" name=\"org.qtproject.QtDBus.QtTypeName.Out0\"/>\n"
"      <annotation value=\"QList&lt;int&gt;\" name=\"org.qtproject.QtDBus.QtTypeName.Out1\"/>\n"
"      <arg direction=\"in\" type=\"ai\" name=\"ids\"/>\n"
"      <arg direction=\"out\" type=\"ai\" name=\"updatesNeeded\"/>\n"
"      <arg direction=\"out\" type=\"ai\" name=\"idErrors\"/>\n"
"    </method>\n"
"    <signal name=\"ItemsPropertiesUpdated\">\n"
"      <annotation value=\"QDBusMenuItemList\" name=\"org.qtproject.QtDBus.QtTypeName.In0\"/>\n"
"      <annotation value=\"QDBusMenuItemKeysList\" name=\"org.qtproject.QtDBus.QtTypeName.In1\"/>\n"
"      <arg direction=\"out\" type=\"a(ia{sv})\" name=\"updatedProps\"/>\n"
"      <arg direction=\"out\" type=\"a(ias)\" name=\"removedProps\"/>\n"
"    </signal>\n"
"    <signal name=\"LayoutUpdated\">\n"
"      <arg direction=\"out\" type=\"u\" name=\"revision\"/>\n"
"      <arg direction=\"out\" type=\"i\" name=\"parent\"/>\n"
"    </signal>\n"
"    <signal name=\"ItemActivationRequested\">\n"
"      <arg direction=\"out\" type=\"i\" name=\"id\"/>\n"
"      <arg direction=\"out\" type=\"u\" name=\"timestamp\"/>\n"
"    </signal>\n"
"  </interface>\n"
        "")
public:
    QDBusMenuAdaptor(QDBusPlatformMenu *topLevelMenu);
    virtual ~QDBusMenuAdaptor();

public: // PROPERTIES
    Q_PROPERTY(QString Status READ status)
    QString status() const;

    Q_PROPERTY(QString TextDirection READ textDirection)
    QString textDirection() const;

    Q_PROPERTY(uint Version READ version)
    uint version() const;

public Q_SLOTS: // METHODS
    bool AboutToShow(int id);
    QList<int> AboutToShowGroup(const QList<int> &ids, QList<int> &idErrors);
    void Event(int id, const QString &eventId, const QDBusVariant &data, uint timestamp);
    QList<int> EventGroup(const QDBusMenuEventList &events);
    QDBusMenuItemList GetGroupProperties(const QList<int> &ids, const QStringList &propertyNames);
    uint GetLayout(int parentId, int recursionDepth, const QStringList &propertyNames, QDBusMenuLayoutItem &layout);
    QDBusVariant GetProperty(int id, const QString &name);

Q_SIGNALS: // SIGNALS
    void ItemActivationRequested(int id, uint timestamp);
    void ItemsPropertiesUpdated(const QDBusMenuItemList &updatedProps, const QDBusMenuItemKeysList &removedProps);
    void LayoutUpdated(uint revision, int parent);

private:
    QDBusPlatformMenu *m_topLevelMenu;
};

QT_END_NAMESPACE

#endif // DBUSMENUADAPTOR_H
