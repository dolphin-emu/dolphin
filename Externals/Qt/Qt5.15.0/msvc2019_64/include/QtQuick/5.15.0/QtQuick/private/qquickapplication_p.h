/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQuick module of the Qt Toolkit.
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

#ifndef QQUICKAPPLICATION_P_H
#define QQUICKAPPLICATION_P_H

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
#include <QtGui/QFont>
#include <qqml.h>
#include <QtQml/private/qqmlglobal_p.h>
#include <private/qtquickglobal_p.h>
#include "../items/qquickscreen_p.h"

QT_BEGIN_NAMESPACE

class Q_AUTOTEST_EXPORT QQuickApplication : public QQmlApplication
{
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY activeChanged) // deprecated, use 'state' instead
    Q_PROPERTY(Qt::LayoutDirection layoutDirection READ layoutDirection NOTIFY layoutDirectionChanged)
    Q_PROPERTY(bool supportsMultipleWindows READ supportsMultipleWindows CONSTANT)
    Q_PROPERTY(Qt::ApplicationState state READ state NOTIFY stateChanged)
    Q_PROPERTY(QFont font READ font CONSTANT)
    Q_PROPERTY(QString displayName READ displayName WRITE setDisplayName NOTIFY displayNameChanged)
    Q_PROPERTY(QQmlListProperty<QQuickScreenInfo> screens READ screens NOTIFY screensChanged)

    QML_NAMED_ELEMENT(Application)
    QML_UNCREATABLE("Application is an abstract class.")

public:
    explicit QQuickApplication(QObject *parent = nullptr);
    virtual ~QQuickApplication();
    bool active() const;
    Qt::LayoutDirection layoutDirection() const;
    bool supportsMultipleWindows() const;
    Qt::ApplicationState state() const;
    QFont font() const;
    QQmlListProperty<QQuickScreenInfo> screens();
    QString displayName() const;
    void setDisplayName(const QString &displayName);

Q_SIGNALS:
    void activeChanged();
    void displayNameChanged();
    void layoutDirectionChanged();
    void stateChanged(Qt::ApplicationState state);
    void screensChanged();

private Q_SLOTS:
    void updateScreens();

private:
    Q_DISABLE_COPY(QQuickApplication)
    QVector<QQuickScreenInfo *> m_screens;
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QQuickApplication)

#endif // QQUICKAPPLICATION_P_H
