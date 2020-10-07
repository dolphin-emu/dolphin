/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Assistant of the Qt Toolkit.
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

#ifndef QHELPFILTERENGINE_H
#define QHELPFILTERENGINE_H

#include <QtHelp/qhelp_global.h>

#include <QtCore/QObject>

QT_BEGIN_NAMESPACE

template <class K, class T>
class QMap;
class QVersionNumber;

class QHelpCollectionHandler;
class QHelpEngineCore;
class QHelpFilterData;
class QHelpFilterEnginePrivate;

class QHELP_EXPORT QHelpFilterEngine : public QObject
{
    Q_OBJECT
public:
    QMap<QString, QString> namespaceToComponent() const;
    QMap<QString, QVersionNumber> namespaceToVersion() const;

    QStringList filters() const;

    QString activeFilter() const;
    bool setActiveFilter(const QString &filterName);

    QStringList availableComponents() const;
    QList<QVersionNumber> availableVersions() const;

    QHelpFilterData filterData(const QString &filterName) const;
    bool setFilterData(const QString &filterName, const QHelpFilterData &filterData);

    bool removeFilter(const QString &filterName);

    QStringList namespacesForFilter(const QString &filterName) const;

    QStringList indices() const;
    QStringList indices(const QString &filterName) const;

Q_SIGNALS:
    void filterActivated(const QString &newFilter);

protected:
    explicit QHelpFilterEngine(QHelpEngineCore *helpEngine);
    virtual ~QHelpFilterEngine();

private:
    void setCollectionHandler(QHelpCollectionHandler *collectionHandler);

    QHelpFilterEnginePrivate *d;
    friend class QHelpEngineCore;
    friend class QHelpEngineCorePrivate;
};

QT_END_NAMESPACE

#endif // QHELPFILTERENGINE_H
