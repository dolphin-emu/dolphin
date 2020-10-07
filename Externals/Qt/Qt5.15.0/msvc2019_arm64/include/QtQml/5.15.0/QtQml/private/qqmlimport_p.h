/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#ifndef QQMLIMPORT_P_H
#define QQMLIMPORT_P_H

#include <QtCore/qurl.h>
#include <QtCore/qcoreapplication.h>
#include <QtCore/qset.h>
#include <QtCore/qstringlist.h>
#include <QtQml/qqmlerror.h>
#include <private/qqmldirparser_p.h>
#include <private/qqmltype_p.h>
#include <private/qstringhash_p.h>

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

QT_BEGIN_NAMESPACE

class QQmlTypeNameCache;
class QQmlEngine;
class QDir;
class QQmlImportNamespace;
class QQmlImportsPrivate;
class QQmlImportDatabase;
class QQmlTypeLoader;
class QQmlTypeLoaderQmldirContent;

namespace QQmlImport {
    enum RecursionRestriction { PreventRecursion, AllowRecursion };
}

struct QQmlImportInstance
{
    QString uri; // e.g. QtQuick
    QString url; // the base path of the import
    QString localDirectoryPath; // the base path of the import if it's a local file
    QQmlType containingType; // points to the containing type for inline components
    int majversion; // the major version imported
    int minversion; // the minor version imported
    bool isLibrary; // true means that this is not a file import
    bool implicitlyImported = false;
    bool isInlineComponent = false;
    QQmlDirComponents qmlDirComponents; // a copy of the components listed in the qmldir
    QQmlDirScripts qmlDirScripts; // a copy of the scripts in the qmldir

    bool setQmldirContent(const QString &resolvedUrl, const QQmlTypeLoaderQmldirContent &qmldir,
                          QQmlImportNamespace *nameSpace, QList<QQmlError> *errors);

    static QQmlDirScripts getVersionedScripts(const QQmlDirScripts &qmldirscripts, int vmaj, int vmin);

    bool resolveType(QQmlTypeLoader *typeLoader, const QHashedStringRef &type,
                     int *vmajor, int *vminor, QQmlType* type_return,
                     QString *base = nullptr, bool *typeRecursionDetected = nullptr,
                     QQmlType::RegistrationType = QQmlType::AnyRegistrationType,
                     QQmlImport::RecursionRestriction recursionRestriction = QQmlImport::PreventRecursion,
                     QList<QQmlError> *errors = nullptr) const;
};

class QQmlImportNamespace
{
public:
    QQmlImportNamespace() : nextNamespace(nullptr) {}
    ~QQmlImportNamespace() { qDeleteAll(imports); }

    QList<QQmlImportInstance *> imports;

    QQmlImportInstance *findImport(const QString &uri) const;

    bool resolveType(QQmlTypeLoader *typeLoader, const QHashedStringRef& type,
                     int *vmajor, int *vminor, QQmlType* type_return,
                     QString *base = nullptr, QList<QQmlError> *errors = nullptr,
                     QQmlType::RegistrationType registrationType = QQmlType::AnyRegistrationType,
                     bool *typeRecursionDeteced = nullptr);

    // Prefix when used as a qualified import.  Otherwise empty.
    QHashedString prefix;

    // Used by QQmlImportsPrivate::qualifiedSets
    // set to this in unqualifiedSet to indicate that the lists of imports needs
    // to be sorted when an inline component import was added
    // We can't use flag pointer, as that does not work with QFieldList
    QQmlImportNamespace *nextNamespace;
    bool needsSorting() const;
    void setNeedsSorting(bool needsSorting);
};

class Q_QML_PRIVATE_EXPORT QQmlImports
{
public:
    enum ImportVersion { FullyVersioned, PartiallyVersioned, Unversioned };

    QQmlImports(QQmlTypeLoader *);
    QQmlImports(const QQmlImports &);
    ~QQmlImports();
    QQmlImports &operator=(const QQmlImports &);

    void setBaseUrl(const QUrl &url, const QString &urlString = QString());
    QUrl baseUrl() const;

    bool resolveType(const QHashedStringRef &type,
                     QQmlType *type_return,
                     int *version_major, int *version_minor,
                     QQmlImportNamespace **ns_return,
                     QList<QQmlError> *errors = nullptr,
                     QQmlType::RegistrationType registrationType = QQmlType::AnyRegistrationType,
                     bool *typeRecursionDetected = nullptr) const;
    bool resolveType(QQmlImportNamespace *,
                     const QHashedStringRef& type,
                     QQmlType *type_return, int *version_major, int *version_minor,
                     QQmlType::RegistrationType registrationType
                     = QQmlType::AnyRegistrationType) const;

    bool addImplicitImport(QQmlImportDatabase *importDb, QList<QQmlError> *errors);

    bool addInlineComponentImport(QQmlImportInstance  *const importInstance, const QString &name, const QUrl importUrl, QQmlType containingType);

    bool addFileImport(QQmlImportDatabase *,
                       const QString& uri, const QString& prefix, int vmaj, int vmin, bool incomplete,
                       QList<QQmlError> *errors);

    bool addLibraryImport(QQmlImportDatabase *importDb,
                          const QString &uri, const QString &prefix, int vmaj, int vmin,
                          const QString &qmldirIdentifier, const QString &qmldirUrl, bool incomplete, QList<QQmlError> *errors);

    bool updateQmldirContent(QQmlImportDatabase *importDb,
                             const QString &uri, const QString &prefix,
                             const QString &qmldirIdentifier, const QString &qmldirUrl, QList<QQmlError> *errors);

    enum LocalQmldirResult {
        QmldirFound,
        QmldirNotFound,
        QmldirInterceptedToRemote
    };

    LocalQmldirResult locateLocalQmldir(
            QQmlImportDatabase *, const QString &uri, int vmaj, int vmin,
            QString *qmldirFilePath, QString *url);

    void populateCache(QQmlTypeNameCache *cache) const;

    struct ScriptReference
    {
        QString nameSpace;
        QString qualifier;
        QUrl location;
    };

    QList<ScriptReference> resolvedScripts() const;

    struct CompositeSingletonReference
    {
        QString typeName;
        QString prefix;
        int majorVersion;
        int minorVersion;
    };

    QList<CompositeSingletonReference> resolvedCompositeSingletons() const;

    static QStringList completeQmldirPaths(const QString &uri, const QStringList &basePaths, int vmaj, int vmin);
    static QString versionString(int vmaj, int vmin, ImportVersion version);

    static bool isLocal(const QString &url);
    static bool isLocal(const QUrl &url);
    static QUrl urlFromLocalFileOrQrcOrUrl(const QString &);

    static void setDesignerSupportRequired(bool b);

private:
    friend class QQmlImportDatabase;
    QQmlImportsPrivate *d;
};

class Q_QML_PRIVATE_EXPORT QQmlImportDatabase
{
    Q_DECLARE_TR_FUNCTIONS(QQmlImportDatabase)
public:
    enum PathType { Local, Remote, LocalOrRemote };

    QQmlImportDatabase(QQmlEngine *);
    ~QQmlImportDatabase();

#if QT_CONFIG(library)
    bool importDynamicPlugin(const QString &filePath, const QString &uri, const QString &importNamespace, int vmaj, QList<QQmlError> *errors);
    bool removeDynamicPlugin(const QString &filePath);
    QStringList dynamicPlugins() const;
#endif

    QStringList importPathList(PathType type = LocalOrRemote) const;
    void setImportPathList(const QStringList &paths);
    void addImportPath(const QString& dir);

    QStringList pluginPathList() const;
    void setPluginPathList(const QStringList &paths);
    void addPluginPath(const QString& path);

private:
    friend class QQmlImportsPrivate;
    QString resolvePlugin(QQmlTypeLoader *typeLoader,
                          const QString &qmldirPath, const QString &qmldirPluginPath,
                          const QString &baseName, const QStringList &suffixes,
                          const QString &prefix = QString());
    QString resolvePlugin(QQmlTypeLoader *typeLoader,
                          const QString &qmldirPath, const QString &qmldirPluginPath,
                          const QString &baseName);
    bool importStaticPlugin(QObject *instance, const QString &basePath, const QString &uri,
                          const QString &typeNamespace, int vmaj, QList<QQmlError> *errors);
    void clearDirCache();
    void finalizePlugin(QObject *instance, const QString &path, const QString &uri);

    struct QmldirCache {
        int versionMajor;
        int versionMinor;
        QString qmldirFilePath;
        QString qmldirPathUrl;
        QmldirCache *next;
    };
    // Maps from an import to a linked list of qmldir info.
    // Used in QQmlImportsPrivate::locateQmldir()
    QStringHash<QmldirCache *> qmldirCache;

    // XXX thread
    QStringList filePluginPath;
    QStringList fileImportPath;

    QSet<QString> qmlDirFilesForWhichPluginsHaveBeenLoaded;
    QSet<QString> initializedPlugins;
    QQmlEngine *engine;
};

void qmlClearEnginePlugins();// For internal use by qmlClearRegisteredProperties

QT_END_NAMESPACE

#endif // QQMLIMPORT_P_H

