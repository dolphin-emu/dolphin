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

#ifndef QHELPCOLLECTIONHANDLER_H
#define QHELPCOLLECTIONHANDLER_H

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

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QObject>
#include <QtCore/QVariant>
#include <QtCore/QStringList>

#include <QtSql/QSqlQuery>

#include "qhelpdbreader_p.h"

QT_BEGIN_NAMESPACE

class QVersionNumber;
class QHelpFilterData;

class QHelpCollectionHandler : public QObject
{
    Q_OBJECT

public:
    struct FileInfo
    {
        QString fileName;
        QString folderName;
        QString namespaceName;
    };
    typedef QList<FileInfo> FileInfoList;

    struct TimeStamp
    {
        int namespaceId = -1;
        int folderId = -1;
        QString fileName;
        int size = 0;
        QString timeStamp;
    };

    struct ContentsData
    {
        QString namespaceName;
        QString folderName;
        QList<QByteArray> contentsList;
    };

    explicit QHelpCollectionHandler(const QString &collectionFile,
        QObject *parent = nullptr);
    ~QHelpCollectionHandler();

    QString collectionFile() const;

    bool openCollectionFile();
    bool copyCollectionFile(const QString &fileName);

    // *** Legacy block start ***
    //  legacy API since Qt 5.13

    // use filters() instead
    QStringList customFilters() const;

    // use QHelpFilterEngine::removeFilter() instead
    bool removeCustomFilter(const QString &filterName);

    // use QHelpFilterEngine::setFilterData() instead
    bool addCustomFilter(const QString &filterName,
        const QStringList &attributes);

    // use files(const QString &, const QString &, const QString &) instead
    QStringList files(const QString &namespaceName,
                      const QStringList &filterAttributes,
                      const QString &extensionFilter) const;

    // use namespaceForFile(const QUrl &, const QString &) instead
    QString namespaceForFile(const QUrl &url,
                             const QStringList &filterAttributes) const;

    // use findFile(const QUrl &, const QString &) instead
    QUrl findFile(const QUrl &url,
                  const QStringList &filterAttributes) const;

    // use indicesForFilter(const QString &) instead
    QStringList indicesForFilter(const QStringList &filterAttributes) const;

    // use contentsForFilter(const QString &) instead
    QList<ContentsData> contentsForFilter(const QStringList &filterAttributes) const;

    // use QHelpFilterEngine::activeFilter() and filterData(const QString &) instead;
    QStringList filterAttributes() const;

    // use filterData(const QString &) instead
    QStringList filterAttributes(const QString &filterName) const;

    // use filterData(const QString &) instead
    QList<QStringList> filterAttributeSets(const QString &namespaceName) const;

    // use linksForIdentifier(const QString &, const QString &) instead
    QMap<QString, QUrl> linksForIdentifier(const QString &id,
                                           const QStringList &filterAttributes) const;

    // use linksForKeyword(const QString &, const QString &) instead
    QMap<QString, QUrl> linksForKeyword(const QString &keyword,
                                        const QStringList &filterAttributes) const;

    // *** Legacy block end ***

    QStringList filters() const;

    QStringList availableComponents() const;
    QStringList availableVersions() const;
    QMap<QString, QString> namespaceToComponent() const;
    QMap<QString, QVersionNumber> namespaceToVersion() const;
    QHelpFilterData filterData(const QString &filterName) const;
    bool setFilterData(const QString &filterName, const QHelpFilterData &filterData);
    bool removeFilter(const QString &filterName);


    FileInfo registeredDocumentation(const QString &namespaceName) const;
    FileInfoList registeredDocumentations() const;
    bool registerDocumentation(const QString &fileName);
    bool unregisterDocumentation(const QString &namespaceName);


    bool fileExists(const QUrl &url) const;
    QStringList files(const QString &namespaceName,
                      const QString &filterName,
                      const QString &extensionFilter) const;
    QString namespaceForFile(const QUrl &url,
                             const QString &filterName) const;
    QUrl findFile(const QUrl &url,
                  const QString &filterName) const;
    QByteArray fileData(const QUrl &url) const;


    QStringList indicesForFilter(const QString &filterName) const;
    QList<ContentsData> contentsForFilter(const QString &filterName) const;

    bool removeCustomValue(const QString &key);
    QVariant customValue(const QString &key, const QVariant &defaultValue) const;
    bool setCustomValue(const QString &key, const QVariant &value);


    int registerNamespace(const QString &nspace, const QString &fileName);
    int registerVirtualFolder(const QString &folderName, int namespaceId);
    int registerComponent(const QString &componentName, int namespaceId);
    bool registerVersion(const QString &version, int namespaceId);

    QMap<QString, QUrl> linksForIdentifier(const QString &id,
                                           const QString &filterName) const;
    QMap<QString, QUrl> linksForKeyword(const QString &keyword,
                                        const QString &filterName) const;
    QStringList namespacesForFilter(const QString &filterName) const;

    void setReadOnly(bool readOnly);

signals:
    void error(const QString &msg) const;

private:
    // legacy stuff
    QMap<QString, QUrl> linksForField(const QString &fieldName,
                                      const QString &fieldValue,
                                      const QStringList &filterAttributes) const;

    QString namespaceVersion(const QString &namespaceName) const;
    QMap<QString, QUrl> linksForField(const QString &fieldName,
                                      const QString &fieldValue,
                                      const QString &filterName) const;
    bool isDBOpened() const;
    bool createTables(QSqlQuery *query);
    void closeDB();
    bool recreateIndexAndNamespaceFilterTables(QSqlQuery *query);
    bool registerIndexAndNamespaceFilterTables(const QString &nameSpace,
                                               bool createDefaultVersionFilter = false);
    void createVersionFilter(const QString &version);
    bool registerFilterAttributes(const QList<QStringList> &attributeSets, int nsId);
    bool registerFileAttributeSets(const QList<QStringList> &attributeSets, int nsId);
    bool registerIndexTable(const QHelpDBReader::IndexTable &indexTable,
                            int nsId, int vfId, const QString &fileName);
    bool unregisterIndexTable(int nsId, int vfId);
    QString absoluteDocPath(const QString &fileName) const;
    bool isTimeStampCorrect(const TimeStamp &timeStamp) const;
    bool hasTimeStampInfo(const QString &nameSpace) const;
    void scheduleVacuum();
    void execVacuum();

    QString m_collectionFile;
    QString m_connectionName;
    QSqlQuery *m_query = nullptr;
    bool m_vacuumScheduled = false;
    bool m_readOnly = false;
};

QT_END_NAMESPACE

#endif  //QHELPCOLLECTIONHANDLER_H
