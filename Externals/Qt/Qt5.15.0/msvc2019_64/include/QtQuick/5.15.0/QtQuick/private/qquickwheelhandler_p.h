/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
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

#ifndef QQUICKWHEELHANDLER_H
#define QQUICKWHEELHANDLER_H

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

#include "qquickitem.h"
#include "qevent.h"
#include "qquicksinglepointhandler_p.h"

QT_BEGIN_NAMESPACE

class QQuickWheelEvent;
class QQuickWheelHandlerPrivate;

class Q_QUICK_PRIVATE_EXPORT QQuickWheelHandler : public QQuickSinglePointHandler
{
    Q_OBJECT
    Q_PROPERTY(Qt::Orientation orientation READ orientation WRITE setOrientation NOTIFY orientationChanged)
    Q_PROPERTY(bool invertible READ isInvertible WRITE setInvertible NOTIFY invertibleChanged)
    Q_PROPERTY(qreal activeTimeout READ activeTimeout WRITE setActiveTimeout NOTIFY activeTimeoutChanged)
    Q_PROPERTY(qreal rotation READ rotation WRITE setRotation NOTIFY rotationChanged)
    Q_PROPERTY(qreal rotationScale READ rotationScale WRITE setRotationScale NOTIFY rotationScaleChanged)
    Q_PROPERTY(QString property READ property WRITE setProperty NOTIFY propertyChanged)
    Q_PROPERTY(qreal targetScaleMultiplier READ targetScaleMultiplier WRITE setTargetScaleMultiplier NOTIFY targetScaleMultiplierChanged)
    Q_PROPERTY(bool targetTransformAroundCursor READ isTargetTransformAroundCursor WRITE setTargetTransformAroundCursor NOTIFY targetTransformAroundCursorChanged)

    QML_NAMED_ELEMENT(WheelHandler)
    QML_ADDED_IN_MINOR_VERSION(14)

public:
    explicit QQuickWheelHandler(QQuickItem *parent = nullptr);

    Qt::Orientation orientation() const;
    void setOrientation(Qt::Orientation orientation);

    bool isInvertible() const;
    void setInvertible(bool invertible);

    qreal activeTimeout() const;
    void setActiveTimeout(qreal timeout);

    qreal rotation() const;
    void setRotation(qreal rotation);

    qreal rotationScale() const;
    void setRotationScale(qreal rotationScale);

    QString property() const;
    void setProperty(const QString &name);

    qreal targetScaleMultiplier() const;
    void setTargetScaleMultiplier(qreal targetScaleMultiplier);

    bool isTargetTransformAroundCursor() const;
    void setTargetTransformAroundCursor(bool ttac);

Q_SIGNALS:
    void wheel(QQuickPointerScrollEvent *event);

    void orientationChanged();
    void invertibleChanged();
    void activeTimeoutChanged();
    void rotationChanged();
    void rotationScaleChanged();
    void propertyChanged();
    void targetScaleMultiplierChanged();
    void targetTransformAroundCursorChanged();

protected:
    bool wantsPointerEvent(QQuickPointerEvent *event) override;
    void handleEventPoint(QQuickEventPoint *point) override;
    void onTargetChanged(QQuickItem *oldTarget) override;
    void onActiveChanged() override;
    void timerEvent(QTimerEvent *event) override;

    Q_DECLARE_PRIVATE(QQuickWheelHandler)
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QQuickWheelHandler)

#endif // QQUICKWHEELHANDLER_H
