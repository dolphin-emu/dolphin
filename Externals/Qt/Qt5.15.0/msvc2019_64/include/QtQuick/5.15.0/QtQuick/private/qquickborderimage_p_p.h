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

#ifndef QQUICKBORDERIMAGE_P_P_H
#define QQUICKBORDERIMAGE_P_P_H

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

#include "qquickimagebase_p_p.h"
#include "qquickscalegrid_p_p.h"

#include <private/qqmlglobal_p.h>

QT_BEGIN_NAMESPACE

#if QT_CONFIG(qml_network)
class QNetworkReply;
#endif
class QQuickBorderImagePrivate : public QQuickImageBasePrivate
{
    Q_DECLARE_PUBLIC(QQuickBorderImage)

public:
    QQuickBorderImagePrivate()
      : border(0), horizontalTileMode(QQuickBorderImage::Stretch),
        verticalTileMode(QQuickBorderImage::Stretch), pixmapChanged(false)
#if QT_CONFIG(qml_network)
      , sciReply(0), redirectCount(0)
#endif
    {
    }

    ~QQuickBorderImagePrivate()
    {
    }


    QQuickScaleGrid *getScaleGrid()
    {
        Q_Q(QQuickBorderImage);
        if (!border) {
            border = new QQuickScaleGrid(q);
            qmlobject_connect(border, QQuickScaleGrid, SIGNAL(borderChanged()),
                              q, QQuickBorderImage, SLOT(doUpdate()));
        }
        return border;
    }

    static void calculateRects(const QQuickScaleGrid *border,
                               const QSize &sourceSize,
                               const QSizeF &targetSize,
                               int horizontalTileMode,
                               int verticalTileMode,
                               qreal devicePixelRatio,
                               QRectF *targetRect,
                               QRectF *innerTargetRect,
                               QRectF *innerSourceRect,
                               QRectF *subSourceRect);

    QQuickScaleGrid *border;
    QUrl sciurl;
    QQuickBorderImage::TileMode horizontalTileMode;
    QQuickBorderImage::TileMode verticalTileMode;
    bool pixmapChanged : 1;

#if QT_CONFIG(qml_network)
    QNetworkReply *sciReply;
    int redirectCount;
#endif
};

QT_END_NAMESPACE

#endif // QQUICKBORDERIMAGE_P_P_H
