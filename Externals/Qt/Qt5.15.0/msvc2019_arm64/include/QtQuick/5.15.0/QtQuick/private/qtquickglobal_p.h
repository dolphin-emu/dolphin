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

#ifndef QTQUICKGLOBAL_P_H
#define QTQUICKGLOBAL_P_H

#include <QtQml/private/qtqmlglobal_p.h>
#include <QtGui/private/qtguiglobal_p.h>
#include <QtQuick/private/qtquick-config_p.h>

#include <QtCore/qloggingcategory.h>

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

#include "qtquickglobal.h"

#define Q_QUICK_PRIVATE_EXPORT Q_QUICK_EXPORT

void Q_QUICK_PRIVATE_EXPORT qml_register_types_QtQuick();

QT_BEGIN_NAMESPACE

void QQuick_initializeProviders();
void QQuick_deinitializeProviders();

Q_DECLARE_LOGGING_CATEGORY(DBG_TOUCH)
Q_DECLARE_LOGGING_CATEGORY(DBG_MOUSE)
Q_DECLARE_LOGGING_CATEGORY(DBG_FOCUS)
Q_DECLARE_LOGGING_CATEGORY(DBG_DIRTY)

QT_END_NAMESPACE

#endif // QTQUICKGLOBAL_P_H
