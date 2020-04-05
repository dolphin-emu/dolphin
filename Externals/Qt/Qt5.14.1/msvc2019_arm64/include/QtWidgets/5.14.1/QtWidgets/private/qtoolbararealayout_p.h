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

#ifndef QTOOLBARAREALAYOUT_P_H
#define QTOOLBARAREALAYOUT_P_H

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
#include "qmenu_p.h"
#include <QList>
#include <QSize>
#include <QRect>

QT_REQUIRE_CONFIG(toolbar);

QT_BEGIN_NAMESPACE

class QToolBar;
class QLayoutItem;
class QMainWindow;
class QStyleOptionToolBar;

class QToolBarAreaLayoutItem
{
public:
    QToolBarAreaLayoutItem(QLayoutItem *item = nullptr)
        : widgetItem(item), pos(0), size(-1), preferredSize(-1), gap(false) {}

    bool skip() const;
    QSize minimumSize() const;
    QSize sizeHint() const;
    QSize realSizeHint() const;

    void resize(Qt::Orientation o, int newSize)
    {
        newSize = qMax(pick(o, minimumSize()), newSize);
        int sizeh = pick(o, sizeHint());
        if (newSize == sizeh) {
            preferredSize = -1;
            size = sizeh;
        } else {
            preferredSize = newSize;
        }
    }

    void extendSize(Qt::Orientation o, int extent)
    {
        int newSize = qMax(pick(o, minimumSize()), (preferredSize > 0 ? preferredSize : pick(o, sizeHint())) + extent);
        int sizeh = pick(o, sizeHint());
        if (newSize == sizeh) {
            preferredSize = -1;
            size = sizeh;
        } else {
            preferredSize = newSize;
        }
    }

    QLayoutItem *widgetItem;
    int pos;
    int size;
    int preferredSize;
    bool gap;
};
Q_DECLARE_TYPEINFO(QToolBarAreaLayoutItem, Q_PRIMITIVE_TYPE);

class QToolBarAreaLayoutLine
{
public:
    QToolBarAreaLayoutLine() {} // for QVector, don't use
    QToolBarAreaLayoutLine(Qt::Orientation orientation);

    QSize sizeHint() const;
    QSize minimumSize() const;

    void fitLayout();
    bool skip() const;

    QRect rect;
    Qt::Orientation o;

    QVector<QToolBarAreaLayoutItem> toolBarItems;
};
Q_DECLARE_TYPEINFO(QToolBarAreaLayoutLine, Q_MOVABLE_TYPE);

class QToolBarAreaLayoutInfo
{
public:
    QToolBarAreaLayoutInfo(QInternal::DockPosition pos = QInternal::TopDock);

    QSize sizeHint() const;
    QSize minimumSize() const;

    void fitLayout();

    QLayoutItem *insertToolBar(QToolBar *before, QToolBar *toolBar);
    void insertItem(QToolBar *before, QLayoutItem *item);
    void removeToolBar(QToolBar *toolBar);
    void insertToolBarBreak(QToolBar *before);
    void removeToolBarBreak(QToolBar *before);
    void moveToolBar(QToolBar *toolbar, int pos);

    QList<int> gapIndex(const QPoint &pos, int *maxDistance) const;
    bool insertGap(const QList<int> &path, QLayoutItem *item);
    void clear();
    QRect itemRect(const QList<int> &path) const;
    int distance(const QPoint &pos) const;

    QVector<QToolBarAreaLayoutLine> lines;
    QRect rect;
    Qt::Orientation o;
    QInternal::DockPosition dockPos;
    bool dirty;
};
Q_DECLARE_TYPEINFO(QToolBarAreaLayoutInfo, Q_MOVABLE_TYPE);

class QToolBarAreaLayout
{
public:
    enum { // sentinel values used to validate state data
        ToolBarStateMarker = 0xfe,
        ToolBarStateMarkerEx = 0xfc
    };

    QRect rect;
    const QMainWindow *mainWindow;
    QToolBarAreaLayoutInfo docks[4];
    bool visible;

    QToolBarAreaLayout(const QMainWindow *win);

    QRect fitLayout();

    QSize minimumSize(const QSize &centerMin) const;
    QRect rectHint(const QRect &r) const;
    QSize sizeHint(const QSize &center) const;
    void apply(bool animate);

    QLayoutItem *itemAt(int *x, int index) const;
    QLayoutItem *takeAt(int *x, int index);
    void deleteAllLayoutItems();

    QLayoutItem *insertToolBar(QToolBar *before, QToolBar *toolBar);
    void removeToolBar(QToolBar *toolBar);
    QLayoutItem *addToolBar(QInternal::DockPosition pos, QToolBar *toolBar);
    void insertToolBarBreak(QToolBar *before);
    void removeToolBarBreak(QToolBar *before);
    void addToolBarBreak(QInternal::DockPosition pos);
    void moveToolBar(QToolBar *toolbar, int pos);

    void insertItem(QInternal::DockPosition pos, QLayoutItem *item);
    void insertItem(QToolBar *before, QLayoutItem *item);

    QInternal::DockPosition findToolBar(const QToolBar *toolBar) const;
    bool toolBarBreak(QToolBar *toolBar) const;

    void getStyleOptionInfo(QStyleOptionToolBar *option, QToolBar *toolBar) const;

    QList<int> indexOf(QWidget *toolBar) const;
    QList<int> gapIndex(const QPoint &pos) const;
    QList<int> currentGapIndex() const;
    bool insertGap(const QList<int> &path, QLayoutItem *item);
    void remove(const QList<int> &path);
    void remove(QLayoutItem *item);
    void clear();
    QToolBarAreaLayoutItem *item(const QList<int> &path);
    QRect itemRect(const QList<int> &path) const;
    QLayoutItem *plug(const QList<int> &path);
    QLayoutItem *unplug(const QList<int> &path, QToolBarAreaLayout *other);

    void saveState(QDataStream &stream) const;
    bool restoreState(QDataStream &stream, const QList<QToolBar*> &toolBars, uchar tmarker, bool testing = false);
    bool isEmpty() const;
};

QT_END_NAMESPACE

#endif // QTOOLBARAREALAYOUT_P_H
