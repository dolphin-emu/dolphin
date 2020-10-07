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

#ifndef QQUICKITEMVIEWTRANSITION_P_P_H
#define QQUICKITEMVIEWTRANSITION_P_P_H

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

#include <QtQuick/private/qtquickglobal_p.h>

QT_REQUIRE_CONFIG(quick_viewtransitions);

#include <QtCore/qobject.h>
#include <QtCore/qpoint.h>
#include <QtQml/qqml.h>
#include <private/qqmlguard_p.h>
#include <private/qquicktransition_p.h>
#include <private/qanimationjobutil_p.h>

QT_BEGIN_NAMESPACE

class QQuickItem;
class QQuickTransition;
class QQuickItemViewTransitionableItem;
class QQuickItemViewTransitionJob;


class Q_QUICK_PRIVATE_EXPORT QQuickItemViewTransitionChangeListener
{
public:
    QQuickItemViewTransitionChangeListener() {}
    virtual ~QQuickItemViewTransitionChangeListener() {}

    virtual void viewItemTransitionFinished(QQuickItemViewTransitionableItem *item) = 0;
};


class Q_QUICK_PRIVATE_EXPORT QQuickItemViewTransitioner
{
public:
    enum TransitionType {
        NoTransition,
        PopulateTransition,
        AddTransition,
        MoveTransition,
        RemoveTransition
    };

    QQuickItemViewTransitioner();
    virtual ~QQuickItemViewTransitioner();

    bool canTransition(QQuickItemViewTransitioner::TransitionType type, bool asTarget) const;
    void transitionNextReposition(QQuickItemViewTransitionableItem *item, QQuickItemViewTransitioner::TransitionType type, bool isTarget);

    void addToTargetLists(QQuickItemViewTransitioner::TransitionType type, QQuickItemViewTransitionableItem *item, int index);
    void resetTargetLists();

    QQuickTransition *transitionObject(QQuickItemViewTransitioner::TransitionType type, bool asTarget) const;
    const QList<int> &targetIndexes(QQuickItemViewTransitioner::TransitionType type) const;
    const QList<QObject *> &targetItems(QQuickItemViewTransitioner::TransitionType type) const;

    inline void setPopulateTransitionEnabled(bool b) { usePopulateTransition = b; }
    inline bool populateTransitionEnabled() const { return usePopulateTransition; }

    inline void setChangeListener(QQuickItemViewTransitionChangeListener *obj) { changeListener = obj; }

    QSet<QQuickItemViewTransitionJob *> runningJobs;

    QList<int> addTransitionIndexes;
    QList<int> moveTransitionIndexes;
    QList<int> removeTransitionIndexes;
    QList<QObject *> addTransitionTargets;
    QList<QObject *> moveTransitionTargets;
    QList<QObject *> removeTransitionTargets;

    QQmlGuard<QQuickTransition> populateTransition;
    QQmlGuard<QQuickTransition> addTransition;
    QQmlGuard<QQuickTransition> addDisplacedTransition;
    QQmlGuard<QQuickTransition> moveTransition;
    QQmlGuard<QQuickTransition> moveDisplacedTransition;
    QQmlGuard<QQuickTransition> removeTransition;
    QQmlGuard<QQuickTransition> removeDisplacedTransition;
    QQmlGuard<QQuickTransition> displacedTransition;

private:
    friend class QQuickItemViewTransitionJob;

    QQuickItemViewTransitionChangeListener *changeListener;
    bool usePopulateTransition;

    void finishedTransition(QQuickItemViewTransitionJob *job, QQuickItemViewTransitionableItem *item);
};


/*
  An item that can be transitioned using QQuickViewTransitionJob.
  */
class Q_QUICK_PRIVATE_EXPORT QQuickItemViewTransitionableItem
{
public:
    QQuickItemViewTransitionableItem(QQuickItem *i);
    virtual ~QQuickItemViewTransitionableItem();

    qreal itemX() const;
    qreal itemY() const;

    void moveTo(const QPointF &pos, bool immediate = false);

    bool transitionScheduledOrRunning() const;
    bool transitionRunning() const;
    bool isPendingRemoval() const;

    bool prepareTransition(QQuickItemViewTransitioner *transitioner, int index, const QRectF &viewBounds);
    void startTransition(QQuickItemViewTransitioner *transitioner, int index);

    SelfDeletable m_selfDeletable;
    QPointF nextTransitionTo;
    QPointF lastMovedTo;
    QPointF nextTransitionFrom;
    QQuickItem *item;
    QQuickItemViewTransitionJob *transition;
    QQuickItemViewTransitioner::TransitionType nextTransitionType;
    bool isTransitionTarget : 1;
    bool nextTransitionToSet : 1;
    bool nextTransitionFromSet : 1;
    bool lastMovedToSet : 1;
    bool prepared : 1;

private:
    friend class QQuickItemViewTransitioner;
    friend class QQuickItemViewTransitionJob;
    void setNextTransition(QQuickItemViewTransitioner::TransitionType, bool isTargetItem);
    bool transitionWillChangePosition() const;
    void finishedTransition();
    void resetNextTransitionPos();
    void clearCurrentScheduledTransition();
    void stopTransition();
};


class QQuickViewTransitionAttached : public QObject
{
    Q_OBJECT

    Q_PROPERTY(int index READ index NOTIFY indexChanged)
    Q_PROPERTY(QQuickItem* item READ item NOTIFY itemChanged)
    Q_PROPERTY(QPointF destination READ destination NOTIFY destinationChanged)

    Q_PROPERTY(QList<int> targetIndexes READ targetIndexes NOTIFY targetIndexesChanged)
    Q_PROPERTY(QQmlListProperty<QObject> targetItems READ targetItems NOTIFY targetItemsChanged)

    QML_NAMED_ELEMENT(ViewTransition)
    QML_UNCREATABLE("ViewTransition is only available via attached properties.")
    QML_ATTACHED(QQuickViewTransitionAttached)

public:
    QQuickViewTransitionAttached(QObject *parent);

    int index() const { return m_index; }
    QQuickItem *item() const { return m_item; }
    QPointF destination() const { return m_destination; }

    QList<int> targetIndexes() const { return m_targetIndexes; }
    QQmlListProperty<QObject> targetItems();

    static QQuickViewTransitionAttached *qmlAttachedProperties(QObject *);

Q_SIGNALS:
    void indexChanged();
    void itemChanged();
    void destinationChanged();

    void targetIndexesChanged();
    void targetItemsChanged();

private:
    friend class QQuickItemViewTransitionJob;
    QPointF m_destination;
    QList<int> m_targetIndexes;
    QList<QObject *> m_targetItems;

    QQuickItem *m_item;
    int m_index;
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QQuickViewTransitionAttached)

#endif // QQUICKITEMVIEWTRANSITION_P_P_H
