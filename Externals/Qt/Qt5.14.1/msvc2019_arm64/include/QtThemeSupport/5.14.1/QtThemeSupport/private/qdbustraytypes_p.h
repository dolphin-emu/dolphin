/****************************************************************************
**
** Copyright (C) 2009 Marco Martin <notmart@gmail.com>
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

#ifndef QDBUSTRAYTYPES_P_H
#define QDBUSTRAYTYPES_P_H

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

#include <QtGui/private/qtguiglobal_p.h>

QT_REQUIRE_CONFIG(systemtrayicon);

#include <QObject>
#include <QString>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QPixmap>

QT_BEGIN_NAMESPACE

// Custom message type to send icons across D-Bus
struct QXdgDBusImageStruct
{
    QXdgDBusImageStruct() { }
    QXdgDBusImageStruct(int w, int h)
        : width(w), height(h), data(width * height * 4, 0) { }
    int width;
    int height;
    QByteArray data;
};
Q_DECLARE_TYPEINFO(QXdgDBusImageStruct, Q_MOVABLE_TYPE);

typedef QVector<QXdgDBusImageStruct> QXdgDBusImageVector;

QXdgDBusImageVector iconToQXdgDBusImageVector(const QIcon &icon);

// Custom message type to send tooltips across D-Bus
struct QXdgDBusToolTipStruct
{
    QString icon;
    QXdgDBusImageVector image;
    QString title;
    QString subTitle;
};
Q_DECLARE_TYPEINFO(QXdgDBusToolTipStruct, Q_MOVABLE_TYPE);

const QDBusArgument &operator<<(QDBusArgument &argument, const QXdgDBusImageStruct &icon);
const QDBusArgument &operator>>(const QDBusArgument &argument, QXdgDBusImageStruct &icon);

const QDBusArgument &operator<<(QDBusArgument &argument, const QXdgDBusImageVector &iconVector);
const QDBusArgument &operator>>(const QDBusArgument &argument, QXdgDBusImageVector &iconVector);

const QDBusArgument &operator<<(QDBusArgument &argument, const QXdgDBusToolTipStruct &toolTip);
const QDBusArgument &operator>>(const QDBusArgument &argument, QXdgDBusToolTipStruct &toolTip);

QT_END_NAMESPACE

Q_DECLARE_METATYPE(QXdgDBusImageStruct)
Q_DECLARE_METATYPE(QXdgDBusImageVector)
Q_DECLARE_METATYPE(QXdgDBusToolTipStruct)

#endif // QDBUSTRAYTYPES_P_H
