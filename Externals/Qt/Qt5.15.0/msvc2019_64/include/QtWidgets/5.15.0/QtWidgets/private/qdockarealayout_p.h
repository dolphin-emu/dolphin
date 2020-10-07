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

#ifndef QDOCKAREALAYOUT_P_H
#define QDOCKAREALAYOUT_P_H

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
#include "QtCore/qrect.h"
#include "QtCore/qpair.h"
#include "QtCore/qlist.h"
#include "QtCore/qvector.h"
#include "QtWidgets/qlayout.h"

QT_REQUIRE_CONFIG(dockwidget);

QT_BEGIN_NAMESPACE

class QLayoutItem;
class QWidget;
class QLayoutItem;
class QDockAreaLayoutInfo;
class QPlaceHolderItem;
class QDockWidget;
class QMainWindow;
class QWidgetAnimator;
class QMainWindowLayout;
struct QLayoutStruct;
class QTabBar;

// The classes in this file represent the tree structure that represents all the docks
// Also see the wiki internal documentation
// At the root of the tree is: QDockAreaLayout, which handles all 4 sides, so there is only one.
// For each side it has one QDockAreaLayoutInfo child. (See QDockAreaLayout::docks.)
// The QDockAreaLayoutInfo have QDockAreaLayoutItems as children (See QDockAreaLayoutInfo::item_list),
// which then has one QDockAreaLayoutInfo as a child. (QDockAreaLayoutItem::subInfo) or
// a widgetItem if this is a node of the tree (QDockAreaLayoutItem::widgetItem)
//
// A path indetifies uniquely one object in this tree, the first number being the side and all the following
// indexes into the QDockAreaLayoutInfo::item_list.

struct QDockAreaLayoutItem
{
    enum ItemFlags { NoFlags = 0, GapItem = 1, KeepSize = 2 };

    explicit QDockAreaLayoutItem(QLayoutItem *_widgetItem = nullptr);
    explicit QDockAreaLayoutItem(QDockAreaLayoutInfo *_subinfo);
    explicit QDockAreaLayoutItem(QPlaceHolderItem *_placeHolderItem);
    QDockAreaLayoutItem(const QDockAreaLayoutItem &other);
    ~QDockAreaLayoutItem();

    QDockAreaLayoutItem &operator = (const QDockAreaLayoutItem &other);

    bool skip() const;
    QSize minimumSize() const;
    QSize maximumSize() const;
    QSize sizeHint() const;
    bool expansive(Qt::Orientation o) const;
    bool hasFixedSize(Qt::Orientation o) const;

    QLayoutItem *widgetItem;
    QDockAreaLayoutInfo *subinfo;
    QPlaceHolderItem *placeHolderItem;
    int pos;
    int size;
    uint flags;
};

class Q_AUTOTEST_EXPORT QPlaceHolderItem
{
public:
    QPlaceHolderItem() : hidden(false), window(false) {}
    explicit QPlaceHolderItem(QWidget *w);

    QString objectName;
    bool hidden, window;
    QRect topLevelRect;
};

class Q_AUTOTEST_EXPORT QDockAreaLayoutInfo
{
public:
    QDockAreaLayoutInfo();
    QDockAreaLayoutInfo(const int *_sep, QInternal::DockPosition _dockPos, Qt::Orientation _o,
                        int tbhape, QMainWindow *window);

    QSize minimumSize() const;
    QSize maximumSize() const;
    QSize sizeHint() const;
    QSize size() const;

    bool insertGap(const QList<int> &path, QLayoutItem *dockWidgetItem);
    QLayoutItem *plug(const QList<int> &path);
    QLayoutItem *unplug(const QList<int> &path);
    enum TabMode { NoTabs, AllowTabs, ForceTabs };
    QList<int> gapIndex(const QPoint &pos, bool nestingEnabled,
                            TabMode tabMode) const;
    void remove(const QList<int> &path);
    void unnest(int index);
    void split(int index, Qt::Orientation orientation, QLayoutItem *dockWidgetItem);
#if QT_CONFIG(tabbar)
    void tab(int index, QLayoutItem *dockWidgetItem);
#endif
    QDockAreaLayoutItem &item(const QList<int> &path);
    QDockAreaLayoutInfo *info(const QList<int> &path);
    QDockAreaLayoutInfo *info(QWidget *widget);

    enum { // sentinel values used to validate state data
        SequenceMarker = 0xfc,
        TabMarker = 0xfa,
        WidgetMarker = 0xfb
    };
    void saveState(QDataStream &stream) const;
    bool restoreState(QDataStream &stream, QList<QDockWidget*> &widgets, bool testing);

    void fitItems();
    bool expansive(Qt::Orientation o) const;
    int changeSize(int index, int size, bool below);
    QRect itemRect(int index, bool isGap = false) const;
    QRect itemRect(const QList<int> &path) const;
    QRect separatorRect(int index) const;
    QRect separatorRect(const QList<int> &path) const;

    void clear();
    bool isEmpty() const;
    bool onlyHasPlaceholders() const;
    bool hasFixedSize() const;
    QList<int> findSeparator(const QPoint &pos) const;
    int next(int idx) const;
    int prev(int idx) const;

    QList<int> indexOf(QWidget *widget) const;
    QList<int> indexOfPlaceHolder(const QString &objectName) const;

    QDockWidget *apply(bool animate);

    void paintSeparators(QPainter *p, QWidget *widget, const QRegion &clip,
                            const QPoint &mouse) const;
    QRegion separatorRegion() const;
    int separatorMove(int index, int delta);
    int separatorMove(const QList<int> &separator, const QPoint &origin, const QPoint &dest);

    QLayoutItem *itemAt(int *x, int index) const;
    QLayoutItem *takeAt(int *x, int index);
    void deleteAllLayoutItems();

    QMainWindowLayout *mainWindowLayout() const;

    const int *sep;
    mutable QVector<QWidget*> separatorWidgets;
    QInternal::DockPosition dockPos;
    Qt::Orientation o;
    QRect rect;
    QMainWindow *mainWindow;
    QList<QDockAreaLayoutItem> item_list;
#if QT_CONFIG(tabbar)
    void updateSeparatorWidgets() const;
    QSet<QWidget*> usedSeparatorWidgets() const;

    quintptr currentTabId() const;
    void setCurrentTab(QWidget *widget);
    void setCurrentTabId(quintptr id);
    QRect tabContentRect() const;
    bool tabbed;
    QTabBar *tabBar;
    int tabBarShape;

    void reparentWidgets(QWidget *p);
    bool updateTabBar() const;
    void setTabBarShape(int shape);
    QSize tabBarMinimumSize() const;
    QSize tabBarSizeHint() const;

    QSet<QTabBar*> usedTabBars() const;

    int tabIndexToListIndex(int) const;
    void moveTab(int from, int to);
#endif // QT_CONFIG(tabbar)
};

class Q_AUTOTEST_EXPORT QDockAreaLayout
{
public:
    enum { EmptyDropAreaSize = 80 }; // when a dock area is empty, how "wide" is it?

    Qt::DockWidgetArea corners[4]; // use a Qt::Corner for indexing
    QRect rect;
    QLayoutItem *centralWidgetItem;
    QMainWindow *mainWindow;
    QRect centralWidgetRect;
    QDockAreaLayout(QMainWindow *win);
    QDockAreaLayoutInfo docks[4];
    int sep; // separator extent
    bool fallbackToSizeHints; //determines if we should use the sizehint for the dock areas (true until the layout is restored or the separator is moved by user)
    mutable QVector<QWidget*> separatorWidgets;

    bool isValid() const;

    enum { DockWidgetStateMarker = 0xfd, FloatingDockWidgetTabMarker = 0xf9 };
    static QRect constrainedRect(QRect rect, QWidget *widget);
    void saveState(QDataStream &stream) const;
    bool restoreState(QDataStream &stream, const QList<QDockWidget*> &widgets, bool testing = false);

    QList<int> indexOfPlaceHolder(const QString &objectName) const;
    QList<int> indexOf(QWidget *dockWidget) const;
    QList<int> gapIndex(const QPoint &pos, bool disallowTabs) const;
    QList<int> findSeparator(const QPoint &pos) const;

    QDockAreaLayoutItem &item(const QList<int> &path);
    QDockAreaLayoutInfo *info(const QList<int> &path);
    const QDockAreaLayoutInfo *info(const QList<int> &path) const;
    QDockAreaLayoutInfo *info(QWidget *widget);
    QRect itemRect(const QList<int> &path) const;
    QRect separatorRect(int index) const;
    QRect separatorRect(const QList<int> &path) const;

    bool insertGap(const QList<int> &path, QLayoutItem *dockWidgetItem);
    QLayoutItem *plug(const QList<int> &path);
    QLayoutItem *unplug(const QList<int> &path);
    void remove(const QList<int> &path);
    void removePlaceHolder(const QString &name);

    void fitLayout();

    void clear();

    QSize sizeHint() const;
    QSize minimumSize() const;

    void addDockWidget(QInternal::DockPosition pos, QDockWidget *dockWidget, Qt::Orientation orientation);
    bool restoreDockWidget(QDockWidget *dockWidget);
    void splitDockWidget(QDockWidget *after, QDockWidget *dockWidget,
                         Qt::Orientation orientation);
#if QT_CONFIG(tabbar)
    void tabifyDockWidget(QDockWidget *first, QDockWidget *second);
#endif
    void resizeDocks(const QList<QDockWidget *> &docks, const QList<int> &sizes, Qt::Orientation o);

    void apply(bool animate);

    void paintSeparators(QPainter *p, QWidget *widget, const QRegion &clip,
                            const QPoint &mouse) const;
    QRegion separatorRegion() const;
    int separatorMove(const QList<int> &separator, const QPoint &origin, const QPoint &dest);
#if QT_CONFIG(tabbar)
    void updateSeparatorWidgets() const;
#endif // QT_CONFIG(tabbar)

    QLayoutItem *itemAt(int *x, int index) const;
    QLayoutItem *takeAt(int *x, int index);
    void deleteAllLayoutItems();

    void getGrid(QVector<QLayoutStruct> *ver_struct_list,
                    QVector<QLayoutStruct> *hor_struct_list);
    void setGrid(QVector<QLayoutStruct> *ver_struct_list,
                    QVector<QLayoutStruct> *hor_struct_list);

    QRect gapRect(const QList<int> &path) const;

    void keepSize(QDockWidget *w);
#if QT_CONFIG(tabbar)
    QSet<QTabBar*> usedTabBars() const;
    QSet<QWidget*> usedSeparatorWidgets() const;
#endif // QT_CONFIG(tabbar)
    void styleChangedEvent();
};

QT_END_NAMESPACE

#endif // QDOCKAREALAYOUT_P_H
