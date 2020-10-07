/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
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

#ifndef QQMLTYPEDATA_P_H
#define QQMLTYPEDATA_P_H

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

#include <private/qqmltypeloader_p.h>
#include <private/qv4executablecompilationunit_p.h>

QT_BEGIN_NAMESPACE

class Q_AUTOTEST_EXPORT QQmlTypeData : public QQmlTypeLoader::Blob
{
    Q_DECLARE_TR_FUNCTIONS(QQmlTypeData)
public:
    struct TypeReference
    {
        TypeReference() : majorVersion(0), minorVersion(0), needsCreation(true) {}

        QV4::CompiledData::Location location;
        QQmlType type;
        int majorVersion;
        int minorVersion;
        QQmlRefPointer<QQmlTypeData> typeData;
        bool selfReference = false;
        QString prefix; // used by CompositeSingleton types
        QString qualifiedName() const;
        bool needsCreation;
    };

    struct ScriptReference
    {
        QV4::CompiledData::Location location;
        QString qualifier;
        QQmlRefPointer<QQmlScriptBlob> script;
    };

private:
    friend class QQmlTypeLoader;

    QQmlTypeData(const QUrl &, QQmlTypeLoader *);
    template<typename Container>
    void setCompileUnit(const Container &container);

public:
    ~QQmlTypeData() override;

    const QList<ScriptReference> &resolvedScripts() const;

    QV4::ExecutableCompilationUnit *compilationUnit() const;
    QV4::ExecutableCompilationUnit *compilationUnitForInlineComponent(unsigned int icObjectId) const;

    // Used by QQmlComponent to get notifications
    struct TypeDataCallback {
        virtual ~TypeDataCallback();
        virtual void typeDataProgress(QQmlTypeData *, qreal) {}
        virtual void typeDataReady(QQmlTypeData *) {}
    };
    void registerCallback(TypeDataCallback *);
    void unregisterCallback(TypeDataCallback *);

    CompositeMetaTypeIds typeIds(int objectId = 0) const;
    QByteArray typeClassName() const { return m_typeClassName; }

protected:
    void done() override;
    void completed() override;
    void dataReceived(const SourceCodeData &) override;
    void initializeFromCachedUnit(const QV4::CompiledData::Unit *unit) override;
    void allDependenciesDone() override;
    void downloadProgressChanged(qreal) override;

    QString stringAt(int index) const override;

private:
    bool tryLoadFromDiskCache();
    bool loadFromSource();
    void restoreIR(QV4::CompiledData::CompilationUnit &&unit);
    void continueLoadFromIR();
    void resolveTypes();
    QQmlError buildTypeResolutionCaches(
            QQmlRefPointer<QQmlTypeNameCache> *typeNameCache,
            QV4::ResolvedTypeReferenceMap *resolvedTypeCache
            ) const;
    void compile(const QQmlRefPointer<QQmlTypeNameCache> &typeNameCache,
                 QV4::ResolvedTypeReferenceMap *resolvedTypeCache,
                 const QV4::CompiledData::DependentTypesHasher &dependencyHasher);
    void createTypeAndPropertyCaches(const QQmlRefPointer<QQmlTypeNameCache> &typeNameCache,
                                     const QV4::ResolvedTypeReferenceMap &resolvedTypeCache);
    bool resolveType(const QString &typeName, int &majorVersion, int &minorVersion,
                     TypeReference &ref, int lineNumber = -1, int columnNumber = -1,
                     bool reportErrors = true,
                     QQmlType::RegistrationType registrationType = QQmlType::AnyRegistrationType,
                     bool *typeRecursionDetected = nullptr);

    void scriptImported(const QQmlRefPointer<QQmlScriptBlob> &blob, const QV4::CompiledData::Location &location, const QString &qualifier, const QString &nameSpace) override;

    SourceCodeData m_backupSourceCode; // used when cache verification fails.
    QScopedPointer<QmlIR::Document> m_document;
    QV4::CompiledData::TypeReferenceMap m_typeReferences;

    QList<ScriptReference> m_scripts;

    QSet<QString> m_namespaces;
    QList<TypeReference> m_compositeSingletons;

    // map from name index to resolved type
    // While this could be a hash, a map is chosen here to provide a stable
    // order, which is used to calculating a check-sum on dependent meta-objects.
    QMap<int, TypeReference> m_resolvedTypes;
    bool m_typesResolved:1;

    // Used for self-referencing types, otherwise -1.
    CompositeMetaTypeIds m_typeIds;
    QByteArray m_typeClassName; // used for meta-object later

    using ExecutableCompilationUnitPtr = QQmlRefPointer<QV4::ExecutableCompilationUnit>;

    QHash<int, InlineComponentData> m_inlineComponentData;

    ExecutableCompilationUnitPtr m_compiledData;
    QHash<int, ExecutableCompilationUnitPtr> m_inlineComponentToCompiledData;

    QList<TypeDataCallback *> m_callbacks;

    bool m_implicitImportLoaded;
    bool loadImplicitImport();
};

QT_END_NAMESPACE

#endif // QQMLTYPEDATA_P_H
