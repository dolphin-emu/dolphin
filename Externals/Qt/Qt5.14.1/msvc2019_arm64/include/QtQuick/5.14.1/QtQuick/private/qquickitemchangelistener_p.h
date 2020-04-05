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

#ifndef QQUICKITEMCHANGELISTENER_P_H
#define QQUICKITEMCHANGELISTENER_P_H

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

#include <QtCore/qglobal.h>

QT_BEGIN_NAMESPACE

class QRectF;
class QQuickItem;
class QQuickAnchorsPrivate;

class QQuickGeometryChange
{
public:
    enum Kind: int {
        Nothing = 0x00,
        X       = 0x01,
        Y       = 0x02,
        Width   = 0x04,
        Height  = 0x08,

        Size = Width | Height,
        All = X | Y | Size
    };

    QQuickGeometryChange(int change = Nothing)
        : kind(change)
    {}

    bool noChange() const { return kind == Nothing; }
    bool anyChange() const { return !noChange(); }

    bool xChange() const { return kind & X; }
    bool yChange() const { return kind & Y; }
    bool widthChange() const { return kind & Width; }
    bool heightChange() const { return kind & Height; }

    bool positionChange() const { return xChange() || yChange(); }
    bool sizeChange() const { return widthChange() || heightChange(); }

    bool horizontalChange() const { return xChange() || widthChange(); }
    bool verticalChange() const { return yChange() || heightChange(); }

    void setXChange(bool enabled) { set(X, enabled); }
    void setYChange(bool enabled) { set(Y, enabled); }
    void setWidthChange(bool enabled) { set(Width, enabled); }
    void setHeightChange(bool enabled) { set(Height, enabled); }
    void setSizeChange(bool enabled) { set(Size, enabled); }
    void setAllChanged(bool enabled) { set(All, enabled); }
    void setHorizontalChange(bool enabled) { set(X | Width, enabled); }
    void setVerticalChange(bool enabled) { set(Y | Height, enabled); }

    void set(int bits, bool enabled)
    {
        if (enabled) {
            kind |= bits;
        } else {
            kind &= ~bits;
        }
    }

    bool matches(QQuickGeometryChange other) const { return kind & other.kind; }

private:
    int kind;
};

#define QT_QUICK_NEW_GEOMETRY_CHANGED_HANDLING

class QQuickItemChangeListener
{
public:
    virtual ~QQuickItemChangeListener() {}

    virtual void itemGeometryChanged(QQuickItem *, QQuickGeometryChange, const QRectF & /* oldGeometry */) {}
    virtual void itemSiblingOrderChanged(QQuickItem *) {}
    virtual void itemVisibilityChanged(QQuickItem *) {}
    virtual void itemEnabledChanged(QQuickItem *) {}
    virtual void itemOpacityChanged(QQuickItem *) {}
    virtual void itemDestroyed(QQuickItem *) {}
    virtual void itemChildAdded(QQuickItem *, QQuickItem * /* child */ ) {}
    virtual void itemChildRemoved(QQuickItem *, QQuickItem * /* child */ ) {}
    virtual void itemParentChanged(QQuickItem *, QQuickItem * /* parent */ ) {}
    virtual void itemRotationChanged(QQuickItem *) {}
    virtual void itemImplicitWidthChanged(QQuickItem *) {}
    virtual void itemImplicitHeightChanged(QQuickItem *) {}

    virtual QQuickAnchorsPrivate *anchorPrivate() { return nullptr; }
};

QT_END_NAMESPACE

#endif // QQUICKITEMCHANGELISTENER_P_H
