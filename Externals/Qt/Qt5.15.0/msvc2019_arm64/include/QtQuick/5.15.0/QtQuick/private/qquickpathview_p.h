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

#ifndef QQUICKPATHVIEW_P_H
#define QQUICKPATHVIEW_P_H

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

QT_REQUIRE_CONFIG(quick_pathview);

#include "qquickitem.h"

#include <private/qtquickglobal_p.h>
#include <private/qquickpath_p.h>

QT_BEGIN_NAMESPACE

class QQmlChangeSet;

class QQuickPathViewPrivate;
class QQuickPathViewAttached;
class Q_QUICK_PRIVATE_EXPORT QQuickPathView : public QQuickItem
{
    Q_OBJECT

    Q_PROPERTY(QVariant model READ model WRITE setModel NOTIFY modelChanged)
    Q_PROPERTY(QQuickPath *path READ path WRITE setPath NOTIFY pathChanged)
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentIndexChanged)
    Q_PROPERTY(QQuickItem *currentItem READ currentItem NOTIFY currentItemChanged)
    Q_PROPERTY(qreal offset READ offset WRITE setOffset NOTIFY offsetChanged)

    Q_PROPERTY(QQmlComponent *highlight READ highlight WRITE setHighlight NOTIFY highlightChanged)
    Q_PROPERTY(QQuickItem *highlightItem READ highlightItem NOTIFY highlightItemChanged)

    Q_PROPERTY(qreal preferredHighlightBegin READ preferredHighlightBegin WRITE setPreferredHighlightBegin NOTIFY preferredHighlightBeginChanged)
    Q_PROPERTY(qreal preferredHighlightEnd READ preferredHighlightEnd WRITE setPreferredHighlightEnd NOTIFY preferredHighlightEndChanged)
    Q_PROPERTY(HighlightRangeMode highlightRangeMode READ highlightRangeMode WRITE setHighlightRangeMode NOTIFY highlightRangeModeChanged)
    Q_PROPERTY(int highlightMoveDuration READ highlightMoveDuration WRITE setHighlightMoveDuration NOTIFY highlightMoveDurationChanged)

    Q_PROPERTY(qreal dragMargin READ dragMargin WRITE setDragMargin NOTIFY dragMarginChanged)
    Q_PROPERTY(qreal maximumFlickVelocity READ maximumFlickVelocity WRITE setMaximumFlickVelocity NOTIFY maximumFlickVelocityChanged)
    Q_PROPERTY(qreal flickDeceleration READ flickDeceleration WRITE setFlickDeceleration NOTIFY flickDecelerationChanged)
    Q_PROPERTY(bool interactive READ isInteractive WRITE setInteractive NOTIFY interactiveChanged)

    Q_PROPERTY(bool moving READ isMoving NOTIFY movingChanged)
    Q_PROPERTY(bool flicking READ isFlicking NOTIFY flickingChanged)
    Q_PROPERTY(bool dragging READ isDragging NOTIFY draggingChanged)

    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QQmlComponent *delegate READ delegate WRITE setDelegate NOTIFY delegateChanged)
    Q_PROPERTY(int pathItemCount READ pathItemCount WRITE setPathItemCount RESET resetPathItemCount NOTIFY pathItemCountChanged)
    Q_PROPERTY(SnapMode snapMode READ snapMode WRITE setSnapMode NOTIFY snapModeChanged)
    Q_PROPERTY(MovementDirection movementDirection READ movementDirection WRITE setMovementDirection NOTIFY movementDirectionChanged REVISION 7)

    Q_PROPERTY(int cacheItemCount READ cacheItemCount WRITE setCacheItemCount NOTIFY cacheItemCountChanged)
    QML_NAMED_ELEMENT(PathView)
    QML_ATTACHED(QQuickPathViewAttached)

public:
    QQuickPathView(QQuickItem *parent = nullptr);
    virtual ~QQuickPathView();

    QVariant model() const;
    void setModel(const QVariant &);

    QQuickPath *path() const;
    void setPath(QQuickPath *);

    int currentIndex() const;
    void setCurrentIndex(int idx);

    QQuickItem *currentItem() const;

    qreal offset() const;
    void setOffset(qreal offset);

    QQmlComponent *highlight() const;
    void setHighlight(QQmlComponent *highlight);
    QQuickItem *highlightItem() const;

    enum HighlightRangeMode { NoHighlightRange, ApplyRange, StrictlyEnforceRange };
    Q_ENUM(HighlightRangeMode)
    HighlightRangeMode highlightRangeMode() const;
    void setHighlightRangeMode(HighlightRangeMode mode);

    qreal preferredHighlightBegin() const;
    void setPreferredHighlightBegin(qreal);

    qreal preferredHighlightEnd() const;
    void setPreferredHighlightEnd(qreal);

    int highlightMoveDuration() const;
    void setHighlightMoveDuration(int);

    qreal dragMargin() const;
    void setDragMargin(qreal margin);

    qreal flickDeceleration() const;
    void setFlickDeceleration(qreal dec);

    qreal maximumFlickVelocity() const;
    void setMaximumFlickVelocity(qreal);

    bool isInteractive() const;
    void setInteractive(bool);

    bool isMoving() const;
    bool isFlicking() const;
    bool isDragging() const;

    int count() const;

    QQmlComponent *delegate() const;
    void setDelegate(QQmlComponent *);

    int pathItemCount() const;
    void setPathItemCount(int);
    void resetPathItemCount();

    int cacheItemCount() const;
    void setCacheItemCount(int);

    enum SnapMode { NoSnap, SnapToItem, SnapOneItem };
    Q_ENUM(SnapMode)
    SnapMode snapMode() const;
    void setSnapMode(SnapMode mode);

    enum MovementDirection { Shortest, Negative, Positive };
    Q_ENUM(MovementDirection)
    MovementDirection movementDirection() const;
    void setMovementDirection(MovementDirection dir);

    enum PositionMode { Beginning, Center, End, Contain=4, SnapPosition }; // 3 == Visible in other views
    Q_ENUM(PositionMode)
    Q_INVOKABLE void positionViewAtIndex(int index, int mode);
    Q_INVOKABLE int indexAt(qreal x, qreal y) const;
    Q_INVOKABLE QQuickItem *itemAt(qreal x, qreal y) const;
    Q_REVISION(13) Q_INVOKABLE QQuickItem *itemAtIndex(int index) const;

    static QQuickPathViewAttached *qmlAttachedProperties(QObject *);

public Q_SLOTS:
    void incrementCurrentIndex();
    void decrementCurrentIndex();

Q_SIGNALS:
    void currentIndexChanged();
    void currentItemChanged();
    void offsetChanged();
    void modelChanged();
    void countChanged();
    void pathChanged();
    void preferredHighlightBeginChanged();
    void preferredHighlightEndChanged();
    void highlightRangeModeChanged();
    void dragMarginChanged();
    void snapPositionChanged();
    void delegateChanged();
    void pathItemCountChanged();
    void maximumFlickVelocityChanged();
    void flickDecelerationChanged();
    void interactiveChanged();
    void movingChanged();
    void flickingChanged();
    void draggingChanged();
    void highlightChanged();
    void highlightItemChanged();
    void highlightMoveDurationChanged();
    void movementStarted();
    void movementEnded();
    Q_REVISION(7) void movementDirectionChanged();
    void flickStarted();
    void flickEnded();
    void dragStarted();
    void dragEnded();
    void snapModeChanged();
    void cacheItemCountChanged();

protected:
    void updatePolish() override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    bool sendMouseEvent(QMouseEvent *event);
    bool childMouseEventFilter(QQuickItem *, QEvent *) override;
    void mouseUngrabEvent() override;
    void componentComplete() override;

private Q_SLOTS:
    void refill();
    void ticked();
    void movementEnding();
    void modelUpdated(const QQmlChangeSet &changeSet, bool reset);
    void createdItem(int index, QObject *item);
    void initItem(int index, QObject *item);
    void destroyingItem(QObject *item);
    void pathUpdated();

private:
    friend class QQuickPathViewAttached;
    Q_DISABLE_COPY(QQuickPathView)
    Q_DECLARE_PRIVATE(QQuickPathView)
};

class QQmlOpenMetaObject;
class QQuickPathViewAttached : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QQuickPathView *view READ view CONSTANT)
    Q_PROPERTY(bool isCurrentItem READ isCurrentItem NOTIFY currentItemChanged)
    Q_PROPERTY(bool onPath READ isOnPath NOTIFY pathChanged)

public:
    QQuickPathViewAttached(QObject *parent);
    ~QQuickPathViewAttached();

    QQuickPathView *view() const { return m_view; }

    bool isCurrentItem() const { return m_isCurrent; }
    void setIsCurrentItem(bool c) {
        if (m_isCurrent != c) {
            m_isCurrent = c;
            Q_EMIT currentItemChanged();
        }
    }

    QVariant value(const QByteArray &name) const;
    void setValue(const QByteArray &name, const QVariant &val);

    bool isOnPath() const { return m_onPath; }
    void setOnPath(bool on) {
        if (on != m_onPath) {
            m_onPath = on;
            Q_EMIT pathChanged();
        }
    }
    qreal m_percent;

Q_SIGNALS:
    void currentItemChanged();
    void pathChanged();

private:
    friend class QQuickPathViewPrivate;
    friend class QQuickPathView;
    QQuickPathView *m_view;
    QQmlOpenMetaObject *m_metaobject;
    bool m_onPath : 1;
    bool m_isCurrent : 1;
};


QT_END_NAMESPACE

QML_DECLARE_TYPE(QQuickPathView)

#endif // QQUICKPATHVIEW_P_H
