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
//    Neither the name of 3Dlabs Inc. Ltd. nor the names of its
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
#ifndef HLSL_PARSE_INCLUDED_
#define HLSL_PARSE_INCLUDED_

#include "../glslang/MachineIndependent/parseVersions.h"
#include "../glslang/MachineIndependent/ParseHelper.h"

namespace glslang {

class HlslParseContext : public TParseContextBase {
public:
    HlslParseContext(TSymbolTable&, TIntermediate&, bool parsingBuiltins,
                     int version, EProfile, int spv, int vulkan, EShLanguage, TInfoSink&,
                     bool forwardCompatible = false, EShMessages messages = EShMsgDefault);
    virtual ~HlslParseContext();
    void setLimits(const TBuiltInResource&);
    bool parseShaderStrings(TPpContext&, TInputScanner& input, bool versionWillBeError = false);
    void getPreamble(std::string&);

    void C_DECL error(const TSourceLoc&, const char* szReason, const char* szToken,
        const char* szExtraInfoFormat, ...);
    void C_DECL  warn(const TSourceLoc&, const char* szReason, const char* szToken,
        const char* szExtraInfoFormat, ...);
    void C_DECL ppError(const TSourceLoc&, const char* szReason, const char* szToken,
        const char* szExtraInfoFormat, ...);
    void C_DECL ppWarn(const TSourceLoc&, const char* szReason, const char* szToken,
        const char* szExtraInfoFormat, ...);

    void reservedPpErrorCheck(const TSourceLoc&, const char* name, const char* op) { }
    bool lineContinuationCheck(const TSourceLoc&, bool endOfComment) { return true; }
    bool lineDirectiveShouldSetNextLine() const { return true; }
    bool builtInName(const TString&);

    void handlePragma(const TSourceLoc&, const TVector<TString>&);
    TIntermTyped* handleVariable(const TSourceLoc&, TSymbol* symbol, const TString* string);
    TIntermTyped* handleBracketDereference(const TSourceLoc&, TIntermTyped* base, TIntermTyped* index);
    void checkIndex(const TSourceLoc&, const TType&, int& index);

    void makeEditable(TSymbol*&);
    TVariable* getEditableVariable(const char* name);
    bool isIoResizeArray(const TType&) const;
    void fixIoArraySize(const TSourceLoc&, TType&);
    void handleIoResizeArrayAccess(const TSourceLoc&, TIntermTyped* base);
    void checkIoArraysConsistency(const TSourceLoc&, bool tailOnly = false);
    int getIoArrayImplicitSize() const;
    void checkIoArrayConsistency(const TSourceLoc&, int requiredSize, const char* feature, TType&, const TString&);

    TIntermTyped* handleBinaryMath(const TSourceLoc&, const char* str, TOperator op, TIntermTyped* left, TIntermTyped* right);
    TIntermTyped* handleUnaryMath(const TSourceLoc&, const char* str, TOperator op, TIntermTyped* childNode);
    TIntermTyped* handleDotDereference(const TSourceLoc&, TIntermTyped* base, const TString& field);
    TFunction* handleFunctionDeclarator(const TSourceLoc&, TFunction& function, bool prototype);
    TIntermAggregate* handleFunctionDefinition(const TSourceLoc&, TFunction&);
    void handleFunctionArgument(TFunction*, TIntermTyped*& arguments, TIntermTyped* newArg);
    TIntermTyped* handleFunctionCall(const TSourceLoc&, TFunction*, TIntermNode*);
    TIntermTyped* handleLengthMethod(const TSourceLoc&, TFunction*, TIntermNode*);
    void addInputArgumentConversions(const TFunction&, TIntermNode*&) const;
    TIntermTyped* addOutputArgumentConversions(const TFunction&, TIntermAggregate&) const;
    void builtInOpCheck(const TSourceLoc&, const TFunction&, TIntermOperator&);
    TFunction* handleConstructorCall(const TSourceLoc&, const TType&);

    bool parseVectorFields(const TSourceLoc&, const TString&, int vecSize, TVectorFields&);
    void assignError(const TSourceLoc&, const char* op, TString left, TString right);
    void unaryOpError(const TSourceLoc&, const char* op, TString operand);
    void binaryOpError(const TSourceLoc&, const char* op, TString left, TString right);
    void variableCheck(TIntermTyped*& nodePtr);
    void constantValueCheck(TIntermTyped* node, const char* token);
    void integerCheck(const TIntermTyped* node, const char* token);
    void globalCheck(const TSourceLoc&, const char* token);
    bool constructorError(const TSourceLoc&, TIntermNode*, TFunction&, TOperator, TType&);
    bool constructorTextureSamplerError(const TSourceLoc&, const TFunction&);
    void arraySizeCheck(const TSourceLoc&, TIntermTyped* expr, TArraySize&);
    void arraySizeRequiredCheck(const TSourceLoc&, const TArraySizes&);
    void structArrayCheck(const TSourceLoc&, const TType& structure);
    void arrayDimMerge(TType& type, const TArraySizes* sizes);
    bool voidErrorCheck(const TSourceLoc&, const TString&, TBasicType);
    void boolCheck(const TSourceLoc&, const TIntermTyped*);
    void boolCheck(const TSourceLoc&, const TPublicType&);
    void globalQualifierFix(const TSourceLoc&, TQualifier&);
    bool structQualifierErrorCheck(const TSourceLoc&, const TPublicType& pType);
    void mergeQualifiers(const TSourceLoc&, TQualifier& dst, const TQualifier& src, bool force);
    int computeSamplerTypeIndex(TSampler&);
    TSymbol* redeclareBuiltinVariable(const TSourceLoc&, const TString&, const TQualifier&, const TShaderQualifiers&, bool& newDeclaration);
    void redeclareBuiltinBlock(const TSourceLoc&, TTypeList& typeList, const TString& blockName, const TString* instanceName, TArraySizes* arraySizes);
    void paramCheckFix(const TSourceLoc&, const TStorageQualifier&, TType& type);
    void paramCheckFix(const TSourceLoc&, const TQualifier&, TType& type);
    void specializationCheck(const TSourceLoc&, const TType&, const char* op);

    void setLayoutQualifier(const TSourceLoc&, TPublicType&, TString&);
    void setLayoutQualifier(const TSourceLoc&, TPublicType&, TString&, const TIntermTyped*);
    void mergeObjectLayoutQualifiers(TQualifier& dest, const TQualifier& src, bool inheritOnly);
    void checkNoShaderLayouts(const TSourceLoc&, const TShaderQualifiers&);

    const TFunction* findFunction(const TSourceLoc& loc, const TFunction& call, bool& builtIn);
    TIntermNode* declareVariable(const TSourceLoc&, TString& identifier, const TType&, TArraySizes* typeArray = 0, TIntermTyped* initializer = 0);
    TIntermTyped* addConstructor(const TSourceLoc&, TIntermNode*, const TType&, TOperator);
    TIntermTyped* constructAggregate(TIntermNode*, const TType&, int, const TSourceLoc&);
    TIntermTyped* constructBuiltIn(const TType&, TOperator, TIntermTyped*, const TSourceLoc&, bool subset);
    void declareBlock(const TSourceLoc&, TTypeList& typeList, const TString* instanceName = 0, TArraySizes* arraySizes = 0);
    void fixBlockLocations(const TSourceLoc&, TQualifier&, TTypeList&, bool memberWithLocation, bool memberWithoutLocation);
    void fixBlockXfbOffsets(TQualifier&, TTypeList&);
    void fixBlockUniformOffsets(TQualifier&, TTypeList&);
    void addQualifierToExisting(const TSourceLoc&, TQualifier, const TString& identifier);
    void addQualifierToExisting(const TSourceLoc&, TQualifier, TIdentifierList&);
    void updateStandaloneQualifierDefaults(const TSourceLoc&, const TPublicType&);
    void wrapupSwitchSubsequence(TIntermAggregate* statements, TIntermNode* branchNode);
    TIntermNode* addSwitch(const TSourceLoc&, TIntermTyped* expression, TIntermAggregate* body);

    void updateImplicitArraySize(const TSourceLoc&, TIntermNode*, int index);

protected:
    void inheritGlobalDefaults(TQualifier& dst) const;
    TVariable* makeInternalVariable(const char* name, const TType&) const;
    TVariable* declareNonArray(const TSourceLoc&, TString& identifier, TType&, bool& newDeclaration);
    void declareArray(const TSourceLoc&, TString& identifier, const TType&, TSymbol*&, bool& newDeclaration);
    TIntermNode* executeInitializer(const TSourceLoc&, TIntermTyped* initializer, TVariable* variable);
    TIntermTyped* convertInitializerList(const TSourceLoc&, const TType&, TIntermTyped* initializer);
    TOperator mapTypeToConstructorOp(const TType&) const;
    void outputMessage(const TSourceLoc&, const char* szReason, const char* szToken,
                       const char* szExtraInfoFormat, TPrefixType prefix,
                       va_list args);

    // Current state of parsing
    struct TPragma contextPragma;
    int loopNestingLevel;        // 0 if outside all loops
    int structNestingLevel;      // 0 if outside blocks and structures
    int controlFlowNestingLevel; // 0 if outside all flow control
    int statementNestingLevel;   // 0 if outside all flow control or compound statements
    TList<TIntermSequence*> switchSequenceStack;  // case, node, case, case, node, ...; ensure only one node between cases;   stack of them for nesting
    TList<int> switchLevel;      // the statementNestingLevel the current switch statement is at, which must match the level of its case statements
    bool inEntrypoint;           // if inside a function, true if the function is the entry point
    bool postMainReturn;         // if inside a function, true if the function is the entry point and this is after a return statement
    const TType* currentFunctionType;  // the return type of the function that's currently being parsed
    bool functionReturnsValue;   // true if a non-void function has a return
    const TString* blockName;
    TQualifier currentBlockQualifier;
    TBuiltInResource resources;
    TLimits& limits;

    HlslParseContext(HlslParseContext&);
    HlslParseContext& operator=(HlslParseContext&);

    TMap<TString, TExtensionBehavior> extensionBehavior;    // for each extension string, what its current behavior is set to
    static const int maxSamplerIndex = EsdNumDims * (EbtNumTypes * (2 * 2 * 2)); // see computeSamplerTypeIndex()
    bool afterEOF;
    TQualifier globalBufferDefaults;
    TQualifier globalUniformDefaults;
    TQualifier globalInputDefaults;
    TQualifier globalOutputDefaults;
    TString currentCaller;        // name of last function body entered (not valid when at global scope)
    TIdSetType inductiveLoopIds;
    TVector<TIntermTyped*> needsIndexLimitationChecking;

    //
    // Geometry shader input arrays:
    //  - array sizing is based on input primitive and/or explicit size
    //
    // Tessellation control output arrays:
    //  - array sizing is based on output layout(vertices=...) and/or explicit size
    //
    // Both:
    //  - array sizing is retroactive
    //  - built-in block redeclarations interact with this
    //
    // Design:
    //  - use a per-context "resize-list", a list of symbols whose array sizes
    //    can be fixed
    //
    //  - the resize-list starts empty at beginning of user-shader compilation, it does
    //    not have built-ins in it
    //
    //  - on built-in array use: copyUp() symbol and add it to the resize-list
    //
    //  - on user array declaration: add it to the resize-list
    //
    //  - on block redeclaration: copyUp() symbol and add it to the resize-list
    //     * note, that appropriately gives an error if redeclaring a block that
    //       was already used and hence already copied-up
    //
    //  - on seeing a layout declaration that sizes the array, fix everything in the 
    //    resize-list, giving errors for mismatch
    //
    //  - on seeing an array size declaration, give errors on mismatch between it and previous
    //    array-sizing declarations
    //
    TVector<TSymbol*> ioArraySymbolResizeList;
};

} // end namespace glslang

#endif // HLSL_PARSE_INCLUDED_
