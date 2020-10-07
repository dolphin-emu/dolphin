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

#ifndef QQMLEXTENSIONINTERFACE_H
#define QQMLEXTENSIONINTERFACE_H

#include <QtQml/qtqmlglobal.h>
#include <QtCore/qobject.h>

QT_BEGIN_NAMESPACE


class QQmlEngine;

class Q_QML_EXPORT QQmlTypesExtensionInterface
{
public:
    virtual ~QQmlTypesExtensionInterface() = default;
    virtual void registerTypes(const char *uri) = 0;
};

class Q_QML_EXPORT QQmlExtensionInterface : public QQmlTypesExtensionInterface
{
public:
    ~QQmlExtensionInterface() override = default;
    virtual void initializeEngine(QQmlEngine *engine, const char *uri) = 0;
};

class Q_QML_EXPORT QQmlEngineExtensionInterface
{
public:
    virtual ~QQmlEngineExtensionInterface() = default;
    virtual void initializeEngine(QQmlEngine *engine, const char *uri) = 0;
};

#define QQmlTypesExtensionInterface_iid "org.qt-project.Qt.QQmlTypesExtensionInterface"
Q_DECLARE_INTERFACE(QQmlTypesExtensionInterface, "org.qt-project.Qt.QQmlTypesExtensionInterface/1.0")

// NOTE: When changing this to a new version and deciding to add backup code to
// continue to support the previous version, make sure to support both of these iids.
#define QQmlExtensionInterface_iid "org.qt-project.Qt.QQmlExtensionInterface/1.0"
#define QQmlExtensionInterface_iid_old "org.qt-project.Qt.QQmlExtensionInterface"

Q_DECLARE_INTERFACE(QQmlExtensionInterface, QQmlExtensionInterface_iid)

#define QQmlEngineExtensionInterface_iid "org.qt-project.Qt.QQmlEngineExtensionInterface"
Q_DECLARE_INTERFACE(QQmlEngineExtensionInterface, QQmlEngineExtensionInterface_iid)

QT_END_NAMESPACE

#endif // QQMLEXTENSIONINTERFACE_H
