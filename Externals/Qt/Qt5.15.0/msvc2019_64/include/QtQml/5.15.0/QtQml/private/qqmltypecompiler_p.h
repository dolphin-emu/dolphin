/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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
#ifndef QQMLTYPECOMPILER_P_H
#define QQMLTYPECOMPILER_P_H

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

#include <qglobal.h>
#include <qqmlerror.h>
#include <qhash.h>
#include <private/qqmltypeloader_p.h>
#include <private/qqmlirbuilder_p.h>
#include <private/qqmlpropertycachecreator_p.h>

QT_BEGIN_NAMESPACE

class QQmlEnginePrivate;
class QQmlError;
class QQmlTypeData;
class QQmlImports;

namespace QmlIR {
struct Document;
}

namespace QV4 {
namespace CompiledData {
struct QmlUnit;
struct Location;
}
}

struct QQmlTypeCompiler
{
    Q_DECLARE_TR_FUNCTIONS(QQmlTypeCompiler)
public:
    QQmlTypeCompiler(QQmlEnginePrivate *engine,
                     QQmlTypeData *typeData,
                     QmlIR::Document *document,
                     const QQmlRefPointer<QQmlTypeNameCache> &typeNameCache,
                     QV4::ResolvedTypeReferenceMap *resolvedTypeCache,
                     const QV4::CompiledData::DependentTypesHasher &dependencyHasher);

    // --- interface used by QQmlPropertyCacheCreator
    typedef QmlIR::Object CompiledObject;
    const QmlIR::Object *objectAt(int index) const { return document->objects.at(index); }
    int objectCount() const { return document->objects.count(); }
    QString stringAt(int idx) const;
    QmlIR::PoolList<QmlIR::Function>::Iterator objectFunctionsBegin(const QmlIR::Object *object) const { return object->functionsBegin(); }
    QmlIR::PoolList<QmlIR::Function>::Iterator objectFunctionsEnd(const QmlIR::Object *object) const { return object->functionsEnd(); }
    QV4::ResolvedTypeReferenceMap *resolvedTypes = nullptr;
    // ---

    QQmlRefPointer<QV4::ExecutableCompilationUnit> compile();

    QList<QQmlError> compilationErrors() const { return errors; }
    void recordError(const QV4::CompiledData::Location &location, const QString &description);
    void recordError(const QQmlJS::DiagnosticMessage &message);
    void recordError(const QQmlError &e);

    int registerString(const QString &str);
    int registerConstant(QV4::ReturnedValue v);

    const QV4::CompiledData::Unit *qmlUnit() const;

    QUrl url() const { return typeData->finalUrl(); }
    QQmlEnginePrivate *enginePrivate() const { return engine; }
    const QQmlImports *imports() const;
    QVector<QmlIR::Object *> *qmlObjects() const;
    void setPropertyCaches(QQmlPropertyCacheVector &&caches);
    const QQmlPropertyCacheVector *propertyCaches() const;
    QQmlPropertyCacheVector &&takePropertyCaches();
    void setComponentRoots(const QVector<quint32> &roots) { m_componentRoots = roots; }
    const QVector<quint32> &componentRoots() const { return m_componentRoots; }
    QQmlJS::MemoryPool *memoryPool();
    QStringRef newStringRef(const QString &string);
    const QV4::Compiler::StringTableGenerator *stringPool() const;

    const QHash<int, QQmlCustomParser*> &customParserCache() const { return customParsers; }

    QString bindingAsString(const QmlIR::Object *object, int scriptIndex) const;

    void addImport(const QString &module, const QString &qualifier, int majorVersion, int minorVersion);

    QV4::ResolvedTypeReference *resolvedType(int id) const
    {
        return resolvedTypes->value(id);
    }

    CompositeMetaTypeIds typeIdsForComponent(int objectId = 0) const;

private:
    QList<QQmlError> errors;
    QQmlEnginePrivate *engine;
    const QV4::CompiledData::DependentTypesHasher &dependencyHasher;
    QmlIR::Document *document;
    // index is string index of type name (use obj->inheritedTypeNameIndex)
    QHash<int, QQmlCustomParser*> customParsers;

    // index in first hash is component index, vector inside contains object indices of objects with id property
    QVector<quint32> m_componentRoots;
    QQmlPropertyCacheVector m_propertyCaches;

    QQmlRefPointer<QQmlTypeNameCache> typeNameCache;
    QQmlTypeData *typeData;
};

struct QQmlCompilePass
{
    QQmlCompilePass(QQmlTypeCompiler *typeCompiler);

    QString stringAt(int idx) const { return compiler->stringAt(idx); }
protected:
    void recordError(const QV4::CompiledData::Location &location, const QString &description) const
    { compiler->recordError(location, description); }
    void recordError(const QQmlError &error)
    { compiler->recordError(error); }

    QV4::ResolvedTypeReference *resolvedType(int id) const
    { return compiler->resolvedType(id); }
    bool containsResolvedType(int id) const
    { return compiler->resolvedTypes->contains(id); }
    QV4::ResolvedTypeReferenceMap::iterator insertResolvedType(
            int id, QV4::ResolvedTypeReference *value)
    { return compiler->resolvedTypes->insert(id, value); }

    QQmlTypeCompiler *compiler;
};

// "Converts" signal expressions to full-fleged function declarations with
// parameters taken from the signal declarations
// It also updates the QV4::CompiledData::Binding objects to set the property name
// to the final signal name (onTextChanged -> textChanged) and sets the IsSignalExpression flag.
struct SignalHandlerConverter : public QQmlCompilePass
{
    Q_DECLARE_TR_FUNCTIONS(SignalHandlerConverter)
public:
    SignalHandlerConverter(QQmlTypeCompiler *typeCompiler);

    bool convertSignalHandlerExpressionsToFunctionDeclarations();

private:
    bool convertSignalHandlerExpressionsToFunctionDeclarations(const QmlIR::Object *obj, const QString &typeName, QQmlPropertyCache *propertyCache);

    QQmlEnginePrivate *enginePrivate;
    const QVector<QmlIR::Object*> &qmlObjects;
    const QQmlImports *imports;
    const QHash<int, QQmlCustomParser*> &customParsers;
    const QSet<QString> &illegalNames;
    const QQmlPropertyCacheVector * const propertyCaches;
};

// ### This will go away when the codegen resolves all enums to constant expressions
// and we replace the constant expression with a literal binding instead of using
// a script.
class QQmlEnumTypeResolver : public QQmlCompilePass
{
    Q_DECLARE_TR_FUNCTIONS(QQmlEnumTypeResolver)
public:
    QQmlEnumTypeResolver(QQmlTypeCompiler *typeCompiler);

    bool resolveEnumBindings();

private:
    bool assignEnumToBinding(QmlIR::Binding *binding, const QStringRef &enumName, int enumValue, bool isQtObject);
    bool assignEnumToBinding(QmlIR::Binding *binding, const QString &enumName, int enumValue, bool isQtObject)
    {
        return assignEnumToBinding(binding, QStringRef(&enumName), enumValue, isQtObject);
    }
    bool tryQualifiedEnumAssignment(const QmlIR::Object *obj, const QQmlPropertyCache *propertyCache,
                                    const QQmlPropertyData *prop,
                                    QmlIR::Binding *binding);
    int evaluateEnum(const QString &scope, const QStringRef &enumName, const QStringRef &enumValue, bool *ok) const;


    const QVector<QmlIR::Object*> &qmlObjects;
    const QQmlPropertyCacheVector * const propertyCaches;
    const QQmlImports *imports;
};

class QQmlCustomParserScriptIndexer: public QQmlCompilePass
{
public:
    QQmlCustomParserScriptIndexer(QQmlTypeCompiler *typeCompiler);

    void annotateBindingsWithScriptStrings();

private:
    void scanObjectRecursively(int objectIndex, bool annotateScriptBindings = false);

    const QVector<QmlIR::Object*> &qmlObjects;
    const QHash<int, QQmlCustomParser*> &customParsers;
};

// Annotate properties bound to aliases with a flag
class QQmlAliasAnnotator : public QQmlCompilePass
{
public:
    QQmlAliasAnnotator(QQmlTypeCompiler *typeCompiler);

    void annotateBindingsToAliases();
private:
    const QVector<QmlIR::Object*> &qmlObjects;
    const QQmlPropertyCacheVector * const propertyCaches;
};

class QQmlScriptStringScanner : public QQmlCompilePass
{
public:
    QQmlScriptStringScanner(QQmlTypeCompiler *typeCompiler);

    void scan();

private:
    const QVector<QmlIR::Object*> &qmlObjects;
    const QQmlPropertyCacheVector * const propertyCaches;
};

class QQmlComponentAndAliasResolver : public QQmlCompilePass
{
    Q_DECLARE_TR_FUNCTIONS(QQmlAnonymousComponentResolver)
public:
    QQmlComponentAndAliasResolver(QQmlTypeCompiler *typeCompiler);

    bool resolve();

protected:
    void findAndRegisterImplicitComponents(const QmlIR::Object *obj, QQmlPropertyCache *propertyCache);
    bool collectIdsAndAliases(int objectIndex);
    bool resolveAliases(int componentIndex);
    void propertyDataForAlias(QmlIR::Alias *alias, int *type, quint32 *propertyFlags);

    enum AliasResolutionResult {
        NoAliasResolved,
        SomeAliasesResolved,
        AllAliasesResolved
    };

    AliasResolutionResult resolveAliasesInObject(int objectIndex, QQmlError *error);

    QQmlEnginePrivate *enginePrivate;
    QQmlJS::MemoryPool *pool;

    QVector<QmlIR::Object*> *qmlObjects;

    // indices of the objects that are actually Component {}
    QVector<quint32> componentRoots;

    // Deliberate choice of map over hash here to ensure stable generated output.
    QMap<int, int> _idToObjectIndex;
    QVector<int> _objectsWithAliases;

    QQmlPropertyCacheVector propertyCaches;
};

class QQmlDeferredAndCustomParserBindingScanner : public QQmlCompilePass
{
public:
    QQmlDeferredAndCustomParserBindingScanner(QQmlTypeCompiler *typeCompiler);

    bool scanObject();

private:
    bool scanObject(int objectIndex);

    QVector<QmlIR::Object*> *qmlObjects;
    const QQmlPropertyCacheVector * const propertyCaches;
    const QHash<int, QQmlCustomParser*> &customParsers;

    bool _seenObjectWithId;
};

class QQmlDefaultPropertyMerger : public QQmlCompilePass
{
public:
    QQmlDefaultPropertyMerger(QQmlTypeCompiler *typeCompiler);

    void mergeDefaultProperties();

private:
    void mergeDefaultProperties(int objectIndex);

    const QVector<QmlIR::Object*> &qmlObjects;
    const QQmlPropertyCacheVector * const propertyCaches;
};

QT_END_NAMESPACE

#endif // QQMLTYPECOMPILER_P_H
