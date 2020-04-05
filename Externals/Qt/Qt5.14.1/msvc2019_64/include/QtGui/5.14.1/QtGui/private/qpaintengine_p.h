/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtGui module of the Qt Toolkit.
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

#ifndef QPAINTENGINE_P_H
#define QPAINTENGINE_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of other Qt classes.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtGui/private/qtguiglobal_p.h>
#include "QtGui/qpainter.h"
#include "QtGui/qpaintengine.h"
#include "QtGui/qregion.h"
#include "private/qobject_p.h"

QT_BEGIN_NAMESPACE

class QPaintDevice;

class Q_GUI_EXPORT QPaintEnginePrivate
{
    Q_DECLARE_PUBLIC(QPaintEngine)
public:
    QPaintEnginePrivate() : pdev(nullptr), q_ptr(nullptr), currentClipDevice(nullptr), hasSystemTransform(0),
                            hasSystemViewport(0) {}
    virtual ~QPaintEnginePrivate();

    QPaintDevice *pdev;
    QPaintEngine *q_ptr;
    QRegion baseSystemClip;
    QRegion systemClip;
    QRect systemRect;
    QRegion systemViewport;
    QTransform systemTransform;
    QPaintDevice *currentClipDevice;
    uint hasSystemTransform : 1;
    uint hasSystemViewport : 1;

    inline void updateSystemClip()
    {
        systemClip = baseSystemClip;
        if (systemClip.isEmpty())
            return;

        if (hasSystemTransform) {
            if (systemTransform.type() <= QTransform::TxTranslate)
                systemClip.translate(qRound(systemTransform.dx()), qRound(systemTransform.dy()));
            else
                systemClip = systemTransform.map(systemClip);
        }

        // Make sure we're inside the viewport.
        if (hasSystemViewport) {
            systemClip &= systemViewport;
            if (systemClip.isEmpty()) {
                // We don't want to paint without system clip, so set it to 1 pixel :)
                systemClip = QRect(systemViewport.boundingRect().topLeft(), QSize(1, 1));
            }
        }
    }

    inline void setSystemTransform(const QTransform &xform)
    {
        systemTransform = xform;
        hasSystemTransform = !xform.isIdentity();
        updateSystemClip();
        if (q_ptr->state)
            systemStateChanged();
    }

    inline void setSystemViewport(const QRegion &region)
    {
        systemViewport = region;
        hasSystemViewport = !systemViewport.isEmpty();
        updateSystemClip();
        if (q_ptr->state)
            systemStateChanged();
    }

    inline void setSystemTransformAndViewport(const QTransform &xform, const QRegion &region)
    {
        systemTransform = xform;
        hasSystemTransform = !xform.isIdentity();
        systemViewport = region;
        hasSystemViewport = !systemViewport.isEmpty();
        updateSystemClip();
        if (q_ptr->state)
            systemStateChanged();
    }

    virtual void systemStateChanged() { }

    void drawBoxTextItem(const QPointF &p, const QTextItemInt &ti);

    static QPaintEnginePrivate *get(QPaintEngine *paintEngine) { return paintEngine->d_func(); }

    virtual QPaintEngine *aggregateEngine() { return nullptr; }
    virtual Qt::HANDLE nativeHandle() { return nullptr; }
};

QT_END_NAMESPACE

#endif // QPAINTENGINE_P_H
