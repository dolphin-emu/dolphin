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

#ifndef QDND_P_H
#define QDND_P_H

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
#include "QtCore/qobject.h"
#include "QtCore/qmap.h"
#include "QtCore/qmimedata.h"
#include "QtGui/qdrag.h"
#include "QtGui/qpixmap.h"
#include "QtGui/qcursor.h"
#include "QtGui/qwindow.h"
#include "QtCore/qpoint.h"
#include "private/qobject_p.h"
#include "QtGui/qbackingstore.h"

QT_REQUIRE_CONFIG(draganddrop);

QT_BEGIN_NAMESPACE

class QPlatformDrag;

class QDragPrivate : public QObjectPrivate
{
public:
    QDragPrivate()
        : source(nullptr)
        , target(nullptr)
        , data(nullptr)
    { }
    QObject *source;
    QObject *target;
    QMimeData *data;
    QPixmap pixmap;
    QPoint hotspot;
    Qt::DropAction executed_action;
    Qt::DropActions supported_actions;
    Qt::DropAction default_action;
    QMap<Qt::DropAction, QPixmap> customCursors;
};

class Q_GUI_EXPORT QDragManager : public QObject {
    Q_OBJECT

public:
    QDragManager();
    ~QDragManager();
    static QDragManager *self();

    Qt::DropAction drag(QDrag *);

    void setCurrentTarget(QObject *target, bool dropped = false);
    QObject *currentTarget() const;

    QPointer<QDrag> object() const { return m_object; }
    QObject *source() const;

private:
    QObject *m_currentDropTarget;
    QPlatformDrag *m_platformDrag;
    QPointer<QDrag> m_object;

    static QDragManager *m_instance;
    Q_DISABLE_COPY_MOVE(QDragManager)
};

QT_END_NAMESPACE

#endif // QDND_P_H
