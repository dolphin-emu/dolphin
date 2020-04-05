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

#ifndef QDYNAMICMAINWINDOWLAYOUT_P_H
#define QDYNAMICMAINWINDOWLAYOUT_P_H

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
#include "qmainwindow.h"

#include "QtWidgets/qlayout.h"
#if QT_CONFIG(tabbar)
#include "QtWidgets/qtabbar.h"
#include "QtGui/qpainter.h"
#include "QtGui/qevent.h"
#endif
#include "QtCore/qvector.h"
#include "QtCore/qset.h"
#include "QtCore/qbasictimer.h"
#include "private/qlayoutengine_p.h"
#include "private/qwidgetanimator_p.h"

#if QT_CONFIG(dockwidget)
#include "qdockarealayout_p.h"
#endif
#if QT_CONFIG(toolbar)
#include "qtoolbararealayout_p.h"
#endif

QT_REQUIRE_CONFIG(mainwindow);

QT_BEGIN_NAMESPACE

class QToolBar;
class QRubberBand;

template <typename Layout> // Make use of the "Curiously recurring template pattern"
class QMainWindowLayoutSeparatorHelper
{
    Layout *layout() { return static_cast<Layout *>(this); }
    const Layout *layout() const { return static_cast<const Layout *>(this); }
    QWidget *window() { return layout()->parentWidget(); }

public:
    Q_DISABLE_COPY_MOVE(QMainWindowLayoutSeparatorHelper)

    QMainWindowLayoutSeparatorHelper() = default;

    QList<int> hoverSeparator;
    QPoint hoverPos;

#if QT_CONFIG(dockwidget)

#if QT_CONFIG(cursor)
    QCursor separatorCursor(const QList<int> &path);
    void adjustCursor(const QPoint &pos);
    QCursor oldCursor;
    QCursor adjustedCursor;
    bool hasOldCursor = false;
    bool cursorAdjusted = false;
#endif // QT_CONFIG(cursor)

    QList<int> movingSeparator;
    QPoint movingSeparatorOrigin, movingSeparatorPos;
    QBasicTimer separatorMoveTimer;

    bool startSeparatorMove(const QPoint &pos);
    bool separatorMove(const QPoint &pos);
    bool endSeparatorMove(const QPoint &pos);
    bool windowEvent(QEvent *e);

#endif // QT_CONFIG(dockwidget)

};

#if QT_CONFIG(dockwidget)

#if QT_CONFIG(cursor)
template <typename Layout>
QCursor QMainWindowLayoutSeparatorHelper<Layout>::separatorCursor(const QList<int> &path)
{
    const QDockAreaLayoutInfo *info = layout()->dockAreaLayoutInfo()->info(path);
    Q_ASSERT(info != nullptr);
    if (path.size() == 1) { // is this the "top-level" separator which separates a dock area
                            // from the central widget?
        switch (path.first()) {
        case QInternal::LeftDock:
        case QInternal::RightDock:
            return Qt::SplitHCursor;
        case QInternal::TopDock:
        case QInternal::BottomDock:
            return Qt::SplitVCursor;
        default:
            break;
        }
    }

    // no, it's a splitter inside a dock area, separating two dock widgets

    return info->o == Qt::Horizontal ? Qt::SplitHCursor : Qt::SplitVCursor;
}

template <typename Layout>
void QMainWindowLayoutSeparatorHelper<Layout>::adjustCursor(const QPoint &pos)
{
    QWidget *w = layout()->window();
    hoverPos = pos;

    if (pos == QPoint(0, 0)) {
        if (!hoverSeparator.isEmpty())
            w->update(layout()->dockAreaLayoutInfo()->separatorRect(hoverSeparator));
        hoverSeparator.clear();

        if (cursorAdjusted) {
            cursorAdjusted = false;
            if (hasOldCursor)
                w->setCursor(oldCursor);
            else
                w->unsetCursor();
        }
    } else if (movingSeparator.isEmpty()) { // Don't change cursor when moving separator
        QList<int> pathToSeparator = layout()->dockAreaLayoutInfo()->findSeparator(pos);

        if (pathToSeparator != hoverSeparator) {
            if (!hoverSeparator.isEmpty())
                w->update(layout()->dockAreaLayoutInfo()->separatorRect(hoverSeparator));

            hoverSeparator = pathToSeparator;

            if (hoverSeparator.isEmpty()) {
                if (cursorAdjusted) {
                    cursorAdjusted = false;
                    if (hasOldCursor)
                        w->setCursor(oldCursor);
                    else
                        w->unsetCursor();
                }
            } else {
                w->update(layout()->dockAreaLayoutInfo()->separatorRect(hoverSeparator));
                if (!cursorAdjusted) {
                    oldCursor = w->cursor();
                    hasOldCursor = w->testAttribute(Qt::WA_SetCursor);
                }
                adjustedCursor = separatorCursor(hoverSeparator);
                w->setCursor(adjustedCursor);
                cursorAdjusted = true;
            }
        }
    }
}
#endif // QT_CONFIG(cursor)

template <typename Layout>
bool QMainWindowLayoutSeparatorHelper<Layout>::windowEvent(QEvent *event)
{
    QWidget *w = window();
    switch (event->type()) {
    case QEvent::Paint: {
        QPainter p(w);
        QRegion r = static_cast<QPaintEvent *>(event)->region();
        layout()->dockAreaLayoutInfo()->paintSeparators(&p, w, r, hoverPos);
        break;
    }

#if QT_CONFIG(cursor)
    case QEvent::HoverMove: {
        adjustCursor(static_cast<QHoverEvent *>(event)->pos());
        break;
    }

    // We don't want QWidget to call update() on the entire QMainWindow
    // on HoverEnter and HoverLeave, hence accept the event (return true).
    case QEvent::HoverEnter:
        return true;
    case QEvent::HoverLeave:
        adjustCursor(QPoint(0, 0));
        return true;
    case QEvent::ShortcutOverride: // when a menu pops up
        adjustCursor(QPoint(0, 0));
        break;
#endif // QT_CONFIG(cursor)

    case QEvent::MouseButtonPress: {
        QMouseEvent *e = static_cast<QMouseEvent *>(event);
        if (e->button() == Qt::LeftButton && startSeparatorMove(e->pos())) {
            // The click was on a separator, eat this event
            e->accept();
            return true;
        }
        break;
    }

    case QEvent::MouseMove: {
        QMouseEvent *e = static_cast<QMouseEvent *>(event);

#if QT_CONFIG(cursor)
        adjustCursor(e->pos());
#endif
        if (e->buttons() & Qt::LeftButton) {
            if (separatorMove(e->pos())) {
                // We're moving a separator, eat this event
                e->accept();
                return true;
            }
        }

        break;
    }

    case QEvent::MouseButtonRelease: {
        QMouseEvent *e = static_cast<QMouseEvent *>(event);
        if (endSeparatorMove(e->pos())) {
            // We've released a separator, eat this event
            e->accept();
            return true;
        }
        break;
    }

#if QT_CONFIG(cursor)
    case QEvent::CursorChange:
        // CursorChange events are triggered as mouse moves to new widgets even
        // if the cursor doesn't actually change, so do not change oldCursor if
        // the "changed" cursor has same shape as adjusted cursor.
        if (cursorAdjusted && adjustedCursor.shape() != w->cursor().shape()) {
            oldCursor = w->cursor();
            hasOldCursor = w->testAttribute(Qt::WA_SetCursor);

            // Ensure our adjusted cursor stays visible
            w->setCursor(adjustedCursor);
        }
        break;
#endif // QT_CONFIG(cursor)
    case QEvent::Timer:
        if (static_cast<QTimerEvent *>(event)->timerId() == separatorMoveTimer.timerId()) {
            // let's move the separators
            separatorMoveTimer.stop();
            if (movingSeparator.isEmpty())
                return true;
            if (movingSeparatorOrigin == movingSeparatorPos)
                return true;

            // when moving the separator, we need to update the previous position
            window()->update(layout()->dockAreaLayoutInfo()->separatorRegion());

            layout()->layoutState = layout()->savedState;
            layout()->dockAreaLayoutInfo()->separatorMove(movingSeparator, movingSeparatorOrigin,
                                                          movingSeparatorPos);
            movingSeparatorPos = movingSeparatorOrigin;
            return true;
        }
        break;
    default:
        break;
    }
    return false;
}

template <typename Layout>
bool QMainWindowLayoutSeparatorHelper<Layout>::startSeparatorMove(const QPoint &pos)
{
    movingSeparator = layout()->dockAreaLayoutInfo()->findSeparator(pos);

    if (movingSeparator.isEmpty())
        return false;

    layout()->savedState = layout()->layoutState;
    movingSeparatorPos = movingSeparatorOrigin = pos;

    return true;
}
template <typename Layout>
bool QMainWindowLayoutSeparatorHelper<Layout>::separatorMove(const QPoint &pos)
{
    if (movingSeparator.isEmpty())
        return false;
    movingSeparatorPos = pos;
    separatorMoveTimer.start(0, window());
    return true;
}
template <typename Layout>
bool QMainWindowLayoutSeparatorHelper<Layout>::endSeparatorMove(const QPoint &)
{
    if (movingSeparator.isEmpty())
        return false;
    movingSeparator.clear();
    layout()->savedState.clear();
    return true;
}

class QDockWidgetGroupWindow : public QWidget
{
    Q_OBJECT
public:
    explicit QDockWidgetGroupWindow(QWidget* parent = nullptr, Qt::WindowFlags f = nullptr)
        : QWidget(parent, f) {}
    QDockAreaLayoutInfo *layoutInfo() const;
#if QT_CONFIG(tabbar)
    const QDockAreaLayoutInfo *tabLayoutInfo() const;
    QDockWidget *activeTabbedDockWidget() const;
#endif
    void destroyOrHideIfEmpty();
    void adjustFlags();
    bool hasNativeDecos() const;

    bool hover(QLayoutItem *widgetItem, const QPoint &mousePos);
    void updateCurrentGapRect();
    void restore();
    void apply();

    QRect currentGapRect;
    QList<int> currentGapPos;

signals:
    void resized();

protected:
    bool event(QEvent *) override;
    void paintEvent(QPaintEvent*) override;

private:
    QSize m_removedFrameSize;
};

// This item will be used in the layout for the gap item. We cannot use QWidgetItem directly
// because QWidgetItem functions return an empty size for widgets that are are floating.
class QDockWidgetGroupWindowItem : public QWidgetItem
{
public:
    explicit QDockWidgetGroupWindowItem(QDockWidgetGroupWindow *parent) : QWidgetItem(parent) {}
    QSize minimumSize() const override { return lay()->minimumSize(); }
    QSize maximumSize() const override { return lay()->maximumSize(); }
    QSize sizeHint() const override { return lay()->sizeHint(); }

private:
    QLayout *lay() const { return const_cast<QDockWidgetGroupWindowItem *>(this)->widget()->layout(); }
};
#endif // QT_CONFIG(dockwidget)

/* This data structure represents the state of all the tool-bars and dock-widgets. It's value based
   so it can be easilly copied into a temporary variable. All operations are performed without moving
   any widgets. Only when we are sure we have the desired state, we call apply(), which moves the
   widgets.
*/

class QMainWindowLayoutState
{
public:
    QRect rect;
    QMainWindow *mainWindow;

    QMainWindowLayoutState(QMainWindow *win);

#if QT_CONFIG(toolbar)
    QToolBarAreaLayout toolBarAreaLayout;
#endif

#if QT_CONFIG(dockwidget)
    QDockAreaLayout dockAreaLayout;
#else
    QLayoutItem *centralWidgetItem;
    QRect centralWidgetRect;
#endif

    void apply(bool animated);
    void deleteAllLayoutItems();
    void deleteCentralWidgetItem();

    QSize sizeHint() const;
    QSize minimumSize() const;
    void fitLayout();

    QLayoutItem *itemAt(int index, int *x) const;
    QLayoutItem *takeAt(int index, int *x);
    QList<int> indexOf(QWidget *widget) const;
    QLayoutItem *item(const QList<int> &path);
    QRect itemRect(const QList<int> &path) const;
    QRect gapRect(const QList<int> &path) const; // ### get rid of this, use itemRect() instead

    bool contains(QWidget *widget) const;

    void setCentralWidget(QWidget *widget);
    QWidget *centralWidget() const;

    QList<int> gapIndex(QWidget *widget, const QPoint &pos) const;
    bool insertGap(const QList<int> &path, QLayoutItem *item);
    void remove(const QList<int> &path);
    void remove(QLayoutItem *item);
    void clear();
    bool isValid() const;

    QLayoutItem *plug(const QList<int> &path);
    QLayoutItem *unplug(const QList<int> &path, QMainWindowLayoutState *savedState = nullptr);

    void saveState(QDataStream &stream) const;
    bool checkFormat(QDataStream &stream);
    bool restoreState(QDataStream &stream, const QMainWindowLayoutState &oldState);
};

class Q_AUTOTEST_EXPORT QMainWindowLayout
    : public QLayout,
      public QMainWindowLayoutSeparatorHelper<QMainWindowLayout>
{
    Q_OBJECT

public:
    QMainWindowLayoutState layoutState, savedState;

    QMainWindowLayout(QMainWindow *mainwindow, QLayout *parentLayout);
    ~QMainWindowLayout();

    QMainWindow::DockOptions dockOptions;
    void setDockOptions(QMainWindow::DockOptions opts);

    // status bar

    QLayoutItem *statusbar;

#if QT_CONFIG(statusbar)
    QStatusBar *statusBar() const;
    void setStatusBar(QStatusBar *sb);
#endif

    // central widget

    QWidget *centralWidget() const;
    void setCentralWidget(QWidget *cw);

    // toolbars

#if QT_CONFIG(toolbar)
    void addToolBarBreak(Qt::ToolBarArea area);
    void insertToolBarBreak(QToolBar *before);
    void removeToolBarBreak(QToolBar *before);

    void addToolBar(Qt::ToolBarArea area, QToolBar *toolbar, bool needAddChildWidget = true);
    void insertToolBar(QToolBar *before, QToolBar *toolbar);
    Qt::ToolBarArea toolBarArea(const QToolBar *toolbar) const;
    bool toolBarBreak(QToolBar *toolBar) const;
    void getStyleOptionInfo(QStyleOptionToolBar *option, QToolBar *toolBar) const;
    void removeToolBar(QToolBar *toolbar);
    void toggleToolBarsVisible();
    void moveToolBar(QToolBar *toolbar, int pos);
#endif

    // dock widgets

#if QT_CONFIG(dockwidget)
    void setCorner(Qt::Corner corner, Qt::DockWidgetArea area);
    Qt::DockWidgetArea corner(Qt::Corner corner) const;
    void addDockWidget(Qt::DockWidgetArea area,
                       QDockWidget *dockwidget,
                       Qt::Orientation orientation);
    void splitDockWidget(QDockWidget *after,
                         QDockWidget *dockwidget,
                         Qt::Orientation orientation);
    Qt::DockWidgetArea dockWidgetArea(QWidget* widget) const;
    bool restoreDockWidget(QDockWidget *dockwidget);
#if QT_CONFIG(tabbar)
    void tabifyDockWidget(QDockWidget *first, QDockWidget *second);
    void raise(QDockWidget *widget);
    void setVerticalTabsEnabled(bool enabled);

    QDockAreaLayoutInfo *dockInfo(QWidget *w);
    bool _documentMode;
    bool documentMode() const;
    void setDocumentMode(bool enabled);

    QTabBar *getTabBar();
    QSet<QTabBar*> usedTabBars;
    QList<QTabBar*> unusedTabBars;
    bool verticalTabsEnabled;

    QWidget *getSeparatorWidget();
    QSet<QWidget*> usedSeparatorWidgets;
    QList<QWidget*> unusedSeparatorWidgets;
    int sep; // separator extent

#if QT_CONFIG(tabwidget)
    QTabWidget::TabPosition tabPositions[4];
    QTabWidget::TabShape _tabShape;

    QTabWidget::TabShape tabShape() const;
    void setTabShape(QTabWidget::TabShape tabShape);
    QTabWidget::TabPosition tabPosition(Qt::DockWidgetArea area) const;
    void setTabPosition(Qt::DockWidgetAreas areas, QTabWidget::TabPosition tabPosition);

    QDockWidgetGroupWindow *createTabbedDockWindow();
#endif // QT_CONFIG(tabwidget)
#endif // QT_CONFIG(tabbar)

    QDockAreaLayout *dockAreaLayoutInfo() { return &layoutState.dockAreaLayout; }
    void keepSize(QDockWidget *w);
#endif // QT_CONFIG(dockwidget)

    // save/restore

    enum VersionMarkers { // sentinel values used to validate state data
        VersionMarker = 0xff
    };
    void saveState(QDataStream &stream) const;
    bool restoreState(QDataStream &stream);

    // QLayout interface

    void addItem(QLayoutItem *item) override;
    void setGeometry(const QRect &r) override;
    QLayoutItem *itemAt(int index) const override;
    QLayoutItem *takeAt(int index) override;
    int count() const override;

    QSize sizeHint() const override;
    QSize minimumSize() const override;
    mutable QSize szHint;
    mutable QSize minSize;
    void invalidate() override;

    // animations

    QWidgetAnimator widgetAnimator;
    QList<int> currentGapPos;
    QRect currentGapRect;
    QWidget *pluggingWidget;
#if QT_CONFIG(rubberband)
    QPointer<QRubberBand> gapIndicator;
#endif
#if QT_CONFIG(dockwidget)
    QPointer<QDockWidgetGroupWindow> currentHoveredFloat; // set when dragging over a floating dock widget
    void setCurrentHoveredFloat(QDockWidgetGroupWindow *w);
#endif

    void hover(QLayoutItem *widgetItem, const QPoint &mousePos);
    bool plug(QLayoutItem *widgetItem);
    QLayoutItem *unplug(QWidget *widget, bool group = false);
    void revert(QLayoutItem *widgetItem);
    void paintDropIndicator(QPainter *p, QWidget *widget, const QRegion &clip);
    void applyState(QMainWindowLayoutState &newState, bool animate = true);
    void restore(bool keepSavedState = false);
    void animationFinished(QWidget *widget);

private Q_SLOTS:
    void updateGapIndicator();
#if QT_CONFIG(dockwidget)
#if QT_CONFIG(tabbar)
    void tabChanged();
    void tabMoved(int from, int to);
#endif
#endif
private:
#if QT_CONFIG(tabbar)
    void updateTabBarShapes();
#endif
};

#if QT_CONFIG(dockwidget) && !defined(QT_NO_DEBUG_STREAM)
class QDebug;
QDebug operator<<(QDebug debug, const QDockAreaLayout &layout);
QDebug operator<<(QDebug debug, const QMainWindowLayout *layout);
#endif

QT_END_NAMESPACE

#endif // QDYNAMICMAINWINDOWLAYOUT_P_H
