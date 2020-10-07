/***************************************************************************
**
** Copyright (C) 2013 BlackBerry Limited. All rights reserved.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtCore module of the Qt Toolkit.
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

#ifndef QFILESELECTOR_P_H
#define QFILESELECTOR_P_H

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

#include <QtCore/QString>
#include <QtCore/QFileSelector>
#include <private/qobject_p.h>

QT_BEGIN_NAMESPACE

struct QFileSelectorSharedData //Not QSharedData because currently is just a global store
{
    QStringList staticSelectors;
    QStringList preloadedStatics;
};

class Q_CORE_EXPORT QFileSelectorPrivate : QObjectPrivate //Exported for use in other modules (like QtGui)
{
    Q_DECLARE_PUBLIC(QFileSelector)
public:
    static void updateSelectors();
    static QStringList platformSelectors();
    static void addStatics(const QStringList &); //For loading GUI statics from other Qt modules
    static QString selectionHelper(const QString &path, const QString &fileName,
                                   const QStringList &selectors, const QChar &indicator = QLatin1Char('+'));
    QFileSelectorPrivate();
    QString select(const QString &filePath) const;

    QStringList extras;
};

QT_END_NAMESPACE

#endif

