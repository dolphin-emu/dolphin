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

#ifndef QMENU_P_H
#define QMENU_P_H

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
#if QT_CONFIG(menubar)
#include "QtWidgets/qmenubar.h"
#endif
#include "QtWidgets/qstyleoption.h"
#include "QtCore/qdatetime.h"
#include "QtCore/qmap.h"
#include "QtCore/qhash.h"
#include "QtCore/qbasictimer.h"
#include "private/qwidget_p.h"

#include <qpa/qplatformmenu.h>

#include <functional>

QT_REQUIRE_CONFIG(menu);

QT_BEGIN_NAMESPACE

static inline int pick(Qt::Orientation o, const QPoint &pos)
{ return o == Qt::Horizontal ? pos.x() : pos.y(); }

static inline int pick(Qt::Orientation o, const QSize &size)
{ return o == Qt::Horizontal ? size.width() : size.height(); }

static inline int &rpick(Qt::Orientation o, QPoint &pos)
{ return o == Qt::Horizontal ? pos.rx() : pos.ry(); }

static inline int &rpick(Qt::Orientation o, QSize &size)
{ return o == Qt::Horizontal ? size.rwidth() : size.rheight(); }

static inline QSizePolicy::Policy pick(Qt::Orientation o, const QSizePolicy &policy)
{ return o == Qt::Horizontal ? policy.horizontalPolicy() : policy.verticalPolicy(); }

static inline int perp(Qt::Orientation o, const QPoint &pos)
{ return o == Qt::Vertical ? pos.x() : pos.y(); }

static inline int perp(Qt::Orientation o, const QSize &size)
{ return o == Qt::Vertical ? size.width() : size.height(); }

static inline int &rperp(Qt::Orientation o, QPoint &pos)
{ return o == Qt::Vertical ? pos.rx() : pos.ry(); }

static inline int &rperp(Qt::Orientation o, QSize &size)
{ return o == Qt::Vertical ? size.rwidth() : size.rheight(); }

static inline int pick(Qt::Orientation o, const QMargins &m)
{ return o == Qt::Horizontal ? (m.left() + m.right()) : (m.top() + m.bottom()); }

static inline int perp(Qt::Orientation o, const QMargins &m)
{ return o == Qt::Vertical ? (m.left() + m.right()) : (m.top() + m.bottom()); }

class QTornOffMenu;
class QEventLoop;

template <typename T>
class QSetValueOnDestroy
{
public:
    QSetValueOnDestroy(T &toSet, T value)
        : toSet(toSet)
        , value(value)
    { }

    ~QSetValueOnDestroy() { toSet = value; }
private:
    T &toSet;
    T value;
};

class QMenuSloppyState
{
    Q_DISABLE_COPY_MOVE(QMenuSloppyState)
public:
    QMenuSloppyState()
        : m_enabled(false)
        , m_uni_directional(false)
        , m_select_other_actions(false)
        , m_use_reset_action(true)
    { }

    ~QMenuSloppyState() { reset(); }

    void initialize(QMenu *menu)
    {
        m_menu = menu;
        m_uni_directional = menu->style()->styleHint(QStyle::SH_Menu_SubMenuUniDirection, nullptr, menu);
        m_uni_dir_fail_at_count = short(menu->style()->styleHint(QStyle::SH_Menu_SubMenuUniDirectionFailCount, nullptr, menu));
        m_select_other_actions = menu->style()->styleHint(QStyle::SH_Menu_SubMenuSloppySelectOtherActions, nullptr , menu);
        m_timeout = short(menu->style()->styleHint(QStyle::SH_Menu_SubMenuSloppyCloseTimeout));
        m_discard_state_when_entering_parent = menu->style()->styleHint(QStyle::SH_Menu_SubMenuResetWhenReenteringParent);
        m_dont_start_time_on_leave = menu->style()->styleHint(QStyle::SH_Menu_SubMenuDontStartSloppyOnLeave);
        reset();
    }

    void reset();
    bool enabled() const { return m_enabled; }

    enum MouseEventResult {
        EventIsProcessed,
        EventShouldBePropagated,
        EventDiscardsSloppyState
    };

    void startTimer()
    {
        if (m_enabled)
            m_time.start(m_timeout, m_menu);
    }

    void startTimerIfNotRunning()
    {
        if (!m_time.isActive())
            startTimer();
    }

    void stopTimer()
    {
        m_time.stop();
    }

    void enter();
    void childEnter();

    void leave();
    void childLeave();

    static qreal slope(const QPointF &p1, const QPointF &p2)
    {
        const QPointF slope = p2 - p1;
        if (qFuzzyIsNull(slope.x()))
            return 9999;
        return slope.y() / slope.x();
    }

    bool checkSlope(qreal oldS, qreal newS, bool wantSteeper)
    {
        if (wantSteeper)
            return oldS <= newS;
        return newS <= oldS;
    }

    MouseEventResult processMouseEvent(const QPointF &mousePos, QAction *resetAction, QAction *currentAction)
    {
        if (m_parent)
            m_parent->stopTimer();

        if (!m_enabled)
            return EventShouldBePropagated;

        startTimerIfNotRunning();

        if (!m_sub_menu) {
            reset();
            return EventShouldBePropagated;
        }

        QSetValueOnDestroy<bool> setFirstMouse(m_first_mouse, false);
        QSetValueOnDestroy<QPointF> setPreviousPoint(m_previous_point, mousePos);

        if (resetAction && resetAction->isSeparator()) {
            m_reset_action = nullptr;
            m_use_reset_action = true;
        } else if (m_reset_action != resetAction) {
            if (m_use_reset_action && resetAction) {
                const QList<QAction *> actions = m_menu->actions();
                const int resetIdx  = actions.indexOf(resetAction);
                const int originIdx = actions.indexOf(m_origin_action);
                if (resetIdx > -1 && originIdx > -1 && qAbs(resetIdx - originIdx) > 1)
                    m_use_reset_action = false;
            }
            m_reset_action = resetAction;
        }

        if (m_action_rect.contains(mousePos)) {
            startTimer();
            return currentAction == m_menu->menuAction() ? EventIsProcessed : EventShouldBePropagated;
        }

        if (m_uni_directional && !m_first_mouse && resetAction != m_origin_action) {
            bool left_to_right = m_menu->layoutDirection() == Qt::LeftToRight;
            QRect sub_menu_rect = m_sub_menu->geometry();
            QPoint sub_menu_top =
                    left_to_right? sub_menu_rect.topLeft() : sub_menu_rect.topRight();
            QPoint sub_menu_bottom =
                    left_to_right? sub_menu_rect.bottomLeft() : sub_menu_rect.bottomRight();
            qreal prev_slope_top = slope(m_previous_point, sub_menu_top);
            qreal prev_slope_bottom = slope(m_previous_point, sub_menu_bottom);

            qreal current_slope_top = slope(mousePos, sub_menu_top);
            qreal current_slope_bottom = slope(mousePos, sub_menu_bottom);

            bool slopeTop = checkSlope(prev_slope_top, current_slope_top, sub_menu_top.y() < mousePos.y());
            bool slopeBottom = checkSlope(prev_slope_bottom, current_slope_bottom, sub_menu_bottom.y() > mousePos.y());
            bool rightDirection = false;
            int mouseDir = int(m_previous_point.y() - mousePos.y());
            if (mouseDir >= 0) {
                rightDirection = rightDirection || slopeTop;
            }
            if (mouseDir <= 0) {
                rightDirection = rightDirection || slopeBottom;
            }

            if (m_uni_dir_discarded_count >= m_uni_dir_fail_at_count && !rightDirection) {
                m_uni_dir_discarded_count = 0;
                return EventDiscardsSloppyState;
            }

            if (!rightDirection)
                m_uni_dir_discarded_count++;
            else
                m_uni_dir_discarded_count = 0;

        }

        return m_select_other_actions ? EventShouldBePropagated : EventIsProcessed;
    }

    void setSubMenuPopup(const QRect &actionRect, QAction *resetAction, QMenu *subMenu);
    bool hasParentActiveDelayTimer() const;
    void timeout();
    int timeForTimeout() const { return m_timeout; }

    bool isTimerId(int timerId) const { return m_time.timerId() == timerId; }
    QMenu *subMenu() const { return m_sub_menu; }

private:
    QMenu *m_menu = nullptr;
    QAction *m_reset_action = nullptr;
    QAction *m_origin_action = nullptr;
    QRectF m_action_rect;
    QPointF m_previous_point;
    QPointer<QMenu> m_sub_menu;
    QMenuSloppyState *m_parent = nullptr;
    QBasicTimer m_time;
    short m_uni_dir_discarded_count = 0;
    short m_uni_dir_fail_at_count = 0;
    short m_timeout = 0;
    bool m_init_guard = false;
    bool m_first_mouse = true;

    bool m_enabled : 1;
    bool m_uni_directional : 1;
    bool m_select_other_actions : 1;
    bool m_discard_state_when_entering_parent : 1;
    bool m_dont_start_time_on_leave : 1;
    bool m_use_reset_action : 1;
};

class QMenuPrivate : public QWidgetPrivate
{
    Q_DECLARE_PUBLIC(QMenu)
public:
    using PositionFunction = std::function<QPoint(const QSize &)>;

    QMenuPrivate() :
        itemsDirty(false),
        hasCheckableItems(false),
        lastContextMenu(false),
        collapsibleSeparators(true),
        toolTipsVisible(false),
        delayedPopupGuard(false),
        hasReceievedEnter(false),
        hasHadMouse(false),
        aboutToHide(false),
        tearoff(false),
        tornoff(false),
        tearoffHighlighted(false),
        doChildEffects(false)
    { }

    ~QMenuPrivate()
    {
        delete scroll;
        if (!platformMenu.isNull() && !platformMenu->parent())
            delete platformMenu.data();
    }
    void init();
    QPlatformMenu *createPlatformMenu();
    void setPlatformMenu(QPlatformMenu *menu);
    void syncPlatformMenu();
    void copyActionToPlatformItem(const QAction *action, QPlatformMenuItem *item);
    QPlatformMenuItem *insertActionInPlatformMenu(const QAction *action, QPlatformMenuItem *beforeItem);

#ifdef Q_OS_MACOS
    void moveWidgetToPlatformItem(QWidget *w, QPlatformMenuItem* item);
#endif

    static QMenuPrivate *get(QMenu *m) { return m->d_func(); }
    int scrollerHeight() const;

    bool isContextMenu() const;

    //item calculations
    QRect actionRect(QAction *) const;

    mutable QVector<QRect> actionRects;
    mutable QHash<QAction *, QWidget *> widgetItems;
    void updateActionRects() const;
    void updateActionRects(const QRect &screen) const;
    QRect popupGeometry() const;
    QRect popupGeometry(int screen) const;
    bool useFullScreenForPopup() const;
    int getLastVisibleAction() const;
    void popup(const QPoint &p, QAction *atAction, PositionFunction positionFunction = {});
    QAction *exec(const QPoint &p, QAction *action, PositionFunction positionFunction = {});

    //selection
    static QMenu *mouseDown;
    QPoint mousePopupPos;

    QAction *currentAction = nullptr;
#ifdef QT_KEYPAD_NAVIGATION
    QAction *selectAction = nullptr;
    QAction *cancelAction = nullptr;
#endif
    struct DelayState {
        DelayState()
        { }
        void initialize(QMenu *parent)
        {
            this->parent = parent;
        }

        void start(int timeout, QAction *toStartAction)
        {
            if (timer.isActive() && toStartAction == action)
                return;
            action = toStartAction;
            timer.start(timeout,parent);
        }
        void stop()
        {
            action = nullptr;
            timer.stop();
        }

        QMenu *parent = nullptr;
        QAction *action = nullptr;
        QBasicTimer timer;
    } delayState;
    enum SelectionReason {
        SelectedFromKeyboard,
        SelectedFromElsewhere
    };
    QWidget *topCausedWidget() const;
    QAction *actionAt(QPoint p) const;
    void setFirstActionActive();
    void setCurrentAction(QAction *, int popup = -1, SelectionReason reason = SelectedFromElsewhere, bool activateFirst = false);
    void popupAction(QAction *, int, bool);
    void setSyncAction();

    //scrolling support
    struct QMenuScroller {
        enum ScrollLocation { ScrollStay, ScrollBottom, ScrollTop, ScrollCenter };
        enum ScrollDirection { ScrollNone=0, ScrollUp=0x01, ScrollDown=0x02 };
        int scrollOffset = 0;
        QBasicTimer scrollTimer;
        quint8 scrollFlags = ScrollNone;
        quint8 scrollDirection = ScrollNone;

        QMenuScroller() { }
        ~QMenuScroller() { }
    } *scroll = nullptr;
    void scrollMenu(QMenuScroller::ScrollLocation location, bool active=false);
    void scrollMenu(QMenuScroller::ScrollDirection direction, bool page=false, bool active=false);
    void scrollMenu(QAction *action, QMenuScroller::ScrollLocation location, bool active=false);

    //synchronous operation (ie exec())
    QEventLoop *eventLoop = nullptr;
    QPointer<QAction> syncAction;

    //search buffer
    QString searchBuffer;
    QBasicTimer searchBufferTimer;

    //passing of mouse events up the parent hierarchy
    QPointer<QMenu> activeMenu;
    bool mouseEventTaken(QMouseEvent *);

    //used to walk up the popup list
    struct QMenuCaused {
        QPointer<QWidget> widget;
        QPointer<QAction> action;
    };
    virtual QVector<QPointer<QWidget> > calcCausedStack() const;
    QMenuCaused causedPopup;
    void hideUpToMenuBar();
    void hideMenu(QMenu *menu);

    //index mappings
    inline QAction *actionAt(int i) const { return q_func()->actions().at(i); }
    inline int indexOf(QAction *act) const { return q_func()->actions().indexOf(act); }

    //tear off support
    QPointer<QTornOffMenu> tornPopup;

    QMenuSloppyState sloppyState;

    //default action
    QPointer<QAction> defaultAction;

    QAction *menuAction = nullptr;
    QAction *defaultMenuAction = nullptr;

    void setOverrideMenuAction(QAction *);
    void _q_overrideMenuActionDestroyed();

    //firing of events
    void activateAction(QAction *, QAction::ActionEvent, bool self=true);
    void activateCausedStack(const QVector<QPointer<QWidget> > &, QAction *, QAction::ActionEvent, bool);

    void _q_actionTriggered();
    void _q_actionHovered();
    void _q_platformMenuAboutToShow();

    bool hasMouseMoved(const QPoint &globalPos);

    void updateLayoutDirection();

    QPointer<QPlatformMenu> platformMenu;

    QPointer<QAction> actionAboutToTrigger;

    QPointer<QWidget> noReplayFor;

    class ScrollerTearOffItem : public QWidget {
    public:
        enum Type { ScrollUp, ScrollDown };
        ScrollerTearOffItem(Type type, QMenuPrivate *mPrivate,
                            QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
        void paintEvent(QPaintEvent *e) override;
        void updateScrollerRects(const QRect &rect);

    private:
        QMenuPrivate *menuPrivate;
        Type scrollType;
    };
    ScrollerTearOffItem *scrollUpTearOffItem = nullptr;
    ScrollerTearOffItem *scrollDownItem = nullptr;

    void drawScroller(QPainter *painter, ScrollerTearOffItem::Type type, const QRect &rect);
    void drawTearOff(QPainter *painter, const QRect &rect);
    QRect rect() const;

    mutable uint maxIconWidth = 0;
    mutable uint tabWidth = 0;
    int motions = 0;
    int mousePopupDelay = 0;

    bool activationRecursionGuard = false;

    mutable quint8 ncols = 0; // "255cols ought to be enough for anybody."

    mutable bool itemsDirty : 1;
    mutable bool hasCheckableItems : 1;
    bool lastContextMenu : 1;
    bool collapsibleSeparators : 1;
    bool toolTipsVisible : 1;
    bool delayedPopupGuard : 1;
    bool hasReceievedEnter : 1;
    // Selection
    bool hasHadMouse : 1;
    bool aboutToHide : 1;
    // Tear-off menus
    bool tearoff : 1;
    bool tornoff : 1;
    bool tearoffHighlighted : 1;
    //menu fading/scrolling effects
    bool doChildEffects : 1;

    int popupScreen = -1;
};

QT_END_NAMESPACE

#endif // QMENU_P_H
