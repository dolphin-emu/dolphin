/****************************************************************************
**
** Copyright (C) 2016 Pelagicore AG
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

#ifndef QQMLLOGGINGCATEGORY_P_H
#define QQMLLOGGINGCATEGORY_P_H

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

#include <QtCore/qobject.h>
#include <QtCore/qstring.h>
#include <QtCore/qloggingcategory.h>

#include <QtQml/qqmlparserstatus.h>
#include <QtQml/qqml.h>

QT_BEGIN_NAMESPACE

class QQmlLoggingCategory : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)

    Q_PROPERTY(QString name READ name WRITE setName)
    Q_PROPERTY(DefaultLogLevel defaultLogLevel READ defaultLogLevel WRITE setDefaultLogLevel REVISION 12)
    QML_NAMED_ELEMENT(LoggingCategory)
    QML_ADDED_IN_MINOR_VERSION(8)

public:
    enum DefaultLogLevel {
        Debug = QtDebugMsg,
        Info = QtInfoMsg,
        Warning = QtWarningMsg,
        Critical = QtCriticalMsg,
        Fatal = QtFatalMsg
    };
    Q_ENUM(DefaultLogLevel);

    QQmlLoggingCategory(QObject *parent = nullptr);
    virtual ~QQmlLoggingCategory();

    DefaultLogLevel defaultLogLevel() const;
    void setDefaultLogLevel(DefaultLogLevel defaultLogLevel);
    QString name() const;
    void setName(const QString &name);

    QLoggingCategory *category() const;

    void classBegin() override;
    void componentComplete() override;

private:
    QByteArray m_name;
    QScopedPointer<QLoggingCategory> m_category;
    DefaultLogLevel m_defaultLogLevel = Debug;
    bool m_initialized;
};

QT_END_NAMESPACE

#endif // QQMLLOGGINGCATEGORY_H
