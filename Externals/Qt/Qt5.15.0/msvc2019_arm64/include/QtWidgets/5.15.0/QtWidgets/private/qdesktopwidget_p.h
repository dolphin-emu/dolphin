/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWidgets module of the Qt Toolkit.
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

#ifndef QDESKTOPWIDGET_P_H
#define QDESKTOPWIDGET_P_H

#include <QtWidgets/private/qtwidgetsglobal_p.h>
#include "QDesktopWidget"
#include "private/qwidget_p.h"

#include <QtCore/qalgorithms.h>
#include <QtGui/qscreen.h>

QT_BEGIN_NAMESPACE

class QDesktopScreenWidget : public QWidget {
    Q_OBJECT
public:
    explicit QDesktopScreenWidget(QScreen *screen, const QRect &geometry);

    int screenNumber() const;
    void setScreenGeometry(const QRect &geometry);

    QScreen *assignedScreen() const { return m_screen.data(); }
    QRect screenGeometry() const { return m_geometry; }

private:
    // The widget updates its screen and geometry automatically. We need to save them separately
    // to detect changes, and trigger the appropriate signals.
    const QPointer<QScreen> m_screen;
    QRect m_geometry;
};

class QDesktopWidgetPrivate : public QWidgetPrivate {
    Q_DECLARE_PUBLIC(QDesktopWidget)

public:
    ~QDesktopWidgetPrivate() { qDeleteAll(screens); }
    void _q_updateScreens();
    void _q_availableGeometryChanged();
    QDesktopScreenWidget *widgetForScreen(QScreen *qScreen) const;

    static bool isVirtualDesktop();

    static QRect geometry();
    static QSize size();
    static int width();
    static int height();

    static int numScreens();
    static int primaryScreen();

    static int screenNumber(const QWidget *widget = nullptr);
    static int screenNumber(const QPoint &);

    static QScreen *screen(int screenNo = -1);

    static const QRect screenGeometry(int screen = -1);
    static const QRect screenGeometry(const QWidget *widget);
    static const QRect screenGeometry(const QPoint &point)
    { return screenGeometry(screenNumber(point)); }

    static const QRect availableGeometry(int screen = -1);
    static const QRect availableGeometry(const QWidget *widget);
    static const QRect availableGeometry(const QPoint &point)
    { return availableGeometry(screenNumber(point)); }

    QList<QDesktopScreenWidget *> screens;
};

QT_END_NAMESPACE

#endif // QDESKTOPWIDGET_QPA_P_H
