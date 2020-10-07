/****************************************************************************
**
** Copyright (C) 2016 BlackBerry Limited. All rights reserved.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#ifndef QQMLFILESELECTOR_H
#define QQMLFILESELECTOR_H

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtQml/QQmlEngine>
#include <QtQml/qtqmlglobal.h>

QT_BEGIN_NAMESPACE

class QFileSelector;
class QQmlFileSelectorPrivate;
class Q_QML_EXPORT QQmlFileSelector : public QObject
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QQmlFileSelector)
public:
    explicit QQmlFileSelector(QQmlEngine *engine, QObject *parent = nullptr);
    ~QQmlFileSelector() override;
    QFileSelector *selector() const Q_DECL_NOTHROW;
    void setSelector(QFileSelector *selector);
    void setExtraSelectors(QStringList &strings); // TODO Qt6: remove
    void setExtraSelectors(const QStringList &strings);
    static QQmlFileSelector* get(QQmlEngine*);

private:
    Q_DISABLE_COPY(QQmlFileSelector)
};

QT_END_NAMESPACE

#endif
