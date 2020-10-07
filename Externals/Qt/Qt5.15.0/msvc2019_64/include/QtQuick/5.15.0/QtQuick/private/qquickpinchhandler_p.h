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

#ifndef QQUICKPINCHHANDLER_H
#define QQUICKPINCHHANDLER_H

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
#include "qquickmultipointhandler_p.h"
#include <private/qquicktranslate_p.h>
#include "qquickdragaxis_p.h"

QT_BEGIN_NAMESPACE

class Q_QUICK_PRIVATE_EXPORT QQuickPinchHandler : public QQuickMultiPointHandler
{
    Q_OBJECT
    Q_PROPERTY(qreal minimumScale READ minimumScale WRITE setMinimumScale NOTIFY minimumScaleChanged)
    Q_PROPERTY(qreal maximumScale READ maximumScale WRITE setMaximumScale NOTIFY maximumScaleChanged)
    Q_PROPERTY(qreal minimumRotation READ minimumRotation WRITE setMinimumRotation NOTIFY minimumRotationChanged)
    Q_PROPERTY(qreal maximumRotation READ maximumRotation WRITE setMaximumRotation NOTIFY maximumRotationChanged)
    Q_PROPERTY(qreal scale READ scale NOTIFY updated)
    Q_PROPERTY(qreal activeScale READ activeScale NOTIFY updated)
    Q_PROPERTY(qreal rotation READ rotation NOTIFY updated)
    Q_PROPERTY(QVector2D translation READ translation NOTIFY updated)
#if QT_DEPRECATED_SINCE(5, 12)
    Q_PROPERTY(qreal minimumX READ minimumX WRITE setMinimumX NOTIFY minimumXChanged)   // ### Qt 6: remove
    Q_PROPERTY(qreal maximumX READ maximumX WRITE setMaximumX NOTIFY maximumXChanged)   // ### Qt 6: remove
    Q_PROPERTY(qreal minimumY READ minimumY WRITE setMinimumY NOTIFY minimumYChanged)   // ### Qt 6: remove
    Q_PROPERTY(qreal maximumY READ maximumY WRITE setMaximumY NOTIFY maximumYChanged)   // ### Qt 6: remove
#endif
    Q_PROPERTY(QQuickDragAxis * xAxis READ xAxis CONSTANT)
    Q_PROPERTY(QQuickDragAxis * yAxis READ yAxis CONSTANT)
    QML_NAMED_ELEMENT(PinchHandler)
    QML_ADDED_IN_MINOR_VERSION(12)

public:
    explicit QQuickPinchHandler(QQuickItem *parent = nullptr);

    qreal minimumScale() const { return m_minimumScale; }
    void setMinimumScale(qreal minimumScale);

    qreal maximumScale() const { return m_maximumScale; }
    void setMaximumScale(qreal maximumScale);

    qreal minimumRotation() const { return m_minimumRotation; }
    void setMinimumRotation(qreal minimumRotation);

    qreal maximumRotation() const { return m_maximumRotation; }
    void setMaximumRotation(qreal maximumRotation);

    QVector2D translation() const { return m_activeTranslation; }
    qreal scale() const { return m_accumulatedScale; }
    qreal activeScale() const { return m_activeScale; }
    qreal rotation() const { return m_activeRotation; }

#if QT_DEPRECATED_SINCE(5, 12)
    void warnAboutMinMaxDeprecated() const;
    qreal minimumX() const { warnAboutMinMaxDeprecated(); return m_minimumX; }
    void setMinimumX(qreal minX);
    qreal maximumX() const { warnAboutMinMaxDeprecated(); return m_maximumX; }
    void setMaximumX(qreal maxX);
    qreal minimumY() const { warnAboutMinMaxDeprecated(); return m_minimumY; }
    void setMinimumY(qreal minY);
    qreal maximumY() const { warnAboutMinMaxDeprecated(); return m_maximumY; }
    void setMaximumY(qreal maxY);
#endif

    QQuickDragAxis *xAxis() { return &m_xAxis; }
    QQuickDragAxis *yAxis() { return &m_yAxis; }

signals:
    void minimumScaleChanged();
    void maximumScaleChanged();
    void minimumRotationChanged();
    void maximumRotationChanged();
    void minimumXChanged();
    void maximumXChanged();
    void minimumYChanged();
    void maximumYChanged();
    void updated();

protected:
    bool wantsPointerEvent(QQuickPointerEvent *event) override;
    void onActiveChanged() override;
    void handlePointerEventImpl(QQuickPointerEvent *event) override;

private:
    // properties
    qreal m_activeScale = 1;
    qreal m_accumulatedScale = 1;
    qreal m_activeRotation = 0;
    QVector2D m_activeTranslation = QVector2D(0, 0);

    qreal m_minimumScale = -qInf();
    qreal m_maximumScale = qInf();

    qreal m_minimumRotation = -qInf();
    qreal m_maximumRotation = qInf();

    qreal m_minimumX = -qInf();
    qreal m_maximumX = qInf();
    qreal m_minimumY = -qInf();
    qreal m_maximumY = qInf();
    QQuickDragAxis m_xAxis;
    QQuickDragAxis m_yAxis;

    // internal
    qreal m_startScale = 1;
    qreal m_startRotation = 0;
    qreal m_startDistance = 0;
    QPointF m_startPos;
    qreal m_accumulatedStartCentroidDistance = 0;
    QVector<PointData> m_startAngles;
    QQuickMatrix4x4 m_transform;
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QQuickPinchHandler)

#endif // QQUICKPINCHHANDLER_H
