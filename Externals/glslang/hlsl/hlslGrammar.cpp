//
//Copyright (C) 2016 Google, Inc.
//Copyright (C) 2016 LunarG, Inc.
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

void HlslGrammar::unimplemented(const char* error)
{
    parseContext.error(token.loc, "Unimplemented", error, "");
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

// sampler_state
//      : LEFT_BRACE [sampler_state_assignment ... ] RIGHT_BRACE 
//
// sampler_state_assignment
//     : sampler_state_identifier EQUAL value SEMICOLON
//
// sampler_state_identifier
//     : ADDRESSU
//     | ADDRESSV
//     | ADDRESSW
//     | BORDERCOLOR
//     | FILTER
//     | MAXANISOTROPY
//     | MAXLOD
//     | MINLOD
//     | MIPLODBIAS
//
bool HlslGrammar::acceptSamplerState()
{
    // TODO: this should be genericized to accept a list of valid tokens and
    // return token/value pairs.  Presently it is specific to texture values.

    if (! acceptTokenClass(EHTokLeftBrace))
        return true;

    parseContext.warn(token.loc, "unimplemented", "immediate sampler state", "");
    
    do {
        // read state name
        HlslToken state;
        if (! acceptIdentifier(state))
            break;  // end of list

        // FXC accepts any case
        TString stateName = *state.string;
        std::transform(stateName.begin(), stateName.end(), stateName.begin(), ::tolower);

        if (! acceptTokenClass(EHTokAssign)) {
            expected("assign");
            return false;
        }

        if (stateName == "minlod" || stateName == "maxlod") {
            if (! peekTokenClass(EHTokIntConstant)) {
                expected("integer");
                return false;
            }

            TIntermTyped* lod = nullptr;
            if (! acceptLiteral(lod))  // should never fail, since we just looked for an integer
                return false;
        } else if (stateName == "maxanisotropy") {
            if (! peekTokenClass(EHTokIntConstant)) {
                expected("integer");
                return false;
            }

            TIntermTyped* maxAnisotropy = nullptr;
            if (! acceptLiteral(maxAnisotropy))  // should never fail, since we just looked for an integer
                return false;
        } else if (stateName == "filter") {
            HlslToken filterMode;
            if (! acceptIdentifier(filterMode)) {
                expected("filter mode");
                return false;
            }
        } else if (stateName == "addressu" || stateName == "addressv" || stateName == "addressw") {
            HlslToken addrMode;
            if (! acceptIdentifier(addrMode)) {
                expected("texture address mode");
                return false;
            }
        } else if (stateName == "miplodbias") {
            TIntermTyped* lodBias = nullptr;
            if (! acceptLiteral(lodBias)) {
                expected("lod bias");
                return false;
            }
        } else if (stateName == "bordercolor") {
            return false;
        } else {
            expected("texture state");
            return false;
        }

        // SEMICOLON
        if (! acceptTokenClass(EHTokSemicolon)) {
            expected("semicolon");
            return false;
        }
    } while (true);

    if (! acceptTokenClass(EHTokRightBrace))
        return false;

    return true;
}

// sampler_declaration_dx9
//    : SAMPLER identifier EQUAL sampler_type sampler_state
//
bool HlslGrammar::acceptSamplerDeclarationDX9(TType& /*type*/)
{
    if (! acceptTokenClass(EHTokSampler))
        return false;
 
    // TODO: remove this when DX9 style declarations are implemented.
    unimplemented("Direct3D 9 sampler declaration");

    // read sampler name
    HlslToken name;
    if (! acceptIdentifier(name)) {
        expected("sampler name");
        return false;
    }

    if (! acceptTokenClass(EHTokAssign)) {
        expected("=");
        return false;
    }

    return false;
}


// declaration
//      : sampler_declaration_dx9 post_decls SEMICOLON
//      | fully_specified_type declarator_list SEMICOLON
//      | fully_specified_type identifier function_parameters post_decls compound_statement  // function definition
//      | fully_specified_type identifier sampler_state post_decls compound_statement        // sampler definition
//      | typedef declaration
//
// declarator_list
//      : declarator COMMA declarator COMMA declarator...  // zero or more declarators
//
// declarator
//      : identifier array_specifier post_decls
//      | identifier array_specifier post_decls EQUAL assignment_expression
//      | identifier function_parameters post_decls                                          // function prototype
//
// Parsing has to go pretty far in to know whether it's a variable, prototype, or
// function definition, so the implementation below doesn't perfectly divide up the grammar
// as above.  (The 'identifier' in the first item in init_declarator list is the
// same as 'identifier' for function declarations.)
//
// 'node' could get populated if the declaration creates code, like an initializer
// or a function body.
//
bool HlslGrammar::acceptDeclaration(TIntermNode*& node)
{
    node = nullptr;
    bool list = false;

    // typedef
    bool typedefDecl = acceptTokenClass(EHTokTypedef);

    TType type;

    // DX9 sampler declaration use a different syntax
    if (acceptSamplerDeclarationDX9(type))
        return true;

    // fully_specified_type
    if (! acceptFullySpecifiedType(type))
        return false;

    if (type.getQualifier().storage == EvqTemporary && parseContext.symbolTable.atGlobalLevel()) {
        if (type.getBasicType() == EbtSampler) {
            // Sampler/textures are uniform by default (if no explicit qualifier is present) in
            // HLSL.  This line silently converts samplers *explicitly* declared static to uniform,
            // which is incorrect but harmless.
            type.getQualifier().storage = EvqUniform; 
        } else {
            type.getQualifier().storage = EvqGlobal;
        }
    }

    // identifier
    HlslToken idToken;
    while (acceptIdentifier(idToken)) {
        // function_parameters
        TFunction* function = new TFunction(idToken.string, type);
        if (acceptFunctionParameters(*function)) {
            // post_decls
            acceptPostDecls(type);

            // compound_statement (function body definition) or just a prototype?
            if (peekTokenClass(EHTokLeftBrace)) {
                if (list)
                    parseContext.error(idToken.loc, "function body can't be in a declarator list", "{", "");
                if (typedefDecl)
                    parseContext.error(idToken.loc, "function body can't be in a typedef", "{", "");
                return acceptFunctionDefinition(*function, node);
            } else {
                if (typedefDecl)
                    parseContext.error(idToken.loc, "function typedefs not implemented", "{", "");
                parseContext.handleFunctionDeclarator(idToken.loc, *function, true);
            }
        } else {
            // a variable declaration

            // array_specifier
            TArraySizes* arraySizes = nullptr;
            acceptArraySpecifier(arraySizes);

            // samplers accept immediate sampler state
            if (type.getBasicType() == EbtSampler) {
                if (! acceptSamplerState())
                    return false;
            }

            // post_decls
            acceptPostDecls(type);

            // EQUAL assignment_expression
            TIntermTyped* expressionNode = nullptr;
            if (acceptTokenClass(EHTokAssign)) {
                if (typedefDecl)
                    parseContext.error(idToken.loc, "can't have an initializer", "typedef", "");
                if (! acceptAssignmentExpression(expressionNode)) {
                    expected("initializer");
                    return false;
                }
            }

            if (typedefDecl)
                parseContext.declareTypedef(idToken.loc, *idToken.string, type, arraySizes);
            else {
                // Declare the variable and add any initializer code to the AST.
                // The top-level node is always made into an aggregate, as that's
                // historically how the AST has been.
                node = intermediate.growAggregate(node,
                                                  parseContext.declareVariable(idToken.loc, *idToken.string, type,
                                                                               arraySizes, expressionNode),
                                                  idToken.loc);
            }
        }

        if (acceptTokenClass(EHTokComma)) {
            list = true;
            continue;
        }
    };

    // The top-level node is a sequence.
    if (node != nullptr)
        node->getAsAggregate()->setOperator(EOpSequence);

    // SEMICOLON
    if (! acceptTokenClass(EHTokSemicolon)) {
        expected(";");
        return false;
    }
    
    return true;
}

// control_declaration
//      : fully_specified_type identifier EQUAL expression
//
bool HlslGrammar::acceptControlDeclaration(TIntermNode*& node)
{
    node = nullptr;

    // fully_specified_type
    TType type;
    if (! acceptFullySpecifiedType(type))
        return false;

    // identifier
    HlslToken idToken;
    if (! acceptIdentifier(idToken)) {
        expected("identifier");
        return false;
    }

    // EQUAL
    TIntermTyped* expressionNode = nullptr;
    if (! acceptTokenClass(EHTokAssign)) {
        expected("=");
        return false;
    }

    // expression
    if (! acceptExpression(expressionNode)) {
        expected("initializer");
        return false;
    }

    node = parseContext.declareVariable(idToken.loc, *idToken.string, type, 0, expressionNode);

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
        case EHTokIn:
            qualifier.storage = EvqIn;
            break;
        case EHTokOut:
            qualifier.storage = EvqOut;
            break;
        case EHTokInOut:
            qualifier.storage = EvqInOut;
            break;
        default:
            return;
        }
        advanceToken();
    } while (true);
}

// template_type
//      : FLOAT
//      | DOUBLE
//      | INT
//      | DWORD
//      | UINT
//      | BOOL
//
bool HlslGrammar::acceptTemplateType(TBasicType& basicType)
{
    switch (peek()) {
    case EHTokFloat:
        basicType = EbtFloat;
        break;
    case EHTokDouble:
        basicType = EbtDouble;
        break;
    case EHTokInt:
    case EHTokDword:
        basicType = EbtInt;
        break;
    case EHTokUint:
        basicType = EbtUint;
        break;
    case EHTokBool:
        basicType = EbtBool;
        break;
    default:
        return false;
    }

    advanceToken();

    return true;
}

// vector_template_type
//      : VECTOR
//      | VECTOR LEFT_ANGLE template_type COMMA integer_literal RIGHT_ANGLE
//
bool HlslGrammar::acceptVectorTemplateType(TType& type)
{
    if (! acceptTokenClass(EHTokVector))
        return false;

    if (! acceptTokenClass(EHTokLeftAngle)) {
        // in HLSL, 'vector' alone means float4.
        new(&type) TType(EbtFloat, EvqTemporary, 4);
        return true;
    }

    TBasicType basicType;
    if (! acceptTemplateType(basicType)) {
        expected("scalar type");
        return false;
    }

    // COMMA
    if (! acceptTokenClass(EHTokComma)) {
        expected(",");
        return false;
    }

    // integer
    if (! peekTokenClass(EHTokIntConstant)) {
        expected("literal integer");
        return false;
    }

    TIntermTyped* vecSize;
    if (! acceptLiteral(vecSize))
        return false;

    const int vecSizeI = vecSize->getAsConstantUnion()->getConstArray()[0].getIConst();

    new(&type) TType(basicType, EvqTemporary, vecSizeI);

    if (vecSizeI == 1)
        type.makeVector();

    if (!acceptTokenClass(EHTokRightAngle)) {
        expected("right angle bracket");
        return false;
    }

    return true;
}

// matrix_template_type
//      : MATRIX
//      | MATRIX LEFT_ANGLE template_type COMMA integer_literal COMMA integer_literal RIGHT_ANGLE
//
bool HlslGrammar::acceptMatrixTemplateType(TType& type)
{
    if (! acceptTokenClass(EHTokMatrix))
        return false;

    if (! acceptTokenClass(EHTokLeftAngle)) {
        // in HLSL, 'matrix' alone means float4x4.
        new(&type) TType(EbtFloat, EvqTemporary, 0, 4, 4);
        return true;
    }

    TBasicType basicType;
    if (! acceptTemplateType(basicType)) {
        expected("scalar type");
        return false;
    }

    // COMMA
    if (! acceptTokenClass(EHTokComma)) {
        expected(",");
        return false;
    }

    // integer rows
    if (! peekTokenClass(EHTokIntConstant)) {
        expected("literal integer");
        return false;
    }

    TIntermTyped* rows;
    if (! acceptLiteral(rows))
        return false;

    // COMMA
    if (! acceptTokenClass(EHTokComma)) {
        expected(",");
        return false;
    }
    
    // integer cols
    if (! peekTokenClass(EHTokIntConstant)) {
        expected("literal integer");
        return false;
    }

    TIntermTyped* cols;
    if (! acceptLiteral(cols))
        return false;

    new(&type) TType(basicType, EvqTemporary, 0,
                     cols->getAsConstantUnion()->getConstArray()[0].getIConst(),
                     rows->getAsConstantUnion()->getConstArray()[0].getIConst());

    if (!acceptTokenClass(EHTokRightAngle)) {
        expected("right angle bracket");
        return false;
    }

    return true;
}


// sampler_type
//      : SAMPLER
//      | SAMPLER1D
//      | SAMPLER2D
//      | SAMPLER3D
//      | SAMPLERCUBE
//      | SAMPLERSTATE
//      | SAMPLERCOMPARISONSTATE
bool HlslGrammar::acceptSamplerType(TType& type)
{
    // read sampler type
    const EHlslTokenClass samplerType = peek();

    TSamplerDim dim = EsdNone;

    switch (samplerType) {
    case EHTokSampler:      break;
    case EHTokSampler1d:    dim = Esd1D; break;
    case EHTokSampler2d:    dim = Esd2D; break;
    case EHTokSampler3d:    dim = Esd3D; break;
    case EHTokSamplerCube:  dim = EsdCube; break;
    case EHTokSamplerState: break;
    case EHTokSamplerComparisonState: break;
    default:
        return false;  // not a sampler declaration
    }

    advanceToken();  // consume the sampler type keyword

    TArraySizes* arraySizes = nullptr; // TODO: array
    bool shadow = false;               // TODO: shadow

    TSampler sampler;
    sampler.setPureSampler(shadow);

    type.shallowCopy(TType(sampler, EvqUniform, arraySizes));

    return true;
}

// texture_type
//      | BUFFER
//      | TEXTURE1D
//      | TEXTURE1DARRAY
//      | TEXTURE2D
//      | TEXTURE2DARRAY
//      | TEXTURE3D
//      | TEXTURECUBE
//      | TEXTURECUBEARRAY
//      | TEXTURE2DMS
//      | TEXTURE2DMSARRAY
bool HlslGrammar::acceptTextureType(TType& type)
{
    const EHlslTokenClass textureType = peek();

    TSamplerDim dim = EsdNone;
    bool array = false;
    bool ms    = false;

    switch (textureType) {
    case EHTokBuffer:            dim = EsdBuffer;                      break;
    case EHTokTexture1d:         dim = Esd1D;                          break;
    case EHTokTexture1darray:    dim = Esd1D; array = true;            break;
    case EHTokTexture2d:         dim = Esd2D;                          break;
    case EHTokTexture2darray:    dim = Esd2D; array = true;            break;
    case EHTokTexture3d:         dim = Esd3D;                          break; 
    case EHTokTextureCube:       dim = EsdCube;                        break;
    case EHTokTextureCubearray:  dim = EsdCube; array = true;          break;
    case EHTokTexture2DMS:       dim = Esd2D; ms = true;               break;
    case EHTokTexture2DMSarray:  dim = Esd2D; array = true; ms = true; break;
    default:
        return false;  // not a texture declaration
    }

    advanceToken();  // consume the texture object keyword

    TType txType(EbtFloat, EvqUniform, 4); // default type is float4
    
    TIntermTyped* msCount = nullptr;

    // texture type: required for multisample types!
    if (acceptTokenClass(EHTokLeftAngle)) {
        if (! acceptType(txType)) {
            expected("scalar or vector type");
            return false;
        }

        const TBasicType basicRetType = txType.getBasicType() ;

        if (basicRetType != EbtFloat && basicRetType != EbtUint && basicRetType != EbtInt) {
            unimplemented("basic type in texture");
            return false;
        }

        if (!txType.isScalar() && !txType.isVector()) {
            expected("scalar or vector type");
            return false;
        }

        if (txType.getVectorSize() != 1 && txType.getVectorSize() != 4) {
            // TODO: handle vec2/3 types
            expected("vector size not yet supported in texture type");
            return false;
        }

        if (ms && acceptTokenClass(EHTokComma)) {
            // read sample count for multisample types, if given
            if (! peekTokenClass(EHTokIntConstant)) {
                expected("multisample count");
                return false;
            }

            if (! acceptLiteral(msCount))  // should never fail, since we just found an integer
                return false;
        }

        if (! acceptTokenClass(EHTokRightAngle)) {
            expected("right angle bracket");
            return false;
        }
    } else if (ms) {
        expected("texture type for multisample");
        return false;
    }

    TArraySizes* arraySizes = nullptr;
    const bool shadow = txType.isScalar() || (txType.isVector() && txType.getVectorSize() == 1);

    TSampler sampler;
    sampler.setTexture(txType.getBasicType(), dim, array, shadow, ms);
    
    type.shallowCopy(TType(sampler, EvqUniform, arraySizes));

    return true;
}


// If token is for a type, update 'type' with the type information,
// and return true and advance.
// Otherwise, return false, and don't advance
bool HlslGrammar::acceptType(TType& type)
{
    switch (peek()) {
    case EHTokVector:
        return acceptVectorTemplateType(type);
        break;

    case EHTokMatrix:
        return acceptMatrixTemplateType(type);
        break;

    case EHTokSampler:                // fall through
    case EHTokSampler1d:              // ...
    case EHTokSampler2d:              // ...
    case EHTokSampler3d:              // ...
    case EHTokSamplerCube:            // ...
    case EHTokSamplerState:           // ...
    case EHTokSamplerComparisonState: // ...
        return acceptSamplerType(type);
        break;

    case EHTokBuffer:                 // fall through
    case EHTokTexture1d:              // ...
    case EHTokTexture1darray:         // ...
    case EHTokTexture2d:              // ...
    case EHTokTexture2darray:         // ...
    case EHTokTexture3d:              // ...
    case EHTokTextureCube:            // ...
    case EHTokTextureCubearray:       // ...
    case EHTokTexture2DMS:            // ...
    case EHTokTexture2DMSarray:       // ...
        return acceptTextureType(type);
        break;

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
// syntactically required ones like in "if ( expression )".
//
// Also accepts a declaration expression; "if (int a = expression)".
//
// Note this one is not set up to be speculative; as it gives
// errors if not found.
//
bool HlslGrammar::acceptParenExpression(TIntermTyped*& expression)
{
    // LEFT_PAREN
    if (! acceptTokenClass(EHTokLeftParen))
        expected("(");

    bool decl = false;
    TIntermNode* declNode = nullptr;
    decl = acceptControlDeclaration(declNode);
    if (decl) {
        if (declNode == nullptr || declNode->getAsTyped() == nullptr) {
            expected("initialized declaration");
            return false;
        } else
            expression = declNode->getAsTyped();
    } else {
        // no declaration
        if (! acceptExpression(expression)) {
            expected("expression");
            return false;
        }
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

// initializer
//      : LEFT_BRACE initializer_list RIGHT_BRACE
//
// initializer_list
//      : assignment_expression COMMA assignment_expression COMMA ...
//
bool HlslGrammar::acceptInitializer(TIntermTyped*& node)
{
    // LEFT_BRACE
    if (! acceptTokenClass(EHTokLeftBrace))
        return false;

    // initializer_list
    TSourceLoc loc = token.loc;
    node = nullptr;
    do {
        // assignment_expression
        TIntermTyped* expr;
        if (! acceptAssignmentExpression(expr)) {
            expected("assignment expression in initializer list");
            return false;
        }
        node = intermediate.growAggregate(node, expr, loc);

        // COMMA
        if (acceptTokenClass(EHTokComma))
            continue;

        // RIGHT_BRACE
        if (acceptTokenClass(EHTokRightBrace))
            return true;

        expected(", or }");
        return false;
    } while (true);
}

// Accept an assignment expression, where assignment operations
// associate right-to-left.  That is, it is implicit, for example
//
//    a op (b op (c op d))
//
// assigment_expression
//      : binary_expression op binary_expression op binary_expression ...
//      | initializer
//
bool HlslGrammar::acceptAssignmentExpression(TIntermTyped*& node)
{
    // initializer
    if (peekTokenClass(EHTokLeftBrace)) {
        if (acceptInitializer(node))
            return true;

        expected("initializer");
        return false;
    }

    // binary_expression
    if (! acceptBinaryExpression(node, PlLogicalOr))
        return false;

    // assignment operation?
    TOperator assignOp = HlslOpMap::assignment(peek());
    if (assignOp == EOpNull)
        return true;

    // assignment op
    TSourceLoc loc = token.loc;
    advanceToken();

    // binary_expression
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
            HlslToken field;
            if (! acceptIdentifier(field)) {
                expected("swizzle or member");
                return false;
            }

            TIntermTyped* base = node; // preserve for method function calls
            node = parseContext.handleDotDereference(field.loc, node, *field.string);

            // In the event of a method node, we look for an open paren and accept the function call.
            if (node->getAsMethodNode() != nullptr && peekTokenClass(EHTokLeftParen)) {
                if (! acceptFunctionCall(field, node, base)) {
                    expected("function parameters");
                    return false;
                }
            }

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
bool HlslGrammar::acceptFunctionCall(HlslToken idToken, TIntermTyped*& node, TIntermTyped* base)
{
    // arguments
    TFunction* function = new TFunction(idToken.string, TType(EbtVoid));
    TIntermTyped* arguments = nullptr;

    // methods have an implicit first argument of the calling object.
    if (base != nullptr)
        parseContext.handleFunctionArgument(function, arguments, base);

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
        TIntermBranch* branch = statement ? statement->getAsBranchNode() : nullptr;
        if (branch != nullptr && (branch->getFlowOp() == EOpCase ||
                                  branch->getFlowOp() == EOpDefault)) {
            // hook up individual subsequences within a switch statement
            parseContext.wrapupSwitchSubsequence(compoundStatement, statement);
            compoundStatement = nullptr;
        } else {
            // hook it up to the growing compound statement
            compoundStatement = intermediate.growAggregate(compoundStatement, statement);
        }
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
    case EHTokDefault:
        return acceptDefaultLabel(statement);

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

// switch_statement
//      : SWITCH LEFT_PAREN expression RIGHT_PAREN compound_statement
//
bool HlslGrammar::acceptSwitchStatement(TIntermNode*& statement)
{
    // SWITCH
    TSourceLoc loc = token.loc;
    if (! acceptTokenClass(EHTokSwitch))
        return false;

    // LEFT_PAREN expression RIGHT_PAREN
    parseContext.pushScope();
    TIntermTyped* switchExpression;
    if (! acceptParenExpression(switchExpression)) {
        parseContext.popScope();
        return false;
    }

    // compound_statement
    parseContext.pushSwitchSequence(new TIntermSequence);
    bool statementOkay = acceptCompoundStatement(statement);
    if (statementOkay)
        statement = parseContext.addSwitch(loc, switchExpression, statement ? statement->getAsAggregate() : nullptr);

    parseContext.popSwitchSequence();
    parseContext.popScope();

    return statementOkay;
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

        // initializer
        TIntermNode* initNode = nullptr;
        if (! acceptControlDeclaration(initNode)) {
            TIntermTyped* initExpr = nullptr;
            acceptExpression(initExpr);
            initNode = initExpr;
        }
        // SEMI_COLON
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

        statement = intermediate.addForLoop(statement, initNode, condition, iterator, true, loc);

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
    EHlslTokenClass jump = peek();
    switch (jump) {
    case EHTokContinue:
    case EHTokBreak:
    case EHTokDiscard:
    case EHTokReturn:
        advanceToken();
        break;
    default:
        // not something we handle in this function
        return false;
    }

    switch (jump) {
    case EHTokContinue:
        statement = intermediate.addBranch(EOpContinue, token.loc);
        break;
    case EHTokBreak:
        statement = intermediate.addBranch(EOpBreak, token.loc);
        break;
    case EHTokDiscard:
        statement = intermediate.addBranch(EOpKill, token.loc);
        break;

    case EHTokReturn:
    {
        // expression
        TIntermTyped* node;
        if (acceptExpression(node)) {
            // hook it up
            statement = intermediate.addBranch(EOpReturn, node, token.loc);
        } else
            statement = intermediate.addBranch(EOpReturn, token.loc);
        break;
    }

    default:
        assert(0);
        return false;
    }

    // SEMICOLON
    if (! acceptTokenClass(EHTokSemicolon))
        expected(";");
    
    return true;
}

// case_label
//      : CASE expression COLON
//
bool HlslGrammar::acceptCaseLabel(TIntermNode*& statement)
{
    TSourceLoc loc = token.loc;
    if (! acceptTokenClass(EHTokCase))
        return false;

    TIntermTyped* expression;
    if (! acceptExpression(expression)) {
        expected("case expression");
        return false;
    }

    if (! acceptTokenClass(EHTokColon)) {
        expected(":");
        return false;
    }

    statement = parseContext.intermediate.addBranch(EOpCase, expression, loc);

    return true;
}

// default_label
//      : DEFAULT COLON
//
bool HlslGrammar::acceptDefaultLabel(TIntermNode*& statement)
{
    TSourceLoc loc = token.loc;
    if (! acceptTokenClass(EHTokDefault))
        return false;

    if (! acceptTokenClass(EHTokColon)) {
        expected(":");
        return false;
    }

    statement = parseContext.intermediate.addBranch(EOpDefault, loc);

    return true;
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
