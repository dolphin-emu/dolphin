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

#ifndef QDYNAMICDOCKWIDGET_P_H
#define QDYNAMICDOCKWIDGET_P_H

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

#include <QtWidgets/private/qtwidgetsglobal_p.h>
#include "QtWidgets/qstyleoption.h"
#include "private/qwidget_p.h"
#include "QtWidgets/qboxlayout.h"
#include "QtWidgets/qdockwidget.h"

#if QT_CONFIG(tabwidget)
#  include "QtWidgets/qtabwidget.h"
#endif

QT_REQUIRE_CONFIG(dockwidget);

QT_BEGIN_NAMESPACE

class QGridLayout;
class QWidgetResizeHandler;
class QDockWidgetTitleButton;
class QSpacerItem;
class QDockWidgetItem;

class QDockWidgetPrivate : public QWidgetPrivate
{
    Q_DECLARE_PUBLIC(QDockWidget)

    struct DragState {
        QPoint pressPos;
        bool dragging;
        QLayoutItem *widgetItem;
        bool ownWidgetItem;
        bool nca;
        bool ctrlDrag;
    };

public:
    void init();
    void _q_toggleView(bool); // private slot
    void _q_toggleTopLevel(); // private slot

    void updateButtons();

#if QT_CONFIG(tabwidget)
    QTabWidget::TabPosition tabPosition = QTabWidget::North;
#endif

    DragState *state = nullptr;

    QDockWidget::DockWidgetFeatures features = QDockWidget::DockWidgetClosable
        | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable;
    Qt::DockWidgetAreas allowedAreas = Qt::AllDockWidgetAreas;

    QFont font;

#ifndef QT_NO_ACTION
    QAction *toggleViewAction = nullptr;
#endif

//    QMainWindow *findMainWindow(QWidget *widget) const;
    QRect undockedGeometry;
    QString fixedWindowTitle;
    QString dockedWindowTitle;

    bool mousePressEvent(QMouseEvent *event);
    bool mouseDoubleClickEvent(QMouseEvent *event);
    bool mouseMoveEvent(QMouseEvent *event);
    bool mouseReleaseEvent(QMouseEvent *event);
    void setWindowState(bool floating, bool unplug = false, const QRect &rect = QRect());
    void nonClientAreaMouseEvent(QMouseEvent *event);
    void initDrag(const QPoint &pos, bool nca);
    void startDrag(bool group = true);
    void endDrag(bool abort = false);
    void moveEvent(QMoveEvent *event);
    void recalculatePressPos(QResizeEvent *event);

    void unplug(const QRect &rect);
    void plug(const QRect &rect);
    void setResizerActive(bool active);

    bool isAnimating() const;

private:
    QWidgetResizeHandler *resizer = nullptr;
};

class Q_WIDGETS_EXPORT QDockWidgetLayout : public QLayout
{
    Q_OBJECT
public:
    QDockWidgetLayout(QWidget *parent = nullptr);
    ~QDockWidgetLayout();
    void addItem(QLayoutItem *item) override;
    QLayoutItem *itemAt(int index) const override;
    QLayoutItem *takeAt(int index) override;
    int count() const override;

    QSize maximumSize() const override;
    QSize minimumSize() const override;
    QSize sizeHint() const override;

    QSize sizeFromContent(const QSize &content, bool floating) const;

    void setGeometry(const QRect &r) override;

    enum Role { Content, CloseButton, FloatButton, TitleBar, RoleCount };
    QWidget *widgetForRole(Role r) const;
    void setWidgetForRole(Role r, QWidget *w);
    QLayoutItem *itemForRole(Role r) const;

    QRect titleArea() const { return _titleArea; }

    int minimumTitleWidth() const;
    int titleHeight() const;
    void updateMaxSize();
    static bool wmSupportsNativeWindowDeco();
    bool nativeWindowDeco() const;
    bool nativeWindowDeco(bool floating) const;

    void setVerticalTitleBar(bool b);

    bool verticalTitleBar;

private:
    QVector<QLayoutItem*> item_list;
    QRect _titleArea;
};

/* The size hints of a QDockWidget will depend on whether it is docked or not.
   This layout item always returns the size hints as if the dock widget was docked. */

class QDockWidgetItem : public QWidgetItem
{
public:
    QDockWidgetItem(QDockWidget *dockWidget);
    QSize minimumSize() const override;
    QSize maximumSize() const override;
    QSize sizeHint() const override;

private:
    inline QLayoutItem *dockWidgetChildItem() const;
    inline QDockWidgetLayout *dockWidgetLayout() const;
};

inline QLayoutItem *QDockWidgetItem::dockWidgetChildItem() const
{
    if (QDockWidgetLayout *layout = dockWidgetLayout())
        return layout->itemForRole(QDockWidgetLayout::Content);
    return nullptr;
}

inline QDockWidgetLayout *QDockWidgetItem::dockWidgetLayout() const
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QWidget *w = const_cast<QDockWidgetItem*>(this)->widget();
#else
    QWidget *w = widget();
#endif
    if (w != nullptr)
        return qobject_cast<QDockWidgetLayout*>(w->layout());
    return nullptr;
}

QT_END_NAMESPACE

#endif // QDYNAMICDOCKWIDGET_P_H
