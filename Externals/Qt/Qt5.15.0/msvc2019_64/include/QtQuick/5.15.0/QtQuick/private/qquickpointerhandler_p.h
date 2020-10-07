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

#ifndef QQUICKPOINTERHANDLER_H
#define QQUICKPOINTERHANDLER_H

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

#include <QtQuick/private/qquickevents_p_p.h>
#include <QtQuick/private/qquickitem_p.h>

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(lcPointerHandlerDispatch)

class QQuickPointerHandlerPrivate;

class Q_QUICK_PRIVATE_EXPORT QQuickPointerHandler : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)

    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
    Q_PROPERTY(QQuickItem * target READ target WRITE setTarget NOTIFY targetChanged)
    Q_PROPERTY(QQuickItem * parent READ parentItem CONSTANT)
    Q_PROPERTY(GrabPermissions grabPermissions READ grabPermissions WRITE setGrabPermissions NOTIFY grabPermissionChanged)
    Q_PROPERTY(qreal margin READ margin WRITE setMargin NOTIFY marginChanged)
    Q_PROPERTY(int dragThreshold READ dragThreshold WRITE setDragThreshold RESET resetDragThreshold NOTIFY dragThresholdChanged REVISION 15)
#if QT_CONFIG(cursor)
    Q_PROPERTY(Qt::CursorShape cursorShape READ cursorShape WRITE setCursorShape RESET resetCursorShape NOTIFY cursorShapeChanged REVISION 15)
#endif

    QML_NAMED_ELEMENT(PointerHandler)
    QML_UNCREATABLE("PointerHandler is an abstract base class.")
    QML_ADDED_IN_MINOR_VERSION(12)

public:
    explicit QQuickPointerHandler(QQuickItem *parent = nullptr);
    ~QQuickPointerHandler();

    enum GrabPermission {
        TakeOverForbidden = 0x0,
        CanTakeOverFromHandlersOfSameType = 0x01,
        CanTakeOverFromHandlersOfDifferentType= 0x02,
        CanTakeOverFromItems = 0x04,
        CanTakeOverFromAnything = 0x0F,
        ApprovesTakeOverByHandlersOfSameType = 0x10,
        ApprovesTakeOverByHandlersOfDifferentType= 0x20,
        ApprovesTakeOverByItems = 0x40,
        ApprovesCancellation = 0x80,
        ApprovesTakeOverByAnything = 0xF0
    };
    Q_DECLARE_FLAGS(GrabPermissions, GrabPermission)
    Q_FLAG(GrabPermissions)

public:
    bool enabled() const;
    void setEnabled(bool enabled);

    bool active() const;

    QQuickItem *target() const;
    void setTarget(QQuickItem *target);

    QQuickItem * parentItem() const;

    void handlePointerEvent(QQuickPointerEvent *event);

    GrabPermissions grabPermissions() const;
    void setGrabPermissions(GrabPermissions grabPermissions);

    qreal margin() const;
    void setMargin(qreal pointDistanceThreshold);

    int dragThreshold() const;
    void setDragThreshold(int t);
    void resetDragThreshold();

#if QT_CONFIG(cursor)
    Qt::CursorShape cursorShape() const;
    void setCursorShape(Qt::CursorShape shape);
    void resetCursorShape();
    bool isCursorShapeExplicitlySet() const;
#endif

Q_SIGNALS:
    void enabledChanged();
    void activeChanged();
    void targetChanged();
    void marginChanged();
    Q_REVISION(15) void dragThresholdChanged();
    void grabChanged(QQuickEventPoint::GrabTransition transition, QQuickEventPoint *point);
    void grabPermissionChanged();
    void canceled(QQuickEventPoint *point);
#if QT_CONFIG(cursor)
    Q_REVISION(15) void cursorShapeChanged();
#endif

protected:
    QQuickPointerHandler(QQuickPointerHandlerPrivate &dd, QQuickItem *parent);

    void classBegin() override;
    void componentComplete() override;

    QQuickPointerEvent *currentEvent();
    virtual bool wantsPointerEvent(QQuickPointerEvent *event);
    virtual bool wantsEventPoint(QQuickEventPoint *point);
    virtual void handlePointerEventImpl(QQuickPointerEvent *event);
    void setActive(bool active);
    virtual void onTargetChanged(QQuickItem *oldTarget) { Q_UNUSED(oldTarget); }
    virtual void onActiveChanged() { }
    virtual void onGrabChanged(QQuickPointerHandler *grabber, QQuickEventPoint::GrabTransition transition, QQuickEventPoint *point);
    virtual bool canGrab(QQuickEventPoint *point);
    virtual bool approveGrabTransition(QQuickEventPoint *point, QObject *proposedGrabber);
    void setPassiveGrab(QQuickEventPoint *point, bool grab = true);
    bool setExclusiveGrab(QQuickEventPoint *point, bool grab = true);
    void cancelAllGrabs(QQuickEventPoint *point);
    QPointF eventPos(const QQuickEventPoint *point) const;
    bool parentContains(const QQuickEventPoint *point) const;

    friend class QQuickEventPoint;
    friend class QQuickItemPrivate;
    friend class QQuickWindowPrivate;

    Q_DECLARE_PRIVATE(QQuickPointerHandler)
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QQuickPointerHandler::GrabPermissions)

QT_END_NAMESPACE

QML_DECLARE_TYPE(QQuickPointerHandler)

#endif // QQUICKPOINTERHANDLER_H
