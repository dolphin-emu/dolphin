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
// HLSL scanning, leveraging the scanning done by the preprocessor.
//

#include <string.h>
#include <unordered_map>
#include <unordered_set>

#include "../glslang/Include/Types.h"
#include "../glslang/MachineIndependent/SymbolTable.h"
#include "../glslang/MachineIndependent/ParseHelper.h"
#include "hlslScanContext.h"
#include "hlslTokens.h"
//#include "Scan.h"

// preprocessor includes
#include "../glslang/MachineIndependent/preprocessor/PpContext.h"
#include "../glslang/MachineIndependent/preprocessor/PpTokens.h"
    
namespace {

struct str_eq
{
    bool operator()(const char* lhs, const char* rhs) const
    {
        return strcmp(lhs, rhs) == 0;
    }
};

struct str_hash
{
    size_t operator()(const char* str) const
    {
        // djb2
        unsigned long hash = 5381;
        int c;

        while ((c = *str++) != 0)
            hash = ((hash << 5) + hash) + c;

        return hash;
    }
};

// A single global usable by all threads, by all versions, by all languages.
// After a single process-level initialization, this is read only and thread safe
std::unordered_map<const char*, glslang::EHlslTokenClass, str_hash, str_eq>* KeywordMap = nullptr;
std::unordered_set<const char*, str_hash, str_eq>* ReservedSet = nullptr;

};

namespace glslang {

void HlslScanContext::fillInKeywordMap()
{
    if (KeywordMap != nullptr) {
        // this is really an error, as this should called only once per process
        // but, the only risk is if two threads called simultaneously
        return;
    }
    KeywordMap = new std::unordered_map<const char*, EHlslTokenClass, str_hash, str_eq>;

    (*KeywordMap)["static"] =                  EHTokStatic;
    (*KeywordMap)["const"] =                   EHTokConst;
    (*KeywordMap)["unorm"] =                   EHTokUnorm;
    (*KeywordMap)["snorm"] =                   EHTokSNorm;
    (*KeywordMap)["extern"] =                  EHTokExtern;
    (*KeywordMap)["uniform"] =                 EHTokUniform;
    (*KeywordMap)["volatile"] =                EHTokVolatile;
    (*KeywordMap)["shared"] =                  EHTokShared;
    (*KeywordMap)["groupshared"] =             EHTokGroupShared;
    (*KeywordMap)["linear"] =                  EHTokLinear;
    (*KeywordMap)["centroid"] =                EHTokCentroid;
    (*KeywordMap)["nointerpolation"] =         EHTokNointerpolation;
    (*KeywordMap)["noperspective"] =           EHTokNoperspective;
    (*KeywordMap)["sample"] =                  EHTokSample;
    (*KeywordMap)["row_major"] =               EHTokRowMajor;
    (*KeywordMap)["column_major"] =            EHTokColumnMajor;
    (*KeywordMap)["packoffset"] =              EHTokPackOffset;

    (*KeywordMap)["Buffer"] =                  EHTokBuffer;
    (*KeywordMap)["vector"] =                  EHTokVector;
    (*KeywordMap)["matrix"] =                  EHTokMatrix;

    (*KeywordMap)["void"] =                    EHTokVoid;
    (*KeywordMap)["bool"] =                    EHTokBool;
    (*KeywordMap)["int"] =                     EHTokInt;
    (*KeywordMap)["uint"] =                    EHTokUint;
    (*KeywordMap)["dword"] =                   EHTokDword;
    (*KeywordMap)["half"] =                    EHTokHalf;
    (*KeywordMap)["float"] =                   EHTokFloat;
    (*KeywordMap)["double"] =                  EHTokDouble;
    (*KeywordMap)["min16float"] =              EHTokMin16float; 
    (*KeywordMap)["min10float"] =              EHTokMin10float;
    (*KeywordMap)["min16int"] =                EHTokMin16int;
    (*KeywordMap)["min12int"] =                EHTokMin12int;
    (*KeywordMap)["min16uint"] =               EHTokMin16int;

    (*KeywordMap)["bool1"] =                   EHTokBool1;
    (*KeywordMap)["bool2"] =                   EHTokBool2;
    (*KeywordMap)["bool3"] =                   EHTokBool3;
    (*KeywordMap)["bool4"] =                   EHTokBool4;
    (*KeywordMap)["float1"] =                  EHTokFloat1;
    (*KeywordMap)["float2"] =                  EHTokFloat2;
    (*KeywordMap)["float3"] =                  EHTokFloat3;
    (*KeywordMap)["float4"] =                  EHTokFloat4;
    (*KeywordMap)["int1"] =                    EHTokInt1;
    (*KeywordMap)["int2"] =                    EHTokInt2;
    (*KeywordMap)["int3"] =                    EHTokInt3;
    (*KeywordMap)["int4"] =                    EHTokInt4;
    (*KeywordMap)["double1"] =                 EHTokDouble1;
    (*KeywordMap)["double2"] =                 EHTokDouble2;
    (*KeywordMap)["double3"] =                 EHTokDouble3;
    (*KeywordMap)["double4"] =                 EHTokDouble4;
    (*KeywordMap)["uint1"] =                   EHTokUint1;
    (*KeywordMap)["uint2"] =                   EHTokUint2;
    (*KeywordMap)["uint3"] =                   EHTokUint3;
    (*KeywordMap)["uint4"] =                   EHTokUint4;

    (*KeywordMap)["int1x1"] =                  EHTokInt1x1;
    (*KeywordMap)["int1x2"] =                  EHTokInt1x2;
    (*KeywordMap)["int1x3"] =                  EHTokInt1x3;
    (*KeywordMap)["int1x4"] =                  EHTokInt1x4;
    (*KeywordMap)["int2x1"] =                  EHTokInt2x1;
    (*KeywordMap)["int2x2"] =                  EHTokInt2x2;
    (*KeywordMap)["int2x3"] =                  EHTokInt2x3;
    (*KeywordMap)["int2x4"] =                  EHTokInt2x4;
    (*KeywordMap)["int3x1"] =                  EHTokInt3x1;
    (*KeywordMap)["int3x2"] =                  EHTokInt3x2;
    (*KeywordMap)["int3x3"] =                  EHTokInt3x3;
    (*KeywordMap)["int3x4"] =                  EHTokInt3x4;
    (*KeywordMap)["int4x1"] =                  EHTokInt4x1;
    (*KeywordMap)["int4x2"] =                  EHTokInt4x2;
    (*KeywordMap)["int4x3"] =                  EHTokInt4x3;
    (*KeywordMap)["int4x4"] =                  EHTokInt4x4;
    (*KeywordMap)["float1x1"] =                EHTokFloat1x1;
    (*KeywordMap)["float1x2"] =                EHTokFloat1x2;
    (*KeywordMap)["float1x3"] =                EHTokFloat1x3;
    (*KeywordMap)["float1x4"] =                EHTokFloat1x4;
    (*KeywordMap)["float2x1"] =                EHTokFloat2x1;
    (*KeywordMap)["float2x2"] =                EHTokFloat2x2;
    (*KeywordMap)["float2x3"] =                EHTokFloat2x3;
    (*KeywordMap)["float2x4"] =                EHTokFloat2x4;
    (*KeywordMap)["float3x1"] =                EHTokFloat3x1;
    (*KeywordMap)["float3x2"] =                EHTokFloat3x2;
    (*KeywordMap)["float3x3"] =                EHTokFloat3x3;
    (*KeywordMap)["float3x4"] =                EHTokFloat3x4;
    (*KeywordMap)["float4x1"] =                EHTokFloat4x1;
    (*KeywordMap)["float4x2"] =                EHTokFloat4x2;
    (*KeywordMap)["float4x3"] =                EHTokFloat4x3;
    (*KeywordMap)["float4x4"] =                EHTokFloat4x4;
    (*KeywordMap)["double1x1"] =               EHTokDouble1x1;
    (*KeywordMap)["double1x2"] =               EHTokDouble1x2;
    (*KeywordMap)["double1x3"] =               EHTokDouble1x3;
    (*KeywordMap)["double1x4"] =               EHTokDouble1x4;
    (*KeywordMap)["double2x1"] =               EHTokDouble2x1;
    (*KeywordMap)["double2x2"] =               EHTokDouble2x2;
    (*KeywordMap)["double2x3"] =               EHTokDouble2x3;
    (*KeywordMap)["double2x4"] =               EHTokDouble2x4;
    (*KeywordMap)["double3x1"] =               EHTokDouble3x1;
    (*KeywordMap)["double3x2"] =               EHTokDouble3x2;
    (*KeywordMap)["double3x3"] =               EHTokDouble3x3;
    (*KeywordMap)["double3x4"] =               EHTokDouble3x4;
    (*KeywordMap)["double4x1"] =               EHTokDouble4x1;
    (*KeywordMap)["double4x2"] =               EHTokDouble4x2;
    (*KeywordMap)["double4x3"] =               EHTokDouble4x3;
    (*KeywordMap)["double4x4"] =               EHTokDouble4x4;

    (*KeywordMap)["sampler"] =                 EHTokSampler;
    (*KeywordMap)["sampler1D"] =               EHTokSampler1d;
    (*KeywordMap)["sampler2D"] =               EHTokSampler2d;
    (*KeywordMap)["sampler3D"] =               EHTokSampler3d;
    (*KeywordMap)["samplerCube"] =             EHTokSamplerCube;
    (*KeywordMap)["sampler_state"] =           EHTokSamplerState;
    (*KeywordMap)["SamplerState"] =            EHTokSamplerState;
    (*KeywordMap)["SamplerComparisonState"] =  EHTokSamplerComparisonState;
    (*KeywordMap)["texture"] =                 EHTokTexture;
    (*KeywordMap)["Texture1D"] =               EHTokTexture1d;
    (*KeywordMap)["Texture1DArray"] =          EHTokTexture1darray;
    (*KeywordMap)["Texture2D"] =               EHTokTexture2d;
    (*KeywordMap)["Texture2DArray"] =          EHTokTexture2darray;
    (*KeywordMap)["Texture3D"] =               EHTokTexture3d;
    (*KeywordMap)["TextureCube"] =             EHTokTextureCube;

    (*KeywordMap)["struct"] =                  EHTokStruct;
    (*KeywordMap)["typedef"] =                 EHTokTypedef;

    (*KeywordMap)["true"] =                    EHTokBoolConstant;
    (*KeywordMap)["false"] =                   EHTokBoolConstant;

    (*KeywordMap)["for"] =                     EHTokFor;
    (*KeywordMap)["do"] =                      EHTokDo;
    (*KeywordMap)["while"] =                   EHTokWhile;
    (*KeywordMap)["break"] =                   EHTokBreak;
    (*KeywordMap)["continue"] =                EHTokContinue;
    (*KeywordMap)["if"] =                      EHTokIf;
    (*KeywordMap)["else"] =                    EHTokElse;
    (*KeywordMap)["discard"] =                 EHTokDiscard;
    (*KeywordMap)["return"] =                  EHTokReturn;
    (*KeywordMap)["switch"] =                  EHTokSwitch;
    (*KeywordMap)["case"] =                    EHTokCase;
    (*KeywordMap)["default"] =                 EHTokDefault;

    // TODO: get correct set here
    ReservedSet = new std::unordered_set<const char*, str_hash, str_eq>;
    
    ReservedSet->insert("auto");
    ReservedSet->insert("catch");
    ReservedSet->insert("char");
    ReservedSet->insert("class");
    ReservedSet->insert("const_cast");
    ReservedSet->insert("enum");
    ReservedSet->insert("explicit");
    ReservedSet->insert("friend");
    ReservedSet->insert("goto");
    ReservedSet->insert("long");
    ReservedSet->insert("mutable");
    ReservedSet->insert("new");
    ReservedSet->insert("operator");
    ReservedSet->insert("private");
    ReservedSet->insert("protected");
    ReservedSet->insert("public");
    ReservedSet->insert("reinterpret_cast");
    ReservedSet->insert("short");
    ReservedSet->insert("signed");
    ReservedSet->insert("sizeof");
    ReservedSet->insert("static_cast");
    ReservedSet->insert("template");
    ReservedSet->insert("this");
    ReservedSet->insert("throw");
    ReservedSet->insert("try");
    ReservedSet->insert("typename");
    ReservedSet->insert("union");
    ReservedSet->insert("unsigned");
    ReservedSet->insert("using");
    ReservedSet->insert("virtual");
}

void HlslScanContext::deleteKeywordMap()
{
    delete KeywordMap;
    KeywordMap = nullptr;
    delete ReservedSet;
    ReservedSet = nullptr;
}

// Wrapper for tokenizeClass()"] =  to get everything inside the token.
void HlslScanContext::tokenize(HlslToken& token)
{
    token.isType = false;
    EHlslTokenClass tokenClass = tokenizeClass(token);
    token.tokenClass = tokenClass;
    if (token.isType)
        afterType = true;
}

//
// Fill in token information for the next token, except for the token class.
// Returns the enum value of the token class of the next token found.
// Return 0 (EndOfTokens) on end of input.
//
EHlslTokenClass HlslScanContext::tokenizeClass(HlslToken& token)
{
    do {
        parserToken = &token;
        TPpToken ppToken;
        tokenText = ppContext.tokenize(&ppToken);
        if (tokenText == nullptr)
            return EHTokNone;

        loc = ppToken.loc;
        parserToken->loc = loc;
        switch (ppToken.token) {
        case ';':  afterType = false;   return EHTokSemicolon;
        case ',':  afterType = false;   return EHTokComma;
        case ':':                       return EHTokColon;
        case '=':  afterType = false;   return EHTokAssign;
        case '(':  afterType = false;   return EHTokLeftParen;
        case ')':  afterType = false;   return EHTokRightParen;
        case '.':  field = true;        return EHTokDot;
        case '!':                       return EHTokBang;
        case '-':                       return EHTokDash;
        case '~':                       return EHTokTilde;
        case '+':                       return EHTokPlus;
        case '*':                       return EHTokStar;
        case '/':                       return EHTokSlash;
        case '%':                       return EHTokPercent;
        case '<':                       return EHTokLeftAngle;
        case '>':                       return EHTokRightAngle;
        case '|':                       return EHTokVerticalBar;
        case '^':                       return EHTokCaret;
        case '&':                       return EHTokAmpersand;
        case '?':                       return EHTokQuestion;
        case '[':                       return EHTokLeftBracket;
        case ']':                       return EHTokRightBracket;
        case '{':                       return EHTokLeftBrace;
        case '}':                       return EHTokRightBrace;
        case '\\':
            parseContext.error(loc, "illegal use of escape character", "\\", "");
            break;

        case PpAtomAdd:                return EHTokAddAssign;
        case PpAtomSub:                return EHTokSubAssign;
        case PpAtomMul:                return EHTokMulAssign;
        case PpAtomDiv:                return EHTokDivAssign;
        case PpAtomMod:                return EHTokModAssign;

        case PpAtomRight:              return EHTokRightOp;
        case PpAtomLeft:               return EHTokLeftOp;

        case PpAtomRightAssign:        return EHTokRightAssign;
        case PpAtomLeftAssign:         return EHTokLeftAssign;
        case PpAtomAndAssign:          return EHTokAndAssign;
        case PpAtomOrAssign:           return EHTokOrAssign;
        case PpAtomXorAssign:          return EHTokXorAssign;

        case PpAtomAnd:                return EHTokAndOp;
        case PpAtomOr:                 return EHTokOrOp;
        case PpAtomXor:                return EHTokXorOp;

        case PpAtomEQ:                 return EHTokEqOp;
        case PpAtomGE:                 return EHTokGeOp;
        case PpAtomNE:                 return EHTokNeOp;
        case PpAtomLE:                 return EHTokLeOp;

        case PpAtomDecrement:          return EHTokDecOp;
        case PpAtomIncrement:          return EHTokIncOp;

        case PpAtomConstInt:           parserToken->i = ppToken.ival;       return EHTokIntConstant;
        case PpAtomConstUint:          parserToken->i = ppToken.ival;       return EHTokUintConstant;
        case PpAtomConstFloat:         parserToken->d = ppToken.dval;       return EHTokFloatConstant;
        case PpAtomConstDouble:        parserToken->d = ppToken.dval;       return EHTokDoubleConstant;
        case PpAtomIdentifier:
        {
            EHlslTokenClass token = tokenizeIdentifier();
            field = false;
            return token;
        }

        case EndOfInput:               return EHTokNone;

        default:
            char buf[2];
            buf[0] = (char)ppToken.token;
            buf[1] = 0;
            parseContext.error(loc, "unexpected token", buf, "");
            break;
        }
    } while (true);
}

EHlslTokenClass HlslScanContext::tokenizeIdentifier()
{
    if (ReservedSet->find(tokenText) != ReservedSet->end())
        return reservedWord();

    auto it = KeywordMap->find(tokenText);
    if (it == KeywordMap->end()) {
        // Should have an identifier of some sort
        return identifierOrType();
    }
    keyword = it->second;

    switch (keyword) {

    // qualifiers
    case EHTokStatic:
    case EHTokConst:
    case EHTokSNorm:
    case EHTokUnorm:
    case EHTokExtern:
    case EHTokUniform:
    case EHTokVolatile:
    case EHTokShared:
    case EHTokGroupShared:
    case EHTokLinear:
    case EHTokCentroid:
    case EHTokNointerpolation:
    case EHTokNoperspective:
    case EHTokSample:
    case EHTokRowMajor:
    case EHTokColumnMajor:
    case EHTokPackOffset:
        return keyword;

    // template types
    case EHTokBuffer:
    case EHTokVector:
    case EHTokMatrix:
        return keyword;

    // scalar types
    case EHTokVoid:
    case EHTokBool:
    case EHTokInt:
    case EHTokUint:
    case EHTokDword:
    case EHTokHalf:
    case EHTokFloat:
    case EHTokDouble:
    case EHTokMin16float:
    case EHTokMin10float:
    case EHTokMin16int:
    case EHTokMin12int:
    case EHTokMin16uint:

    // vector types
    case EHTokBool1:
    case EHTokBool2:
    case EHTokBool3:
    case EHTokBool4:
    case EHTokFloat1:
    case EHTokFloat2:
    case EHTokFloat3:
    case EHTokFloat4:
    case EHTokInt1:
    case EHTokInt2:
    case EHTokInt3:
    case EHTokInt4:
    case EHTokDouble1:
    case EHTokDouble2:
    case EHTokDouble3:
    case EHTokDouble4:
    case EHTokUint1:
    case EHTokUint2:
    case EHTokUint3:
    case EHTokUint4:

    // matrix types
    case EHTokInt1x1:
    case EHTokInt1x2:
    case EHTokInt1x3:
    case EHTokInt1x4:
    case EHTokInt2x1:
    case EHTokInt2x2:
    case EHTokInt2x3:
    case EHTokInt2x4:
    case EHTokInt3x1:
    case EHTokInt3x2:
    case EHTokInt3x3:
    case EHTokInt3x4:
    case EHTokInt4x1:
    case EHTokInt4x2:
    case EHTokInt4x3:
    case EHTokInt4x4:
    case EHTokFloat1x1:
    case EHTokFloat1x2:
    case EHTokFloat1x3:
    case EHTokFloat1x4:
    case EHTokFloat2x1:
    case EHTokFloat2x2:
    case EHTokFloat2x3:
    case EHTokFloat2x4:
    case EHTokFloat3x1:
    case EHTokFloat3x2:
    case EHTokFloat3x3:
    case EHTokFloat3x4:
    case EHTokFloat4x1:
    case EHTokFloat4x2:
    case EHTokFloat4x3:
    case EHTokFloat4x4:
    case EHTokDouble1x1:
    case EHTokDouble1x2:
    case EHTokDouble1x3:
    case EHTokDouble1x4:
    case EHTokDouble2x1:
    case EHTokDouble2x2:
    case EHTokDouble2x3:
    case EHTokDouble2x4:
    case EHTokDouble3x1:
    case EHTokDouble3x2:
    case EHTokDouble3x3:
    case EHTokDouble3x4:
    case EHTokDouble4x1:
    case EHTokDouble4x2:
    case EHTokDouble4x3:
    case EHTokDouble4x4:
        parserToken->isType = true;
        return keyword;

    // texturing types
    case EHTokSampler:
    case EHTokSampler1d:
    case EHTokSampler2d:
    case EHTokSampler3d:
    case EHTokSamplerCube:
    case EHTokSamplerState:
    case EHTokSamplerComparisonState:
    case EHTokTexture:
    case EHTokTexture1d:
    case EHTokTexture1darray:
    case EHTokTexture2d:
    case EHTokTexture2darray:
    case EHTokTexture3d:
    case EHTokTextureCube:
        parserToken->isType = true;
        return keyword;

    // variable, user type, ...
    case EHTokStruct:
    case EHTokTypedef:

    case EHTokBoolConstant:
        if (strcmp("true", tokenText) == 0)
            parserToken->b = true;
        else
            parserToken->b = false;
        return keyword;

    // control flow
    case EHTokFor:
    case EHTokDo:
    case EHTokWhile:
    case EHTokBreak:
    case EHTokContinue:
    case EHTokIf:
    case EHTokElse:
    case EHTokDiscard:
    case EHTokReturn:
    case EHTokCase:
    case EHTokSwitch:
    case EHTokDefault:
        return keyword;

    default:
        parseContext.infoSink.info.message(EPrefixInternalError, "Unknown glslang keyword", loc);
        return EHTokNone;
    }
}

EHlslTokenClass HlslScanContext::identifierOrType()
{
    parserToken->string = NewPoolTString(tokenText);
    if (field)
        return EHTokIdentifier;

    parserToken->symbol = parseContext.symbolTable.find(*parserToken->string);
    if (afterType == false && parserToken->symbol) {
        if (const TVariable* variable = parserToken->symbol->getAsVariable()) {
            if (variable->isUserType()) {
                afterType = true;

                return EHTokTypeName;
            }
        }
    }

    return EHTokIdentifier;
}

// Give an error for use of a reserved symbol.
// However, allow built-in declarations to use reserved words, to allow
// extension support before the extension is enabled.
EHlslTokenClass HlslScanContext::reservedWord()
{
    if (! parseContext.symbolTable.atBuiltInLevel())
        parseContext.error(loc, "Reserved word.", tokenText, "", "");

    return EHTokNone;
}

EHlslTokenClass HlslScanContext::identifierOrReserved(bool reserved)
{
    if (reserved) {
        reservedWord();

        return EHTokNone;
    }

    if (parseContext.forwardCompatible)
        parseContext.warn(loc, "using future reserved keyword", tokenText, "");

    return identifierOrType();
}

// For a keyword that was never reserved, until it suddenly
// showed up.
EHlslTokenClass HlslScanContext::nonreservedKeyword(int version)
{
    if (parseContext.version < version)
        return identifierOrType();

    return keyword;
}

} // end namespace glslang
