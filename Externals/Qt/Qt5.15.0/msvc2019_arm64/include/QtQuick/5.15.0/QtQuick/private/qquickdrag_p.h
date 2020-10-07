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

#ifndef QQUICKDRAG_P_H
#define QQUICKDRAG_P_H

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

#include <QtQuick/qquickitem.h>

#include <private/qintrusivelist_p.h>
#include <private/qqmlguard_p.h>

#include <QtCore/qmimedata.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qurl.h>

QT_REQUIRE_CONFIG(quick_draganddrop);

QT_BEGIN_NAMESPACE

class QQuickItem;
class QQuickDrag;
class QQuickDragPrivate;

class QQuickDragGrabber
{
    class Item : public QQmlGuard<QQuickItem>
    {
    public:
        Item(QQuickItem *item) : QQmlGuard<QQuickItem>(item) {}

        QIntrusiveListNode node;
    protected:
        void objectDestroyed(QQuickItem *) override { delete this; }
    };

    typedef QIntrusiveList<Item, &Item::node> ItemList;

public:
    QQuickDragGrabber() : m_target(nullptr) {}
    ~QQuickDragGrabber() { while (!m_items.isEmpty()) delete m_items.first(); }


    QObject *target() const
    {
        if (m_target)
            return m_target;
        else if (!m_items.isEmpty())
            return *m_items.first();
        else
            return nullptr;
    }
    void setTarget(QObject *target) { m_target = target; }
    void resetTarget() { m_target = nullptr; }

    bool isEmpty() const { return m_items.isEmpty(); }

    typedef ItemList::iterator iterator;
    iterator begin() { return m_items.begin(); }
    iterator end() { return m_items.end(); }

    void grab(QQuickItem *item) { m_items.insert(new Item(item)); }
    iterator release(iterator at) { Item *item = *at; at = at.erase(); delete item; return at; }

private:

    ItemList m_items;
    QObject *m_target;
};

class QQuickDropEventEx : public QDropEvent
{
public:
    void setProposedAction(Qt::DropAction action) { default_action = action; drop_action = action; }

    static void setProposedAction(QDropEvent *event, Qt::DropAction action) {
        static_cast<QQuickDropEventEx *>(event)->setProposedAction(action);
    }

    void copyActions(const QDropEvent &from) {
        default_action = from.proposedAction(); drop_action = from.dropAction(); }

    static void copyActions(QDropEvent *to, const QDropEvent &from) {
        static_cast<QQuickDropEventEx *>(to)->copyActions(from);
    }
};

class QQuickDragMimeData : public QMimeData
{
    Q_OBJECT
public:
    QQuickDragMimeData()
        : m_source(nullptr)
    {
    }

    QStringList keys() const { return m_keys; }
    QObject *source() const { return m_source; }

private:
    QObject *m_source;
    Qt::DropActions m_supportedActions;
    QStringList m_keys;

    friend class QQuickDragAttached;
    friend class QQuickDragAttachedPrivate;
};

class QQmlV4Function;
class QQuickDragAttached;
class Q_AUTOTEST_EXPORT QQuickDrag : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QQuickItem *target READ target WRITE setTarget NOTIFY targetChanged RESET resetTarget)
    Q_PROPERTY(Axis axis READ axis WRITE setAxis NOTIFY axisChanged)
    Q_PROPERTY(qreal minimumX READ xmin WRITE setXmin NOTIFY minimumXChanged)
    Q_PROPERTY(qreal maximumX READ xmax WRITE setXmax NOTIFY maximumXChanged)
    Q_PROPERTY(qreal minimumY READ ymin WRITE setYmin NOTIFY minimumYChanged)
    Q_PROPERTY(qreal maximumY READ ymax WRITE setYmax NOTIFY maximumYChanged)
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
    Q_PROPERTY(bool filterChildren READ filterChildren WRITE setFilterChildren NOTIFY filterChildrenChanged)
    Q_PROPERTY(bool smoothed READ smoothed WRITE setSmoothed NOTIFY smoothedChanged)
    // Note, threshold was added in QtQuick 2.2 but REVISION is not supported (or needed) for grouped
    // properties See QTBUG-33179
    Q_PROPERTY(qreal threshold READ threshold WRITE setThreshold NOTIFY thresholdChanged RESET resetThreshold)
    //### consider drag and drop

    QML_NAMED_ELEMENT(Drag)
    QML_UNCREATABLE("Drag is only available via attached properties.")
    QML_ATTACHED(QQuickDragAttached)

public:
    QQuickDrag(QObject *parent=nullptr);
    ~QQuickDrag();

    enum DragType { None, Automatic, Internal };
    Q_ENUM(DragType)

    QQuickItem *target() const;
    void setTarget(QQuickItem *target);
    void resetTarget();

    enum Axis { XAxis=0x01, YAxis=0x02, XAndYAxis=0x03, XandYAxis=XAndYAxis };
    Q_ENUM(Axis)
    Axis axis() const;
    void setAxis(Axis);

    qreal xmin() const;
    void setXmin(qreal);
    qreal xmax() const;
    void setXmax(qreal);
    qreal ymin() const;
    void setYmin(qreal);
    qreal ymax() const;
    void setYmax(qreal);

    bool smoothed() const;
    void setSmoothed(bool smooth);

    qreal threshold() const;
    void setThreshold(qreal);
    void resetThreshold();

    bool active() const;
    void setActive(bool);

    bool filterChildren() const;
    void setFilterChildren(bool);

    static QQuickDragAttached *qmlAttachedProperties(QObject *obj);

Q_SIGNALS:
    void targetChanged();
    void axisChanged();
    void minimumXChanged();
    void maximumXChanged();
    void minimumYChanged();
    void maximumYChanged();
    void activeChanged();
    void filterChildrenChanged();
    void smoothedChanged();
    void thresholdChanged();

private:
    QQuickItem *_target;
    Axis _axis;
    qreal _xmin;
    qreal _xmax;
    qreal _ymin;
    qreal _ymax;
    bool _active : 1;
    bool _filterChildren: 1;
    bool _smoothed : 1;
    qreal _threshold;
    Q_DISABLE_COPY(QQuickDrag)
};

class QQuickDragAttachedPrivate;
class QQuickDragAttached : public QObject
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QQuickDragAttached)

    Q_PROPERTY(bool active READ isActive WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(QObject *source READ source WRITE setSource NOTIFY sourceChanged RESET resetSource)
    Q_PROPERTY(QObject *target READ target NOTIFY targetChanged)
    Q_PROPERTY(QPointF hotSpot READ hotSpot WRITE setHotSpot NOTIFY hotSpotChanged)
    Q_PROPERTY(QUrl imageSource READ imageSource WRITE setImageSource NOTIFY imageSourceChanged)
    Q_PROPERTY(QStringList keys READ keys WRITE setKeys NOTIFY keysChanged)
    Q_PROPERTY(QVariantMap mimeData READ mimeData WRITE setMimeData NOTIFY mimeDataChanged)
    Q_PROPERTY(Qt::DropActions supportedActions READ supportedActions WRITE setSupportedActions NOTIFY supportedActionsChanged)
    Q_PROPERTY(Qt::DropAction proposedAction READ proposedAction WRITE setProposedAction NOTIFY proposedActionChanged)
    Q_PROPERTY(QQuickDrag::DragType dragType READ dragType WRITE setDragType NOTIFY dragTypeChanged)

    QML_ANONYMOUS

public:
    QQuickDragAttached(QObject *parent);
    ~QQuickDragAttached();

    bool isActive() const;
    void setActive(bool active);

    QObject *source() const;
    void setSource(QObject *item);
    void resetSource();

    QObject *target() const;

    QPointF hotSpot() const;
    void setHotSpot(const QPointF &hotSpot);

    QUrl imageSource() const;
    void setImageSource(const QUrl &url);

    QStringList keys() const;
    void setKeys(const QStringList &keys);

    QVariantMap mimeData() const;
    void setMimeData(const QVariantMap &mimeData);

    Qt::DropActions supportedActions() const;
    void setSupportedActions(Qt::DropActions actions);

    Qt::DropAction proposedAction() const;
    void setProposedAction(Qt::DropAction action);

    QQuickDrag::DragType dragType() const;
    void setDragType(QQuickDrag::DragType dragType);

    Q_INVOKABLE int drop();

    bool event(QEvent *event) override;

public Q_SLOTS:
    void start(QQmlV4Function *);
    void startDrag(QQmlV4Function *);
    void cancel();

Q_SIGNALS:
    void dragStarted();
    void dragFinished(Qt::DropAction dropAction);

    void activeChanged();
    void sourceChanged();
    void targetChanged();
    void hotSpotChanged();
    void imageSourceChanged();
    void keysChanged();
    void mimeDataChanged();
    void supportedActionsChanged();
    void proposedActionChanged();
    void dragTypeChanged();
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QQuickDrag)

#endif
