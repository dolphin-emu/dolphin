//
//Copyright (C) 2016 Google, Inc.
//
//All rights reserved.
//
//Redistribution and use in source and binary forms, with or without
//modification, are permitted provided that the following conditions
//are met:
//
//    Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//
//    Redistributions in binary form must reproduce the above
//    copyright notice, this list of conditions and the following
//    disclaimer in the documentation and/or other materials provided
//    with the distribution.
//
//    Neither the name of Google, Inc., nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
//THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
//"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
//LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
//FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
//COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
//INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
//BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
//LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
//CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
//LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
//ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
//POSSIBILITY OF SUCH DAMAGE.
//

//
// This is a set of mutually recursive methods implementing the HLSL grammar.
// Generally, each returns
//  - through an argument: a type specifically appropriate to which rule it
//    recognized
//  - through the return value: true/false to indicate whether or not it
//    recognized its rule
//
// As much as possible, only grammar recognition should happen in this file,
// with all other work being farmed out to hlslParseHelper.cpp, which in turn
// will build the AST.
//
// The next token, yet to be "accepted" is always sitting in 'token'.
// When a method says it accepts a rule, that means all tokens involved
// in the rule will have been consumed, and none left in 'token'.
//

#include "hlslTokens.h"
#include "hlslGrammar.h"

namespace glslang {

// Root entry point to this recursive decent parser.
// Return true if compilation unit was successfully accepted.
bool HlslGrammar::parse()
{
    advanceToken();
    return acceptCompilationUnit();
}

void HlslGrammar::expected(const char* syntax)
{
    parseContext.error(token.loc, "Expected", syntax, "");
}

// Only process the next token if it is an identifier.
// Return true if it was an identifier.
bool HlslGrammar::acceptIdentifier(HlslToken& idToken)
{
    if (peekTokenClass(EHTokIdentifier)) {
        idToken = token;
        advanceToken();
        return true;
    }

    return false;
}

// compilationUnit
//      : list of externalDeclaration
//
bool HlslGrammar::acceptCompilationUnit()
{
    TIntermNode* unitNode = nullptr;

    while (! peekTokenClass(EHTokNone)) {
        // externalDeclaration
        TIntermNode* declarationNode;
        if (! acceptDeclaration(declarationNode))
            return false;

        // hook it up
        unitNode = intermediate.growAggregate(unitNode, declarationNode);
    }

    // set root of AST
    intermediate.setTreeRoot(unitNode);

    return true;
}

// declaration
//      : SEMICOLON
//      : fully_specified_type SEMICOLON
//      | fully_specified_type identifier array_specifier post_decls (EQUAL expression)opt SEMICOLON
//      | fully_specified_type identifier function_parameters post_decls SEMICOLON           // function prototype
//      | fully_specified_type identifier function_parameters post_decls compound_statement  // function definition
//
// 'node' could get created if the declaration creates code, like an initializer
// or a function body.
//
bool HlslGrammar::acceptDeclaration(TIntermNode*& node)
{
    node = nullptr;

    // fully_specified_type
    TType type;
    if (! acceptFullySpecifiedType(type))
        return false;

    // identifier
    HlslToken idToken;
    if (acceptIdentifier(idToken)) {
        // array_specifier
        TArraySizes* arraySizes = nullptr;
        acceptArraySpecifier(arraySizes);

        // post_decls
        acceptPostDecls(type);

        // EQUAL expression
        TIntermTyped* expressionNode = nullptr;
        if (acceptTokenClass(EHTokAssign)) {
            if (! acceptExpression(expressionNode)) {
                expected("initializer");
                return false;
            }
        }

        // SEMICOLON
        if (acceptTokenClass(EHTokSemicolon)) {
            node = parseContext.declareVariable(idToken.loc, *idToken.string, type, arraySizes, expressionNode);
            return true;
        }

        // function_parameters
        TFunction* function = new TFunction(idToken.string, type);
        if (acceptFunctionParameters(*function)) {
            // post_decls
            acceptPostDecls(type);

            // compound_statement
            if (peekTokenClass(EHTokLeftBrace))
                return acceptFunctionDefinition(*function, node);

            // SEMICOLON
            if (acceptTokenClass(EHTokSemicolon))
                return true;

            return false;
        }
    }

    // SEMICOLON
    if (acceptTokenClass(EHTokSemicolon))
        return true;

    return true;
}

// fully_specified_type
//      : type_specifier
//      | type_qualifier type_specifier
//
bool HlslGrammar::acceptFullySpecifiedType(TType& type)
{
    // type_qualifier
    TQualifier qualifier;
    qualifier.clear();
    acceptQualifier(qualifier);

    // type_specifier
    if (! acceptType(type))
        return false;
    type.getQualifier() = qualifier;

    return true;
}

// type_qualifier
//      : qualifier qualifier ...
//
// Zero or more of these, so this can't return false.
//
void HlslGrammar::acceptQualifier(TQualifier& qualifier)
{
    do {
        switch (peek()) {
        case EHTokStatic:
            // normal glslang default
            break;
        case EHTokExtern:
            // TODO: no meaning in glslang?
            break;
        case EHTokShared:
            // TODO: hint
            break;
        case EHTokGroupShared:
            qualifier.storage = EvqShared;
            break;
        case EHTokUniform:
            qualifier.storage = EvqUniform;
            break;
        case EHTokConst:
            qualifier.storage = EvqConst;
            break;
        case EHTokVolatile:
            qualifier.volatil = true;
            break;
        case EHTokLinear:
            qualifier.storage = EvqVaryingIn;
            qualifier.smooth = true;
            break;
        case EHTokCentroid:
            qualifier.centroid = true;
            break;
        case EHTokNointerpolation:
            qualifier.flat = true;
            break;
        case EHTokNoperspective:
            qualifier.nopersp = true;
            break;
        case EHTokSample:
            qualifier.sample = true;
            break;
        case EHTokRowMajor:
            qualifier.layoutMatrix = ElmRowMajor;
            break;
        case EHTokColumnMajor:
            qualifier.layoutMatrix = ElmColumnMajor;
            break;
        case EHTokPrecise:
            qualifier.noContraction = true;
            break;
        default:
            return;
        }
        advanceToken();
    } while (true);
}

// If token is for a type, update 'type' with the type information,
// and return true and advance.
// Otherwise, return false, and don't advance
bool HlslGrammar::acceptType(TType& type)
{
    switch (peek()) {
    case EHTokStruct:
        return acceptStruct(type);
        break;

    case EHTokIdentifier:
        // An identifier could be for a user-defined type.
        // Note we cache the symbol table lookup, to save for a later rule
        // when this is not a type.
        token.symbol = parseContext.symbolTable.find(*token.string);
        if (token.symbol && token.symbol->getAsVariable() && token.symbol->getAsVariable()->isUserType()) {
            type.shallowCopy(token.symbol->getType());
            advanceToken();
            return true;
        } else
            return false;

    case EHTokVoid:
        new(&type) TType(EbtVoid);
        break;

    case EHTokFloat:
        new(&type) TType(EbtFloat);
        break;
    case EHTokFloat1:
        new(&type) TType(EbtFloat);
        type.makeVector();
        break;
    case EHTokFloat2:
        new(&type) TType(EbtFloat, EvqTemporary, 2);
        break;
    case EHTokFloat3:
        new(&type) TType(EbtFloat, EvqTemporary, 3);
        break;
    case EHTokFloat4:
        new(&type) TType(EbtFloat, EvqTemporary, 4);
        break;

    case EHTokDouble:
        new(&type) TType(EbtDouble);
        break;
    case EHTokDouble1:
        new(&type) TType(EbtDouble);
        type.makeVector();
        break;
    case EHTokDouble2:
        new(&type) TType(EbtDouble, EvqTemporary, 2);
        break;
    case EHTokDouble3:
        new(&type) TType(EbtDouble, EvqTemporary, 3);
        break;
    case EHTokDouble4:
        new(&type) TType(EbtDouble, EvqTemporary, 4);
        break;

    case EHTokInt:
    case EHTokDword:
        new(&type) TType(EbtInt);
        break;
    case EHTokInt1:
        new(&type) TType(EbtInt);
        type.makeVector();
        break;
    case EHTokInt2:
        new(&type) TType(EbtInt, EvqTemporary, 2);
        break;
    case EHTokInt3:
        new(&type) TType(EbtInt, EvqTemporary, 3);
        break;
    case EHTokInt4:
        new(&type) TType(EbtInt, EvqTemporary, 4);
        break;

    case EHTokUint:
        new(&type) TType(EbtUint);
        break;
    case EHTokUint1:
        new(&type) TType(EbtUint);
        type.makeVector();
        break;
    case EHTokUint2:
        new(&type) TType(EbtUint, EvqTemporary, 2);
        break;
    case EHTokUint3:
        new(&type) TType(EbtUint, EvqTemporary, 3);
        break;
    case EHTokUint4:
        new(&type) TType(EbtUint, EvqTemporary, 4);
        break;

    case EHTokBool:
        new(&type) TType(EbtBool);
        break;
    case EHTokBool1:
        new(&type) TType(EbtBool);
        type.makeVector();
        break;
    case EHTokBool2:
        new(&type) TType(EbtBool, EvqTemporary, 2);
        break;
    case EHTokBool3:
        new(&type) TType(EbtBool, EvqTemporary, 3);
        break;
    case EHTokBool4:
        new(&type) TType(EbtBool, EvqTemporary, 4);
        break;

    case EHTokInt1x1:
        new(&type) TType(EbtInt, EvqTemporary, 0, 1, 1);
        break;
    case EHTokInt1x2:
        new(&type) TType(EbtInt, EvqTemporary, 0, 2, 1);
        break;
    case EHTokInt1x3:
        new(&type) TType(EbtInt, EvqTemporary, 0, 3, 1);
        break;
    case EHTokInt1x4:
        new(&type) TType(EbtInt, EvqTemporary, 0, 4, 1);
        break;
    case EHTokInt2x1:
        new(&type) TType(EbtInt, EvqTemporary, 0, 1, 2);
        break;
    case EHTokInt2x2:
        new(&type) TType(EbtInt, EvqTemporary, 0, 2, 2);
        break;
    case EHTokInt2x3:
        new(&type) TType(EbtInt, EvqTemporary, 0, 3, 2);
        break;
    case EHTokInt2x4:
        new(&type) TType(EbtInt, EvqTemporary, 0, 4, 2);
        break;
    case EHTokInt3x1:
        new(&type) TType(EbtInt, EvqTemporary, 0, 1, 3);
        break;
    case EHTokInt3x2:
        new(&type) TType(EbtInt, EvqTemporary, 0, 2, 3);
        break;
    case EHTokInt3x3:
        new(&type) TType(EbtInt, EvqTemporary, 0, 3, 3);
        break;
    case EHTokInt3x4:
        new(&type) TType(EbtInt, EvqTemporary, 0, 4, 3);
        break;
    case EHTokInt4x1:
        new(&type) TType(EbtInt, EvqTemporary, 0, 1, 4);
        break;
    case EHTokInt4x2:
        new(&type) TType(EbtInt, EvqTemporary, 0, 2, 4);
        break;
    case EHTokInt4x3:
        new(&type) TType(EbtInt, EvqTemporary, 0, 3, 4);
        break;
    case EHTokInt4x4:
        new(&type) TType(EbtInt, EvqTemporary, 0, 4, 4);
        break;

    case EHTokUint1x1:
        new(&type) TType(EbtUint, EvqTemporary, 0, 1, 1);
        break;
    case EHTokUint1x2:
        new(&type) TType(EbtUint, EvqTemporary, 0, 2, 1);
        break;
    case EHTokUint1x3:
        new(&type) TType(EbtUint, EvqTemporary, 0, 3, 1);
        break;
    case EHTokUint1x4:
        new(&type) TType(EbtUint, EvqTemporary, 0, 4, 1);
        break;
    case EHTokUint2x1:
        new(&type) TType(EbtUint, EvqTemporary, 0, 1, 2);
        break;
    case EHTokUint2x2:
        new(&type) TType(EbtUint, EvqTemporary, 0, 2, 2);
        break;
    case EHTokUint2x3:
        new(&type) TType(EbtUint, EvqTemporary, 0, 3, 2);
        break;
    case EHTokUint2x4:
        new(&type) TType(EbtUint, EvqTemporary, 0, 4, 2);
        break;
    case EHTokUint3x1:
        new(&type) TType(EbtUint, EvqTemporary, 0, 1, 3);
        break;
    case EHTokUint3x2:
        new(&type) TType(EbtUint, EvqTemporary, 0, 2, 3);
        break;
    case EHTokUint3x3:
        new(&type) TType(EbtUint, EvqTemporary, 0, 3, 3);
        break;
    case EHTokUint3x4:
        new(&type) TType(EbtUint, EvqTemporary, 0, 4, 3);
        break;
    case EHTokUint4x1:
        new(&type) TType(EbtUint, EvqTemporary, 0, 1, 4);
        break;
    case EHTokUint4x2:
        new(&type) TType(EbtUint, EvqTemporary, 0, 2, 4);
        break;
    case EHTokUint4x3:
        new(&type) TType(EbtUint, EvqTemporary, 0, 3, 4);
        break;
    case EHTokUint4x4:
        new(&type) TType(EbtUint, EvqTemporary, 0, 4, 4);
        break;

    case EHTokBool1x1:
        new(&type) TType(EbtBool, EvqTemporary, 0, 1, 1);
        break;
    case EHTokBool1x2:
        new(&type) TType(EbtBool, EvqTemporary, 0, 2, 1);
        break;
    case EHTokBool1x3:
        new(&type) TType(EbtBool, EvqTemporary, 0, 3, 1);
        break;
    case EHTokBool1x4:
        new(&type) TType(EbtBool, EvqTemporary, 0, 4, 1);
        break;
    case EHTokBool2x1:
        new(&type) TType(EbtBool, EvqTemporary, 0, 1, 2);
        break;
    case EHTokBool2x2:
        new(&type) TType(EbtBool, EvqTemporary, 0, 2, 2);
        break;
    case EHTokBool2x3:
        new(&type) TType(EbtBool, EvqTemporary, 0, 3, 2);
        break;
    case EHTokBool2x4:
        new(&type) TType(EbtBool, EvqTemporary, 0, 4, 2);
        break;
    case EHTokBool3x1:
        new(&type) TType(EbtBool, EvqTemporary, 0, 1, 3);
        break;
    case EHTokBool3x2:
        new(&type) TType(EbtBool, EvqTemporary, 0, 2, 3);
        break;
    case EHTokBool3x3:
        new(&type) TType(EbtBool, EvqTemporary, 0, 3, 3);
        break;
    case EHTokBool3x4:
        new(&type) TType(EbtBool, EvqTemporary, 0, 4, 3);
        break;
    case EHTokBool4x1:
        new(&type) TType(EbtBool, EvqTemporary, 0, 1, 4);
        break;
    case EHTokBool4x2:
        new(&type) TType(EbtBool, EvqTemporary, 0, 2, 4);
        break;
    case EHTokBool4x3:
        new(&type) TType(EbtBool, EvqTemporary, 0, 3, 4);
        break;
    case EHTokBool4x4:
        new(&type) TType(EbtBool, EvqTemporary, 0, 4, 4);
        break;

    case EHTokFloat1x1:
        new(&type) TType(EbtFloat, EvqTemporary, 0, 1, 1);
        break;
    case EHTokFloat1x2:
        new(&type) TType(EbtFloat, EvqTemporary, 0, 2, 1);
        break;
    case EHTokFloat1x3:
        new(&type) TType(EbtFloat, EvqTemporary, 0, 3, 1);
        break;
    case EHTokFloat1x4:
        new(&type) TType(EbtFloat, EvqTemporary, 0, 4, 1);
        break;
    case EHTokFloat2x1:
        new(&type) TType(EbtFloat, EvqTemporary, 0, 1, 2);
        break;
    case EHTokFloat2x2:
        new(&type) TType(EbtFloat, EvqTemporary, 0, 2, 2);
        break;
    case EHTokFloat2x3:
        new(&type) TType(EbtFloat, EvqTemporary, 0, 3, 2);
        break;
    case EHTokFloat2x4:
        new(&type) TType(EbtFloat, EvqTemporary, 0, 4, 2);
        break;
    case EHTokFloat3x1:
        new(&type) TType(EbtFloat, EvqTemporary, 0, 1, 3);
        break;
    case EHTokFloat3x2:
        new(&type) TType(EbtFloat, EvqTemporary, 0, 2, 3);
        break;
    case EHTokFloat3x3:
        new(&type) TType(EbtFloat, EvqTemporary, 0, 3, 3);
        break;
    case EHTokFloat3x4:
        new(&type) TType(EbtFloat, EvqTemporary, 0, 4, 3);
        break;
    case EHTokFloat4x1:
        new(&type) TType(EbtFloat, EvqTemporary, 0, 1, 4);
        break;
    case EHTokFloat4x2:
        new(&type) TType(EbtFloat, EvqTemporary, 0, 2, 4);
        break;
    case EHTokFloat4x3:
        new(&type) TType(EbtFloat, EvqTemporary, 0, 3, 4);
        break;
    case EHTokFloat4x4:
        new(&type) TType(EbtFloat, EvqTemporary, 0, 4, 4);
        break;

    case EHTokDouble1x1:
        new(&type) TType(EbtDouble, EvqTemporary, 0, 1, 1);
        break;
    case EHTokDouble1x2:
        new(&type) TType(EbtDouble, EvqTemporary, 0, 2, 1);
        break;
    case EHTokDouble1x3:
        new(&type) TType(EbtDouble, EvqTemporary, 0, 3, 1);
        break;
    case EHTokDouble1x4:
        new(&type) TType(EbtDouble, EvqTemporary, 0, 4, 1);
        break;
    case EHTokDouble2x1:
        new(&type) TType(EbtDouble, EvqTemporary, 0, 1, 2);
        break;
    case EHTokDouble2x2:
        new(&type) TType(EbtDouble, EvqTemporary, 0, 2, 2);
        break;
    case EHTokDouble2x3:
        new(&type) TType(EbtDouble, EvqTemporary, 0, 3, 2);
        break;
    case EHTokDouble2x4:
        new(&type) TType(EbtDouble, EvqTemporary, 0, 4, 2);
        break;
    case EHTokDouble3x1:
        new(&type) TType(EbtDouble, EvqTemporary, 0, 1, 3);
        break;
    case EHTokDouble3x2:
        new(&type) TType(EbtDouble, EvqTemporary, 0, 2, 3);
        break;
    case EHTokDouble3x3:
        new(&type) TType(EbtDouble, EvqTemporary, 0, 3, 3);
        break;
    case EHTokDouble3x4:
        new(&type) TType(EbtDouble, EvqTemporary, 0, 4, 3);
        break;
    case EHTokDouble4x1:
        new(&type) TType(EbtDouble, EvqTemporary, 0, 1, 4);
        break;
    case EHTokDouble4x2:
        new(&type) TType(EbtDouble, EvqTemporary, 0, 2, 4);
        break;
    case EHTokDouble4x3:
        new(&type) TType(EbtDouble, EvqTemporary, 0, 3, 4);
        break;
    case EHTokDouble4x4:
        new(&type) TType(EbtDouble, EvqTemporary, 0, 4, 4);
        break;

    default:
        return false;
    }

    advanceToken();

    return true;
}

// struct
//      : STRUCT IDENTIFIER LEFT_BRACE struct_declaration_list RIGHT_BRACE
//      | STRUCT            LEFT_BRACE struct_declaration_list RIGHT_BRACE
//
bool HlslGrammar::acceptStruct(TType& type)
{
    // STRUCT
    if (! acceptTokenClass(EHTokStruct))
        return false;

    // IDENTIFIER
    TString structName = "";
    if (peekTokenClass(EHTokIdentifier)) {
        structName = *token.string;
        advanceToken();
    }

    // LEFT_BRACE
    if (! acceptTokenClass(EHTokLeftBrace)) {
        expected("{");
        return false;
    }

    // struct_declaration_list
    TTypeList* typeList;
    if (! acceptStructDeclarationList(typeList)) {
        expected("struct member declarations");
        return false;
    }

    // RIGHT_BRACE
    if (! acceptTokenClass(EHTokRightBrace)) {
        expected("}");
        return false;
    }

    // create the user-defined type
    new(&type) TType(typeList, structName);

    // If it was named, which means it can be reused later, add
    // it to the symbol table.
    if (structName.size() > 0) {
        TVariable* userTypeDef = new TVariable(&structName, type, true);
        if (! parseContext.symbolTable.insert(*userTypeDef))
            parseContext.error(token.loc, "redefinition", structName.c_str(), "struct");
    }

    return true;
}

// struct_declaration_list
//      : struct_declaration SEMI_COLON struct_declaration SEMI_COLON ...
//
// struct_declaration
//      : fully_specified_type struct_declarator COMMA struct_declarator ...
//
// struct_declarator
//      : IDENTIFIER post_decls
//      | IDENTIFIER array_specifier post_decls
//
bool HlslGrammar::acceptStructDeclarationList(TTypeList*& typeList)
{
    typeList = new TTypeList();

    do {
        // success on seeing the RIGHT_BRACE coming up
        if (peekTokenClass(EHTokRightBrace))
            return true;

        // struct_declaration

        // fully_specified_type
        TType memberType;
        if (! acceptFullySpecifiedType(memberType)) {
            expected("member type");
            return false;
        }

        // struct_declarator COMMA struct_declarator ...
        do {
            // peek IDENTIFIER
            if (! peekTokenClass(EHTokIdentifier)) {
                expected("member name");
                return false;
            }

            // add it to the list of members
            TTypeLoc member = { new TType(EbtVoid), token.loc };
            member.type->shallowCopy(memberType);
            member.type->setFieldName(*token.string);
            typeList->push_back(member);

            // accept IDENTIFIER
            advanceToken();

            // array_specifier
            TArraySizes* arraySizes = nullptr;
            acceptArraySpecifier(arraySizes);
            if (arraySizes)
                typeList->back().type->newArraySizes(*arraySizes);

            acceptPostDecls(*member.type);

            // success on seeing the SEMICOLON coming up
            if (peekTokenClass(EHTokSemicolon))
                break;

            // COMMA
            if (! acceptTokenClass(EHTokComma)) {
                expected(",");
                return false;
            }

        } while (true);

        // SEMI_COLON
        if (! acceptTokenClass(EHTokSemicolon)) {
            expected(";");
            return false;
        }

    } while (true);
}

// function_parameters
//      : LEFT_PAREN parameter_declaration COMMA parameter_declaration ... RIGHT_PAREN
//      | LEFT_PAREN VOID RIGHT_PAREN
//
bool HlslGrammar::acceptFunctionParameters(TFunction& function)
{
    // LEFT_PAREN
    if (! acceptTokenClass(EHTokLeftParen))
        return false;

    // VOID RIGHT_PAREN
    if (! acceptTokenClass(EHTokVoid)) {
        do {
            // parameter_declaration
            if (! acceptParameterDeclaration(function))
                break;

            // COMMA
            if (! acceptTokenClass(EHTokComma))
                break;
        } while (true);
    }

    // RIGHT_PAREN
    if (! acceptTokenClass(EHTokRightParen)) {
        expected(")");
        return false;
    }

    return true;
}

// parameter_declaration
//      : fully_specified_type post_decls
//      | fully_specified_type identifier array_specifier post_decls
//
bool HlslGrammar::acceptParameterDeclaration(TFunction& function)
{
    // fully_specified_type
    TType* type = new TType;
    if (! acceptFullySpecifiedType(*type))
        return false;

    // identifier
    HlslToken idToken;
    acceptIdentifier(idToken);

    // array_specifier
    TArraySizes* arraySizes = nullptr;
    acceptArraySpecifier(arraySizes);
    if (arraySizes)
        type->newArraySizes(*arraySizes);

    // post_decls
    acceptPostDecls(*type);

    parseContext.paramFix(*type);

    TParameter param = { idToken.string, type };
    function.addParameter(param);

    return true;
}

// Do the work to create the function definition in addition to
// parsing the body (compound_statement).
bool HlslGrammar::acceptFunctionDefinition(TFunction& function, TIntermNode*& node)
{
    TFunction* functionDeclarator = parseContext.handleFunctionDeclarator(token.loc, function, false /* not prototype */);

    // This does a pushScope()
    node = parseContext.handleFunctionDefinition(token.loc, *functionDeclarator);

    // compound_statement
    TIntermNode* functionBody = nullptr;
    if (acceptCompoundStatement(functionBody)) {
        node = intermediate.growAggregate(node, functionBody);
        intermediate.setAggregateOperator(node, EOpFunction, functionDeclarator->getType(), token.loc);
        node->getAsAggregate()->setName(functionDeclarator->getMangledName().c_str());
        parseContext.popScope();

        return true;
    }

    return false;
}

// Accept an expression with parenthesis around it, where
// the parenthesis ARE NOT expression parenthesis, but the
// syntactically required ones like in "if ( expression )"
//
// Note this one is not set up to be speculative; as it gives
// errors if not found.
//
bool HlslGrammar::acceptParenExpression(TIntermTyped*& expression)
{
    // LEFT_PAREN
    if (! acceptTokenClass(EHTokLeftParen))
        expected("(");

    if (! acceptExpression(expression)) {
        expected("expression");
        return false;
    }

    // RIGHT_PAREN
    if (! acceptTokenClass(EHTokRightParen))
        expected(")");

    return true;
}

// The top-level full expression recognizer.
//
// expression
//      : assignment_expression COMMA assignment_expression COMMA assignment_expression ...
//
bool HlslGrammar::acceptExpression(TIntermTyped*& node)
{
    node = nullptr;

    // assignment_expression
    if (! acceptAssignmentExpression(node))
        return false;

    if (! peekTokenClass(EHTokComma))
        return true;

    do {
        // ... COMMA
        TSourceLoc loc = token.loc;
        advanceToken();

        // ... assignment_expression
        TIntermTyped* rightNode = nullptr;
        if (! acceptAssignmentExpression(rightNode)) {
            expected("assignment expression");
            return false;
        }

        node = intermediate.addComma(node, rightNode, loc);

        if (! peekTokenClass(EHTokComma))
            return true;
    } while (true);
}

// Accept an assignment expression, where assignment operations
// associate right-to-left.  This is, it is implicit, for example
//
//    a op (b op (c op d))
//
// assigment_expression
//      : binary_expression op binary_expression op binary_expression ...
//
bool HlslGrammar::acceptAssignmentExpression(TIntermTyped*& node)
{
    if (! acceptBinaryExpression(node, PlLogicalOr))
        return false;

    TOperator assignOp = HlslOpMap::assignment(peek());
    if (assignOp == EOpNull)
        return true;

    // ... op
    TSourceLoc loc = token.loc;
    advanceToken();

    // ... binary_expression
    // But, done by recursing this function, which automatically
    // gets the right-to-left associativity.
    TIntermTyped* rightNode = nullptr;
    if (! acceptAssignmentExpression(rightNode)) {
        expected("assignment expression");
        return false;
    }

    node = intermediate.addAssign(assignOp, node, rightNode, loc);

    if (! peekTokenClass(EHTokComma))
        return true;

    return true;
}

// Accept a binary expression, for binary operations that
// associate left-to-right.  This is, it is implicit, for example
//
//    ((a op b) op c) op d
//
// binary_expression
//      : expression op expression op expression ...
//
// where 'expression' is the next higher level in precedence.
//
bool HlslGrammar::acceptBinaryExpression(TIntermTyped*& node, PrecedenceLevel precedenceLevel)
{
    if (precedenceLevel > PlMul)
        return acceptUnaryExpression(node);

    // assignment_expression
    if (! acceptBinaryExpression(node, (PrecedenceLevel)(precedenceLevel + 1)))
        return false;

    TOperator op = HlslOpMap::binary(peek());
    PrecedenceLevel tokenLevel = HlslOpMap::precedenceLevel(op);
    if (tokenLevel < precedenceLevel)
        return true;

    do {
        // ... op
        TSourceLoc loc = token.loc;
        advanceToken();

        // ... expression
        TIntermTyped* rightNode = nullptr;
        if (! acceptBinaryExpression(rightNode, (PrecedenceLevel)(precedenceLevel + 1))) {
            expected("expression");
            return false;
        }

        node = intermediate.addBinaryMath(op, node, rightNode, loc);

        if (! peekTokenClass(EHTokComma))
            return true;
    } while (true);
}

// unary_expression
//      : (type) unary_expression
//      | + unary_expression
//      | - unary_expression
//      | ! unary_expression
//      | ~ unary_expression
//      | ++ unary_expression
//      | -- unary_expression
//      | postfix_expression
//
bool HlslGrammar::acceptUnaryExpression(TIntermTyped*& node)
{
    // (type) unary_expression
    // Have to look two steps ahead, because this could be, e.g., a
    // postfix_expression instead, since that also starts with at "(".
    if (acceptTokenClass(EHTokLeftParen)) {
        TType castType;
        if (acceptType(castType)) {
            if (! acceptTokenClass(EHTokRightParen)) {
                expected(")");
                return false;
            }

            // We've matched "(type)" now, get the expression to cast
            TSourceLoc loc = token.loc;
            if (! acceptUnaryExpression(node))
                return false;

            // Hook it up like a constructor
            TFunction* constructorFunction = parseContext.handleConstructorCall(loc, castType);
            if (constructorFunction == nullptr) {
                expected("type that can be constructed");
                return false;
            }
            TIntermTyped* arguments = nullptr;
            parseContext.handleFunctionArgument(constructorFunction, arguments, node);
            node = parseContext.handleFunctionCall(loc, constructorFunction, arguments);

            return true;
        } else {
            // This isn't a type cast, but it still started "(", so if it is a
            // unary expression, it can only be a postfix_expression, so try that.
            // Back it up first.
            recedeToken();
            return acceptPostfixExpression(node);
        }
    }

    // peek for "op unary_expression"
    TOperator unaryOp = HlslOpMap::preUnary(peek());
    
    // postfix_expression (if no unary operator)
    if (unaryOp == EOpNull)
        return acceptPostfixExpression(node);

    // op unary_expression
    TSourceLoc loc = token.loc;
    advanceToken();
    if (! acceptUnaryExpression(node))
        return false;

    // + is a no-op
    if (unaryOp == EOpAdd)
        return true;

    node = intermediate.addUnaryMath(unaryOp, node, loc);

    return node != nullptr;
}

// postfix_expression
//      : LEFT_PAREN expression RIGHT_PAREN
//      | literal
//      | constructor
//      | identifier
//      | function_call
//      | postfix_expression LEFT_BRACKET integer_expression RIGHT_BRACKET
//      | postfix_expression DOT IDENTIFIER
//      | postfix_expression INC_OP
//      | postfix_expression DEC_OP
//
bool HlslGrammar::acceptPostfixExpression(TIntermTyped*& node)
{
    // Not implemented as self-recursive:
    // The logical "right recursion" is done with an loop at the end

    // idToken will pick up either a variable or a function name in a function call
    HlslToken idToken;

    // Find something before the postfix operations, as they can't operate
    // on nothing.  So, no "return true", they fall through, only "return false".
    if (acceptTokenClass(EHTokLeftParen)) {
        // LEFT_PAREN expression RIGHT_PAREN
        if (! acceptExpression(node)) {
            expected("expression");
            return false;
        }
        if (! acceptTokenClass(EHTokRightParen)) {
            expected(")");
            return false;
        }
    } else if (acceptLiteral(node)) {
        // literal (nothing else to do yet), go on to the 
    } else if (acceptConstructor(node)) {
        // constructor (nothing else to do yet)
    } else if (acceptIdentifier(idToken)) {
        // identifier or function_call name
        if (! peekTokenClass(EHTokLeftParen)) {
            node = parseContext.handleVariable(idToken.loc, idToken.symbol, token.string);
        } else if (acceptFunctionCall(idToken, node)) {
            // function_call (nothing else to do yet)
        } else {
            expected("function call arguments");
            return false;
        }
    } else {
        // nothing found, can't post operate
        return false;
    }

    // Something was found, chain as many postfix operations as exist.
    do {
        TSourceLoc loc = token.loc;
        TOperator postOp = HlslOpMap::postUnary(peek());

        // Consume only a valid post-unary operator, otherwise we are done.
        switch (postOp) {
        case EOpIndexDirectStruct:
        case EOpIndexIndirect:
        case EOpPostIncrement:
        case EOpPostDecrement:
            advanceToken();
            break;
        default:
            return true;
        }

        // We have a valid post-unary operator, process it.
        switch (postOp) {
        case EOpIndexDirectStruct:
        {
            // DOT IDENTIFIER
            // includes swizzles and struct members
            // TODO: possibly includes "method" syntax
            HlslToken field;
            if (! acceptIdentifier(field)) {
                expected("swizzle or member");
                return false;
            }
            node = parseContext.handleDotDereference(field.loc, node, *field.string);
            break;
        }
        case EOpIndexIndirect:
        {
            // LEFT_BRACKET integer_expression RIGHT_BRACKET
            TIntermTyped* indexNode = nullptr;
            if (! acceptExpression(indexNode) ||
                ! peekTokenClass(EHTokRightBracket)) {
                expected("expression followed by ']'");
                return false;
            }
            advanceToken();
            node = parseContext.handleBracketDereference(indexNode->getLoc(), node, indexNode);
            break;
        }
        case EOpPostIncrement:
            // INC_OP
            // fall through
        case EOpPostDecrement:
            // DEC_OP
            node = intermediate.addUnaryMath(postOp, node, loc);
            break;
        default:
            assert(0);
            break;
        }
    } while (true);
}

// constructor
//      : type argument_list
//
bool HlslGrammar::acceptConstructor(TIntermTyped*& node)
{
    // type
    TType type;
    if (acceptType(type)) {
        TFunction* constructorFunction = parseContext.handleConstructorCall(token.loc, type);
        if (constructorFunction == nullptr)
            return false;

        // arguments
        TIntermTyped* arguments = nullptr;
        if (! acceptArguments(constructorFunction, arguments)) {
            expected("constructor arguments");
            return false;
        }

        // hook it up
        node = parseContext.handleFunctionCall(arguments->getLoc(), constructorFunction, arguments);

        return true;
    }

    return false;
}

// The function_call identifier was already recognized, and passed in as idToken.
//
// function_call
//      : [idToken] arguments
//
bool HlslGrammar::acceptFunctionCall(HlslToken idToken, TIntermTyped*& node)
{
    // arguments
    TFunction* function = new TFunction(idToken.string, TType(EbtVoid));
    TIntermTyped* arguments = nullptr;
    if (! acceptArguments(function, arguments))
        return false;

    node = parseContext.handleFunctionCall(idToken.loc, function, arguments);

    return true;
}

// arguments
//      : LEFT_PAREN expression COMMA expression COMMA ... RIGHT_PAREN
//
// The arguments are pushed onto the 'function' argument list and
// onto the 'arguments' aggregate.
//
bool HlslGrammar::acceptArguments(TFunction* function, TIntermTyped*& arguments)
{
    // LEFT_PAREN
    if (! acceptTokenClass(EHTokLeftParen))
        return false;

    do {
        // expression
        TIntermTyped* arg;
        if (! acceptAssignmentExpression(arg))
            break;

        // hook it up
        parseContext.handleFunctionArgument(function, arguments, arg);

        // COMMA
        if (! acceptTokenClass(EHTokComma))
            break;
    } while (true);

    // RIGHT_PAREN
    if (! acceptTokenClass(EHTokRightParen)) {
        expected(")");
        return false;
    }

    return true;
}

bool HlslGrammar::acceptLiteral(TIntermTyped*& node)
{
    switch (token.tokenClass) {
    case EHTokIntConstant:
        node = intermediate.addConstantUnion(token.i, token.loc, true);
        break;
    case EHTokFloatConstant:
        node = intermediate.addConstantUnion(token.d, EbtFloat, token.loc, true);
        break;
    case EHTokDoubleConstant:
        node = intermediate.addConstantUnion(token.d, EbtDouble, token.loc, true);
        break;
    case EHTokBoolConstant:
        node = intermediate.addConstantUnion(token.b, token.loc, true);
        break;

    default:
        return false;
    }

    advanceToken();

    return true;
}

// compound_statement
//      : LEFT_CURLY statement statement ... RIGHT_CURLY
//
bool HlslGrammar::acceptCompoundStatement(TIntermNode*& retStatement)
{
    TIntermAggregate* compoundStatement = nullptr;

    // LEFT_CURLY
    if (! acceptTokenClass(EHTokLeftBrace))
        return false;

    // statement statement ...
    TIntermNode* statement = nullptr;
    while (acceptStatement(statement)) {
        // hook it up
        compoundStatement = intermediate.growAggregate(compoundStatement, statement);
    }
    if (compoundStatement)
        compoundStatement->setOperator(EOpSequence);

    retStatement = compoundStatement;

    // RIGHT_CURLY
    return acceptTokenClass(EHTokRightBrace);
}

bool HlslGrammar::acceptScopedStatement(TIntermNode*& statement)
{
    parseContext.pushScope();
    bool result = acceptStatement(statement);
    parseContext.popScope();

    return result;
}

bool HlslGrammar::acceptScopedCompoundStatement(TIntermNode*& statement)
{
    parseContext.pushScope();
    bool result = acceptCompoundStatement(statement);
    parseContext.popScope();

    return result;
}

// statement
//      : attributes attributed_statement
//
// attributed_statement
//      : compound_statement
//      | SEMICOLON
//      | expression SEMICOLON
//      | declaration_statement
//      | selection_statement
//      | switch_statement
//      | case_label
//      | iteration_statement
//      | jump_statement
//
bool HlslGrammar::acceptStatement(TIntermNode*& statement)
{
    statement = nullptr;

    // attributes
    acceptAttributes();

    // attributed_statement
    switch (peek()) {
    case EHTokLeftBrace:
        return acceptScopedCompoundStatement(statement);

    case EHTokIf:
        return acceptSelectionStatement(statement);

    case EHTokSwitch:
        return acceptSwitchStatement(statement);

    case EHTokFor:
    case EHTokDo:
    case EHTokWhile:
        return acceptIterationStatement(statement);

    case EHTokContinue:
    case EHTokBreak:
    case EHTokDiscard:
    case EHTokReturn:
        return acceptJumpStatement(statement);

    case EHTokCase:
        return acceptCaseLabel(statement);

    case EHTokSemicolon:
        return acceptTokenClass(EHTokSemicolon);

    case EHTokRightBrace:
        // Performance: not strictly necessary, but stops a bunch of hunting early,
        // and is how sequences of statements end.
        return false;

    default:
        {
            // declaration
            if (acceptDeclaration(statement))
                return true;

            // expression
            TIntermTyped* node;
            if (acceptExpression(node))
                statement = node;
            else
                return false;

            // SEMICOLON (following an expression)
            if (! acceptTokenClass(EHTokSemicolon)) {
                expected(";");
                return false;
            }
        }
    }

    return true;
}

// attributes
//      : list of zero or more of:  LEFT_BRACKET attribute RIGHT_BRACKET
//
// attribute:
//      : UNROLL
//      | UNROLL LEFT_PAREN literal RIGHT_PAREN
//      | FASTOPT
//      | ALLOW_UAV_CONDITION
//      | BRANCH
//      | FLATTEN
//      | FORCECASE
//      | CALL
//
void HlslGrammar::acceptAttributes()
{
    // For now, accept the [ XXX(X) ] syntax, but drop.
    // TODO: subset to correct set?  Pass on?
    do {
        // LEFT_BRACKET?
        if (! acceptTokenClass(EHTokLeftBracket))
            return;

        // attribute
        if (peekTokenClass(EHTokIdentifier)) {
            // 'token.string' is the attribute
            advanceToken();
        } else if (! peekTokenClass(EHTokRightBracket)) {
            expected("identifier");
            advanceToken();
        }

        // (x)
        if (acceptTokenClass(EHTokLeftParen)) {
            TIntermTyped* node;
            if (! acceptLiteral(node))
                expected("literal");
            // 'node' has the literal in it
            if (! acceptTokenClass(EHTokRightParen))
                expected(")");
        }

        // RIGHT_BRACKET
        if (acceptTokenClass(EHTokRightBracket))
            continue;

        expected("]");
        return;

    } while (true);
}

// selection_statement
//      : IF LEFT_PAREN expression RIGHT_PAREN statement
//      : IF LEFT_PAREN expression RIGHT_PAREN statement ELSE statement
//
bool HlslGrammar::acceptSelectionStatement(TIntermNode*& statement)
{
    TSourceLoc loc = token.loc;

    // IF
    if (! acceptTokenClass(EHTokIf))
        return false;

    // so that something declared in the condition is scoped to the lifetimes
    // of the then-else statements
    parseContext.pushScope();

    // LEFT_PAREN expression RIGHT_PAREN
    TIntermTyped* condition;
    if (! acceptParenExpression(condition))
        return false;

    // create the child statements
    TIntermNodePair thenElse = { nullptr, nullptr };

    // then statement
    if (! acceptScopedStatement(thenElse.node1)) {
        expected("then statement");
        return false;
    }

    // ELSE
    if (acceptTokenClass(EHTokElse)) {
        // else statement
        if (! acceptScopedStatement(thenElse.node2)) {
            expected("else statement");
            return false;
        }
    }

    // Put the pieces together
    statement = intermediate.addSelection(condition, thenElse, loc);
    parseContext.popScope();

    return true;
}

bool HlslGrammar::acceptSwitchStatement(TIntermNode*& statement)
{
    return false;
}

// iteration_statement
//      : WHILE LEFT_PAREN condition RIGHT_PAREN statement
//      | DO LEFT_BRACE statement RIGHT_BRACE WHILE LEFT_PAREN expression RIGHT_PAREN SEMICOLON
//      | FOR LEFT_PAREN for_init_statement for_rest_statement RIGHT_PAREN statement
//
// Non-speculative, only call if it needs to be found; WHILE or DO or FOR already seen.
bool HlslGrammar::acceptIterationStatement(TIntermNode*& statement)
{
    TSourceLoc loc = token.loc;
    TIntermTyped* condition = nullptr;

    EHlslTokenClass loop = peek();
    assert(loop == EHTokDo || loop == EHTokFor || loop == EHTokWhile);

    //  WHILE or DO or FOR
    advanceToken();

    switch (loop) {
    case EHTokWhile:
        // so that something declared in the condition is scoped to the lifetime
        // of the while sub-statement
        parseContext.pushScope();
        parseContext.nestLooping();

        // LEFT_PAREN condition RIGHT_PAREN
        if (! acceptParenExpression(condition))
            return false;

        // statement
        if (! acceptScopedStatement(statement)) {
            expected("while sub-statement");
            return false;
        }

        parseContext.unnestLooping();
        parseContext.popScope();

        statement = intermediate.addLoop(statement, condition, nullptr, true, loc);

        return true;

    case EHTokDo:
        parseContext.nestLooping();

        if (! acceptTokenClass(EHTokLeftBrace))
            expected("{");

        // statement
        if (! peekTokenClass(EHTokRightBrace) && ! acceptScopedStatement(statement)) {
            expected("do sub-statement");
            return false;
        }

        if (! acceptTokenClass(EHTokRightBrace))
            expected("}");

        // WHILE
        if (! acceptTokenClass(EHTokWhile)) {
            expected("while");
            return false;
        }

        // LEFT_PAREN condition RIGHT_PAREN
        TIntermTyped* condition;
        if (! acceptParenExpression(condition))
            return false;

        if (! acceptTokenClass(EHTokSemicolon))
            expected(";");

        parseContext.unnestLooping();

        statement = intermediate.addLoop(statement, condition, 0, false, loc);

        return true;

    case EHTokFor:
    {
        // LEFT_PAREN
        if (! acceptTokenClass(EHTokLeftParen))
            expected("(");

        // so that something declared in the condition is scoped to the lifetime
        // of the for sub-statement
        parseContext.pushScope();

        // initializer SEMI_COLON
        TIntermTyped* initializer = nullptr; // TODO, "for (initializer" needs to support decl. statement
        acceptExpression(initializer);
        if (! acceptTokenClass(EHTokSemicolon))
            expected(";");

        parseContext.nestLooping();

        // condition SEMI_COLON
        acceptExpression(condition);
        if (! acceptTokenClass(EHTokSemicolon))
            expected(";");

        // iterator SEMI_COLON
        TIntermTyped* iterator = nullptr;
        acceptExpression(iterator);
        if (! acceptTokenClass(EHTokRightParen))
            expected(")");

        // statement
        if (! acceptScopedStatement(statement)) {
            expected("for sub-statement");
            return false;
        }

        statement = intermediate.addForLoop(statement, initializer, condition, iterator, true, loc);

        parseContext.popScope();
        parseContext.unnestLooping();

        return true;
    }

    default:
        return false;
    }
}

// jump_statement
//      : CONTINUE SEMICOLON
//      | BREAK SEMICOLON
//      | DISCARD SEMICOLON
//      | RETURN SEMICOLON
//      | RETURN expression SEMICOLON
//
bool HlslGrammar::acceptJumpStatement(TIntermNode*& statement)
{
    switch (peek()) {
    case EHTokContinue:
    case EHTokBreak:
    case EHTokDiscard:
        // TODO
        return false;

    case EHTokReturn:
        // return
        if (acceptTokenClass(EHTokReturn)) {
            // expression
            TIntermTyped* node;
            if (acceptExpression(node)) {
                // hook it up
                statement = intermediate.addBranch(EOpReturn, node, token.loc);
            } else
                statement = intermediate.addBranch(EOpReturn, token.loc);

            // SEMICOLON
            if (! acceptTokenClass(EHTokSemicolon)) {
                expected(";");
                return false;
            }

            return true;
        }

    default:
        return false;
    }
}


bool HlslGrammar::acceptCaseLabel(TIntermNode*& statement)
{
    return false;
}

// array_specifier
//      : LEFT_BRACKET integer_expression RGHT_BRACKET post_decls // optional
//
void HlslGrammar::acceptArraySpecifier(TArraySizes*& arraySizes)
{
    arraySizes = nullptr;

    if (! acceptTokenClass(EHTokLeftBracket))
        return;

    TSourceLoc loc = token.loc;
    TIntermTyped* sizeExpr;
    if (! acceptAssignmentExpression(sizeExpr)) {
        expected("array-sizing expression");
        return;
    }

    if (! acceptTokenClass(EHTokRightBracket)) {
        expected("]");
        return;
    }

    TArraySize arraySize;
    parseContext.arraySizeCheck(loc, sizeExpr, arraySize);
    arraySizes = new TArraySizes;
    arraySizes->addInnerSize(arraySize);
}

// post_decls
//      : COLON semantic    // optional
//        COLON PACKOFFSET LEFT_PAREN ... RIGHT_PAREN  // optional
//        COLON REGISTER    // optional
//        annotations       // optional
//
void HlslGrammar::acceptPostDecls(TType& type)
{
    do {
        // COLON 
        if (acceptTokenClass(EHTokColon)) {
            HlslToken idToken;
            if (acceptTokenClass(EHTokPackOffset)) {
                if (! acceptTokenClass(EHTokLeftParen)) {
                    expected("(");
                    return;
                }
                acceptTokenClass(EHTokIdentifier);
                acceptTokenClass(EHTokDot);
                acceptTokenClass(EHTokIdentifier);
                if (! acceptTokenClass(EHTokRightParen)) {
                    expected(")");
                    break;
                }
                // TODO: process the packoffset information
            } else if (! acceptIdentifier(idToken)) {
                expected("semantic or packoffset or register");
                return;
            } else if (*idToken.string == "register") {
                if (! acceptTokenClass(EHTokLeftParen)) {
                    expected("(");
                    return;
                }
                acceptTokenClass(EHTokIdentifier);
                acceptTokenClass(EHTokComma);
                acceptTokenClass(EHTokIdentifier);
                acceptTokenClass(EHTokLeftBracket);
                if (peekTokenClass(EHTokIntConstant))
                    advanceToken();
                acceptTokenClass(EHTokRightBracket);
                if (! acceptTokenClass(EHTokRightParen)) {
                    expected(")");
                    break;
                }
                // TODO: process the register information
            } else {
                // semantic, in idToken.string
                parseContext.handleSemantic(type, *idToken.string);
            }
        } else if (acceptTokenClass(EHTokLeftAngle)) {
            // TODO: process annotations, just accepting them for now
            do {
                if (peekTokenClass(EHTokNone))
                    return;
                if (acceptTokenClass(EHTokRightAngle))
                    break;
                advanceToken();
            } while (true);
        } else
            break;

    } while (true);
}

} // end namespace glslang
