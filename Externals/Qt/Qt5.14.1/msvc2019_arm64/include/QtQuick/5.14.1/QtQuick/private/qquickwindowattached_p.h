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

#ifndef QQUICKWINDOW_ATTACHED_P_H
#define QQUICKWINDOW_ATTACHED_P_H

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

#include <private/qtquickglobal_p.h>
#include <qqml.h>
#include <QWindow>

QT_BEGIN_NAMESPACE

class QQuickItem;
class QQuickWindow;

class Q_QUICK_PRIVATE_EXPORT QQuickWindowAttached : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QWindow::Visibility visibility READ visibility NOTIFY visibilityChanged)
    Q_PROPERTY(bool active READ isActive NOTIFY activeChanged)
    Q_PROPERTY(QQuickItem* activeFocusItem READ activeFocusItem NOTIFY activeFocusItemChanged)
    Q_PROPERTY(QQuickItem* contentItem READ contentItem NOTIFY contentItemChanged)
    Q_PROPERTY(int width READ width NOTIFY widthChanged)
    Q_PROPERTY(int height READ height NOTIFY heightChanged)
    Q_PROPERTY(QQuickWindow *window READ window NOTIFY windowChanged)

public:
    QQuickWindowAttached(QObject* attachee);

    QWindow::Visibility visibility() const;
    bool isActive() const;
    QQuickItem* activeFocusItem() const;
    QQuickItem* contentItem() const;
    int width() const;
    int height() const;
    QQuickWindow *window() const;

Q_SIGNALS:

    void visibilityChanged();
    void activeChanged();
    void activeFocusItemChanged();
    void contentItemChanged();
    void widthChanged();
    void heightChanged();
    void windowChanged();

protected Q_SLOTS:
    void windowChange(QQuickWindow*);

private:
    QQuickWindow* m_window;
    QQuickItem* m_attachee;
};

QT_END_NAMESPACE

#endif
