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

#ifndef QGRAPHICSPROXYWIDGET_P_H
#define QGRAPHICSPROXYWIDGET_P_H

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

#include <QtWidgets/private/qtwidgetsglobal_p.h>
#include "qgraphicsproxywidget.h"
#include "private/qgraphicswidget_p.h"

QT_REQUIRE_CONFIG(graphicsview);

QT_BEGIN_NAMESPACE

class Q_AUTOTEST_EXPORT QGraphicsProxyWidgetPrivate : public QGraphicsWidgetPrivate
{
    Q_DECLARE_PUBLIC(QGraphicsProxyWidget)
public:
    QGraphicsProxyWidgetPrivate();
    ~QGraphicsProxyWidgetPrivate();

    void init();
    void sendWidgetMouseEvent(QGraphicsSceneMouseEvent *event);
    void sendWidgetMouseEvent(QGraphicsSceneHoverEvent *event);
    void sendWidgetKeyEvent(QKeyEvent *event);
    void setWidget_helper(QWidget *widget, bool autoShow);

    QWidget *findFocusChild(QWidget *child, bool next) const;
    void removeSubFocusHelper(QWidget *widget, Qt::FocusReason reason);

    void _q_removeWidgetSlot();

    void embedSubWindow(QWidget *);
    void unembedSubWindow(QWidget *);

    bool isProxyWidget() const override;

    QPointer<QWidget> widget;
    QPointer<QWidget> lastWidgetUnderMouse;
    QPointer<QWidget> embeddedMouseGrabber;
    QWidget *dragDropWidget;
    Qt::DropAction lastDropAction;

    void updateWidgetGeometryFromProxy();
    void updateProxyGeometryFromWidget();

    void updateProxyInputMethodAcceptanceFromWidget();

    QPointF mapToReceiver(const QPointF &pos, const QWidget *receiver) const;

    enum ChangeMode {
        NoMode,
        ProxyToWidgetMode,
        WidgetToProxyMode
    };
    quint32 posChangeMode : 2;
    quint32 sizeChangeMode : 2;
    quint32 visibleChangeMode : 2;
    quint32 enabledChangeMode : 2;
    quint32 styleChangeMode : 2;
    quint32 paletteChangeMode : 2;
    quint32 tooltipChangeMode : 2;
    quint32 focusFromWidgetToProxy : 1;
    quint32 proxyIsGivingFocus : 1;
};

QT_END_NAMESPACE

#endif
