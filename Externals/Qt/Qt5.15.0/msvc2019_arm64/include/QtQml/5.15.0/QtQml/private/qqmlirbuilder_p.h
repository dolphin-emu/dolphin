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
#ifndef QQMLIRBUILDER_P_H
#define QQMLIRBUILDER_P_H

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

#include <private/qqmljsast_p.h>
#include <private/qqmljsengine_p.h>
#include <private/qv4compiler_p.h>
#include <private/qv4compileddata_p.h>
#include <private/qqmljsmemorypool_p.h>
#include <private/qqmljsfixedpoolarray_p.h>
#include <private/qv4codegen_p.h>
#include <private/qv4compiler_p.h>
#include <QTextStream>
#include <QCoreApplication>

QT_BEGIN_NAMESPACE

class QQmlPropertyCache;
class QQmlContextData;
class QQmlTypeNameCache;
struct QQmlIRLoader;

namespace QmlIR {

struct Document;

template <typename T>
struct PoolList
{
    PoolList()
        : first(nullptr)
        , last(nullptr)
    {}

    T *first;
    T *last;
    int count = 0;

    int append(T *item) {
        item->next = nullptr;
        if (last)
            last->next = item;
        else
            first = item;
        last = item;
        return count++;
    }

    void prepend(T *item) {
        item->next = first;
        first = item;
        if (!last)
            last = first;
        ++count;
    }

    template <typename Sortable, typename Base, Sortable Base::*sortMember>
    T *findSortedInsertionPoint(T *item) const
    {
        T *insertPos = nullptr;

        for (T *it = first; it; it = it->next) {
            if (!(it->*sortMember <= item->*sortMember))
                break;
            insertPos = it;
        }

        return insertPos;
    }

    void insertAfter(T *insertionPoint, T *item) {
        if (!insertionPoint) {
            prepend(item);
        } else if (insertionPoint == last) {
            append(item);
        } else {
            item->next = insertionPoint->next;
            insertionPoint->next = item;
            ++count;
        }
    }

    T *unlink(T *before, T *item) {
        T * const newNext = item->next;

        if (before)
            before->next = newNext;
        else
            first = newNext;

        if (item == last) {
            if (newNext)
                last = newNext;
            else
                last = first;
        }

        --count;
        return newNext;
    }

    T *slowAt(int index) const
    {
        T *result = first;
        while (index > 0 && result) {
            result = result->next;
            --index;
        }
        return result;
    }

    struct Iterator {
        // turn Iterator into a proper iterator
        using iterator_category = std::forward_iterator_tag;
        using value_type = T;
        using difference_type = ptrdiff_t;
        using pointer = T *;
        using reference = T &;

        T *ptr;

        explicit Iterator(T *p) : ptr(p) {}

        T *operator->() {
            return ptr;
        }

        const T *operator->() const {
            return ptr;
        }

        T &operator*() {
            return *ptr;
        }

        const T &operator*() const {
            return *ptr;
        }

        Iterator& operator++() {
            ptr = ptr->next;
            return *this;
        }

        Iterator operator++(int) {
            Iterator that {ptr};
            ptr = ptr->next;
            return that;
        }

        bool operator==(const Iterator &rhs) const {
            return ptr == rhs.ptr;
        }

        bool operator!=(const Iterator &rhs) const {
            return ptr != rhs.ptr;
        }
    };

    Iterator begin() { return Iterator(first); }
    Iterator end() { return Iterator(nullptr); }

    using iterator = Iterator;
};

struct Object;

struct EnumValue : public QV4::CompiledData::EnumValue
{
    EnumValue *next;
};

struct Enum
{
    int nameIndex;
    QV4::CompiledData::Location location;
    PoolList<EnumValue> *enumValues;

    int enumValueCount() const { return enumValues->count; }
    PoolList<EnumValue>::Iterator enumValuesBegin() const { return enumValues->begin(); }
    PoolList<EnumValue>::Iterator enumValuesEnd() const { return enumValues->end(); }

    Enum *next;
};


struct Parameter : public QV4::CompiledData::Parameter
{
    Parameter *next;

    bool init(QV4::Compiler::JSUnitGenerator *stringGenerator, const QString &parameterName, const QString &typeName);
    static bool init(QV4::CompiledData::Parameter *param, const QV4::Compiler::JSUnitGenerator *stringGenerator,
                     int parameterNameIndex, int typeNameIndex);
    static bool initType(QV4::CompiledData::ParameterType *paramType,
                         const QV4::Compiler::JSUnitGenerator *stringGenerator, int typeNameIndex);

    static QV4::CompiledData::BuiltinType stringToBuiltinType(const QString &typeName);
};

struct Signal
{
    int nameIndex;
    QV4::CompiledData::Location location;
    PoolList<Parameter> *parameters;

    QStringList parameterStringList(const QV4::Compiler::StringTableGenerator *stringPool) const;

    int parameterCount() const { return parameters->count; }
    PoolList<Parameter>::Iterator parametersBegin() const { return parameters->begin(); }
    PoolList<Parameter>::Iterator parametersEnd() const { return parameters->end(); }

    Signal *next;
};

struct Property : public QV4::CompiledData::Property
{
    Property *next;
};

struct Binding : public QV4::CompiledData::Binding
{
    // The offset in the source file where the binding appeared. This is used for sorting to ensure
    // that assignments to list properties are done in the correct order. We use the offset here instead
    // of Binding::location as the latter has limited precision.
    quint32 offset;
    // Binding's compiledScriptIndex is index in object's functionsAndExpressions
    Binding *next;
};

struct InlineComponent : public QV4::CompiledData::InlineComponent
{
    InlineComponent *next;
};

struct Alias : public QV4::CompiledData::Alias
{
    Alias *next;
};

struct RequiredPropertyExtraData : public QV4::CompiledData::RequiredPropertyExtraData
{
    RequiredPropertyExtraData *next;
};

struct Function
{
    QV4::CompiledData::Location location;
    int nameIndex;
    quint32 index; // index in parsedQML::functions
    QQmlJS::FixedPoolArray<Parameter> formals;
    QV4::CompiledData::ParameterType returnType;

    // --- QQmlPropertyCacheCreator interface
    const Parameter *formalsBegin() const { return formals.begin(); }
    const Parameter *formalsEnd() const { return formals.end(); }
    // ---

    Function *next;
};

struct Q_QMLCOMPILER_PRIVATE_EXPORT CompiledFunctionOrExpression
{
    CompiledFunctionOrExpression()
    {}

    QQmlJS::AST::Node *parentNode = nullptr; // FunctionDeclaration, Statement or Expression
    QQmlJS::AST::Node *node = nullptr; // FunctionDeclaration, Statement or Expression
    quint32 nameIndex = 0;
    CompiledFunctionOrExpression *next = nullptr;
};

struct Q_QMLCOMPILER_PRIVATE_EXPORT Object
{
    Q_DECLARE_TR_FUNCTIONS(Object)
public:
    quint32 inheritedTypeNameIndex;
    quint32 idNameIndex;
    int id;
    int indexOfDefaultPropertyOrAlias;
    bool defaultPropertyIsAlias;
    bool isInlineComponent = false;
    quint32 flags;

    QV4::CompiledData::Location location;
    QV4::CompiledData::Location locationOfIdProperty;

    const Property *firstProperty() const { return properties->first; }
    int propertyCount() const { return properties->count; }
    Alias *firstAlias() const { return aliases->first; }
    int aliasCount() const { return aliases->count; }
    const Enum *firstEnum() const { return qmlEnums->first; }
    int enumCount() const { return qmlEnums->count; }
    const Signal *firstSignal() const { return qmlSignals->first; }
    int signalCount() const { return qmlSignals->count; }
    Binding *firstBinding() const { return bindings->first; }
    int bindingCount() const { return bindings->count; }
    const Function *firstFunction() const { return functions->first; }
    int functionCount() const { return functions->count; }
    const InlineComponent *inlineComponent() const { return inlineComponents->first; }
    int inlineComponentCount() const { return inlineComponents->count; }
    const RequiredPropertyExtraData *requiredPropertyExtraData() const {return requiredPropertyExtraDatas->first; }
    int requiredPropertyExtraDataCount() const { return requiredPropertyExtraDatas->count; }
    void simplifyRequiredProperties();

    PoolList<Binding>::Iterator bindingsBegin() const { return bindings->begin(); }
    PoolList<Binding>::Iterator bindingsEnd() const { return bindings->end(); }
    PoolList<Property>::Iterator propertiesBegin() const { return properties->begin(); }
    PoolList<Property>::Iterator propertiesEnd() const { return properties->end(); }
    PoolList<Alias>::Iterator aliasesBegin() const { return aliases->begin(); }
    PoolList<Alias>::Iterator aliasesEnd() const { return aliases->end(); }
    PoolList<Enum>::Iterator enumsBegin() const { return qmlEnums->begin(); }
    PoolList<Enum>::Iterator enumsEnd() const { return qmlEnums->end(); }
    PoolList<Signal>::Iterator signalsBegin() const { return qmlSignals->begin(); }
    PoolList<Signal>::Iterator signalsEnd() const { return qmlSignals->end(); }
    PoolList<Function>::Iterator functionsBegin() const { return functions->begin(); }
    PoolList<Function>::Iterator functionsEnd() const { return functions->end(); }
    PoolList<InlineComponent>::Iterator inlineComponentsBegin() const { return inlineComponents->begin(); }
    PoolList<InlineComponent>::Iterator inlineComponentsEnd() const { return inlineComponents->end(); }
    PoolList<RequiredPropertyExtraData>::Iterator requiredPropertyExtraDataBegin() const {return requiredPropertyExtraDatas->begin(); }
    PoolList<RequiredPropertyExtraData>::Iterator requiredPropertyExtraDataEnd() const {return requiredPropertyExtraDatas->end(); }

    // If set, then declarations for this object (and init bindings for these) should go into the
    // specified object. Used for declarations inside group properties.
    Object *declarationsOverride;

    void init(QQmlJS::MemoryPool *pool, int typeNameIndex, int idIndex, const QQmlJS::SourceLocation &location = QQmlJS::SourceLocation());

    QString appendEnum(Enum *enumeration);
    QString appendSignal(Signal *signal);
    QString appendProperty(Property *prop, const QString &propertyName, bool isDefaultProperty, const QQmlJS::SourceLocation &defaultToken, QQmlJS::SourceLocation *errorLocation);
    QString appendAlias(Alias *prop, const QString &aliasName, bool isDefaultProperty, const QQmlJS::SourceLocation &defaultToken, QQmlJS::SourceLocation *errorLocation);
    void appendFunction(QmlIR::Function *f);
    void appendInlineComponent(InlineComponent *ic);
    void appendRequiredPropertyExtraData(RequiredPropertyExtraData *extraData);

    QString appendBinding(Binding *b, bool isListBinding);
    Binding *findBinding(quint32 nameIndex) const;
    Binding *unlinkBinding(Binding *before, Binding *binding) { return bindings->unlink(before, binding); }
    void insertSorted(Binding *b);
    QString bindingAsString(Document *doc, int scriptIndex) const;

    PoolList<CompiledFunctionOrExpression> *functionsAndExpressions;
    QQmlJS::FixedPoolArray<int> runtimeFunctionIndices;

    QQmlJS::FixedPoolArray<quint32> namedObjectsInComponent;
    int namedObjectsInComponentCount() const { return namedObjectsInComponent.size(); }
    const quint32 *namedObjectsInComponentTable() const { return namedObjectsInComponent.begin(); }

private:
    friend struct ::QQmlIRLoader;

    PoolList<Property> *properties;
    PoolList<Alias> *aliases;
    PoolList<Enum> *qmlEnums;
    PoolList<Signal> *qmlSignals;
    PoolList<Binding> *bindings;
    PoolList<Function> *functions;
    PoolList<InlineComponent> *inlineComponents;
    PoolList<RequiredPropertyExtraData> *requiredPropertyExtraDatas;
};

struct Q_QMLCOMPILER_PRIVATE_EXPORT Pragma
{
    enum PragmaType {
        PragmaSingleton = 0x1
    };
    quint32 type;

    QV4::CompiledData::Location location;
};

struct Q_QMLCOMPILER_PRIVATE_EXPORT Document
{
    Document(bool debugMode);
    QString code;
    QQmlJS::Engine jsParserEngine;
    QV4::Compiler::Module jsModule;
    QList<const QV4::CompiledData::Import *> imports;
    QList<Pragma*> pragmas;
    QQmlJS::AST::UiProgram *program;
    QVector<Object*> objects;
    QV4::Compiler::JSUnitGenerator jsGenerator;

    QV4::CompiledData::CompilationUnit javaScriptCompilationUnit;

    int registerString(const QString &str) { return jsGenerator.registerString(str); }
    QString stringAt(int index) const { return jsGenerator.stringForIndex(index); }

    int objectCount() const {return objects.size();}
    Object* objectAt(int i) const {return objects.at(i);}
};

class Q_QMLCOMPILER_PRIVATE_EXPORT ScriptDirectivesCollector : public QQmlJS::Directives
{
    QmlIR::Document *document;
    QQmlJS::Engine *engine;
    QV4::Compiler::JSUnitGenerator *jsGenerator;

public:
    ScriptDirectivesCollector(QmlIR::Document *doc);

    void pragmaLibrary() override;
    void importFile(const QString &jsfile, const QString &module, int lineNumber, int column) override;
    void importModule(const QString &uri, const QString &version, const QString &module, int lineNumber, int column) override;
};

struct Q_QMLCOMPILER_PRIVATE_EXPORT IRBuilder : public QQmlJS::AST::Visitor
{
    Q_DECLARE_TR_FUNCTIONS(QQmlCodeGenerator)
public:
    IRBuilder(const QSet<QString> &illegalNames);
    bool generateFromQml(const QString &code, const QString &url, Document *output);

    static bool isSignalPropertyName(const QString &name);

    using QQmlJS::AST::Visitor::visit;
    using QQmlJS::AST::Visitor::endVisit;

    bool visit(QQmlJS::AST::UiArrayMemberList *ast) override;
    bool visit(QQmlJS::AST::UiImport *ast) override;
    bool visit(QQmlJS::AST::UiPragma *ast) override;
    bool visit(QQmlJS::AST::UiHeaderItemList *ast) override;
    bool visit(QQmlJS::AST::UiObjectInitializer *ast) override;
    bool visit(QQmlJS::AST::UiObjectMemberList *ast) override;
    bool visit(QQmlJS::AST::UiParameterList *ast) override;
    bool visit(QQmlJS::AST::UiProgram *) override;
    bool visit(QQmlJS::AST::UiQualifiedId *ast) override;
    bool visit(QQmlJS::AST::UiArrayBinding *ast) override;
    bool visit(QQmlJS::AST::UiObjectBinding *ast) override;
    bool visit(QQmlJS::AST::UiObjectDefinition *ast) override;
    bool visit(QQmlJS::AST::UiInlineComponent *ast) override;
    bool visit(QQmlJS::AST::UiEnumDeclaration *ast) override;
    bool visit(QQmlJS::AST::UiPublicMember *ast) override;
    bool visit(QQmlJS::AST::UiScriptBinding *ast) override;
    bool visit(QQmlJS::AST::UiSourceElement *ast) override;
    bool visit(QQmlJS::AST::UiRequired *ast) override;

    void throwRecursionDepthError() override
    {
        recordError(QQmlJS::SourceLocation(),
                    QStringLiteral("Maximum statement or expression depth exceeded"));
    }

    void accept(QQmlJS::AST::Node *node);

    // returns index in _objects
    bool defineQMLObject(int *objectIndex, QQmlJS::AST::UiQualifiedId *qualifiedTypeNameId, const QQmlJS::SourceLocation &location, QQmlJS::AST::UiObjectInitializer *initializer, Object *declarationsOverride = nullptr);
    bool defineQMLObject(int *objectIndex, QQmlJS::AST::UiObjectDefinition *node, Object *declarationsOverride = nullptr)
    { return defineQMLObject(objectIndex, node->qualifiedTypeNameId, node->qualifiedTypeNameId->firstSourceLocation(), node->initializer, declarationsOverride); }

    static QString asString(QQmlJS::AST::UiQualifiedId *node);
    QStringRef asStringRef(QQmlJS::AST::Node *node);
    static void extractVersion(const QStringRef &string, int *maj, int *min);
    QStringRef textRefAt(const QQmlJS::SourceLocation &loc) const
    { return QStringRef(&sourceCode, loc.offset, loc.length); }
    QStringRef textRefAt(const QQmlJS::SourceLocation &first,
                         const QQmlJS::SourceLocation &last) const;

    void setBindingValue(QV4::CompiledData::Binding *binding, QQmlJS::AST::Statement *statement,
                         QQmlJS::AST::Node *parentNode);
    void tryGeneratingTranslationBinding(const QStringRef &base, QQmlJS::AST::ArgumentList *args, QV4::CompiledData::Binding *binding);

    void appendBinding(QQmlJS::AST::UiQualifiedId *name, QQmlJS::AST::Statement *value,
                       QQmlJS::AST::Node *parentNode);
    void appendBinding(QQmlJS::AST::UiQualifiedId *name, int objectIndex, bool isOnAssignment = false);
    void appendBinding(const QQmlJS::SourceLocation &qualifiedNameLocation,
                       const QQmlJS::SourceLocation &nameLocation, quint32 propertyNameIndex,
                       QQmlJS::AST::Statement *value, QQmlJS::AST::Node *parentNode);
    void appendBinding(const QQmlJS::SourceLocation &qualifiedNameLocation,
                       const QQmlJS::SourceLocation &nameLocation, quint32 propertyNameIndex,
                       int objectIndex, bool isListItem = false, bool isOnAssignment = false);

    bool appendAlias(QQmlJS::AST::UiPublicMember *node);

    Object *bindingsTarget() const;

    bool setId(const QQmlJS::SourceLocation &idLocation, QQmlJS::AST::Statement *value);

    // resolves qualified name (font.pixelSize for example) and returns the last name along
    // with the object any right-hand-side of a binding should apply to.
    bool resolveQualifiedId(QQmlJS::AST::UiQualifiedId **nameToResolve, Object **object, bool onAssignment = false);

    void recordError(const QQmlJS::SourceLocation &location, const QString &description);

    quint32 registerString(const QString &str) const { return jsGenerator->registerString(str); }
    template <typename _Tp> _Tp *New() { return pool->New<_Tp>(); }

    QString stringAt(int index) const { return jsGenerator->stringForIndex(index); }

    static bool isStatementNodeScript(QQmlJS::AST::Statement *statement);
    static bool isRedundantNullInitializerForPropertyDeclaration(Property *property, QQmlJS::AST::Statement *statement);

    QString sanityCheckFunctionNames(Object *obj, const QSet<QString> &illegalNames, QQmlJS::SourceLocation *errorLocation);

    QList<QQmlJS::DiagnosticMessage> errors;

    QSet<QString> illegalNames;
    QSet<QString> inlineComponentsNames;

    QList<const QV4::CompiledData::Import *> _imports;
    QList<Pragma*> _pragmas;
    QVector<Object*> _objects;

    QV4::CompiledData::TypeReferenceMap _typeReferences;

    Object *_object;
    Property *_propertyDeclaration;

    QQmlJS::MemoryPool *pool;
    QString sourceCode;
    QV4::Compiler::JSUnitGenerator *jsGenerator;

    bool insideInlineComponent = false;
};

struct Q_QMLCOMPILER_PRIVATE_EXPORT QmlUnitGenerator
{
    void generate(Document &output, const QV4::CompiledData::DependentTypesHasher &dependencyHasher = QV4::CompiledData::DependentTypesHasher());

private:
    typedef bool (Binding::*BindingFilter)() const;
    char *writeBindings(char *bindingPtr, const Object *o, BindingFilter filter) const;
};

struct Q_QMLCOMPILER_PRIVATE_EXPORT JSCodeGen : public QV4::Compiler::Codegen
{
    JSCodeGen(Document *document, const QSet<QString> &globalNames);

    // Returns mapping from input functions to index in IR::Module::functions / compiledData->runtimeFunctions
    QVector<int> generateJSCodeForFunctionsAndBindings(const QList<CompiledFunctionOrExpression> &functions);

    bool generateCodeForComponents(const QVector<quint32> &componentRoots);
    bool compileComponent(int contextObject);
    bool compileJavaScriptCodeInObjectsRecursively(int objectIndex, int scopeObjectIndex);

private:
    Document *document;
};

} // namespace QmlIR

QT_END_NAMESPACE

#endif // QQMLIRBUILDER_P_H
