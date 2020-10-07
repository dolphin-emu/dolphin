/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the tools applications of the Qt Toolkit.
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

#ifndef QV4EXECUTABLECOMPILATIONUNIT_P_H
#define QV4EXECUTABLECOMPILATIONUNIT_P_H

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

#include <private/qv4compileddata_p.h>
#include <private/qv4identifier_p.h>
#include <private/qqmlrefcount_p.h>
#include <private/qintrusivelist_p.h>
#include <private/qqmlpropertycachevector_p.h>
#include <private/qqmltype_p.h>
#include <private/qqmlnullablevalue_p.h>
#include <private/qqmlmetatype_p.h>

QT_BEGIN_NAMESPACE

class QQmlScriptData;
class QQmlEnginePrivate;

struct InlineComponentData {

    InlineComponentData() = default;
    InlineComponentData(const CompositeMetaTypeIds &typeIds, int objectIndex, int nameIndex, int totalObjectCount, int totalBindingCount, int totalParserStatusCount)
        :   typeIds(typeIds)
          , objectIndex(objectIndex)
          , nameIndex(nameIndex)
          , totalObjectCount(totalObjectCount)
          , totalBindingCount(totalBindingCount)
          , totalParserStatusCount(totalParserStatusCount) {}

    CompositeMetaTypeIds typeIds;
    int objectIndex = -1;
    int nameIndex = -1;
    int totalObjectCount = 0;
    int totalBindingCount = 0;
    int totalParserStatusCount = 0;
};

namespace QV4 {

// index is per-object binding index
typedef QVector<QQmlPropertyData*> BindingPropertyData;

class CompilationUnitMapper;
struct ResolvedTypeReference;
// map from name index
// While this could be a hash, a map is chosen here to provide a stable
// order, which is used to calculating a check-sum on dependent meta-objects.
struct ResolvedTypeReferenceMap: public QMap<int, ResolvedTypeReference*>
{
    bool addToHash(QCryptographicHash *hash, QQmlEngine *engine) const;
};

class Q_QML_PRIVATE_EXPORT ExecutableCompilationUnit final: public CompiledData::CompilationUnit,
                                                            public QQmlRefCount
{
    Q_DISABLE_COPY_MOVE(ExecutableCompilationUnit)
public:
    friend class QQmlRefPointer<ExecutableCompilationUnit>;

    static QQmlRefPointer<ExecutableCompilationUnit> create(
            CompiledData::CompilationUnit &&compilationUnit)
    {
        return QQmlRefPointer<ExecutableCompilationUnit>(
                new ExecutableCompilationUnit(std::move(compilationUnit)),
                QQmlRefPointer<ExecutableCompilationUnit>::Adopt);
    }

    static QQmlRefPointer<ExecutableCompilationUnit> create()
    {
        return QQmlRefPointer<ExecutableCompilationUnit>(
                new ExecutableCompilationUnit,
                QQmlRefPointer<ExecutableCompilationUnit>::Adopt);
    }

    QIntrusiveListNode nextCompilationUnit;
    ExecutionEngine *engine = nullptr;
    QQmlEnginePrivate *qmlEngine = nullptr; // only used in QML environment for composite types, not in plain QJSEngine case.

    // url() and fileName() shall be used to load the actual QML/JS code or to show errors or
    // warnings about that code. They include any potential URL interceptions and thus represent the
    // "physical" location of the code.
    //
    // finalUrl() and finalUrlString() shall be used to resolve further URLs referred to in the code
    // They are _not_ intercepted and thus represent the "logical" name for the code.

    QUrl url() const { if (m_url.isNull) m_url = QUrl(fileName()); return m_url; }
    QUrl finalUrl() const
    {
        if (m_finalUrl.isNull)
            m_finalUrl = QUrl(finalUrlString());
        return m_finalUrl;
    }

    QV4::Lookup *runtimeLookups = nullptr;
    QVector<QV4::Function *> runtimeFunctions;
    QVector<QV4::Heap::InternalClass *> runtimeBlocks;
    mutable QVector<QV4::Heap::Object *> templateObjects;
    mutable QQmlNullableValue<QUrl> m_url;
    mutable QQmlNullableValue<QUrl> m_finalUrl;

    // QML specific fields
    QQmlPropertyCacheVector propertyCaches;
    QQmlRefPointer<QQmlPropertyCache> rootPropertyCache() const { return propertyCaches.at(/*root object*/0); }

    QQmlRefPointer<QQmlTypeNameCache> typeNameCache;

    // index is object index. This allows fast access to the
    // property data when initializing bindings, avoiding expensive
    // lookups by string (property name).
    QVector<BindingPropertyData> bindingPropertyDataPerObject;

    // mapping from component object index (CompiledData::Unit object index that points to component) to identifier hash of named objects
    // this is initialized on-demand by QQmlContextData
    QHash<int, IdentifierHash> namedObjectsPerComponentCache;
    inline IdentifierHash namedObjectsPerComponent(int componentObjectIndex);

    void finalizeCompositeType(QQmlEnginePrivate *qmlEngine, CompositeMetaTypeIds typeIdsForComponent);

    int m_totalBindingsCount = 0; // Number of bindings used in this type
    int m_totalParserStatusCount = 0; // Number of instantiated types that are QQmlParserStatus subclasses
    int m_totalObjectCount = 0; // Number of objects explicitly instantiated
    int icRoot = -1;

    int totalBindingsCount() const;
    int totalParserStatusCount() const;
    int totalObjectCount() const;

    QVector<QQmlRefPointer<QQmlScriptData>> dependentScripts;
    ResolvedTypeReferenceMap resolvedTypes;
    ResolvedTypeReference *resolvedType(int id) const { return resolvedTypes.value(id); }

    bool verifyChecksum(const CompiledData::DependentTypesHasher &dependencyHasher) const;

    CompositeMetaTypeIds typeIdsForComponent(int objectid = 0) const;

    int metaTypeId = -1;
    int listMetaTypeId = -1;
    bool isRegisteredWithEngine = false;

    QHash<int, InlineComponentData> inlineComponentData;

    QScopedPointer<CompilationUnitMapper> backingFile;

    // --- interface for QQmlPropertyCacheCreator
    using CompiledObject = CompiledData::Object;
    using CompiledFunction = CompiledData::Function;

    int objectCount() const { return qmlData->nObjects; }
    const CompiledObject *objectAt(int index) const
    {
        return qmlData->objectAt(index);
    }

    int importCount() const { return qmlData->nImports; }
    const CompiledData::Import *importAt(int index) const
    {
        return qmlData->importAt(index);
    }

    Heap::Object *templateObjectAt(int index) const;

    struct FunctionIterator
    {
        FunctionIterator(const CompiledData::Unit *unit, const CompiledObject *object, int index)
            : unit(unit), object(object), index(index) {}
        const CompiledData::Unit *unit;
        const CompiledObject *object;
        int index;

        const CompiledFunction *operator->() const
        {
            return unit->functionAt(object->functionOffsetTable()[index]);
        }

        void operator++() { ++index; }
        bool operator==(const FunctionIterator &rhs) const { return index == rhs.index; }
        bool operator!=(const FunctionIterator &rhs) const { return index != rhs.index; }
    };

    FunctionIterator objectFunctionsBegin(const CompiledObject *object) const
    {
        return FunctionIterator(data, object, 0);
    }

    FunctionIterator objectFunctionsEnd(const CompiledObject *object) const
    {
        return FunctionIterator(data, object, object->nFunctions);
    }

    bool isESModule() const
    {
        return data->flags & CompiledData::Unit::IsESModule;
    }

    bool isSharedLibrary() const
    {
        return data->flags & CompiledData::Unit::IsSharedLibrary;
    }

    QStringList moduleRequests() const;
    Heap::Module *instantiate(ExecutionEngine *engine);
    const Value *resolveExport(QV4::String *exportName)
    {
        QVector<ResolveSetEntry> resolveSet;
        return resolveExportRecursively(exportName, &resolveSet);
    }

    QStringList exportedNames() const
    {
        QStringList names;
        QVector<const ExecutableCompilationUnit*> exportNameSet;
        getExportedNamesRecursively(&names, &exportNameSet);
        names.sort();
        auto last = std::unique(names.begin(), names.end());
        names.erase(last, names.end());
        return names;
    }

    void evaluate();
    void evaluateModuleRequests();

    QV4::Function *linkToEngine(QV4::ExecutionEngine *engine);
    void unlink();

    void markObjects(MarkStack *markStack);

    bool loadFromDisk(const QUrl &url, const QDateTime &sourceTimeStamp, QString *errorString);

    static QString localCacheFilePath(const QUrl &url);
    bool saveToDisk(const QUrl &unitUrl, QString *errorString);

    QString bindingValueAsString(const CompiledData::Binding *binding) const;
    QString bindingValueAsScriptString(const CompiledData::Binding *binding) const;
    double bindingValueAsNumber(const CompiledData::Binding *binding) const
    {
        if (binding->type != CompiledData::Binding::Type_Number)
            return 0.0;
        return constants[binding->value.constantValueIndex].doubleValue();
    }

    static bool verifyHeader(const CompiledData::Unit *unit, QDateTime expectedSourceTimeStamp,
                             QString *errorString);

protected:
    quint32 totalStringCount() const
    { return data->stringTableSize; }

private:
    struct ResolveSetEntry
    {
        ResolveSetEntry() {}
        ResolveSetEntry(ExecutableCompilationUnit *module, QV4::String *exportName)
            : module(module), exportName(exportName) {}
        ExecutableCompilationUnit *module = nullptr;
        QV4::String *exportName = nullptr;
    };

    ExecutableCompilationUnit();
    ExecutableCompilationUnit(CompiledData::CompilationUnit &&compilationUnit);
    ~ExecutableCompilationUnit();

    const Value *resolveExportRecursively(QV4::String *exportName,
                                          QVector<ResolveSetEntry> *resolveSet);

    QUrl urlAt(int index) const { return QUrl(stringAt(index)); }

    Q_NEVER_INLINE IdentifierHash createNamedObjectsPerComponent(int componentObjectIndex);
    const CompiledData::ExportEntry *lookupNameInExportTable(
            const CompiledData::ExportEntry *firstExportEntry, int tableSize,
            QV4::String *name) const;

    void getExportedNamesRecursively(
            QStringList *names, QVector<const ExecutableCompilationUnit *> *exportNameSet,
            bool includeDefaultExport = true) const;
};

struct ResolvedTypeReference
{
public:
    ResolvedTypeReference()
        : m_compilationUnit(nullptr)
        , m_stronglyReferencesCompilationUnit(true)
        , majorVersion(0)
        , minorVersion(0)
        , isFullyDynamicType(false)
    {}

    ~ResolvedTypeReference()
    {
        if (m_stronglyReferencesCompilationUnit && m_compilationUnit)
            m_compilationUnit->release();
    }

    QQmlRefPointer<QV4::ExecutableCompilationUnit> compilationUnit() { return m_compilationUnit; }
    void setCompilationUnit(QQmlRefPointer<QV4::ExecutableCompilationUnit> unit)
    {
        if (m_compilationUnit == unit.data())
            return;
        if (m_stronglyReferencesCompilationUnit) {
            if (m_compilationUnit)
                m_compilationUnit->release();
            m_compilationUnit = unit.take();
        } else {
            m_compilationUnit = unit.data();
        }
    }

    bool referencesCompilationUnit() const { return m_stronglyReferencesCompilationUnit; }
    void setReferencesCompilationUnit(bool doReference)
    {
        if (doReference == m_stronglyReferencesCompilationUnit)
            return;
        m_stronglyReferencesCompilationUnit = doReference;
        if (!m_compilationUnit)
            return;
        if (doReference) {
            m_compilationUnit->addref();
        } else if (m_compilationUnit->count() == 1) {
            m_compilationUnit->release();
            m_compilationUnit = nullptr;
        } else {
            m_compilationUnit->release();
        }
    }

    QQmlRefPointer<QQmlPropertyCache> propertyCache() const;
    QQmlRefPointer<QQmlPropertyCache> createPropertyCache(QQmlEngine *);
    bool addToHash(QCryptographicHash *hash, QQmlEngine *engine);

    void doDynamicTypeCheck();

    QQmlType type;
    QQmlRefPointer<QQmlPropertyCache> typePropertyCache;
private:
    Q_DISABLE_COPY_MOVE(ResolvedTypeReference)

    QV4::ExecutableCompilationUnit *m_compilationUnit;
    bool m_stronglyReferencesCompilationUnit;

public:
    int majorVersion;
    int minorVersion;
    // Types such as QQmlPropertyMap can add properties dynamically at run-time and
    // therefore cannot have a property cache installed when instantiated.
    bool isFullyDynamicType;
};

IdentifierHash ExecutableCompilationUnit::namedObjectsPerComponent(int componentObjectIndex)
{
    auto it = namedObjectsPerComponentCache.find(componentObjectIndex);
    if (Q_UNLIKELY(it == namedObjectsPerComponentCache.end()))
        return createNamedObjectsPerComponent(componentObjectIndex);
    return *it;
}

} // namespace QV4

QT_END_NAMESPACE

#endif // QV4EXECUTABLECOMPILATIONUNIT_P_H
