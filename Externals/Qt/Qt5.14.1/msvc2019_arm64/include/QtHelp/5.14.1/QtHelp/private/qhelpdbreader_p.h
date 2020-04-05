/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#ifndef QHELPDBREADER_H
#define QHELPDBREADER_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API. It exists for the convenience
// of the help generator tools. This header file may change from version
// to version without notice, or even be removed.
//
// We mean it.
//

#include <QtCore/QObject>
#include <QtCore/QStringList>
#include <QtCore/QUrl>
#include <QtCore/QByteArray>
#include <QtCore/QSet>

QT_BEGIN_NAMESPACE

class QSqlQuery;

class QHelpDBReader : public QObject
{
    Q_OBJECT

public:
    class IndexItem
    {
    public:
        IndexItem() = default;
        QString name;
        QString identifier;
        int fileId = 0;
        QString anchor;
        QStringList filterAttributes;
    };

    class FileItem
    {
    public:
        FileItem() = default;
        QString name;
        QString title;
        QStringList filterAttributes;
    };

    class ContentsItem
    {
    public:
        ContentsItem() = default;
        QByteArray data;
        QStringList filterAttributes;
    };

    class IndexTable
    {
    public:
        QList<IndexItem> indexItems;
        QList<FileItem> fileItems;
        QList<ContentsItem> contentsItems;
        QStringList usedFilterAttributes;
    };

    QHelpDBReader(const QString &dbName);
    QHelpDBReader(const QString &dbName, const QString &uniqueId,
        QObject *parent);
    ~QHelpDBReader();

    bool init();

    QString namespaceName() const;
    QString virtualFolder() const;
    QString version() const;
    IndexTable indexTable() const;
    QList<QStringList> filterAttributeSets() const;
    QMap<QString, QByteArray> filesData(const QStringList &filterAttributes,
        const QString &extensionFilter = QString()) const;
    QByteArray fileData(const QString &virtualFolder,
        const QString &filePath) const;

    QStringList customFilters() const;
    QStringList filterAttributes(const QString &filterName = QString()) const;

    QVariant metaData(const QString &name) const;

private:
    QString quote(const QString &string) const;
    bool initDB();
    QString qtVersionHeuristic() const;

    bool m_initDone = false;
    QString m_dbName;
    QString m_uniqueId;
    QString m_error;
    QSqlQuery *m_query = nullptr;
    mutable QString m_namespace;
};

QT_END_NAMESPACE

#endif
