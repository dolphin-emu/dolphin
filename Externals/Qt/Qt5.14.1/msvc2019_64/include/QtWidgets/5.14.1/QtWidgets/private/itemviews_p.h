/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
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

#ifndef ACCESSIBLE_ITEMVIEWS_H
#define ACCESSIBLE_ITEMVIEWS_H

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
#include "QtCore/qpointer.h"
#include <QtGui/qaccessible.h>
#include <QtWidgets/qaccessiblewidget.h>
#include <QtWidgets/qabstractitemview.h>
#include <QtWidgets/qheaderview.h>

QT_REQUIRE_CONFIG(itemviews);

QT_BEGIN_NAMESPACE

#ifndef QT_NO_ACCESSIBILITY

class QAccessibleTableCell;
class QAccessibleTableHeaderCell;

class QAccessibleTable :public QAccessibleTableInterface, public QAccessibleObject
{
public:
    explicit QAccessibleTable(QWidget *w);
    bool isValid() const override;

    QAccessible::Role role() const override;
    QAccessible::State state() const override;
    QString text(QAccessible::Text t) const override;
    QRect rect() const override;

    QAccessibleInterface *childAt(int x, int y) const override;
    int childCount() const override;
    int indexOfChild(const QAccessibleInterface *) const override;

    QAccessibleInterface *parent() const override;
    QAccessibleInterface *child(int index) const override;

    void *interface_cast(QAccessible::InterfaceType t) override;

    // table interface
    virtual QAccessibleInterface *cellAt(int row, int column) const override;
    virtual QAccessibleInterface *caption() const override;
    virtual QAccessibleInterface *summary() const override;
    virtual QString columnDescription(int column) const override;
    virtual QString rowDescription(int row) const override;
    virtual int columnCount() const override;
    virtual int rowCount() const override;

    // selection
    virtual int selectedCellCount() const override;
    virtual int selectedColumnCount() const override;
    virtual int selectedRowCount() const override;
    virtual QList<QAccessibleInterface*> selectedCells() const override;
    virtual QList<int> selectedColumns() const override;
    virtual QList<int> selectedRows() const override;
    virtual bool isColumnSelected(int column) const override;
    virtual bool isRowSelected(int row) const override;
    virtual bool selectRow(int row) override;
    virtual bool selectColumn(int column) override;
    virtual bool unselectRow(int row) override;
    virtual bool unselectColumn(int column) override;

    QAbstractItemView *view() const;

    void modelChange(QAccessibleTableModelChangeEvent *event) override;

protected:
    inline QAccessible::Role cellRole() const {
        switch (m_role) {
        case QAccessible::List:
            return QAccessible::ListItem;
        case QAccessible::Table:
            return QAccessible::Cell;
        case QAccessible::Tree:
            return QAccessible::TreeItem;
        default:
            Q_ASSERT(0);
        }
        return QAccessible::NoRole;
    }

    QHeaderView *horizontalHeader() const;
    QHeaderView *verticalHeader() const;

    // maybe vector
    typedef QHash<int, QAccessible::Id> ChildCache;
    mutable ChildCache childToId;

    virtual ~QAccessibleTable();

private:
    // the child index for a model index
    inline int logicalIndex(const QModelIndex &index) const;
    QAccessible::Role m_role;
};

#if QT_CONFIG(treeview)
class QAccessibleTree :public QAccessibleTable
{
public:
    explicit QAccessibleTree(QWidget *w)
        : QAccessibleTable(w)
    {}


    QAccessibleInterface *childAt(int x, int y) const override;
    int childCount() const override;
    QAccessibleInterface *child(int index) const override;

    int indexOfChild(const QAccessibleInterface *) const override;

    int rowCount() const override;

    // table interface
    QAccessibleInterface *cellAt(int row, int column) const override;
    QString rowDescription(int row) const override;
    bool isRowSelected(int row) const override;
    bool selectRow(int row) override;

private:
    QModelIndex indexFromLogical(int row, int column = 0) const;

    inline int logicalIndex(const QModelIndex &index) const;
};
#endif

class QAccessibleTableCell: public QAccessibleInterface, public QAccessibleTableCellInterface, public QAccessibleActionInterface
{
public:
    QAccessibleTableCell(QAbstractItemView *view, const QModelIndex &m_index, QAccessible::Role role);

    void *interface_cast(QAccessible::InterfaceType t) override;
    QObject *object() const override { return nullptr; }
    QAccessible::Role role() const override;
    QAccessible::State state() const override;
    QRect rect() const override;
    bool isValid() const override;

    QAccessibleInterface *childAt(int, int) const override { return nullptr; }
    int childCount() const override { return 0; }
    int indexOfChild(const QAccessibleInterface *) const override { return -1; }

    QString text(QAccessible::Text t) const override;
    void setText(QAccessible::Text t, const QString &text) override;

    QAccessibleInterface *parent() const override;
    QAccessibleInterface *child(int) const override;

    // cell interface
    virtual int columnExtent() const override;
    virtual QList<QAccessibleInterface*> columnHeaderCells() const override;
    virtual int columnIndex() const override;
    virtual int rowExtent() const override;
    virtual QList<QAccessibleInterface*> rowHeaderCells() const override;
    virtual int rowIndex() const override;
    virtual bool isSelected() const override;
    virtual QAccessibleInterface* table() const override;

    //action interface
    virtual QStringList actionNames() const override;
    virtual void doAction(const QString &actionName) override;
    virtual QStringList keyBindingsForAction(const QString &actionName) const override;

private:
    QHeaderView *verticalHeader() const;
    QHeaderView *horizontalHeader() const;
    QPointer<QAbstractItemView > view;
    QPersistentModelIndex m_index;
    QAccessible::Role m_role;

    void selectCell();
    void unselectCell();

friend class QAccessibleTable;
#if QT_CONFIG(treeview)
friend class QAccessibleTree;
#endif
};


class QAccessibleTableHeaderCell: public QAccessibleInterface
{
public:
    // For header cells, pass the header view in addition
    QAccessibleTableHeaderCell(QAbstractItemView *view, int index, Qt::Orientation orientation);

    QObject *object() const override { return nullptr; }
    QAccessible::Role role() const override;
    QAccessible::State state() const override;
    QRect rect() const override;
    bool isValid() const override;

    QAccessibleInterface *childAt(int, int) const override { return nullptr; }
    int childCount() const override { return 0; }
    int indexOfChild(const QAccessibleInterface *) const override { return -1; }

    QString text(QAccessible::Text t) const override;
    void setText(QAccessible::Text t, const QString &text) override;

    QAccessibleInterface *parent() const override;
    QAccessibleInterface *child(int index) const override;

private:
    QHeaderView *headerView() const;

    QPointer<QAbstractItemView> view;
    int index;
    Qt::Orientation orientation;

friend class QAccessibleTable;
#if QT_CONFIG(treeview)
friend class QAccessibleTree;
#endif
};

// This is the corner button on the top left of a table.
// It can be used to select all cells or it is not active at all.
// For now it is ignored.
class QAccessibleTableCornerButton: public QAccessibleInterface
{
public:
    QAccessibleTableCornerButton(QAbstractItemView *view_)
        :view(view_)
    {}

    QObject *object() const override { return nullptr; }
    QAccessible::Role role() const override { return QAccessible::Pane; }
    QAccessible::State state() const override { return QAccessible::State(); }
    QRect rect() const override { return QRect(); }
    bool isValid() const override { return true; }

    QAccessibleInterface *childAt(int, int) const override { return nullptr; }
    int childCount() const override { return 0; }
    int indexOfChild(const QAccessibleInterface *) const override { return -1; }

    QString text(QAccessible::Text) const override { return QString(); }
    void setText(QAccessible::Text, const QString &) override {}

    QAccessibleInterface *parent() const override {
        return QAccessible::queryAccessibleInterface(view);
    }
    QAccessibleInterface *child(int) const override {
        return nullptr;
    }

private:
    QPointer<QAbstractItemView> view;
};


#endif // QT_NO_ACCESSIBILITY

QT_END_NAMESPACE

#endif // ACCESSIBLE_ITEMVIEWS_H
