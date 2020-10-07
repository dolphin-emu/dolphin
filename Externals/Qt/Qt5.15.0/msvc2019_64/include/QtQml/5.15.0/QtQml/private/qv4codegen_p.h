/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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
#ifndef QV4CODEGEN_P_H
#define QV4CODEGEN_P_H

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

#include <private/qqmljsastvisitor_p.h>
#include <private/qqmljsengine_p.h>
#include <private/qqmljsast_p.h>
#include <private/qqmljsdiagnosticmessage_p.h>
#include <private/qv4compiler_p.h>
#include <private/qv4compilercontext_p.h>
#include <private/qv4util_p.h>
#include <private/qv4bytecodegenerator_p.h>
#include <private/qv4calldata_p.h>

QT_BEGIN_NAMESPACE

namespace QV4 {

namespace Moth {
struct Instruction;
}

namespace CompiledData {
struct CompilationUnit;
}

namespace Compiler {

struct ControlFlow;
struct ControlFlowCatch;
struct ControlFlowFinally;

class Q_QMLCOMPILER_PRIVATE_EXPORT Codegen: protected QQmlJS::AST::Visitor
{
protected:
    using BytecodeGenerator = QV4::Moth::BytecodeGenerator;
    using Instruction = QV4::Moth::Instruction;
public:
    Codegen(QV4::Compiler::JSUnitGenerator *jsUnitGenerator, bool strict);


    void generateFromProgram(const QString &fileName,
                             const QString &finalUrl,
                             const QString &sourceCode,
                             QQmlJS::AST::Program *ast,
                             Module *module,
                             ContextType contextType = ContextType::Global);

    void generateFromModule(const QString &fileName,
                            const QString &finalUrl,
                            const QString &sourceCode,
                            QQmlJS::AST::ESModule *ast,
                            Module *module);

public:
    class VolatileMemoryLocationScanner;
    class VolatileMemoryLocations {
        friend VolatileMemoryLocationScanner;
        bool allVolatile = false;
        QVector<QStringView> specificLocations;
    public:
        bool isVolatile(const QStringView &name) {
            if (allVolatile)
                return true;
            return specificLocations.contains(name);
        }

        void add(const QStringRef &name) { if (!allVolatile) specificLocations.append(name); }
        void setAllVolatile() { allVolatile = true; }
    };
    class RValue {
        Codegen *codegen;
        enum Type {
            Invalid,
            Accumulator,
            StackSlot,
            Const
        } type;
        union {
            Moth::StackSlot theStackSlot;
            QV4::ReturnedValue constant;
        };

    public:
        static RValue fromStackSlot(Codegen *codegen, Moth::StackSlot stackSlot) {
            RValue r;
            r.codegen = codegen;
            r.type = StackSlot;
            r.theStackSlot = stackSlot;
            return r;
        }
        static RValue fromAccumulator(Codegen *codegen) {
            RValue r;
            r.codegen = codegen;
            r.type = Accumulator;
            return r;
        }
        static RValue fromConst(Codegen *codegen, QV4::ReturnedValue value) {
            RValue r;
            r.codegen = codegen;
            r.type = Const;
            r.constant = value;
            return r;
        }

        bool operator==(const RValue &other) const;

        bool isValid() const { return type != Invalid; }
        bool isAccumulator() const { return type == Accumulator; }
        bool isStackSlot() const { return type == StackSlot; }
        bool isConst() const { return type == Const; }

        Moth::StackSlot stackSlot() const {
            Q_ASSERT(isStackSlot());
            return theStackSlot;
        }

        QV4::ReturnedValue constantValue() const {
            Q_ASSERT(isConst());
            return constant;
        }

        Q_REQUIRED_RESULT RValue storeOnStack() const;
        void loadInAccumulator() const;
    };
    struct Reference {
        enum Type {
            Invalid,
            Accumulator,
            Super,
            SuperProperty,
            StackSlot,
            ScopedLocal,
            Name,
            Member,
            Subscript,
            Import,
            LastLValue = Import,
            Const
        } type = Invalid;

        bool isLValue() const { return !isReadonly && type > Accumulator; }

        Reference(Codegen *cg, Type t = Invalid) : Reference()
        {
            type = t;
            codegen = cg;
        }

        Reference(const QString &name = QString()) :
            constant(0),
            name(name),
            isArgOrEval(false),
            isReadonly(false),
            isReferenceToConst(false),
            requiresTDZCheck(false),
            subscriptRequiresTDZCheck(false),
            stackSlotIsLocalOrArgument(false),
            isVolatile(false),
            global(false),
            qmlGlobal(false)
        {}

        Reference(const Reference &) = default;
        Reference(Reference &&) = default;
        Reference &operator =(const Reference &) = default;
        Reference &operator =(Reference &&) = default;

        bool operator==(const Reference &other) const;
        bool operator!=(const Reference &other) const
        { return !(*this == other); }

        bool isValid() const { return type != Invalid; }
        bool loadTriggersSideEffect() const {
            switch (type) {
            case Name:
            case Member:
            case Subscript:
            case SuperProperty:
                return true;
            default:
                return requiresTDZCheck;
            }
        }
        bool isConstant() const { return type == Const; }
        bool isAccumulator() const { return type == Accumulator; }
        bool isSuper() const { return type == Super; }
        bool isSuperProperty() const { return type == SuperProperty; }
        bool isStackSlot() const { return type == StackSlot; }
        bool isRegister() const {
            return isStackSlot();
        }

        static Reference fromAccumulator(Codegen *cg) {
            return Reference(cg, Accumulator);
        }
        static Reference fromSuper(Codegen *cg) {
            return Reference(cg, Super);
        }
        static Reference fromStackSlot(Codegen *cg, int tempIndex = -1, bool isLocal = false) {
            Reference r(cg, StackSlot);
            if (tempIndex == -1)
                tempIndex = cg->bytecodeGenerator->newRegister();
            r.theStackSlot = Moth::StackSlot::createRegister(tempIndex);
            r.stackSlotIsLocalOrArgument = isLocal;
            return r;
        }
        static Reference fromArgument(Codegen *cg, int index, bool isVolatile) {
            Reference r(cg, StackSlot);
            r.theStackSlot = Moth::StackSlot::createRegister(
                    index + sizeof(CallData) / sizeof(StaticValue) - 1);
            r.stackSlotIsLocalOrArgument = true;
            r.isVolatile = isVolatile;
            return r;
        }
        static Reference fromScopedLocal(Codegen *cg, int index, int scope) {
            Reference r(cg, ScopedLocal);
            r.index = index;
            r.scope = scope;
            return r;
        }
        static Reference fromImport(Codegen *cg, int index) {
            Reference r(cg, Import);
            r.index = index;
            return r;
        }
        static Reference fromName(Codegen *cg, const QString &name) {
            Reference r(cg, Name);
            r.name = name;
            return r;
        }
        static Reference fromMember(const Reference &baseRef, const QString &name) {
            Reference r(baseRef.codegen, Member);
            r.propertyBase = baseRef.asRValue();
            r.propertyNameIndex = r.codegen->registerString(name);
            r.requiresTDZCheck = baseRef.requiresTDZCheck;
            return r;
        }
        static Reference fromSuperProperty(const Reference &property) {
            Q_ASSERT(property.isStackSlot());
            Reference r(property.codegen, SuperProperty);
            r.property = property.stackSlot();
            r.subscriptRequiresTDZCheck = property.requiresTDZCheck;
            return r;
        }
        static Reference fromSubscript(const Reference &baseRef, const Reference &subscript) {
            Q_ASSERT(baseRef.isStackSlot());
            Reference r(baseRef.codegen, Subscript);
            r.elementBase = baseRef.stackSlot();
            r.elementSubscript = subscript.asRValue();
            r.requiresTDZCheck = baseRef.requiresTDZCheck;
            r.subscriptRequiresTDZCheck = subscript.requiresTDZCheck;
            return r;
        }
        static Reference fromConst(Codegen *cg, QV4::ReturnedValue constant) {
            Reference r(cg, Const);
            r.constant = constant;
            r.isReadonly = true;
            return r;
        }
        static Reference fromThis(Codegen *cg) {
            Reference r = fromStackSlot(cg, CallData::This);
            r.isReadonly = true;
            // ### Optimize this. Functions that are not derived constructors or arrow functions can't have an
            // empty this object
            r.requiresTDZCheck = true;
            return r;
        }

        RValue asRValue() const;
        Reference asLValue() const;

        Q_REQUIRED_RESULT static Reference storeConstOnStack(Codegen *cg, QV4::ReturnedValue constant)
        { return Reference::fromConst(cg, constant).storeOnStack(); }

        static void storeConstOnStack(Codegen *cg, QV4::ReturnedValue constant, int stackSlot)
        { Reference::fromConst(cg, constant).storeOnStack(stackSlot); }

        Q_REQUIRED_RESULT Reference storeOnStack() const;
        void storeOnStack(int tempIndex) const;
        Q_REQUIRED_RESULT Reference storeRetainAccumulator() const;
        Reference storeConsumeAccumulator() const;

        Q_REQUIRED_RESULT Reference baseObject() const;

        bool storeWipesAccumulator() const;
        void loadInAccumulator() const;

        int nameAsIndex() const {
            Q_ASSERT(type == Name);
            return codegen->registerString(name);
        }

        Moth::StackSlot stackSlot() const {
            if (Q_UNLIKELY(!isStackSlot()))
                Q_UNREACHABLE();
            return theStackSlot;
        }

        union {
            Moth::StackSlot theStackSlot;
            QV4::ReturnedValue constant;
            struct { // Scoped arguments/Local
                int index;
                int scope;
            };
            struct {
                RValue propertyBase;
                int propertyNameIndex;
            };
            struct {
                Moth::StackSlot elementBase;
                RValue elementSubscript;
            };
            Moth::StackSlot property; // super property
        };
        QString name;
        Codegen *codegen = nullptr;

        quint32 isArgOrEval:1;
        quint32 isReadonly:1;
        quint32 isReferenceToConst:1;
        quint32 requiresTDZCheck:1;
        quint32 subscriptRequiresTDZCheck:1;
        quint32 stackSlotIsLocalOrArgument:1;
        quint32 isVolatile:1;
        quint32 global:1;
        quint32 qmlGlobal:1;

    private:
        void storeAccumulator() const;
        Reference doStoreOnStack(int tempIndex) const;
    };

    struct RegisterScope {
        RegisterScope(Codegen *cg)
            : generator(cg->bytecodeGenerator),
              regCountForScope(generator->currentReg) {}
        ~RegisterScope() {
            generator->currentReg = regCountForScope;
        }
        BytecodeGenerator *generator;
        int regCountForScope;
    };

    struct ObjectPropertyValue {
        ObjectPropertyValue() {}

        Reference rvalue;
        int getter = -1; // index in _module->functions or -1 if not set
        int setter = -1;
        uint keyAsIndex = UINT_MAX;

        bool hasGetter() const { return getter >= 0; }
        bool hasSetter() const { return setter >= 0; }
    };
protected:

    enum Format { ex, cx, nx };
    class Result {
        Reference _result;

        const BytecodeGenerator::Label *_iftrue = nullptr;
        const BytecodeGenerator::Label *_iffalse = nullptr;
        Format _format = ex;
        Format _requested;
        bool _trueBlockFollowsCondition = false;

    public:
        explicit Result(const QString &name)
            : _result(name)
            , _requested(ex)
        {}

        explicit Result(const Reference &lrvalue)
            : _result(lrvalue)
            , _requested(ex)
        {}

        explicit Result(Format requested = ex)
            : _requested(requested) {}

        explicit Result(const BytecodeGenerator::Label *iftrue,
                        const BytecodeGenerator::Label *iffalse,
                        bool trueBlockFollowsCondition)
            : _iftrue(iftrue)
            , _iffalse(iffalse)
            , _requested(cx)
            , _trueBlockFollowsCondition(trueBlockFollowsCondition)
        {
            Q_ASSERT(iftrue);
            Q_ASSERT(iffalse);
        }

        const BytecodeGenerator::Label *iftrue() const {
            Q_ASSERT(_requested == cx);
            return _iftrue;
        }

        const BytecodeGenerator::Label *iffalse() const {
            Q_ASSERT(_requested == cx);
            return _iffalse;
        }

        Format format() const {
            return _format;
        }

        bool accept(Format f)
        {
            if (_requested == f) {
                _format = f;
                return true;
            }
            return false;
        }

        bool trueBlockFollowsCondition() const {
            return _trueBlockFollowsCondition;
        }

        const Reference &result() const {
            return _result;
        }

        void setResult(const Reference &result) {
            _result = result;
        }

        void setResult(Reference &&result) {
            _result = std::move(result);
        }

        void clearResultName() {
            _result.name.clear();
        }
    };

    void enterContext(QQmlJS::AST::Node *node);
    int leaveContext();
public:
    Context *enterBlock(QQmlJS::AST::Node *node);
    int leaveBlock() { return leaveContext(); }
protected:
    void leaveLoop();

    enum UnaryOperation {
        UPlus,
        UMinus,
        PreIncrement,
        PreDecrement,
        PostIncrement,
        PostDecrement,
        Not,
        Compl
    };

    Reference unop(UnaryOperation op, const Reference &expr);

    void addCJump();

public:
    int registerString(const QString &name) {
        return jsUnitGenerator->registerString(name);
    }
    int registerConstant(QV4::ReturnedValue v) { return jsUnitGenerator->registerConstant(v); }
    int registerGetterLookup(int nameIndex) { return jsUnitGenerator->registerGetterLookup(nameIndex); }
    int registerSetterLookup(int nameIndex) { return jsUnitGenerator->registerSetterLookup(nameIndex); }
    int registerGlobalGetterLookup(int nameIndex) { return jsUnitGenerator->registerGlobalGetterLookup(nameIndex); }
    int registerQmlContextPropertyGetterLookup(int nameIndex) { return jsUnitGenerator->registerQmlContextPropertyGetterLookup(nameIndex); }

    // Returns index in _module->functions
    virtual int defineFunction(const QString &name, QQmlJS::AST::Node *ast,
                               QQmlJS::AST::FormalParameterList *formals,
                               QQmlJS::AST::StatementList *body);

protected:
    void statement(QQmlJS::AST::Statement *ast);
    void statement(QQmlJS::AST::ExpressionNode *ast);
    void condition(QQmlJS::AST::ExpressionNode *ast, const BytecodeGenerator::Label *iftrue,
                   const BytecodeGenerator::Label *iffalse,
                   bool trueBlockFollowsCondition);

    inline Reference expression(QQmlJS::AST::ExpressionNode *ast, const QString &name = QString())
    {
        if (!ast || hasError())
            return Reference();

        pushExpr(name);
        ast->accept(this);
        return popResult();
    }

    inline void accept(QQmlJS::AST::Node *node)
    {
        if (!hasError() && node)
            node->accept(this);
    }

    void program(QQmlJS::AST::Program *ast);
    void statementList(QQmlJS::AST::StatementList *ast);
    void variableDeclaration(QQmlJS::AST::PatternElement *ast);
    void variableDeclarationList(QQmlJS::AST::VariableDeclarationList *ast);

    Reference targetForPatternElement(QQmlJS::AST::PatternElement *p);
    void initializeAndDestructureBindingElement(QQmlJS::AST::PatternElement *e, const Reference &baseRef = Reference(), bool isDefinition = false);
    void destructurePropertyList(const Reference &object, QQmlJS::AST::PatternPropertyList *bindingList, bool isDefinition = false);
    void destructureElementList(const Reference &array, QQmlJS::AST::PatternElementList *bindingList, bool isDefinition = false);
    void destructurePattern(QQmlJS::AST::Pattern *p, const Reference &rhs);

    Reference referenceForPropertyName(const Codegen::Reference &object, QQmlJS::AST::PropertyName *name);

    void emitReturn(const Reference &expr);

    // nodes
    bool visit(QQmlJS::AST::ArgumentList *ast) override;
    bool visit(QQmlJS::AST::CaseBlock *ast) override;
    bool visit(QQmlJS::AST::CaseClause *ast) override;
    bool visit(QQmlJS::AST::CaseClauses *ast) override;
    bool visit(QQmlJS::AST::Catch *ast) override;
    bool visit(QQmlJS::AST::DefaultClause *ast) override;
    bool visit(QQmlJS::AST::Elision *ast) override;
    bool visit(QQmlJS::AST::Finally *ast) override;
    bool visit(QQmlJS::AST::FormalParameterList *ast) override;
    bool visit(QQmlJS::AST::Program *ast) override;
    bool visit(QQmlJS::AST::StatementList *ast) override;
    bool visit(QQmlJS::AST::UiArrayMemberList *ast) override;
    bool visit(QQmlJS::AST::UiImport *ast) override;
    bool visit(QQmlJS::AST::UiHeaderItemList *ast) override;
    bool visit(QQmlJS::AST::UiPragma *ast) override;
    bool visit(QQmlJS::AST::UiObjectInitializer *ast) override;
    bool visit(QQmlJS::AST::UiObjectMemberList *ast) override;
    bool visit(QQmlJS::AST::UiParameterList *ast) override;
    bool visit(QQmlJS::AST::UiProgram *ast) override;
    bool visit(QQmlJS::AST::UiQualifiedId *ast) override;
    bool visit(QQmlJS::AST::VariableDeclarationList *ast) override;

    bool visit(QQmlJS::AST::PatternElement *ast) override;
    bool visit(QQmlJS::AST::PatternElementList *ast) override;
    bool visit(QQmlJS::AST::PatternProperty *ast) override;
    bool visit(QQmlJS::AST::PatternPropertyList *ast) override;

    bool visit(QQmlJS::AST::ExportDeclaration *ast) override;

    bool visit(QQmlJS::AST::TypeAnnotation *ast) override;

    // expressions
    bool visit(QQmlJS::AST::Expression *ast) override;
    bool visit(QQmlJS::AST::ArrayPattern *ast) override;
    bool visit(QQmlJS::AST::ArrayMemberExpression *ast) override;
    bool visit(QQmlJS::AST::BinaryExpression *ast) override;
    bool visit(QQmlJS::AST::CallExpression *ast) override;
    bool visit(QQmlJS::AST::ConditionalExpression *ast) override;
    bool visit(QQmlJS::AST::DeleteExpression *ast) override;
    bool visit(QQmlJS::AST::FalseLiteral *ast) override;
    bool visit(QQmlJS::AST::SuperLiteral *ast) override;
    bool visit(QQmlJS::AST::FieldMemberExpression *ast) override;
    bool visit(QQmlJS::AST::TaggedTemplate *ast) override;
    bool visit(QQmlJS::AST::FunctionExpression *ast) override;
    bool visit(QQmlJS::AST::IdentifierExpression *ast) override;
    bool visit(QQmlJS::AST::NestedExpression *ast) override;
    bool visit(QQmlJS::AST::NewExpression *ast) override;
    bool visit(QQmlJS::AST::NewMemberExpression *ast) override;
    bool visit(QQmlJS::AST::NotExpression *ast) override;
    bool visit(QQmlJS::AST::NullExpression *ast) override;
    bool visit(QQmlJS::AST::NumericLiteral *ast) override;
    bool visit(QQmlJS::AST::ObjectPattern *ast) override;
    bool visit(QQmlJS::AST::PostDecrementExpression *ast) override;
    bool visit(QQmlJS::AST::PostIncrementExpression *ast) override;
    bool visit(QQmlJS::AST::PreDecrementExpression *ast) override;
    bool visit(QQmlJS::AST::PreIncrementExpression *ast) override;
    bool visit(QQmlJS::AST::RegExpLiteral *ast) override;
    bool visit(QQmlJS::AST::StringLiteral *ast) override;
    bool visit(QQmlJS::AST::TemplateLiteral *ast) override;
    bool visit(QQmlJS::AST::ThisExpression *ast) override;
    bool visit(QQmlJS::AST::TildeExpression *ast) override;
    bool visit(QQmlJS::AST::TrueLiteral *ast) override;
    bool visit(QQmlJS::AST::TypeOfExpression *ast) override;
    bool visit(QQmlJS::AST::UnaryMinusExpression *ast) override;
    bool visit(QQmlJS::AST::UnaryPlusExpression *ast) override;
    bool visit(QQmlJS::AST::VoidExpression *ast) override;
    bool visit(QQmlJS::AST::FunctionDeclaration *ast) override;
    bool visit(QQmlJS::AST::YieldExpression *ast) override;
    bool visit(QQmlJS::AST::ClassExpression *ast) override;
    bool visit(QQmlJS::AST::ClassDeclaration *ast) override;

    // statements
    bool visit(QQmlJS::AST::Block *ast) override;
    bool visit(QQmlJS::AST::BreakStatement *ast) override;
    bool visit(QQmlJS::AST::ContinueStatement *ast) override;
    bool visit(QQmlJS::AST::DebuggerStatement *ast) override;
    bool visit(QQmlJS::AST::DoWhileStatement *ast) override;
    bool visit(QQmlJS::AST::EmptyStatement *ast) override;
    bool visit(QQmlJS::AST::ExpressionStatement *ast) override;
    bool visit(QQmlJS::AST::ForEachStatement *ast) override;
    bool visit(QQmlJS::AST::ForStatement *ast) override;
    bool visit(QQmlJS::AST::IfStatement *ast) override;
    bool visit(QQmlJS::AST::LabelledStatement *ast) override;
    bool visit(QQmlJS::AST::ReturnStatement *ast) override;
    bool visit(QQmlJS::AST::SwitchStatement *ast) override;
    bool visit(QQmlJS::AST::ThrowStatement *ast) override;
    bool visit(QQmlJS::AST::TryStatement *ast) override;
    bool visit(QQmlJS::AST::VariableStatement *ast) override;
    bool visit(QQmlJS::AST::WhileStatement *ast) override;
    bool visit(QQmlJS::AST::WithStatement *ast) override;

    // ui object members
    bool visit(QQmlJS::AST::UiArrayBinding *ast) override;
    bool visit(QQmlJS::AST::UiObjectBinding *ast) override;
    bool visit(QQmlJS::AST::UiObjectDefinition *ast) override;
    bool visit(QQmlJS::AST::UiPublicMember *ast) override;
    bool visit(QQmlJS::AST::UiScriptBinding *ast) override;
    bool visit(QQmlJS::AST::UiSourceElement *ast) override;

    bool throwSyntaxErrorOnEvalOrArgumentsInStrictMode(const Reference &r,
                                                       const QQmlJS::SourceLocation &loc);
    virtual void throwSyntaxError(const QQmlJS::SourceLocation &loc, const QString &detail);
    virtual void throwReferenceError(const QQmlJS::SourceLocation &loc, const QString &detail);
    void throwRecursionDepthError() override
    {
        throwSyntaxError(QQmlJS::SourceLocation(),
                         QStringLiteral("Maximum statement or expression depth exceeded"));
    }

public:
    enum ErrorType {
        NoError,
        SyntaxError,
        ReferenceError
    };

    ErrorType errorType() const { return _errorType; }
    bool hasError() const { return _errorType != NoError; }
    QQmlJS::DiagnosticMessage error() const;
    QUrl url() const;

    Reference binopHelper(QSOperator::Op oper, Reference &left, Reference &right);
    Reference jumpBinop(QSOperator::Op oper, Reference &left, Reference &right);
    struct Arguments { int argc; int argv; bool hasSpread; };
    Arguments pushArgs(QQmlJS::AST::ArgumentList *args);
    void handleCall(Reference &base, Arguments calldata, int slotForFunction, int slotForThisObject);

    Arguments pushTemplateArgs(QQmlJS::AST::TemplateLiteral *args);
    bool handleTaggedTemplate(Reference base, QQmlJS::AST::TaggedTemplate *ast);
    void createTemplateObject(QQmlJS::AST::TemplateLiteral *t);

    void setUseFastLookups(bool b) { useFastLookups = b; }

    void handleTryCatch(QQmlJS::AST::TryStatement *ast);
    void handleTryFinally(QQmlJS::AST::TryStatement *ast);


    Reference referenceForName(
            const QString &name, bool lhs,
            const QQmlJS::SourceLocation &accessLocation = QQmlJS::SourceLocation());

    QV4::CompiledData::CompilationUnit generateCompilationUnit(bool generateUnitData = true);
    static QV4::CompiledData::CompilationUnit compileModule(
            bool debugMode, const QString &url, const QString &sourceCode,
            const QDateTime &sourceTimeStamp, QList<QQmlJS::DiagnosticMessage> *diagnostics);

    Context *currentContext() const { return _context; }
    BytecodeGenerator *generator() const { return bytecodeGenerator; }

    void loadClosure(int index);

    Module *module() const { return _module; }

    BytecodeGenerator::Label returnLabel() {
        if (!_returnLabel)
            _returnLabel = new BytecodeGenerator::Label(bytecodeGenerator->newLabel());
        return *_returnLabel;
    }

    void setGlobalNames(const QSet<QString>& globalNames) {
        m_globalNames = globalNames;
    }

    static const char *s_globalNames[];

protected:
    friend class ScanFunctions;
    friend struct ControlFlow;
    friend struct ControlFlowCatch;
    friend struct ControlFlowFinally;

    inline void setExprResult(const Reference &result) { m_expressions.back().setResult(result); }
    inline void setExprResult(Reference &&result) { m_expressions.back().setResult(std::move(result)); }
    inline Reference exprResult() const { return m_expressions.back().result(); }
    inline void clearExprResultName() { m_expressions.back().clearResultName(); }

    inline bool exprAccept(Format f) { return m_expressions.back().accept(f); }

    inline const Result &currentExpr() const { return m_expressions.back(); }

    inline void pushExpr(Result &&expr) { m_expressions.push_back(std::move(expr)); }
    inline void pushExpr(const Result &expr) { m_expressions.push_back(expr); }
    inline void pushExpr(const QString &name = QString()) { m_expressions.emplace_back(name); }

    inline Result popExpr()
    {
        const Result result = m_expressions.back();
        m_expressions.pop_back();
        return result;
    }

    inline Reference popResult() {
        const Reference result = m_expressions.back().result();
        m_expressions.pop_back();
        return result;
    }

    std::vector<Result> m_expressions;
    VolatileMemoryLocations _volatileMemoryLocations;
    Module *_module;
    int _returnAddress;
    Context *_context;
    Context *_functionContext = nullptr;
    QQmlJS::AST::LabelledStatement *_labelledStatement;
    QV4::Compiler::JSUnitGenerator *jsUnitGenerator;
    BytecodeGenerator *bytecodeGenerator = nullptr;
    Moth::BytecodeGenerator::Label *_returnLabel = nullptr;
    bool _strictMode;
    bool useFastLookups = true;
    bool requiresReturnValue = false;
    bool insideSwitch = false;
    bool inFormalParameterList = false;
    bool functionEndsWithReturn = false;
    bool _tailCallsAreAllowed = true;
    QSet<QString> m_globalNames;

    ControlFlow *controlFlow = nullptr;

    bool _fileNameIsUrl;
    ErrorType _errorType = NoError;
    QQmlJS::DiagnosticMessage _error;

    class TailCallBlocker
    {
    public:
        TailCallBlocker(Codegen *cg, bool onoff = false)
            : _cg(cg)
            , _saved(_cg->_tailCallsAreAllowed)
            , _onoff(onoff)
        { _cg->_tailCallsAreAllowed = onoff; }

        ~TailCallBlocker()
        { _cg->_tailCallsAreAllowed = _saved; }

        void unblock() const
        { _cg->_tailCallsAreAllowed = _saved; }

        void reblock() const
        { _cg->_tailCallsAreAllowed = _onoff; }

    private:
        Codegen *_cg;
        bool _saved;
        bool _onoff;
    };

private:
    VolatileMemoryLocations scanVolatileMemoryLocations(QQmlJS::AST::Node *ast);
    void handleConstruct(const Reference &base, QQmlJS::AST::ArgumentList *args);
    void throwError(ErrorType errorType, const QQmlJS::SourceLocation &loc,
                    const QString &detail);
};

}

}

QT_END_NAMESPACE

#endif // QV4CODEGEN_P_H
