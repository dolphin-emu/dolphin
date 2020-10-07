/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
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

#ifndef QQUICKTABLEVIEW_P_P_H
#define QQUICKTABLEVIEW_P_P_H

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

#include "qquicktableview_p.h"

#include <QtCore/qtimer.h>
#include <QtQmlModels/private/qqmltableinstancemodel_p.h>
#include <QtQml/private/qqmlincubator_p.h>
#include <QtQmlModels/private/qqmlchangeset_p.h>
#include <QtQml/qqmlinfo.h>

#include <QtQuick/private/qquickflickable_p_p.h>
#include <QtQuick/private/qquickitemviewfxitem_p_p.h>

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(lcTableViewDelegateLifecycle)

static const qreal kDefaultRowHeight = 50;
static const qreal kDefaultColumnWidth = 50;

class FxTableItem;
class QQuickTableSectionSizeProviderPrivate;

class Q_QUICK_PRIVATE_EXPORT QQuickTableSectionSizeProvider : public QObject {
    Q_OBJECT

public:
    QQuickTableSectionSizeProvider(QObject *parent=nullptr);
    void setSize(int section, qreal size);
    qreal size(int section);
    bool resetSize(int section);
    void resetAll();

Q_SIGNALS:
    void sizeChanged();

private:
    Q_DISABLE_COPY(QQuickTableSectionSizeProvider)
    Q_DECLARE_PRIVATE(QQuickTableSectionSizeProvider)
};

class Q_QUICK_PRIVATE_EXPORT QQuickTableViewPrivate : public QQuickFlickablePrivate
{
    Q_DECLARE_PUBLIC(QQuickTableView)

public:
    class TableEdgeLoadRequest
    {
        // Whenever we need to load new rows or columns in the
        // table, we fill out a TableEdgeLoadRequest.
        // TableEdgeLoadRequest is just a struct that keeps track
        // of which cells that needs to be loaded, and which cell
        // the table is currently loading. The loading itself is
        // done by QQuickTableView.

    public:
        void begin(const QPoint &cell, const QPointF &pos, QQmlIncubator::IncubationMode incubationMode)
        {
            Q_ASSERT(!m_active);
            m_active = true;
            m_edge = Qt::Edge(0);
            m_mode = incubationMode;
            m_edgeIndex = cell.x();
            m_visibleCellsInEdge.clear();
            m_visibleCellsInEdge.append(cell.y());
            m_currentIndex = 0;
            m_startPos = pos;
            qCDebug(lcTableViewDelegateLifecycle()) << "begin top-left:" << toString();
        }

        void begin(Qt::Edge edgeToLoad, int edgeIndex, const QList<int> visibleCellsInEdge, QQmlIncubator::IncubationMode incubationMode)
        {
            Q_ASSERT(!m_active);
            m_active = true;
            m_edge = edgeToLoad;
            m_edgeIndex = edgeIndex;
            m_visibleCellsInEdge = visibleCellsInEdge;
            m_mode = incubationMode;
            m_currentIndex = 0;
            qCDebug(lcTableViewDelegateLifecycle()) << "begin:" << toString();
        }

        inline void markAsDone() { m_active = false; }
        inline bool isActive() { return m_active; }

        inline QPoint currentCell() { return cellAt(m_currentIndex); }
        inline bool hasCurrentCell() { return m_currentIndex < m_visibleCellsInEdge.count(); }
        inline void moveToNextCell() { ++m_currentIndex; }

        inline Qt::Edge edge() { return m_edge; }
        inline int row() { return cellAt(0).y(); }
        inline int column() { return cellAt(0).x(); }
        inline QQmlIncubator::IncubationMode incubationMode() { return m_mode; }

        inline QPointF startPosition() { return m_startPos; }

        QString toString()
        {
            QString str;
            QDebug dbg(&str);
            dbg.nospace() << "TableSectionLoadRequest(" << "edge:"
                << m_edge << ", edgeIndex:" << m_edgeIndex << ", incubation:";

            switch (m_mode) {
            case QQmlIncubator::Asynchronous:
                dbg << "Asynchronous";
                break;
            case QQmlIncubator::AsynchronousIfNested:
                dbg << "AsynchronousIfNested";
                break;
            case QQmlIncubator::Synchronous:
                dbg << "Synchronous";
                break;
            }

            return str;
        }

    private:
        Qt::Edge m_edge = Qt::Edge(0);
        QList<int> m_visibleCellsInEdge;
        int m_edgeIndex = 0;
        int m_currentIndex = 0;
        bool m_active = false;
        QQmlIncubator::IncubationMode m_mode = QQmlIncubator::AsynchronousIfNested;
        QPointF m_startPos;

        inline QPoint cellAt(int index) {
            return !m_edge || (m_edge & (Qt::LeftEdge | Qt::RightEdge))
                    ? QPoint(m_edgeIndex, m_visibleCellsInEdge[index])
                    : QPoint(m_visibleCellsInEdge[index], m_edgeIndex);
        }
    };

    class EdgeRange {
    public:
        EdgeRange();
        bool containsIndex(Qt::Edge edge, int index);

        int startIndex;
        int endIndex;
        qreal size;
    };

    enum class RebuildState {
        Begin = 0,
        LoadInitalTable,
        VerifyTable,
        LayoutTable,
        LoadAndUnloadAfterLayout,
        PreloadColumns,
        PreloadRows,
        MovePreloadedItemsToPool,
        Done
    };

    enum class RebuildOption {
        None = 0,
        LayoutOnly = 0x1,
        ViewportOnly = 0x2,
        CalculateNewTopLeftRow = 0x4,
        CalculateNewTopLeftColumn = 0x8,
        All = 0x10,
    };
    Q_DECLARE_FLAGS(RebuildOptions, RebuildOption)

public:
    QQuickTableViewPrivate();
    ~QQuickTableViewPrivate() override;

    static inline QQuickTableViewPrivate *get(QQuickTableView *q) { return q->d_func(); }

    void updatePolish() override;
    void fixup(AxisData &data, qreal minExtent, qreal maxExtent) override;

public:
    QHash<int, FxTableItem *> loadedItems;

    // model, tableModel and modelVariant all point to the same model. modelVariant
    // is the model assigned by the user. And tableModel is the wrapper model we create
    // around it. But if the model is an instance model directly, we cannot wrap it, so
    // we need a pointer for that case as well.
    QQmlInstanceModel* model = nullptr;
    QPointer<QQmlTableInstanceModel> tableModel = nullptr;
    QVariant modelVariant;

    // When the applications assignes a new model or delegate to the view, we keep them
    // around until we're ready to take them into use (syncWithPendingChanges).
    QVariant assignedModel = QVariant(int(0));
    QQmlComponent *assignedDelegate = nullptr;

    // loadedRows/Columns describes the rows and columns that are currently loaded (from top left
    // row/column to bottom right row/column). loadedTableOuterRect describes the actual
    // pixels that all the loaded delegate items cover, and is matched agains the viewport to determine when
    // we need to fill up with more rows/columns. loadedTableInnerRect describes the pixels
    // that the loaded table covers if you remove one row/column on each side of the table, and
    // is used to determine rows/columns that are no longer visible and can be unloaded.
    QMap<int, int> loadedColumns;
    QMap<int, int> loadedRows;
    QRectF loadedTableOuterRect;
    QRectF loadedTableInnerRect;

    QPointF origin = QPointF(0, 0);
    QSizeF endExtent = QSizeF(0, 0);

    QRectF viewportRect = QRectF(0, 0, -1, -1);

    QSize tableSize;

    RebuildState rebuildState = RebuildState::Done;
    RebuildOptions rebuildOptions = RebuildOption::All;
    RebuildOptions scheduledRebuildOptions = RebuildOption::All;

    TableEdgeLoadRequest loadRequest;

    QSizeF cellSpacing = QSizeF(0, 0);

    QQmlTableInstanceModel::ReusableFlag reusableFlag = QQmlTableInstanceModel::Reusable;

    bool blockItemCreatedCallback = false;
    bool layoutWarningIssued = false;
    bool polishing = false;
    bool syncVertically = false;
    bool syncHorizontally = false;
    bool inSetLocalViewportPos = false;
    bool inSyncViewportPosRecursive = false;
    bool inUpdateContentSize = false;

    // isTransposed is currently only used by HeaderView.
    // Consider making it public.
    bool isTransposed = false;

    QJSValue rowHeightProvider;
    QJSValue columnWidthProvider;
    QQuickTableSectionSizeProvider rowHeights;
    QQuickTableSectionSizeProvider columnWidths;

    EdgeRange cachedNextVisibleEdgeIndex[4];
    EdgeRange cachedColumnWidth;
    EdgeRange cachedRowHeight;

    // TableView uses contentWidth/height to report the size of the table (this
    // will e.g make scrollbars written for Flickable work out of the box). This
    // value is continuously calculated, and will change/improve as more columns
    // are loaded into view. At the same time, we want to open up for the
    // possibility that the application can set the content width explicitly, in
    // case it knows what the exact width should be from the start. We therefore
    // override the contentWidth/height properties from QQuickFlickable, to be able
    // to implement this combined behavior. This also lets us lazy build the table
    // if the application needs to know the content size early on.
    QQmlNullableValue<qreal> explicitContentWidth;
    QQmlNullableValue<qreal> explicitContentHeight;

    QSizeF averageEdgeSize;

    QPointer<QQuickTableView> assignedSyncView;
    QPointer<QQuickTableView> syncView;
    QList<QPointer<QQuickTableView> > syncChildren;
    Qt::Orientations assignedSyncDirection = Qt::Horizontal | Qt::Vertical;

    const static QPoint kLeft;
    const static QPoint kRight;
    const static QPoint kUp;
    const static QPoint kDown;

#ifdef QT_DEBUG
    QString forcedIncubationMode = qEnvironmentVariable("QT_TABLEVIEW_INCUBATION_MODE");
#endif

public:
    QQuickTableViewAttached *getAttachedObject(const QObject *object) const;

    int modelIndexAtCell(const QPoint &cell) const;
    QPoint cellAtModelIndex(int modelIndex) const;

    qreal sizeHintForColumn(int column);
    qreal sizeHintForRow(int row);
    QSize calculateTableSize();
    void updateTableSize();

    inline bool isColumnHidden(int column);
    inline bool isRowHidden(int row);

    qreal getColumnLayoutWidth(int column);
    qreal getRowLayoutHeight(int row);
    qreal getColumnWidth(int column);
    qreal getRowHeight(int row);

    inline int topRow() const { return loadedRows.firstKey(); }
    inline int bottomRow() const { return loadedRows.lastKey(); }
    inline int leftColumn() const { return loadedColumns.firstKey(); }
    inline int rightColumn() const { return loadedColumns.lastKey(); }

    QQuickTableView *rootSyncView() const;

    bool updateTableRecursive();
    bool updateTable();
    void relayoutTableItems();

    void layoutVerticalEdge(Qt::Edge tableEdge);
    void layoutHorizontalEdge(Qt::Edge tableEdge);
    void layoutTopLeftItem();
    void layoutTableEdgeFromLoadRequest();

    void updateContentWidth();
    void updateContentHeight();
    void updateAverageEdgeSize();
    RebuildOptions checkForVisibilityChanges();
    void forceLayout();

    void updateExtents();
    void syncLoadedTableRectFromLoadedTable();
    void syncLoadedTableFromLoadRequest();

    int nextVisibleEdgeIndex(Qt::Edge edge, int startIndex);
    int nextVisibleEdgeIndexAroundLoadedTable(Qt::Edge edge);
    inline int edgeToArrayIndex(Qt::Edge edge);
    void clearEdgeSizeCache();

    bool canLoadTableEdge(Qt::Edge tableEdge, const QRectF fillRect) const;
    bool canUnloadTableEdge(Qt::Edge tableEdge, const QRectF fillRect) const;
    Qt::Edge nextEdgeToLoad(const QRectF rect);
    Qt::Edge nextEdgeToUnload(const QRectF rect);

    qreal cellWidth(const QPoint &cell);
    qreal cellHeight(const QPoint &cell);

    FxTableItem *loadedTableItem(const QPoint &cell) const;
    FxTableItem *createFxTableItem(const QPoint &cell, QQmlIncubator::IncubationMode incubationMode);
    FxTableItem *loadFxTableItem(const QPoint &cell, QQmlIncubator::IncubationMode incubationMode);

    void releaseItem(FxTableItem *fxTableItem, QQmlTableInstanceModel::ReusableFlag reusableFlag);
    void releaseLoadedItems(QQmlTableInstanceModel::ReusableFlag reusableFlag);

    void unloadItem(const QPoint &cell);
    void loadEdge(Qt::Edge edge, QQmlIncubator::IncubationMode incubationMode);
    void unloadEdge(Qt::Edge edge);
    void loadAndUnloadVisibleEdges();
    void drainReusePoolAfterLoadRequest();
    void processLoadRequest();

    void processRebuildTable();
    bool moveToNextRebuildState();
    void calculateTopLeft(QPoint &topLeft, QPointF &topLeftPos);
    void beginRebuildTable();
    void layoutAfterLoadingInitialTable();

    void scheduleRebuildTable(QQuickTableViewPrivate::RebuildOptions options);

    int resolveImportVersion();
    void createWrapperModel();

    void initItemCallback(int modelIndex, QObject *item);
    void itemCreatedCallback(int modelIndex, QObject *object);
    void itemPooledCallback(int modelIndex, QObject *object);
    void itemReusedCallback(int modelIndex, QObject *object);
    void modelUpdated(const QQmlChangeSet &changeSet, bool reset);

    virtual void syncWithPendingChanges();
    virtual void syncDelegate();
    virtual QVariant modelImpl() const;
    virtual void setModelImpl(const QVariant &newModel);
    virtual void syncModel();
    inline void syncRebuildOptions();
    virtual void syncSyncView();

    void connectToModel();
    void disconnectFromModel();

    void rowsMovedCallback(const QModelIndex &parent, int start, int end, const QModelIndex &destination, int row);
    void columnsMovedCallback(const QModelIndex &parent, int start, int end, const QModelIndex &destination, int column);
    void rowsInsertedCallback(const QModelIndex &parent, int begin, int end);
    void rowsRemovedCallback(const QModelIndex &parent, int begin, int end);
    void columnsInsertedCallback(const QModelIndex &parent, int begin, int end);
    void columnsRemovedCallback(const QModelIndex &parent, int begin, int end);
    void layoutChangedCallback(const QList<QPersistentModelIndex> &parents, QAbstractItemModel::LayoutChangeHint hint);
    void modelResetCallback();

    void scheduleRebuildIfFastFlick();
    void setLocalViewportX(qreal contentX);
    void setLocalViewportY(qreal contentY);
    void syncViewportPosRecursive();

    void fetchMoreData();

    void _q_componentFinalized();
    void registerCallbackWhenBindingsAreEvaluated();

    inline QString tableLayoutToString() const;
    void dumpTable() const;
};

class FxTableItem : public QQuickItemViewFxItem
{
public:
    FxTableItem(QQuickItem *item, QQuickTableView *table, bool own)
        : QQuickItemViewFxItem(item, own, QQuickTableViewPrivate::get(table))
    {
    }

    qreal position() const override { return 0; }
    qreal endPosition() const override { return 0; }
    qreal size() const override { return 0; }
    qreal sectionSize() const override { return 0; }
    bool contains(qreal, qreal) const override { return false; }

    QPoint cell;
};

QT_END_NAMESPACE

#endif
