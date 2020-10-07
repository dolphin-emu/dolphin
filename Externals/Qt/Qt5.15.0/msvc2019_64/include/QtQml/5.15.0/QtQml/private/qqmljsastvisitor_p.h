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

#ifndef QQMLJSASTVISITOR_P_H
#define QQMLJSASTVISITOR_P_H

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

#include "qqmljsastfwd_p.h"
#include "qqmljsglobal_p.h"

QT_BEGIN_NAMESPACE

namespace QQmlJS { namespace AST {

class QML_PARSER_EXPORT BaseVisitor
{
public:
    class RecursionDepthCheck
    {
        Q_DISABLE_COPY(RecursionDepthCheck)
    public:
        RecursionDepthCheck(RecursionDepthCheck &&) = delete;
        RecursionDepthCheck &operator=(RecursionDepthCheck &&) = delete;

        RecursionDepthCheck(BaseVisitor *visitor) : m_visitor(visitor)
        {
            ++(m_visitor->m_recursionDepth);
        }

        ~RecursionDepthCheck()
        {
            --(m_visitor->m_recursionDepth);
        }

        bool operator()() const {
            return m_visitor->m_recursionDepth < s_recursionLimit;
        }

    private:
        static const quint16 s_recursionLimit = 4096;
        BaseVisitor *m_visitor;
    };

    BaseVisitor(quint16 parentRecursionDepth = 0);
    virtual ~BaseVisitor();

    virtual bool preVisit(Node *) = 0;
    virtual void postVisit(Node *) = 0;

    // Ui
    virtual bool visit(UiProgram *) = 0;
    virtual bool visit(UiHeaderItemList *) = 0;
    virtual bool visit(UiPragma *) = 0;
    virtual bool visit(UiImport *) = 0;
    virtual bool visit(UiPublicMember *) = 0;
    virtual bool visit(UiSourceElement *) = 0;
    virtual bool visit(UiObjectDefinition *) = 0;
    virtual bool visit(UiObjectInitializer *) = 0;
    virtual bool visit(UiObjectBinding *) = 0;
    virtual bool visit(UiScriptBinding *) = 0;
    virtual bool visit(UiArrayBinding *) = 0;
    virtual bool visit(UiParameterList *) = 0;
    virtual bool visit(UiObjectMemberList *) = 0;
    virtual bool visit(UiArrayMemberList *) = 0;
    virtual bool visit(UiQualifiedId *) = 0;
    virtual bool visit(UiEnumDeclaration *) = 0;
    virtual bool visit(UiEnumMemberList *) = 0;
    virtual bool visit(UiVersionSpecifier *) = 0;
    virtual bool visit(UiInlineComponent *) = 0;
    virtual bool visit(UiAnnotation *) = 0;
    virtual bool visit(UiAnnotationList *) = 0;
    virtual bool visit(UiRequired *) = 0;

    virtual void endVisit(UiProgram *) = 0;
    virtual void endVisit(UiImport *) = 0;
    virtual void endVisit(UiHeaderItemList *) = 0;
    virtual void endVisit(UiPragma *) = 0;
    virtual void endVisit(UiPublicMember *) = 0;
    virtual void endVisit(UiSourceElement *) = 0;
    virtual void endVisit(UiObjectDefinition *) = 0;
    virtual void endVisit(UiObjectInitializer *) = 0;
    virtual void endVisit(UiObjectBinding *) = 0;
    virtual void endVisit(UiScriptBinding *) = 0;
    virtual void endVisit(UiArrayBinding *) = 0;
    virtual void endVisit(UiParameterList *) = 0;
    virtual void endVisit(UiObjectMemberList *) = 0;
    virtual void endVisit(UiArrayMemberList *) = 0;
    virtual void endVisit(UiQualifiedId *) = 0;
    virtual void endVisit(UiEnumDeclaration *) = 0;
    virtual void endVisit(UiEnumMemberList *) = 0;
    virtual void endVisit(UiVersionSpecifier *) = 0;
    virtual void endVisit(UiInlineComponent *) = 0;
    virtual void endVisit(UiAnnotation *) = 0;
    virtual void endVisit(UiAnnotationList *) = 0;
    virtual void endVisit(UiRequired *) = 0;

    // QQmlJS
    virtual bool visit(ThisExpression *) = 0;
    virtual void endVisit(ThisExpression *) = 0;

    virtual bool visit(IdentifierExpression *) = 0;
    virtual void endVisit(IdentifierExpression *) = 0;

    virtual bool visit(NullExpression *) = 0;
    virtual void endVisit(NullExpression *) = 0;

    virtual bool visit(TrueLiteral *) = 0;
    virtual void endVisit(TrueLiteral *) = 0;

    virtual bool visit(FalseLiteral *) = 0;
    virtual void endVisit(FalseLiteral *) = 0;

    virtual bool visit(SuperLiteral *) = 0;
    virtual void endVisit(SuperLiteral *) = 0;

    virtual bool visit(StringLiteral *) = 0;
    virtual void endVisit(StringLiteral *) = 0;

    virtual bool visit(TemplateLiteral *) = 0;
    virtual void endVisit(TemplateLiteral *) = 0;

    virtual bool visit(NumericLiteral *) = 0;
    virtual void endVisit(NumericLiteral *) = 0;

    virtual bool visit(RegExpLiteral *) = 0;
    virtual void endVisit(RegExpLiteral *) = 0;

    virtual bool visit(ArrayPattern *) = 0;
    virtual void endVisit(ArrayPattern *) = 0;

    virtual bool visit(ObjectPattern *) = 0;
    virtual void endVisit(ObjectPattern *) = 0;

    virtual bool visit(PatternElementList *) = 0;
    virtual void endVisit(PatternElementList *) = 0;

    virtual bool visit(PatternPropertyList *) = 0;
    virtual void endVisit(PatternPropertyList *) = 0;

    virtual bool visit(PatternElement *) = 0;
    virtual void endVisit(PatternElement *) = 0;

    virtual bool visit(PatternProperty *) = 0;
    virtual void endVisit(PatternProperty *) = 0;

    virtual bool visit(Elision *) = 0;
    virtual void endVisit(Elision *) = 0;

    virtual bool visit(NestedExpression *) = 0;
    virtual void endVisit(NestedExpression *) = 0;

    virtual bool visit(IdentifierPropertyName *) = 0;
    virtual void endVisit(IdentifierPropertyName *) = 0;

    virtual bool visit(StringLiteralPropertyName *) = 0;
    virtual void endVisit(StringLiteralPropertyName *) = 0;

    virtual bool visit(NumericLiteralPropertyName *) = 0;
    virtual void endVisit(NumericLiteralPropertyName *) = 0;

    virtual bool visit(ComputedPropertyName *) = 0;
    virtual void endVisit(ComputedPropertyName *) = 0;

    virtual bool visit(ArrayMemberExpression *) = 0;
    virtual void endVisit(ArrayMemberExpression *) = 0;

    virtual bool visit(FieldMemberExpression *) = 0;
    virtual void endVisit(FieldMemberExpression *) = 0;

    virtual bool visit(TaggedTemplate *) = 0;
    virtual void endVisit(TaggedTemplate *) = 0;

    virtual bool visit(NewMemberExpression *) = 0;
    virtual void endVisit(NewMemberExpression *) = 0;

    virtual bool visit(NewExpression *) = 0;
    virtual void endVisit(NewExpression *) = 0;

    virtual bool visit(CallExpression *) = 0;
    virtual void endVisit(CallExpression *) = 0;

    virtual bool visit(ArgumentList *) = 0;
    virtual void endVisit(ArgumentList *) = 0;

    virtual bool visit(PostIncrementExpression *) = 0;
    virtual void endVisit(PostIncrementExpression *) = 0;

    virtual bool visit(PostDecrementExpression *) = 0;
    virtual void endVisit(PostDecrementExpression *) = 0;

    virtual bool visit(DeleteExpression *) = 0;
    virtual void endVisit(DeleteExpression *) = 0;

    virtual bool visit(VoidExpression *) = 0;
    virtual void endVisit(VoidExpression *) = 0;

    virtual bool visit(TypeOfExpression *) = 0;
    virtual void endVisit(TypeOfExpression *) = 0;

    virtual bool visit(PreIncrementExpression *) = 0;
    virtual void endVisit(PreIncrementExpression *) = 0;

    virtual bool visit(PreDecrementExpression *) = 0;
    virtual void endVisit(PreDecrementExpression *) = 0;

    virtual bool visit(UnaryPlusExpression *) = 0;
    virtual void endVisit(UnaryPlusExpression *) = 0;

    virtual bool visit(UnaryMinusExpression *) = 0;
    virtual void endVisit(UnaryMinusExpression *) = 0;

    virtual bool visit(TildeExpression *) = 0;
    virtual void endVisit(TildeExpression *) = 0;

    virtual bool visit(NotExpression *) = 0;
    virtual void endVisit(NotExpression *) = 0;

    virtual bool visit(BinaryExpression *) = 0;
    virtual void endVisit(BinaryExpression *) = 0;

    virtual bool visit(ConditionalExpression *) = 0;
    virtual void endVisit(ConditionalExpression *) = 0;

    virtual bool visit(Expression *) = 0;
    virtual void endVisit(Expression *) = 0;

    virtual bool visit(Block *) = 0;
    virtual void endVisit(Block *) = 0;

    virtual bool visit(StatementList *) = 0;
    virtual void endVisit(StatementList *) = 0;

    virtual bool visit(VariableStatement *) = 0;
    virtual void endVisit(VariableStatement *) = 0;

    virtual bool visit(VariableDeclarationList *) = 0;
    virtual void endVisit(VariableDeclarationList *) = 0;

    virtual bool visit(EmptyStatement *) = 0;
    virtual void endVisit(EmptyStatement *) = 0;

    virtual bool visit(ExpressionStatement *) = 0;
    virtual void endVisit(ExpressionStatement *) = 0;

    virtual bool visit(IfStatement *) = 0;
    virtual void endVisit(IfStatement *) = 0;

    virtual bool visit(DoWhileStatement *) = 0;
    virtual void endVisit(DoWhileStatement *) = 0;

    virtual bool visit(WhileStatement *) = 0;
    virtual void endVisit(WhileStatement *) = 0;

    virtual bool visit(ForStatement *) = 0;
    virtual void endVisit(ForStatement *) = 0;

    virtual bool visit(ForEachStatement *) = 0;
    virtual void endVisit(ForEachStatement *) = 0;

    virtual bool visit(ContinueStatement *) = 0;
    virtual void endVisit(ContinueStatement *) = 0;

    virtual bool visit(BreakStatement *) = 0;
    virtual void endVisit(BreakStatement *) = 0;

    virtual bool visit(ReturnStatement *) = 0;
    virtual void endVisit(ReturnStatement *) = 0;

    virtual bool visit(YieldExpression *) = 0;
    virtual void endVisit(YieldExpression *) = 0;

    virtual bool visit(WithStatement *) = 0;
    virtual void endVisit(WithStatement *) = 0;

    virtual bool visit(SwitchStatement *) = 0;
    virtual void endVisit(SwitchStatement *) = 0;

    virtual bool visit(CaseBlock *) = 0;
    virtual void endVisit(CaseBlock *) = 0;

    virtual bool visit(CaseClauses *) = 0;
    virtual void endVisit(CaseClauses *) = 0;

    virtual bool visit(CaseClause *) = 0;
    virtual void endVisit(CaseClause *) = 0;

    virtual bool visit(DefaultClause *) = 0;
    virtual void endVisit(DefaultClause *) = 0;

    virtual bool visit(LabelledStatement *) = 0;
    virtual void endVisit(LabelledStatement *) = 0;

    virtual bool visit(ThrowStatement *) = 0;
    virtual void endVisit(ThrowStatement *) = 0;

    virtual bool visit(TryStatement *) = 0;
    virtual void endVisit(TryStatement *) = 0;

    virtual bool visit(Catch *) = 0;
    virtual void endVisit(Catch *) = 0;

    virtual bool visit(Finally *) = 0;
    virtual void endVisit(Finally *) = 0;

    virtual bool visit(FunctionDeclaration *) = 0;
    virtual void endVisit(FunctionDeclaration *) = 0;

    virtual bool visit(FunctionExpression *) = 0;
    virtual void endVisit(FunctionExpression *) = 0;

    virtual bool visit(FormalParameterList *) = 0;
    virtual void endVisit(FormalParameterList *) = 0;

    virtual bool visit(ClassExpression *) = 0;
    virtual void endVisit(ClassExpression *) = 0;

    virtual bool visit(ClassDeclaration *) = 0;
    virtual void endVisit(ClassDeclaration *) = 0;

    virtual bool visit(ClassElementList *) = 0;
    virtual void endVisit(ClassElementList *) = 0;

    virtual bool visit(Program *) = 0;
    virtual void endVisit(Program *) = 0;

    virtual bool visit(NameSpaceImport *) = 0;
    virtual void endVisit(NameSpaceImport *) = 0;

    virtual bool visit(ImportSpecifier *) = 0;
    virtual void endVisit(ImportSpecifier *) = 0;

    virtual bool visit(ImportsList *) = 0;
    virtual void endVisit(ImportsList *) = 0;

    virtual bool visit(NamedImports *) = 0;
    virtual void endVisit(NamedImports *) = 0;

    virtual bool visit(FromClause *) = 0;
    virtual void endVisit(FromClause *) = 0;

    virtual bool visit(ImportClause *) = 0;
    virtual void endVisit(ImportClause *) = 0;

    virtual bool visit(ImportDeclaration *) = 0;
    virtual void endVisit(ImportDeclaration *) = 0;

    virtual bool visit(ExportSpecifier *) = 0;
    virtual void endVisit(ExportSpecifier *) = 0;

    virtual bool visit(ExportsList *) = 0;
    virtual void endVisit(ExportsList *) = 0;

    virtual bool visit(ExportClause *) = 0;
    virtual void endVisit(ExportClause *) = 0;

    virtual bool visit(ExportDeclaration *) = 0;
    virtual void endVisit(ExportDeclaration *) = 0;

    virtual bool visit(ESModule *) = 0;
    virtual void endVisit(ESModule *) = 0;

    virtual bool visit(DebuggerStatement *) = 0;
    virtual void endVisit(DebuggerStatement *) = 0;

    virtual bool visit(Type *) = 0;
    virtual void endVisit(Type *) = 0;

    virtual bool visit(TypeArgumentList *) = 0;
    virtual void endVisit(TypeArgumentList *) = 0;

    virtual bool visit(TypeAnnotation *) = 0;
    virtual void endVisit(TypeAnnotation *) = 0;

    virtual void throwRecursionDepthError() = 0;

    quint16 recursionDepth() const { return m_recursionDepth; }

protected:
    quint16 m_recursionDepth = 0;
    friend class RecursionDepthCheck;
};

class QML_PARSER_EXPORT Visitor: public BaseVisitor
{
public:
    Visitor(quint16 parentRecursionDepth = 0);

    bool preVisit(Node *) override { return true; }
    void postVisit(Node *) override {}

    // Ui
    bool visit(UiProgram *) override { return true; }
    bool visit(UiHeaderItemList *) override { return true; }
    bool visit(UiPragma *) override { return true; }
    bool visit(UiImport *) override { return true; }
    bool visit(UiPublicMember *) override { return true; }
    bool visit(UiSourceElement *) override { return true; }
    bool visit(UiObjectDefinition *) override { return true; }
    bool visit(UiObjectInitializer *) override { return true; }
    bool visit(UiObjectBinding *) override { return true; }
    bool visit(UiScriptBinding *) override { return true; }
    bool visit(UiArrayBinding *) override { return true; }
    bool visit(UiParameterList *) override { return true; }
    bool visit(UiObjectMemberList *) override { return true; }
    bool visit(UiArrayMemberList *) override { return true; }
    bool visit(UiQualifiedId *) override { return true; }
    bool visit(UiEnumDeclaration *) override { return true; }
    bool visit(UiEnumMemberList *) override { return true; }
    bool visit(UiVersionSpecifier *) override { return true; }
    bool visit(UiInlineComponent *) override { return true; }
    bool visit(UiAnnotation *) override { return true; }
    bool visit(UiAnnotationList *) override { return true; }
    bool visit(UiRequired *) override { return true; }

    void endVisit(UiProgram *) override {}
    void endVisit(UiImport *) override {}
    void endVisit(UiHeaderItemList *) override {}
    void endVisit(UiPragma *) override {}
    void endVisit(UiPublicMember *) override {}
    void endVisit(UiSourceElement *) override {}
    void endVisit(UiObjectDefinition *) override {}
    void endVisit(UiObjectInitializer *) override {}
    void endVisit(UiObjectBinding *) override {}
    void endVisit(UiScriptBinding *) override {}
    void endVisit(UiArrayBinding *) override {}
    void endVisit(UiParameterList *) override {}
    void endVisit(UiObjectMemberList *) override {}
    void endVisit(UiArrayMemberList *) override {}
    void endVisit(UiQualifiedId *) override {}
    void endVisit(UiEnumDeclaration *) override {}
    void endVisit(UiEnumMemberList *) override {}
    void endVisit(UiVersionSpecifier *) override {}
    void endVisit(UiInlineComponent *) override {}
    void endVisit(UiAnnotation *) override {}
    void endVisit(UiAnnotationList *) override {}
    void endVisit(UiRequired *) override {}

    // QQmlJS
    bool visit(ThisExpression *) override { return true; }
    void endVisit(ThisExpression *) override {}

    bool visit(IdentifierExpression *) override { return true; }
    void endVisit(IdentifierExpression *) override {}

    bool visit(NullExpression *) override { return true; }
    void endVisit(NullExpression *) override {}

    bool visit(TrueLiteral *) override { return true; }
    void endVisit(TrueLiteral *) override {}

    bool visit(FalseLiteral *) override { return true; }
    void endVisit(FalseLiteral *) override {}

    bool visit(SuperLiteral *) override { return true; }
    void endVisit(SuperLiteral *) override {}

    bool visit(StringLiteral *) override { return true; }
    void endVisit(StringLiteral *) override {}

    bool visit(TemplateLiteral *) override { return true; }
    void endVisit(TemplateLiteral *) override {}

    bool visit(NumericLiteral *) override { return true; }
    void endVisit(NumericLiteral *) override {}

    bool visit(RegExpLiteral *) override { return true; }
    void endVisit(RegExpLiteral *) override {}

    bool visit(ArrayPattern *) override { return true; }
    void endVisit(ArrayPattern *) override {}

    bool visit(ObjectPattern *) override { return true; }
    void endVisit(ObjectPattern *) override {}

    bool visit(PatternElementList *) override { return true; }
    void endVisit(PatternElementList *) override {}

    bool visit(PatternPropertyList *) override { return true; }
    void endVisit(PatternPropertyList *) override {}

    bool visit(PatternElement *) override { return true; }
    void endVisit(PatternElement *) override {}

    bool visit(PatternProperty *) override { return true; }
    void endVisit(PatternProperty *) override {}

    bool visit(Elision *) override { return true; }
    void endVisit(Elision *) override {}

    bool visit(NestedExpression *) override { return true; }
    void endVisit(NestedExpression *) override {}

    bool visit(IdentifierPropertyName *) override { return true; }
    void endVisit(IdentifierPropertyName *) override {}

    bool visit(StringLiteralPropertyName *) override { return true; }
    void endVisit(StringLiteralPropertyName *) override {}

    bool visit(NumericLiteralPropertyName *) override { return true; }
    void endVisit(NumericLiteralPropertyName *) override {}

    bool visit(ComputedPropertyName *) override { return true; }
    void endVisit(ComputedPropertyName *) override {}

    bool visit(ArrayMemberExpression *) override { return true; }
    void endVisit(ArrayMemberExpression *) override {}

    bool visit(FieldMemberExpression *) override { return true; }
    void endVisit(FieldMemberExpression *) override {}

    bool visit(TaggedTemplate *) override { return true; }
    void endVisit(TaggedTemplate *) override {}

    bool visit(NewMemberExpression *) override { return true; }
    void endVisit(NewMemberExpression *) override {}

    bool visit(NewExpression *) override { return true; }
    void endVisit(NewExpression *) override {}

    bool visit(CallExpression *) override { return true; }
    void endVisit(CallExpression *) override {}

    bool visit(ArgumentList *) override { return true; }
    void endVisit(ArgumentList *) override {}

    bool visit(PostIncrementExpression *) override { return true; }
    void endVisit(PostIncrementExpression *) override {}

    bool visit(PostDecrementExpression *) override { return true; }
    void endVisit(PostDecrementExpression *) override {}

    bool visit(DeleteExpression *) override { return true; }
    void endVisit(DeleteExpression *) override {}

    bool visit(VoidExpression *) override { return true; }
    void endVisit(VoidExpression *) override {}

    bool visit(TypeOfExpression *) override { return true; }
    void endVisit(TypeOfExpression *) override {}

    bool visit(PreIncrementExpression *) override { return true; }
    void endVisit(PreIncrementExpression *) override {}

    bool visit(PreDecrementExpression *) override { return true; }
    void endVisit(PreDecrementExpression *) override {}

    bool visit(UnaryPlusExpression *) override { return true; }
    void endVisit(UnaryPlusExpression *) override {}

    bool visit(UnaryMinusExpression *) override { return true; }
    void endVisit(UnaryMinusExpression *) override {}

    bool visit(TildeExpression *) override { return true; }
    void endVisit(TildeExpression *) override {}

    bool visit(NotExpression *) override { return true; }
    void endVisit(NotExpression *) override {}

    bool visit(BinaryExpression *) override { return true; }
    void endVisit(BinaryExpression *) override {}

    bool visit(ConditionalExpression *) override { return true; }
    void endVisit(ConditionalExpression *) override {}

    bool visit(Expression *) override { return true; }
    void endVisit(Expression *) override {}

    bool visit(Block *) override { return true; }
    void endVisit(Block *) override {}

    bool visit(StatementList *) override { return true; }
    void endVisit(StatementList *) override {}

    bool visit(VariableStatement *) override { return true; }
    void endVisit(VariableStatement *) override {}

    bool visit(VariableDeclarationList *) override { return true; }
    void endVisit(VariableDeclarationList *) override {}

    bool visit(EmptyStatement *) override { return true; }
    void endVisit(EmptyStatement *) override {}

    bool visit(ExpressionStatement *) override { return true; }
    void endVisit(ExpressionStatement *) override {}

    bool visit(IfStatement *) override { return true; }
    void endVisit(IfStatement *) override {}

    bool visit(DoWhileStatement *) override { return true; }
    void endVisit(DoWhileStatement *) override {}

    bool visit(WhileStatement *) override { return true; }
    void endVisit(WhileStatement *) override {}

    bool visit(ForStatement *) override { return true; }
    void endVisit(ForStatement *) override {}

    bool visit(ForEachStatement *) override { return true; }
    void endVisit(ForEachStatement *) override {}

    bool visit(ContinueStatement *) override { return true; }
    void endVisit(ContinueStatement *) override {}

    bool visit(BreakStatement *) override { return true; }
    void endVisit(BreakStatement *) override {}

    bool visit(ReturnStatement *) override { return true; }
    void endVisit(ReturnStatement *) override {}

    bool visit(YieldExpression *) override { return true; }
    void endVisit(YieldExpression *) override {}

    bool visit(WithStatement *) override { return true; }
    void endVisit(WithStatement *) override {}

    bool visit(SwitchStatement *) override { return true; }
    void endVisit(SwitchStatement *) override {}

    bool visit(CaseBlock *) override { return true; }
    void endVisit(CaseBlock *) override {}

    bool visit(CaseClauses *) override { return true; }
    void endVisit(CaseClauses *) override {}

    bool visit(CaseClause *) override { return true; }
    void endVisit(CaseClause *) override {}

    bool visit(DefaultClause *) override { return true; }
    void endVisit(DefaultClause *) override {}

    bool visit(LabelledStatement *) override { return true; }
    void endVisit(LabelledStatement *) override {}

    bool visit(ThrowStatement *) override { return true; }
    void endVisit(ThrowStatement *) override {}

    bool visit(TryStatement *) override { return true; }
    void endVisit(TryStatement *) override {}

    bool visit(Catch *) override { return true; }
    void endVisit(Catch *) override {}

    bool visit(Finally *) override { return true; }
    void endVisit(Finally *) override {}

    bool visit(FunctionDeclaration *) override { return true; }
    void endVisit(FunctionDeclaration *) override {}

    bool visit(FunctionExpression *) override { return true; }
    void endVisit(FunctionExpression *) override {}

    bool visit(FormalParameterList *) override { return true; }
    void endVisit(FormalParameterList *) override {}

    bool visit(ClassExpression *) override { return true; }
    void endVisit(ClassExpression *) override {}

    bool visit(ClassDeclaration *) override { return true; }
    void endVisit(ClassDeclaration *) override {}

    bool visit(ClassElementList *) override { return true; }
    void endVisit(ClassElementList *) override {}

    bool visit(Program *) override { return true; }
    void endVisit(Program *) override {}

    bool visit(NameSpaceImport *) override { return true; }
    void endVisit(NameSpaceImport *) override {}

    bool visit(ImportSpecifier *) override { return true; }
    void endVisit(ImportSpecifier *) override {}

    bool visit(ImportsList *) override { return true; }
    void endVisit(ImportsList *) override {}

    bool visit(NamedImports *) override { return true; }
    void endVisit(NamedImports *) override {}

    bool visit(FromClause *) override { return true; }
    void endVisit(FromClause *) override {}

    bool visit(ImportClause *) override { return true; }
    void endVisit(ImportClause *) override {}

    bool visit(ImportDeclaration *) override { return true; }
    void endVisit(ImportDeclaration *) override {}

    bool visit(ExportSpecifier *) override { return true; }
    void endVisit(ExportSpecifier *) override {}

    bool visit(ExportsList *) override { return true; }
    void endVisit(ExportsList *) override {}

    bool visit(ExportClause *) override { return true; }
    void endVisit(ExportClause *) override {}

    bool visit(ExportDeclaration *) override { return true; }
    void endVisit(ExportDeclaration *) override {}

    bool visit(ESModule *) override { return true; }
    void endVisit(ESModule *) override {}

    bool visit(DebuggerStatement *) override { return true; }
    void endVisit(DebuggerStatement *) override {}

    bool visit(Type *) override { return true; }
    void endVisit(Type *) override {}

    bool visit(TypeArgumentList *) override { return true; }
    void endVisit(TypeArgumentList *) override {}

    bool visit(TypeAnnotation *) override { return true; }
    void endVisit(TypeAnnotation *) override {}
};

} } // namespace AST

QT_END_NAMESPACE

#endif // QQMLJSASTVISITOR_P_H
