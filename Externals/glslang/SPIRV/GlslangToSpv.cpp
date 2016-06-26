//
//Copyright (C) 2014-2015 LunarG, Inc.
//Copyright (C) 2015-2016 Google, Inc.
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
// Visit the nodes in the glslang intermediate tree representation to
// translate them to SPIR-V.
//

#include "spirv.hpp"
#include "GlslangToSpv.h"
#include "SpvBuilder.h"
namespace spv {
   #include "GLSL.std.450.h"
}

// Glslang includes
#include "../glslang/MachineIndependent/localintermediate.h"
#include "../glslang/MachineIndependent/SymbolTable.h"
#include "../glslang/Include/Common.h"

#include <fstream>
#include <list>
#include <map>
#include <stack>
#include <string>
#include <vector>

namespace {

// For low-order part of the generator's magic number. Bump up
// when there is a change in the style (e.g., if SSA form changes,
// or a different instruction sequence to do something gets used).
const int GeneratorVersion = 1;

namespace {
class SpecConstantOpModeGuard {
public:
    SpecConstantOpModeGuard(spv::Builder* builder)
        : builder_(builder) {
        previous_flag_ = builder->isInSpecConstCodeGenMode();
    }
    ~SpecConstantOpModeGuard() {
        previous_flag_ ? builder_->setToSpecConstCodeGenMode()
                       : builder_->setToNormalCodeGenMode();
    }
    void turnOnSpecConstantOpMode() {
        builder_->setToSpecConstCodeGenMode();
    }

private:
    spv::Builder* builder_;
    bool previous_flag_;
};
}

//
// The main holder of information for translating glslang to SPIR-V.
//
// Derives from the AST walking base class.
//
class TGlslangToSpvTraverser : public glslang::TIntermTraverser {
public:
    TGlslangToSpvTraverser(const glslang::TIntermediate*, spv::SpvBuildLogger* logger);
    virtual ~TGlslangToSpvTraverser();

    bool visitAggregate(glslang::TVisit, glslang::TIntermAggregate*);
    bool visitBinary(glslang::TVisit, glslang::TIntermBinary*);
    void visitConstantUnion(glslang::TIntermConstantUnion*);
    bool visitSelection(glslang::TVisit, glslang::TIntermSelection*);
    bool visitSwitch(glslang::TVisit, glslang::TIntermSwitch*);
    void visitSymbol(glslang::TIntermSymbol* symbol);
    bool visitUnary(glslang::TVisit, glslang::TIntermUnary*);
    bool visitLoop(glslang::TVisit, glslang::TIntermLoop*);
    bool visitBranch(glslang::TVisit visit, glslang::TIntermBranch*);

    void dumpSpv(std::vector<unsigned int>& out);

protected:
    spv::Decoration TranslateInterpolationDecoration(const glslang::TQualifier& qualifier);
    spv::BuiltIn TranslateBuiltInDecoration(glslang::TBuiltInVariable);
    spv::ImageFormat TranslateImageFormat(const glslang::TType& type);
    spv::Id createSpvVariable(const glslang::TIntermSymbol*);
    spv::Id getSampledType(const glslang::TSampler&);
    spv::Id convertGlslangToSpvType(const glslang::TType& type);
    spv::Id convertGlslangToSpvType(const glslang::TType& type, glslang::TLayoutPacking, const glslang::TQualifier&);
    spv::Id makeArraySizeId(const glslang::TArraySizes&, int dim);
    spv::Id accessChainLoad(const glslang::TType& type);
    void    accessChainStore(const glslang::TType& type, spv::Id rvalue);
    glslang::TLayoutPacking getExplicitLayout(const glslang::TType& type) const;
    int getArrayStride(const glslang::TType& arrayType, glslang::TLayoutPacking, glslang::TLayoutMatrix);
    int getMatrixStride(const glslang::TType& matrixType, glslang::TLayoutPacking, glslang::TLayoutMatrix);
    void updateMemberOffset(const glslang::TType& structType, const glslang::TType& memberType, int& currentOffset, int& nextOffset, glslang::TLayoutPacking, glslang::TLayoutMatrix);

    bool isShaderEntrypoint(const glslang::TIntermAggregate* node);
    void makeFunctions(const glslang::TIntermSequence&);
    void makeGlobalInitializers(const glslang::TIntermSequence&);
    void visitFunctions(const glslang::TIntermSequence&);
    void handleFunctionEntry(const glslang::TIntermAggregate* node);
    void translateArguments(const glslang::TIntermAggregate& node, std::vector<spv::Id>& arguments);
    void translateArguments(glslang::TIntermUnary& node, std::vector<spv::Id>& arguments);
    spv::Id createImageTextureFunctionCall(glslang::TIntermOperator* node);
    spv::Id handleUserFunctionCall(const glslang::TIntermAggregate*);

    spv::Id createBinaryOperation(glslang::TOperator op, spv::Decoration precision, spv::Decoration noContraction, spv::Id typeId, spv::Id left, spv::Id right, glslang::TBasicType typeProxy, bool reduceComparison = true);
    spv::Id createBinaryMatrixOperation(spv::Op, spv::Decoration precision, spv::Decoration noContraction, spv::Id typeId, spv::Id left, spv::Id right);
    spv::Id createUnaryOperation(glslang::TOperator op, spv::Decoration precision, spv::Decoration noContraction, spv::Id typeId, spv::Id operand,glslang::TBasicType typeProxy);
    spv::Id createUnaryMatrixOperation(spv::Op, spv::Decoration precision, spv::Decoration noContraction, spv::Id typeId, spv::Id operand,glslang::TBasicType typeProxy);
    spv::Id createConversion(glslang::TOperator op, spv::Decoration precision, spv::Id destTypeId, spv::Id operand);
    spv::Id makeSmearedConstant(spv::Id constant, int vectorSize);
    spv::Id createAtomicOperation(glslang::TOperator op, spv::Decoration precision, spv::Id typeId, std::vector<spv::Id>& operands, glslang::TBasicType typeProxy);
    spv::Id createInvocationsOperation(glslang::TOperator, spv::Id typeId, spv::Id operand);
    spv::Id createMiscOperation(glslang::TOperator op, spv::Decoration precision, spv::Id typeId, std::vector<spv::Id>& operands, glslang::TBasicType typeProxy);
    spv::Id createNoArgOperation(glslang::TOperator op);
    spv::Id getSymbolId(const glslang::TIntermSymbol* node);
    void addDecoration(spv::Id id, spv::Decoration dec);
    void addDecoration(spv::Id id, spv::Decoration dec, unsigned value);
    void addMemberDecoration(spv::Id id, int member, spv::Decoration dec);
    void addMemberDecoration(spv::Id id, int member, spv::Decoration dec, unsigned value);
    spv::Id createSpvConstant(const glslang::TIntermTyped&);
    spv::Id createSpvConstantFromConstUnionArray(const glslang::TType& type, const glslang::TConstUnionArray&, int& nextConst, bool specConstant);
    bool isTrivialLeaf(const glslang::TIntermTyped* node);
    bool isTrivial(const glslang::TIntermTyped* node);
    spv::Id createShortCircuit(glslang::TOperator, glslang::TIntermTyped& left, glslang::TIntermTyped& right);

    spv::Function* shaderEntry;
    spv::Instruction* entryPoint;
    int sequenceDepth;

    spv::SpvBuildLogger* logger;

    // There is a 1:1 mapping between a spv builder and a module; this is thread safe
    spv::Builder builder;
    bool inMain;
    bool mainTerminated;
    bool linkageOnly;                  // true when visiting the set of objects in the AST present only for establishing interface, whether or not they were statically used
    std::set<spv::Id> iOSet;           // all input/output variables from either static use or declaration of interface
    const glslang::TIntermediate* glslangIntermediate;
    spv::Id stdBuiltins;

    std::unordered_map<int, spv::Id> symbolValues;
    std::unordered_set<int> constReadOnlyParameters;  // set of formal function parameters that have glslang qualifier constReadOnly, so we know they are not local function "const" that are write-once
    std::unordered_map<std::string, spv::Function*> functionMap;
    std::unordered_map<const glslang::TTypeList*, spv::Id> structMap[glslang::ElpCount][glslang::ElmCount];
    std::unordered_map<const glslang::TTypeList*, std::vector<int> > memberRemapper;  // for mapping glslang block indices to spv indices (e.g., due to hidden members)
    std::stack<bool> breakForLoop;  // false means break for switch
};

//
// Helper functions for translating glslang representations to SPIR-V enumerants.
//

// Translate glslang profile to SPIR-V source language.
spv::SourceLanguage TranslateSourceLanguage(glslang::EShSource source, EProfile profile)
{
    switch (source) {
    case glslang::EShSourceGlsl:
        switch (profile) {
        case ENoProfile:
        case ECoreProfile:
        case ECompatibilityProfile:
            return spv::SourceLanguageGLSL;
        case EEsProfile:
            return spv::SourceLanguageESSL;
        default:
            return spv::SourceLanguageUnknown;
        }
    case glslang::EShSourceHlsl:
        return spv::SourceLanguageHLSL;
    default:
        return spv::SourceLanguageUnknown;
    }
}

// Translate glslang language (stage) to SPIR-V execution model.
spv::ExecutionModel TranslateExecutionModel(EShLanguage stage)
{
    switch (stage) {
    case EShLangVertex:           return spv::ExecutionModelVertex;
    case EShLangTessControl:      return spv::ExecutionModelTessellationControl;
    case EShLangTessEvaluation:   return spv::ExecutionModelTessellationEvaluation;
    case EShLangGeometry:         return spv::ExecutionModelGeometry;
    case EShLangFragment:         return spv::ExecutionModelFragment;
    case EShLangCompute:          return spv::ExecutionModelGLCompute;
    default:
        assert(0);
        return spv::ExecutionModelFragment;
    }
}

// Translate glslang type to SPIR-V storage class.
spv::StorageClass TranslateStorageClass(const glslang::TType& type)
{
    if (type.getQualifier().isPipeInput())
        return spv::StorageClassInput;
    else if (type.getQualifier().isPipeOutput())
        return spv::StorageClassOutput;
    else if (type.getQualifier().isUniformOrBuffer()) {
        if (type.getQualifier().layoutPushConstant)
            return spv::StorageClassPushConstant;
        if (type.getBasicType() == glslang::EbtBlock)
            return spv::StorageClassUniform;
        else if (type.getBasicType() == glslang::EbtAtomicUint)
            return spv::StorageClassAtomicCounter;
        else
            return spv::StorageClassUniformConstant;
        // TODO: how are we distuingishing between default and non-default non-writable uniforms?  Do default uniforms even exist?
    } else {
        switch (type.getQualifier().storage) {
        case glslang::EvqShared:        return spv::StorageClassWorkgroup;  break;
        case glslang::EvqGlobal:        return spv::StorageClassPrivate;
        case glslang::EvqConstReadOnly: return spv::StorageClassFunction;
        case glslang::EvqTemporary:     return spv::StorageClassFunction;
        default:
            assert(0);
            return spv::StorageClassFunction;
        }
    }
}

// Translate glslang sampler type to SPIR-V dimensionality.
spv::Dim TranslateDimensionality(const glslang::TSampler& sampler)
{
    switch (sampler.dim) {
    case glslang::Esd1D:      return spv::Dim1D;
    case glslang::Esd2D:      return spv::Dim2D;
    case glslang::Esd3D:      return spv::Dim3D;
    case glslang::EsdCube:    return spv::DimCube;
    case glslang::EsdRect:    return spv::DimRect;
    case glslang::EsdBuffer:  return spv::DimBuffer;
    case glslang::EsdSubpass: return spv::DimSubpassData;
    default:
        assert(0);
        return spv::Dim2D;
    }
}

// Translate glslang type to SPIR-V precision decorations.
spv::Decoration TranslatePrecisionDecoration(const glslang::TType& type)
{
    switch (type.getQualifier().precision) {
    case glslang::EpqLow:    return spv::DecorationRelaxedPrecision;
    case glslang::EpqMedium: return spv::DecorationRelaxedPrecision;
    default:
        return spv::NoPrecision;
    }
}

// Translate glslang type to SPIR-V block decorations.
spv::Decoration TranslateBlockDecoration(const glslang::TType& type)
{
    if (type.getBasicType() == glslang::EbtBlock) {
        switch (type.getQualifier().storage) {
        case glslang::EvqUniform:      return spv::DecorationBlock;
        case glslang::EvqBuffer:       return spv::DecorationBufferBlock;
        case glslang::EvqVaryingIn:    return spv::DecorationBlock;
        case glslang::EvqVaryingOut:   return spv::DecorationBlock;
        default:
            assert(0);
            break;
        }
    }

    return (spv::Decoration)spv::BadValue;
}

// Translate glslang type to SPIR-V memory decorations.
void TranslateMemoryDecoration(const glslang::TQualifier& qualifier, std::vector<spv::Decoration>& memory)
{
    if (qualifier.coherent)
        memory.push_back(spv::DecorationCoherent);
    if (qualifier.volatil)
        memory.push_back(spv::DecorationVolatile);
    if (qualifier.restrict)
        memory.push_back(spv::DecorationRestrict);
    if (qualifier.readonly)
        memory.push_back(spv::DecorationNonWritable);
    if (qualifier.writeonly)
       memory.push_back(spv::DecorationNonReadable);
}

// Translate glslang type to SPIR-V layout decorations.
spv::Decoration TranslateLayoutDecoration(const glslang::TType& type, glslang::TLayoutMatrix matrixLayout)
{
    if (type.isMatrix()) {
        switch (matrixLayout) {
        case glslang::ElmRowMajor:
            return spv::DecorationRowMajor;
        case glslang::ElmColumnMajor:
            return spv::DecorationColMajor;
        default:
            // opaque layouts don't need a majorness
            return (spv::Decoration)spv::BadValue;
        }
    } else {
        switch (type.getBasicType()) {
        default:
            return (spv::Decoration)spv::BadValue;
            break;
        case glslang::EbtBlock:
            switch (type.getQualifier().storage) {
            case glslang::EvqUniform:
            case glslang::EvqBuffer:
                switch (type.getQualifier().layoutPacking) {
                case glslang::ElpShared:  return spv::DecorationGLSLShared;
                case glslang::ElpPacked:  return spv::DecorationGLSLPacked;
                default:
                    return (spv::Decoration)spv::BadValue;
                }
            case glslang::EvqVaryingIn:
            case glslang::EvqVaryingOut:
                assert(type.getQualifier().layoutPacking == glslang::ElpNone);
                return (spv::Decoration)spv::BadValue;
            default:
                assert(0);
                return (spv::Decoration)spv::BadValue;
            }
        }
    }
}

// Translate glslang type to SPIR-V interpolation decorations.
// Returns spv::Decoration(spv::BadValue) when no decoration
// should be applied.
spv::Decoration TGlslangToSpvTraverser::TranslateInterpolationDecoration(const glslang::TQualifier& qualifier)
{
    if (qualifier.smooth) {
        // Smooth decoration doesn't exist in SPIR-V 1.0
        return (spv::Decoration)spv::BadValue;
    }
    if (qualifier.nopersp)
        return spv::DecorationNoPerspective;
    else if (qualifier.patch)
        return spv::DecorationPatch;
    else if (qualifier.flat)
        return spv::DecorationFlat;
    else if (qualifier.centroid)
        return spv::DecorationCentroid;
    else if (qualifier.sample) {
        builder.addCapability(spv::CapabilitySampleRateShading);
        return spv::DecorationSample;
    } else
        return (spv::Decoration)spv::BadValue;
}

// If glslang type is invariant, return SPIR-V invariant decoration.
spv::Decoration TranslateInvariantDecoration(const glslang::TQualifier& qualifier)
{
    if (qualifier.invariant)
        return spv::DecorationInvariant;
    else
        return (spv::Decoration)spv::BadValue;
}

// If glslang type is noContraction, return SPIR-V NoContraction decoration.
spv::Decoration TranslateNoContractionDecoration(const glslang::TQualifier& qualifier)
{
    if (qualifier.noContraction)
        return spv::DecorationNoContraction;
    else
        return (spv::Decoration)spv::BadValue;
}

// Translate glslang built-in variable to SPIR-V built in decoration.
spv::BuiltIn TGlslangToSpvTraverser::TranslateBuiltInDecoration(glslang::TBuiltInVariable builtIn)
{
    switch (builtIn) {
    case glslang::EbvPointSize:
        switch (glslangIntermediate->getStage()) {
        case EShLangGeometry:
            builder.addCapability(spv::CapabilityGeometryPointSize);
            break;
        case EShLangTessControl:
        case EShLangTessEvaluation:
            builder.addCapability(spv::CapabilityTessellationPointSize);
            break;
        default:
            break;
        }
        return spv::BuiltInPointSize;

    case glslang::EbvClipDistance:
        builder.addCapability(spv::CapabilityClipDistance);
        return spv::BuiltInClipDistance;

    case glslang::EbvCullDistance:
        builder.addCapability(spv::CapabilityCullDistance);
        return spv::BuiltInCullDistance;

    case glslang::EbvViewportIndex:
        builder.addCapability(spv::CapabilityMultiViewport);
        return spv::BuiltInViewportIndex;

    case glslang::EbvSampleId:
        builder.addCapability(spv::CapabilitySampleRateShading);
        return spv::BuiltInSampleId;

    case glslang::EbvSamplePosition:
        builder.addCapability(spv::CapabilitySampleRateShading);
        return spv::BuiltInSamplePosition;

    case glslang::EbvSampleMask:
        builder.addCapability(spv::CapabilitySampleRateShading);
        return spv::BuiltInSampleMask;

    case glslang::EbvPosition:             return spv::BuiltInPosition;
    case glslang::EbvVertexId:             return spv::BuiltInVertexId;
    case glslang::EbvInstanceId:           return spv::BuiltInInstanceId;
    case glslang::EbvVertexIndex:          return spv::BuiltInVertexIndex;
    case glslang::EbvInstanceIndex:        return spv::BuiltInInstanceIndex;
    case glslang::EbvBaseVertex:
    case glslang::EbvBaseInstance:
    case glslang::EbvDrawId:
        // TODO: Add SPIR-V builtin ID.
        logger->missingFunctionality("shader draw parameters");
        return (spv::BuiltIn)spv::BadValue;
    case glslang::EbvPrimitiveId:          return spv::BuiltInPrimitiveId;
    case glslang::EbvInvocationId:         return spv::BuiltInInvocationId;
    case glslang::EbvLayer:                return spv::BuiltInLayer;
    case glslang::EbvTessLevelInner:       return spv::BuiltInTessLevelInner;
    case glslang::EbvTessLevelOuter:       return spv::BuiltInTessLevelOuter;
    case glslang::EbvTessCoord:            return spv::BuiltInTessCoord;
    case glslang::EbvPatchVertices:        return spv::BuiltInPatchVertices;
    case glslang::EbvFragCoord:            return spv::BuiltInFragCoord;
    case glslang::EbvPointCoord:           return spv::BuiltInPointCoord;
    case glslang::EbvFace:                 return spv::BuiltInFrontFacing;
    case glslang::EbvFragDepth:            return spv::BuiltInFragDepth;
    case glslang::EbvHelperInvocation:     return spv::BuiltInHelperInvocation;
    case glslang::EbvNumWorkGroups:        return spv::BuiltInNumWorkgroups;
    case glslang::EbvWorkGroupSize:        return spv::BuiltInWorkgroupSize;
    case glslang::EbvWorkGroupId:          return spv::BuiltInWorkgroupId;
    case glslang::EbvLocalInvocationId:    return spv::BuiltInLocalInvocationId;
    case glslang::EbvLocalInvocationIndex: return spv::BuiltInLocalInvocationIndex;
    case glslang::EbvGlobalInvocationId:   return spv::BuiltInGlobalInvocationId;
    case glslang::EbvSubGroupSize:
    case glslang::EbvSubGroupInvocation:
    case glslang::EbvSubGroupEqMask:
    case glslang::EbvSubGroupGeMask:
    case glslang::EbvSubGroupGtMask:
    case glslang::EbvSubGroupLeMask:
    case glslang::EbvSubGroupLtMask:
        // TODO: Add SPIR-V builtin ID.
        logger->missingFunctionality("shader ballot");
        return (spv::BuiltIn)spv::BadValue;
    default:                               return (spv::BuiltIn)spv::BadValue;
    }
}

// Translate glslang image layout format to SPIR-V image format.
spv::ImageFormat TGlslangToSpvTraverser::TranslateImageFormat(const glslang::TType& type)
{
    assert(type.getBasicType() == glslang::EbtSampler);

    // Check for capabilities
    switch (type.getQualifier().layoutFormat) {
    case glslang::ElfRg32f:
    case glslang::ElfRg16f:
    case glslang::ElfR11fG11fB10f:
    case glslang::ElfR16f:
    case glslang::ElfRgba16:
    case glslang::ElfRgb10A2:
    case glslang::ElfRg16:
    case glslang::ElfRg8:
    case glslang::ElfR16:
    case glslang::ElfR8:
    case glslang::ElfRgba16Snorm:
    case glslang::ElfRg16Snorm:
    case glslang::ElfRg8Snorm:
    case glslang::ElfR16Snorm:
    case glslang::ElfR8Snorm:

    case glslang::ElfRg32i:
    case glslang::ElfRg16i:
    case glslang::ElfRg8i:
    case glslang::ElfR16i:
    case glslang::ElfR8i:

    case glslang::ElfRgb10a2ui:
    case glslang::ElfRg32ui:
    case glslang::ElfRg16ui:
    case glslang::ElfRg8ui:
    case glslang::ElfR16ui:
    case glslang::ElfR8ui:
        builder.addCapability(spv::CapabilityStorageImageExtendedFormats);
        break;

    default:
        break;
    }

    // do the translation
    switch (type.getQualifier().layoutFormat) {
    case glslang::ElfNone:          return spv::ImageFormatUnknown;
    case glslang::ElfRgba32f:       return spv::ImageFormatRgba32f;
    case glslang::ElfRgba16f:       return spv::ImageFormatRgba16f;
    case glslang::ElfR32f:          return spv::ImageFormatR32f;
    case glslang::ElfRgba8:         return spv::ImageFormatRgba8;
    case glslang::ElfRgba8Snorm:    return spv::ImageFormatRgba8Snorm;
    case glslang::ElfRg32f:         return spv::ImageFormatRg32f;
    case glslang::ElfRg16f:         return spv::ImageFormatRg16f;
    case glslang::ElfR11fG11fB10f:  return spv::ImageFormatR11fG11fB10f;
    case glslang::ElfR16f:          return spv::ImageFormatR16f;
    case glslang::ElfRgba16:        return spv::ImageFormatRgba16;
    case glslang::ElfRgb10A2:       return spv::ImageFormatRgb10A2;
    case glslang::ElfRg16:          return spv::ImageFormatRg16;
    case glslang::ElfRg8:           return spv::ImageFormatRg8;
    case glslang::ElfR16:           return spv::ImageFormatR16;
    case glslang::ElfR8:            return spv::ImageFormatR8;
    case glslang::ElfRgba16Snorm:   return spv::ImageFormatRgba16Snorm;
    case glslang::ElfRg16Snorm:     return spv::ImageFormatRg16Snorm;
    case glslang::ElfRg8Snorm:      return spv::ImageFormatRg8Snorm;
    case glslang::ElfR16Snorm:      return spv::ImageFormatR16Snorm;
    case glslang::ElfR8Snorm:       return spv::ImageFormatR8Snorm;
    case glslang::ElfRgba32i:       return spv::ImageFormatRgba32i;
    case glslang::ElfRgba16i:       return spv::ImageFormatRgba16i;
    case glslang::ElfRgba8i:        return spv::ImageFormatRgba8i;
    case glslang::ElfR32i:          return spv::ImageFormatR32i;
    case glslang::ElfRg32i:         return spv::ImageFormatRg32i;
    case glslang::ElfRg16i:         return spv::ImageFormatRg16i;
    case glslang::ElfRg8i:          return spv::ImageFormatRg8i;
    case glslang::ElfR16i:          return spv::ImageFormatR16i;
    case glslang::ElfR8i:           return spv::ImageFormatR8i;
    case glslang::ElfRgba32ui:      return spv::ImageFormatRgba32ui;
    case glslang::ElfRgba16ui:      return spv::ImageFormatRgba16ui;
    case glslang::ElfRgba8ui:       return spv::ImageFormatRgba8ui;
    case glslang::ElfR32ui:         return spv::ImageFormatR32ui;
    case glslang::ElfRg32ui:        return spv::ImageFormatRg32ui;
    case glslang::ElfRg16ui:        return spv::ImageFormatRg16ui;
    case glslang::ElfRgb10a2ui:     return spv::ImageFormatRgb10a2ui;
    case glslang::ElfRg8ui:         return spv::ImageFormatRg8ui;
    case glslang::ElfR16ui:         return spv::ImageFormatR16ui;
    case glslang::ElfR8ui:          return spv::ImageFormatR8ui;
    default:                        return (spv::ImageFormat)spv::BadValue;
    }
}

// Return whether or not the given type is something that should be tied to a
// descriptor set.
bool IsDescriptorResource(const glslang::TType& type)
{
    // uniform and buffer blocks are included, unless it is a push_constant
    if (type.getBasicType() == glslang::EbtBlock)
        return type.getQualifier().isUniformOrBuffer() && ! type.getQualifier().layoutPushConstant;

    // non block...
    // basically samplerXXX/subpass/sampler/texture are all included
    // if they are the global-scope-class, not the function parameter
    // (or local, if they ever exist) class.
    if (type.getBasicType() == glslang::EbtSampler)
        return type.getQualifier().isUniformOrBuffer();

    // None of the above.
    return false;
}

void InheritQualifiers(glslang::TQualifier& child, const glslang::TQualifier& parent)
{
    if (child.layoutMatrix == glslang::ElmNone)
        child.layoutMatrix = parent.layoutMatrix;

    if (parent.invariant)
        child.invariant = true;
    if (parent.nopersp)
        child.nopersp = true;
    if (parent.flat)
        child.flat = true;
    if (parent.centroid)
        child.centroid = true;
    if (parent.patch)
        child.patch = true;
    if (parent.sample)
        child.sample = true;
    if (parent.coherent)
        child.coherent = true;
    if (parent.volatil)
        child.volatil = true;
    if (parent.restrict)
        child.restrict = true;
    if (parent.readonly)
        child.readonly = true;
    if (parent.writeonly)
        child.writeonly = true;
}

bool HasNonLayoutQualifiers(const glslang::TQualifier& qualifier)
{
    // This should list qualifiers that simultaneous satisfy:
    // - struct members can inherit from a struct declaration
    // - effect decorations on the struct members (note smooth does not, and expecting something like volatile to effect the whole object)
    // - are not part of the offset/st430/etc or row/column-major layout
    return qualifier.invariant || qualifier.nopersp || qualifier.flat || qualifier.centroid || qualifier.patch || qualifier.sample || qualifier.hasLocation();
}

//
// Implement the TGlslangToSpvTraverser class.
//

TGlslangToSpvTraverser::TGlslangToSpvTraverser(const glslang::TIntermediate* glslangIntermediate, spv::SpvBuildLogger* buildLogger)
    : TIntermTraverser(true, false, true), shaderEntry(0), sequenceDepth(0), logger(buildLogger),
      builder((glslang::GetKhronosToolId() << 16) | GeneratorVersion, logger),
      inMain(false), mainTerminated(false), linkageOnly(false),
      glslangIntermediate(glslangIntermediate)
{
    spv::ExecutionModel executionModel = TranslateExecutionModel(glslangIntermediate->getStage());

    builder.clearAccessChain();
    builder.setSource(TranslateSourceLanguage(glslangIntermediate->getSource(), glslangIntermediate->getProfile()), glslangIntermediate->getVersion());
    stdBuiltins = builder.import("GLSL.std.450");
    builder.setMemoryModel(spv::AddressingModelLogical, spv::MemoryModelGLSL450);
    shaderEntry = builder.makeEntrypoint(glslangIntermediate->getEntryPoint().c_str());
    entryPoint = builder.addEntryPoint(executionModel, shaderEntry, glslangIntermediate->getEntryPoint().c_str());

    // Add the source extensions
    const auto& sourceExtensions = glslangIntermediate->getRequestedExtensions();
    for (auto it = sourceExtensions.begin(); it != sourceExtensions.end(); ++it)
        builder.addSourceExtension(it->c_str());

    // Add the top-level modes for this shader.

    if (glslangIntermediate->getXfbMode()) {
        builder.addCapability(spv::CapabilityTransformFeedback);
        builder.addExecutionMode(shaderEntry, spv::ExecutionModeXfb);
    }

    unsigned int mode;
    switch (glslangIntermediate->getStage()) {
    case EShLangVertex:
        builder.addCapability(spv::CapabilityShader);
        break;

    case EShLangTessControl:
        builder.addCapability(spv::CapabilityTessellation);
        builder.addExecutionMode(shaderEntry, spv::ExecutionModeOutputVertices, glslangIntermediate->getVertices());
        break;

    case EShLangTessEvaluation:
        builder.addCapability(spv::CapabilityTessellation);
        switch (glslangIntermediate->getInputPrimitive()) {
        case glslang::ElgTriangles:           mode = spv::ExecutionModeTriangles;     break;
        case glslang::ElgQuads:               mode = spv::ExecutionModeQuads;         break;
        case glslang::ElgIsolines:            mode = spv::ExecutionModeIsolines;      break;
        default:                              mode = spv::BadValue;                        break;
        }
        if (mode != spv::BadValue)
            builder.addExecutionMode(shaderEntry, (spv::ExecutionMode)mode);

        switch (glslangIntermediate->getVertexSpacing()) {
        case glslang::EvsEqual:            mode = spv::ExecutionModeSpacingEqual;          break;
        case glslang::EvsFractionalEven:   mode = spv::ExecutionModeSpacingFractionalEven; break;
        case glslang::EvsFractionalOdd:    mode = spv::ExecutionModeSpacingFractionalOdd;  break;
        default:                           mode = spv::BadValue;                           break;
        }
        if (mode != spv::BadValue)
            builder.addExecutionMode(shaderEntry, (spv::ExecutionMode)mode);

        switch (glslangIntermediate->getVertexOrder()) {
        case glslang::EvoCw:     mode = spv::ExecutionModeVertexOrderCw;  break;
        case glslang::EvoCcw:    mode = spv::ExecutionModeVertexOrderCcw; break;
        default:                 mode = spv::BadValue;                    break;
        }
        if (mode != spv::BadValue)
            builder.addExecutionMode(shaderEntry, (spv::ExecutionMode)mode);

        if (glslangIntermediate->getPointMode())
            builder.addExecutionMode(shaderEntry, spv::ExecutionModePointMode);
        break;

    case EShLangGeometry:
        builder.addCapability(spv::CapabilityGeometry);
        switch (glslangIntermediate->getInputPrimitive()) {
        case glslang::ElgPoints:             mode = spv::ExecutionModeInputPoints;             break;
        case glslang::ElgLines:              mode = spv::ExecutionModeInputLines;              break;
        case glslang::ElgLinesAdjacency:     mode = spv::ExecutionModeInputLinesAdjacency;     break;
        case glslang::ElgTriangles:          mode = spv::ExecutionModeTriangles;               break;
        case glslang::ElgTrianglesAdjacency: mode = spv::ExecutionModeInputTrianglesAdjacency; break;
        default:                             mode = spv::BadValue;         break;
        }
        if (mode != spv::BadValue)
            builder.addExecutionMode(shaderEntry, (spv::ExecutionMode)mode);

        builder.addExecutionMode(shaderEntry, spv::ExecutionModeInvocations, glslangIntermediate->getInvocations());

        switch (glslangIntermediate->getOutputPrimitive()) {
        case glslang::ElgPoints:        mode = spv::ExecutionModeOutputPoints;                 break;
        case glslang::ElgLineStrip:     mode = spv::ExecutionModeOutputLineStrip;              break;
        case glslang::ElgTriangleStrip: mode = spv::ExecutionModeOutputTriangleStrip;          break;
        default:                        mode = spv::BadValue;              break;
        }
        if (mode != spv::BadValue)
            builder.addExecutionMode(shaderEntry, (spv::ExecutionMode)mode);
        builder.addExecutionMode(shaderEntry, spv::ExecutionModeOutputVertices, glslangIntermediate->getVertices());
        break;

    case EShLangFragment:
        builder.addCapability(spv::CapabilityShader);
        if (glslangIntermediate->getPixelCenterInteger())
            builder.addExecutionMode(shaderEntry, spv::ExecutionModePixelCenterInteger);

        if (glslangIntermediate->getOriginUpperLeft())
            builder.addExecutionMode(shaderEntry, spv::ExecutionModeOriginUpperLeft);
        else
            builder.addExecutionMode(shaderEntry, spv::ExecutionModeOriginLowerLeft);

        if (glslangIntermediate->getEarlyFragmentTests())
            builder.addExecutionMode(shaderEntry, spv::ExecutionModeEarlyFragmentTests);

        switch(glslangIntermediate->getDepth()) {
        case glslang::EldGreater:  mode = spv::ExecutionModeDepthGreater; break;
        case glslang::EldLess:     mode = spv::ExecutionModeDepthLess;    break;
        default:                   mode = spv::BadValue;                  break;
        }
        if (mode != spv::BadValue)
            builder.addExecutionMode(shaderEntry, (spv::ExecutionMode)mode);

        if (glslangIntermediate->getDepth() != glslang::EldUnchanged && glslangIntermediate->isDepthReplacing())
            builder.addExecutionMode(shaderEntry, spv::ExecutionModeDepthReplacing);
        break;

    case EShLangCompute:
        builder.addCapability(spv::CapabilityShader);
        builder.addExecutionMode(shaderEntry, spv::ExecutionModeLocalSize, glslangIntermediate->getLocalSize(0),
                                                                           glslangIntermediate->getLocalSize(1),
                                                                           glslangIntermediate->getLocalSize(2));
        break;

    default:
        break;
    }

}

// Finish everything and dump
void TGlslangToSpvTraverser::dumpSpv(std::vector<unsigned int>& out)
{
    // finish off the entry-point SPV instruction by adding the Input/Output <id>
    for (auto it = iOSet.cbegin(); it != iOSet.cend(); ++it)
        entryPoint->addIdOperand(*it);

    builder.eliminateDeadDecorations();
    builder.dump(out);
}

TGlslangToSpvTraverser::~TGlslangToSpvTraverser()
{
    if (! mainTerminated) {
        spv::Block* lastMainBlock = shaderEntry->getLastBlock();
        builder.setBuildPoint(lastMainBlock);
        builder.leaveFunction();
    }
}

//
// Implement the traversal functions.
//
// Return true from interior nodes to have the external traversal
// continue on to children.  Return false if children were
// already processed.
//

//
// Symbols can turn into
//  - uniform/input reads
//  - output writes
//  - complex lvalue base setups:  foo.bar[3]....  , where we see foo and start up an access chain
//  - something simple that degenerates into the last bullet
//
void TGlslangToSpvTraverser::visitSymbol(glslang::TIntermSymbol* symbol)
{
    SpecConstantOpModeGuard spec_constant_op_mode_setter(&builder);
    if (symbol->getType().getQualifier().isSpecConstant())
        spec_constant_op_mode_setter.turnOnSpecConstantOpMode();

    // getSymbolId() will set up all the IO decorations on the first call.
    // Formal function parameters were mapped during makeFunctions().
    spv::Id id = getSymbolId(symbol);

    // Include all "static use" and "linkage only" interface variables on the OpEntryPoint instruction
    if (builder.isPointer(id)) {
        spv::StorageClass sc = builder.getStorageClass(id);
        if (sc == spv::StorageClassInput || sc == spv::StorageClassOutput)
            iOSet.insert(id);
    }

    // Only process non-linkage-only nodes for generating actual static uses
    if (! linkageOnly || symbol->getQualifier().isSpecConstant()) {
        // Prepare to generate code for the access

        // L-value chains will be computed left to right.  We're on the symbol now,
        // which is the left-most part of the access chain, so now is "clear" time,
        // followed by setting the base.
        builder.clearAccessChain();

        // For now, we consider all user variables as being in memory, so they are pointers,
        // except for
        // A) "const in" arguments to a function, which are an intermediate object.
        //    See comments in handleUserFunctionCall().
        // B) Specialization constants (normal constant don't even come in as a variable),
        //    These are also pure R-values.
        glslang::TQualifier qualifier = symbol->getQualifier();
        if ((qualifier.storage == glslang::EvqConstReadOnly && constReadOnlyParameters.find(symbol->getId()) != constReadOnlyParameters.end()) ||
             qualifier.isSpecConstant())
            builder.setAccessChainRValue(id);
        else
            builder.setAccessChainLValue(id);
    }
}

bool TGlslangToSpvTraverser::visitBinary(glslang::TVisit /* visit */, glslang::TIntermBinary* node)
{
    SpecConstantOpModeGuard spec_constant_op_mode_setter(&builder);
    if (node->getType().getQualifier().isSpecConstant())
        spec_constant_op_mode_setter.turnOnSpecConstantOpMode();

    // First, handle special cases
    switch (node->getOp()) {
    case glslang::EOpAssign:
    case glslang::EOpAddAssign:
    case glslang::EOpSubAssign:
    case glslang::EOpMulAssign:
    case glslang::EOpVectorTimesMatrixAssign:
    case glslang::EOpVectorTimesScalarAssign:
    case glslang::EOpMatrixTimesScalarAssign:
    case glslang::EOpMatrixTimesMatrixAssign:
    case glslang::EOpDivAssign:
    case glslang::EOpModAssign:
    case glslang::EOpAndAssign:
    case glslang::EOpInclusiveOrAssign:
    case glslang::EOpExclusiveOrAssign:
    case glslang::EOpLeftShiftAssign:
    case glslang::EOpRightShiftAssign:
        // A bin-op assign "a += b" means the same thing as "a = a + b"
        // where a is evaluated before b. For a simple assignment, GLSL
        // says to evaluate the left before the right.  So, always, left
        // node then right node.
        {
            // get the left l-value, save it away
            builder.clearAccessChain();
            node->getLeft()->traverse(this);
            spv::Builder::AccessChain lValue = builder.getAccessChain();

            // evaluate the right
            builder.clearAccessChain();
            node->getRight()->traverse(this);
            spv::Id rValue = accessChainLoad(node->getRight()->getType());

            if (node->getOp() != glslang::EOpAssign) {
                // the left is also an r-value
                builder.setAccessChain(lValue);
                spv::Id leftRValue = accessChainLoad(node->getLeft()->getType());

                // do the operation
                rValue = createBinaryOperation(node->getOp(), TranslatePrecisionDecoration(node->getType()),
                                               TranslateNoContractionDecoration(node->getType().getQualifier()),
                                               convertGlslangToSpvType(node->getType()), leftRValue, rValue,
                                               node->getType().getBasicType());

                // these all need their counterparts in createBinaryOperation()
                assert(rValue != spv::NoResult);
            }

            // store the result
            builder.setAccessChain(lValue);
            accessChainStore(node->getType(), rValue);

            // assignments are expressions having an rValue after they are evaluated...
            builder.clearAccessChain();
            builder.setAccessChainRValue(rValue);
        }
        return false;
    case glslang::EOpIndexDirect:
    case glslang::EOpIndexDirectStruct:
        {
            // Get the left part of the access chain.
            node->getLeft()->traverse(this);

            // Add the next element in the chain

            int index = node->getRight()->getAsConstantUnion()->getConstArray()[0].getIConst();
            if (node->getLeft()->getBasicType() == glslang::EbtBlock && node->getOp() == glslang::EOpIndexDirectStruct) {
                // This may be, e.g., an anonymous block-member selection, which generally need
                // index remapping due to hidden members in anonymous blocks.
                std::vector<int>& remapper = memberRemapper[node->getLeft()->getType().getStruct()];
                assert(remapper.size() > 0);
                index = remapper[index];
            }

            if (! node->getLeft()->getType().isArray() &&
                node->getLeft()->getType().isVector() &&
                node->getOp() == glslang::EOpIndexDirect) {
                // This is essentially a hard-coded vector swizzle of size 1,
                // so short circuit the access-chain stuff with a swizzle.
                std::vector<unsigned> swizzle;
                swizzle.push_back(node->getRight()->getAsConstantUnion()->getConstArray()[0].getIConst());
                builder.accessChainPushSwizzle(swizzle, convertGlslangToSpvType(node->getLeft()->getType()));
            } else {
                // normal case for indexing array or structure or block
                builder.accessChainPush(builder.makeIntConstant(index));
            }
        }
        return false;
    case glslang::EOpIndexIndirect:
        {
            // Structure or array or vector indirection.
            // Will use native SPIR-V access-chain for struct and array indirection;
            // matrices are arrays of vectors, so will also work for a matrix.
            // Will use the access chain's 'component' for variable index into a vector.

            // This adapter is building access chains left to right.
            // Set up the access chain to the left.
            node->getLeft()->traverse(this);

            // save it so that computing the right side doesn't trash it
            spv::Builder::AccessChain partial = builder.getAccessChain();

            // compute the next index in the chain
            builder.clearAccessChain();
            node->getRight()->traverse(this);
            spv::Id index = accessChainLoad(node->getRight()->getType());

            // restore the saved access chain
            builder.setAccessChain(partial);

            if (! node->getLeft()->getType().isArray() && node->getLeft()->getType().isVector())
                builder.accessChainPushComponent(index, convertGlslangToSpvType(node->getLeft()->getType()));
            else
                builder.accessChainPush(index);
        }
        return false;
    case glslang::EOpVectorSwizzle:
        {
            node->getLeft()->traverse(this);
            glslang::TIntermSequence& swizzleSequence = node->getRight()->getAsAggregate()->getSequence();
            std::vector<unsigned> swizzle;
            for (int i = 0; i < (int)swizzleSequence.size(); ++i)
                swizzle.push_back(swizzleSequence[i]->getAsConstantUnion()->getConstArray()[0].getIConst());
            builder.accessChainPushSwizzle(swizzle, convertGlslangToSpvType(node->getLeft()->getType()));
        }
        return false;
    case glslang::EOpLogicalOr:
    case glslang::EOpLogicalAnd:
        {

            // These may require short circuiting, but can sometimes be done as straight
            // binary operations.  The right operand must be short circuited if it has
            // side effects, and should probably be if it is complex.
            if (isTrivial(node->getRight()->getAsTyped()))
                break; // handle below as a normal binary operation
            // otherwise, we need to do dynamic short circuiting on the right operand
            spv::Id result = createShortCircuit(node->getOp(), *node->getLeft()->getAsTyped(), *node->getRight()->getAsTyped());
            builder.clearAccessChain();
            builder.setAccessChainRValue(result);
        }
        return false;
    default:
        break;
    }

    // Assume generic binary op...

    // get right operand
    builder.clearAccessChain();
    node->getLeft()->traverse(this);
    spv::Id left = accessChainLoad(node->getLeft()->getType());

    // get left operand
    builder.clearAccessChain();
    node->getRight()->traverse(this);
    spv::Id right = accessChainLoad(node->getRight()->getType());

    // get result
    spv::Id result = createBinaryOperation(node->getOp(), TranslatePrecisionDecoration(node->getType()),
                                           TranslateNoContractionDecoration(node->getType().getQualifier()),
                                           convertGlslangToSpvType(node->getType()), left, right,
                                           node->getLeft()->getType().getBasicType());

    builder.clearAccessChain();
    if (! result) {
        logger->missingFunctionality("unknown glslang binary operation");
        return true;  // pick up a child as the place-holder result
    } else {
        builder.setAccessChainRValue(result);
        return false;
    }
}

bool TGlslangToSpvTraverser::visitUnary(glslang::TVisit /* visit */, glslang::TIntermUnary* node)
{
    SpecConstantOpModeGuard spec_constant_op_mode_setter(&builder);
    if (node->getType().getQualifier().isSpecConstant())
        spec_constant_op_mode_setter.turnOnSpecConstantOpMode();

    spv::Id result = spv::NoResult;

    // try texturing first
    result = createImageTextureFunctionCall(node);
    if (result != spv::NoResult) {
        builder.clearAccessChain();
        builder.setAccessChainRValue(result);

        return false; // done with this node
    }

    // Non-texturing.

    if (node->getOp() == glslang::EOpArrayLength) {
        // Quite special; won't want to evaluate the operand.

        // Normal .length() would have been constant folded by the front-end.
        // So, this has to be block.lastMember.length().
        // SPV wants "block" and member number as the operands, go get them.
        assert(node->getOperand()->getType().isRuntimeSizedArray());
        glslang::TIntermTyped* block = node->getOperand()->getAsBinaryNode()->getLeft();
        block->traverse(this);
        unsigned int member = node->getOperand()->getAsBinaryNode()->getRight()->getAsConstantUnion()->getConstArray()[0].getUConst();
        spv::Id length = builder.createArrayLength(builder.accessChainGetLValue(), member);

        builder.clearAccessChain();
        builder.setAccessChainRValue(length);

        return false;
    }

    // Start by evaluating the operand

    builder.clearAccessChain();
    node->getOperand()->traverse(this);

    spv::Id operand = spv::NoResult;

    if (node->getOp() == glslang::EOpAtomicCounterIncrement ||
        node->getOp() == glslang::EOpAtomicCounterDecrement ||
        node->getOp() == glslang::EOpAtomicCounter          ||
        node->getOp() == glslang::EOpInterpolateAtCentroid)
        operand = builder.accessChainGetLValue(); // Special case l-value operands
    else
        operand = accessChainLoad(node->getOperand()->getType());

    spv::Decoration precision = TranslatePrecisionDecoration(node->getType());
    spv::Decoration noContraction = TranslateNoContractionDecoration(node->getType().getQualifier());

    // it could be a conversion
    if (! result)
        result = createConversion(node->getOp(), precision, convertGlslangToSpvType(node->getType()), operand);

    // if not, then possibly an operation
    if (! result)
        result = createUnaryOperation(node->getOp(), precision, noContraction, convertGlslangToSpvType(node->getType()), operand, node->getOperand()->getBasicType());

    if (result) {
        builder.clearAccessChain();
        builder.setAccessChainRValue(result);

        return false; // done with this node
    }

    // it must be a special case, check...
    switch (node->getOp()) {
    case glslang::EOpPostIncrement:
    case glslang::EOpPostDecrement:
    case glslang::EOpPreIncrement:
    case glslang::EOpPreDecrement:
        {
            // we need the integer value "1" or the floating point "1.0" to add/subtract
            spv::Id one = 0;
            if (node->getBasicType() == glslang::EbtFloat)
                one = builder.makeFloatConstant(1.0F);
            else if (node->getBasicType() == glslang::EbtInt64 || node->getBasicType() == glslang::EbtUint64)
                one = builder.makeInt64Constant(1);
            else
                one = builder.makeIntConstant(1);
            glslang::TOperator op;
            if (node->getOp() == glslang::EOpPreIncrement ||
                node->getOp() == glslang::EOpPostIncrement)
                op = glslang::EOpAdd;
            else
                op = glslang::EOpSub;

            spv::Id result = createBinaryOperation(op, TranslatePrecisionDecoration(node->getType()),
                                                   TranslateNoContractionDecoration(node->getType().getQualifier()),
                                                   convertGlslangToSpvType(node->getType()), operand, one,
                                                   node->getType().getBasicType());
            assert(result != spv::NoResult);

            // The result of operation is always stored, but conditionally the
            // consumed result.  The consumed result is always an r-value.
            builder.accessChainStore(result);
            builder.clearAccessChain();
            if (node->getOp() == glslang::EOpPreIncrement ||
                node->getOp() == glslang::EOpPreDecrement)
                builder.setAccessChainRValue(result);
            else
                builder.setAccessChainRValue(operand);
        }

        return false;

    case glslang::EOpEmitStreamVertex:
        builder.createNoResultOp(spv::OpEmitStreamVertex, operand);
        return false;
    case glslang::EOpEndStreamPrimitive:
        builder.createNoResultOp(spv::OpEndStreamPrimitive, operand);
        return false;

    default:
        logger->missingFunctionality("unknown glslang unary");
        return true;  // pick up operand as placeholder result
    }
}

bool TGlslangToSpvTraverser::visitAggregate(glslang::TVisit visit, glslang::TIntermAggregate* node)
{
    SpecConstantOpModeGuard spec_constant_op_mode_setter(&builder);
    if (node->getType().getQualifier().isSpecConstant())
        spec_constant_op_mode_setter.turnOnSpecConstantOpMode();

    spv::Id result = spv::NoResult;

    // try texturing
    result = createImageTextureFunctionCall(node);
    if (result != spv::NoResult) {
        builder.clearAccessChain();
        builder.setAccessChainRValue(result);

        return false;
    } else if (node->getOp() == glslang::EOpImageStore) {
        // "imageStore" is a special case, which has no result
        return false;
    }

    glslang::TOperator binOp = glslang::EOpNull;
    bool reduceComparison = true;
    bool isMatrix = false;
    bool noReturnValue = false;
    bool atomic = false;

    assert(node->getOp());

    spv::Decoration precision = TranslatePrecisionDecoration(node->getType());

    switch (node->getOp()) {
    case glslang::EOpSequence:
    {
        if (preVisit)
            ++sequenceDepth;
        else
            --sequenceDepth;

        if (sequenceDepth == 1) {
            // If this is the parent node of all the functions, we want to see them
            // early, so all call points have actual SPIR-V functions to reference.
            // In all cases, still let the traverser visit the children for us.
            makeFunctions(node->getAsAggregate()->getSequence());

            // Also, we want all globals initializers to go into the entry of main(), before
            // anything else gets there, so visit out of order, doing them all now.
            makeGlobalInitializers(node->getAsAggregate()->getSequence());

            // Initializers are done, don't want to visit again, but functions link objects need to be processed,
            // so do them manually.
            visitFunctions(node->getAsAggregate()->getSequence());

            return false;
        }

        return true;
    }
    case glslang::EOpLinkerObjects:
    {
        if (visit == glslang::EvPreVisit)
            linkageOnly = true;
        else
            linkageOnly = false;

        return true;
    }
    case glslang::EOpComma:
    {
        // processing from left to right naturally leaves the right-most
        // lying around in the access chain
        glslang::TIntermSequence& glslangOperands = node->getSequence();
        for (int i = 0; i < (int)glslangOperands.size(); ++i)
            glslangOperands[i]->traverse(this);

        return false;
    }
    case glslang::EOpFunction:
        if (visit == glslang::EvPreVisit) {
            if (isShaderEntrypoint(node)) {
                inMain = true;
                builder.setBuildPoint(shaderEntry->getLastBlock());
            } else {
                handleFunctionEntry(node);
            }
        } else {
            if (inMain)
                mainTerminated = true;
            builder.leaveFunction();
            inMain = false;
        }

        return true;
    case glslang::EOpParameters:
        // Parameters will have been consumed by EOpFunction processing, but not
        // the body, so we still visited the function node's children, making this
        // child redundant.
        return false;
    case glslang::EOpFunctionCall:
    {
        if (node->isUserDefined())
            result = handleUserFunctionCall(node);
        //assert(result);  // this can happen for bad shaders because the call graph completeness checking is not yet done
        if (result) {
            builder.clearAccessChain();
            builder.setAccessChainRValue(result);
        } else
            logger->missingFunctionality("missing user function; linker needs to catch that");

        return false;
    }
    case glslang::EOpConstructMat2x2:
    case glslang::EOpConstructMat2x3:
    case glslang::EOpConstructMat2x4:
    case glslang::EOpConstructMat3x2:
    case glslang::EOpConstructMat3x3:
    case glslang::EOpConstructMat3x4:
    case glslang::EOpConstructMat4x2:
    case glslang::EOpConstructMat4x3:
    case glslang::EOpConstructMat4x4:
    case glslang::EOpConstructDMat2x2:
    case glslang::EOpConstructDMat2x3:
    case glslang::EOpConstructDMat2x4:
    case glslang::EOpConstructDMat3x2:
    case glslang::EOpConstructDMat3x3:
    case glslang::EOpConstructDMat3x4:
    case glslang::EOpConstructDMat4x2:
    case glslang::EOpConstructDMat4x3:
    case glslang::EOpConstructDMat4x4:
        isMatrix = true;
        // fall through
    case glslang::EOpConstructFloat:
    case glslang::EOpConstructVec2:
    case glslang::EOpConstructVec3:
    case glslang::EOpConstructVec4:
    case glslang::EOpConstructDouble:
    case glslang::EOpConstructDVec2:
    case glslang::EOpConstructDVec3:
    case glslang::EOpConstructDVec4:
    case glslang::EOpConstructBool:
    case glslang::EOpConstructBVec2:
    case glslang::EOpConstructBVec3:
    case glslang::EOpConstructBVec4:
    case glslang::EOpConstructInt:
    case glslang::EOpConstructIVec2:
    case glslang::EOpConstructIVec3:
    case glslang::EOpConstructIVec4:
    case glslang::EOpConstructUint:
    case glslang::EOpConstructUVec2:
    case glslang::EOpConstructUVec3:
    case glslang::EOpConstructUVec4:
    case glslang::EOpConstructInt64:
    case glslang::EOpConstructI64Vec2:
    case glslang::EOpConstructI64Vec3:
    case glslang::EOpConstructI64Vec4:
    case glslang::EOpConstructUint64:
    case glslang::EOpConstructU64Vec2:
    case glslang::EOpConstructU64Vec3:
    case glslang::EOpConstructU64Vec4:
    case glslang::EOpConstructStruct:
    case glslang::EOpConstructTextureSampler:
    {
        std::vector<spv::Id> arguments;
        translateArguments(*node, arguments);
        spv::Id resultTypeId = convertGlslangToSpvType(node->getType());
        spv::Id constructed;
        if (node->getOp() == glslang::EOpConstructTextureSampler)
            constructed = builder.createOp(spv::OpSampledImage, resultTypeId, arguments);
        else if (node->getOp() == glslang::EOpConstructStruct || node->getType().isArray()) {
            std::vector<spv::Id> constituents;
            for (int c = 0; c < (int)arguments.size(); ++c)
                constituents.push_back(arguments[c]);
            constructed = builder.createCompositeConstruct(resultTypeId, constituents);
        } else if (isMatrix)
            constructed = builder.createMatrixConstructor(precision, arguments, resultTypeId);
        else
            constructed = builder.createConstructor(precision, arguments, resultTypeId);

        builder.clearAccessChain();
        builder.setAccessChainRValue(constructed);

        return false;
    }

    // These six are component-wise compares with component-wise results.
    // Forward on to createBinaryOperation(), requesting a vector result.
    case glslang::EOpLessThan:
    case glslang::EOpGreaterThan:
    case glslang::EOpLessThanEqual:
    case glslang::EOpGreaterThanEqual:
    case glslang::EOpVectorEqual:
    case glslang::EOpVectorNotEqual:
    {
        // Map the operation to a binary
        binOp = node->getOp();
        reduceComparison = false;
        switch (node->getOp()) {
        case glslang::EOpVectorEqual:     binOp = glslang::EOpVectorEqual;      break;
        case glslang::EOpVectorNotEqual:  binOp = glslang::EOpVectorNotEqual;   break;
        default:                          binOp = node->getOp();                break;
        }

        break;
    }
    case glslang::EOpMul:
        // compontent-wise matrix multiply
        binOp = glslang::EOpMul;
        break;
    case glslang::EOpOuterProduct:
        // two vectors multiplied to make a matrix
        binOp = glslang::EOpOuterProduct;
        break;
    case glslang::EOpDot:
    {
        // for scalar dot product, use multiply
        glslang::TIntermSequence& glslangOperands = node->getSequence();
        if (! glslangOperands[0]->getAsTyped()->isVector())
            binOp = glslang::EOpMul;
        break;
    }
    case glslang::EOpMod:
        // when an aggregate, this is the floating-point mod built-in function,
        // which can be emitted by the one in createBinaryOperation()
        binOp = glslang::EOpMod;
        break;
    case glslang::EOpEmitVertex:
    case glslang::EOpEndPrimitive:
    case glslang::EOpBarrier:
    case glslang::EOpMemoryBarrier:
    case glslang::EOpMemoryBarrierAtomicCounter:
    case glslang::EOpMemoryBarrierBuffer:
    case glslang::EOpMemoryBarrierImage:
    case glslang::EOpMemoryBarrierShared:
    case glslang::EOpGroupMemoryBarrier:
        noReturnValue = true;
        // These all have 0 operands and will naturally finish up in the code below for 0 operands
        break;

    case glslang::EOpAtomicAdd:
    case glslang::EOpAtomicMin:
    case glslang::EOpAtomicMax:
    case glslang::EOpAtomicAnd:
    case glslang::EOpAtomicOr:
    case glslang::EOpAtomicXor:
    case glslang::EOpAtomicExchange:
    case glslang::EOpAtomicCompSwap:
        atomic = true;
        break;

    default:
        break;
    }

    //
    // See if it maps to a regular operation.
    //
    if (binOp != glslang::EOpNull) {
        glslang::TIntermTyped* left = node->getSequence()[0]->getAsTyped();
        glslang::TIntermTyped* right = node->getSequence()[1]->getAsTyped();
        assert(left && right);

        builder.clearAccessChain();
        left->traverse(this);
        spv::Id leftId = accessChainLoad(left->getType());

        builder.clearAccessChain();
        right->traverse(this);
        spv::Id rightId = accessChainLoad(right->getType());

        result = createBinaryOperation(binOp, precision, TranslateNoContractionDecoration(node->getType().getQualifier()),
                                       convertGlslangToSpvType(node->getType()), leftId, rightId,
                                       left->getType().getBasicType(), reduceComparison);

        // code above should only make binOp that exists in createBinaryOperation
        assert(result != spv::NoResult);
        builder.clearAccessChain();
        builder.setAccessChainRValue(result);

        return false;
    }

    //
    // Create the list of operands.
    //
    glslang::TIntermSequence& glslangOperands = node->getSequence();
    std::vector<spv::Id> operands;
    for (int arg = 0; arg < (int)glslangOperands.size(); ++arg) {
        builder.clearAccessChain();
        glslangOperands[arg]->traverse(this);

        // special case l-value operands; there are just a few
        bool lvalue = false;
        switch (node->getOp()) {
        case glslang::EOpFrexp:
        case glslang::EOpModf:
            if (arg == 1)
                lvalue = true;
            break;
        case glslang::EOpInterpolateAtSample:
        case glslang::EOpInterpolateAtOffset:
            if (arg == 0)
                lvalue = true;
            break;
        case glslang::EOpAtomicAdd:
        case glslang::EOpAtomicMin:
        case glslang::EOpAtomicMax:
        case glslang::EOpAtomicAnd:
        case glslang::EOpAtomicOr:
        case glslang::EOpAtomicXor:
        case glslang::EOpAtomicExchange:
        case glslang::EOpAtomicCompSwap:
            if (arg == 0)
                lvalue = true;
            break;
        case glslang::EOpAddCarry:
        case glslang::EOpSubBorrow:
            if (arg == 2)
                lvalue = true;
            break;
        case glslang::EOpUMulExtended:
        case glslang::EOpIMulExtended:
            if (arg >= 2)
                lvalue = true;
            break;
        default:
            break;
        }
        if (lvalue)
            operands.push_back(builder.accessChainGetLValue());
        else
            operands.push_back(accessChainLoad(glslangOperands[arg]->getAsTyped()->getType()));
    }

    if (atomic) {
        // Handle all atomics
        result = createAtomicOperation(node->getOp(), precision, convertGlslangToSpvType(node->getType()), operands, node->getBasicType());
    } else {
        // Pass through to generic operations.
        switch (glslangOperands.size()) {
        case 0:
            result = createNoArgOperation(node->getOp());
            break;
        case 1:
            result = createUnaryOperation(
                node->getOp(), precision,
                TranslateNoContractionDecoration(node->getType().getQualifier()),
                convertGlslangToSpvType(node->getType()), operands.front(),
                glslangOperands[0]->getAsTyped()->getBasicType());
            break;
        default:
            result = createMiscOperation(node->getOp(), precision, convertGlslangToSpvType(node->getType()), operands, node->getBasicType());
            break;
        }
    }

    if (noReturnValue)
        return false;

    if (! result) {
        logger->missingFunctionality("unknown glslang aggregate");
        return true;  // pick up a child as a placeholder operand
    } else {
        builder.clearAccessChain();
        builder.setAccessChainRValue(result);
        return false;
    }
}

bool TGlslangToSpvTraverser::visitSelection(glslang::TVisit /* visit */, glslang::TIntermSelection* node)
{
    // This path handles both if-then-else and ?:
    // The if-then-else has a node type of void, while
    // ?: has a non-void node type
    spv::Id result = 0;
    if (node->getBasicType() != glslang::EbtVoid) {
        // don't handle this as just on-the-fly temporaries, because there will be two names
        // and better to leave SSA to later passes
        result = builder.createVariable(spv::StorageClassFunction, convertGlslangToSpvType(node->getType()));
    }

    // emit the condition before doing anything with selection
    node->getCondition()->traverse(this);

    // make an "if" based on the value created by the condition
    spv::Builder::If ifBuilder(accessChainLoad(node->getCondition()->getType()), builder);

    if (node->getTrueBlock()) {
        // emit the "then" statement
        node->getTrueBlock()->traverse(this);
        if (result)
            builder.createStore(accessChainLoad(node->getTrueBlock()->getAsTyped()->getType()), result);
    }

    if (node->getFalseBlock()) {
        ifBuilder.makeBeginElse();
        // emit the "else" statement
        node->getFalseBlock()->traverse(this);
        if (result)
            builder.createStore(accessChainLoad(node->getFalseBlock()->getAsTyped()->getType()), result);
    }

    ifBuilder.makeEndIf();

    if (result) {
        // GLSL only has r-values as the result of a :?, but
        // if we have an l-value, that can be more efficient if it will
        // become the base of a complex r-value expression, because the
        // next layer copies r-values into memory to use the access-chain mechanism
        builder.clearAccessChain();
        builder.setAccessChainLValue(result);
    }

    return false;
}

bool TGlslangToSpvTraverser::visitSwitch(glslang::TVisit /* visit */, glslang::TIntermSwitch* node)
{
    // emit and get the condition before doing anything with switch
    node->getCondition()->traverse(this);
    spv::Id selector = accessChainLoad(node->getCondition()->getAsTyped()->getType());

    // browse the children to sort out code segments
    int defaultSegment = -1;
    std::vector<TIntermNode*> codeSegments;
    glslang::TIntermSequence& sequence = node->getBody()->getSequence();
    std::vector<int> caseValues;
    std::vector<int> valueIndexToSegment(sequence.size());  // note: probably not all are used, it is an overestimate
    for (glslang::TIntermSequence::iterator c = sequence.begin(); c != sequence.end(); ++c) {
        TIntermNode* child = *c;
        if (child->getAsBranchNode() && child->getAsBranchNode()->getFlowOp() == glslang::EOpDefault)
            defaultSegment = (int)codeSegments.size();
        else if (child->getAsBranchNode() && child->getAsBranchNode()->getFlowOp() == glslang::EOpCase) {
            valueIndexToSegment[caseValues.size()] = (int)codeSegments.size();
            caseValues.push_back(child->getAsBranchNode()->getExpression()->getAsConstantUnion()->getConstArray()[0].getIConst());
        } else
            codeSegments.push_back(child);
    }

    // handle the case where the last code segment is missing, due to no code
    // statements between the last case and the end of the switch statement
    if ((caseValues.size() && (int)codeSegments.size() == valueIndexToSegment[caseValues.size() - 1]) ||
        (int)codeSegments.size() == defaultSegment)
        codeSegments.push_back(nullptr);

    // make the switch statement
    std::vector<spv::Block*> segmentBlocks; // returned, as the blocks allocated in the call
    builder.makeSwitch(selector, (int)codeSegments.size(), caseValues, valueIndexToSegment, defaultSegment, segmentBlocks);

    // emit all the code in the segments
    breakForLoop.push(false);
    for (unsigned int s = 0; s < codeSegments.size(); ++s) {
        builder.nextSwitchSegment(segmentBlocks, s);
        if (codeSegments[s])
            codeSegments[s]->traverse(this);
        else
            builder.addSwitchBreak();
    }
    breakForLoop.pop();

    builder.endSwitch(segmentBlocks);

    return false;
}

void TGlslangToSpvTraverser::visitConstantUnion(glslang::TIntermConstantUnion* node)
{
    int nextConst = 0;
    spv::Id constant = createSpvConstantFromConstUnionArray(node->getType(), node->getConstArray(), nextConst, false);

    builder.clearAccessChain();
    builder.setAccessChainRValue(constant);
}

bool TGlslangToSpvTraverser::visitLoop(glslang::TVisit /* visit */, glslang::TIntermLoop* node)
{
    auto blocks = builder.makeNewLoop();
    builder.createBranch(&blocks.head);
    // Spec requires back edges to target header blocks, and every header block
    // must dominate its merge block.  Make a header block first to ensure these
    // conditions are met.  By definition, it will contain OpLoopMerge, followed
    // by a block-ending branch.  But we don't want to put any other body/test
    // instructions in it, since the body/test may have arbitrary instructions,
    // including merges of its own.
    builder.setBuildPoint(&blocks.head);
    builder.createLoopMerge(&blocks.merge, &blocks.continue_target, spv::LoopControlMaskNone);
    if (node->testFirst() && node->getTest()) {
        spv::Block& test = builder.makeNewBlock();
        builder.createBranch(&test);

        builder.setBuildPoint(&test);
        node->getTest()->traverse(this);
        spv::Id condition =
            accessChainLoad(node->getTest()->getType());
        builder.createConditionalBranch(condition, &blocks.body, &blocks.merge);

        builder.setBuildPoint(&blocks.body);
        breakForLoop.push(true);
        if (node->getBody())
            node->getBody()->traverse(this);
        builder.createBranch(&blocks.continue_target);
        breakForLoop.pop();

        builder.setBuildPoint(&blocks.continue_target);
        if (node->getTerminal())
            node->getTerminal()->traverse(this);
        builder.createBranch(&blocks.head);
    } else {
        builder.createBranch(&blocks.body);

        breakForLoop.push(true);
        builder.setBuildPoint(&blocks.body);
        if (node->getBody())
            node->getBody()->traverse(this);
        builder.createBranch(&blocks.continue_target);
        breakForLoop.pop();

        builder.setBuildPoint(&blocks.continue_target);
        if (node->getTerminal())
            node->getTerminal()->traverse(this);
        if (node->getTest()) {
            node->getTest()->traverse(this);
            spv::Id condition =
                accessChainLoad(node->getTest()->getType());
            builder.createConditionalBranch(condition, &blocks.head, &blocks.merge);
        } else {
            // TODO: unless there was a break/return/discard instruction
            // somewhere in the body, this is an infinite loop, so we should
            // issue a warning.
            builder.createBranch(&blocks.head);
        }
    }
    builder.setBuildPoint(&blocks.merge);
    builder.closeLoop();
    return false;
}

bool TGlslangToSpvTraverser::visitBranch(glslang::TVisit /* visit */, glslang::TIntermBranch* node)
{
    if (node->getExpression())
        node->getExpression()->traverse(this);

    switch (node->getFlowOp()) {
    case glslang::EOpKill:
        builder.makeDiscard();
        break;
    case glslang::EOpBreak:
        if (breakForLoop.top())
            builder.createLoopExit();
        else
            builder.addSwitchBreak();
        break;
    case glslang::EOpContinue:
        builder.createLoopContinue();
        break;
    case glslang::EOpReturn:
        if (node->getExpression())
            builder.makeReturn(false, accessChainLoad(node->getExpression()->getType()));
        else
            builder.makeReturn(false);

        builder.clearAccessChain();
        break;

    default:
        assert(0);
        break;
    }

    return false;
}

spv::Id TGlslangToSpvTraverser::createSpvVariable(const glslang::TIntermSymbol* node)
{
    // First, steer off constants, which are not SPIR-V variables, but
    // can still have a mapping to a SPIR-V Id.
    // This includes specialization constants.
    if (node->getQualifier().isConstant()) {
        return createSpvConstant(*node);
    }

    // Now, handle actual variables
    spv::StorageClass storageClass = TranslateStorageClass(node->getType());
    spv::Id spvType = convertGlslangToSpvType(node->getType());

    const char* name = node->getName().c_str();
    if (glslang::IsAnonymous(name))
        name = "";

    return builder.createVariable(storageClass, spvType, name);
}

// Return type Id of the sampled type.
spv::Id TGlslangToSpvTraverser::getSampledType(const glslang::TSampler& sampler)
{
    switch (sampler.type) {
        case glslang::EbtFloat:    return builder.makeFloatType(32);
        case glslang::EbtInt:      return builder.makeIntType(32);
        case glslang::EbtUint:     return builder.makeUintType(32);
        default:
            assert(0);
            return builder.makeFloatType(32);
    }
}

// Convert from a glslang type to an SPV type, by calling into a
// recursive version of this function. This establishes the inherited
// layout state rooted from the top-level type.
spv::Id TGlslangToSpvTraverser::convertGlslangToSpvType(const glslang::TType& type)
{
    return convertGlslangToSpvType(type, getExplicitLayout(type), type.getQualifier());
}

// Do full recursive conversion of an arbitrary glslang type to a SPIR-V Id.
// explicitLayout can be kept the same throughout the hierarchical recursive walk.
spv::Id TGlslangToSpvTraverser::convertGlslangToSpvType(const glslang::TType& type, glslang::TLayoutPacking explicitLayout, const glslang::TQualifier& qualifier)
{
    spv::Id spvType = spv::NoResult;

    switch (type.getBasicType()) {
    case glslang::EbtVoid:
        spvType = builder.makeVoidType();
        assert (! type.isArray());
        break;
    case glslang::EbtFloat:
        spvType = builder.makeFloatType(32);
        break;
    case glslang::EbtDouble:
        spvType = builder.makeFloatType(64);
        break;
    case glslang::EbtBool:
        // "transparent" bool doesn't exist in SPIR-V.  The GLSL convention is
        // a 32-bit int where non-0 means true.
        if (explicitLayout != glslang::ElpNone)
            spvType = builder.makeUintType(32);
        else
            spvType = builder.makeBoolType();
        break;
    case glslang::EbtInt:
        spvType = builder.makeIntType(32);
        break;
    case glslang::EbtUint:
        spvType = builder.makeUintType(32);
        break;
    case glslang::EbtInt64:
        builder.addCapability(spv::CapabilityInt64);
        spvType = builder.makeIntType(64);
        break;
    case glslang::EbtUint64:
        builder.addCapability(spv::CapabilityInt64);
        spvType = builder.makeUintType(64);
        break;
    case glslang::EbtAtomicUint:
        logger->tbdFunctionality("Is atomic_uint an opaque handle in the uniform storage class, or an addresses in the atomic storage class?");
        spvType = builder.makeUintType(32);
        break;
    case glslang::EbtSampler:
        {
            const glslang::TSampler& sampler = type.getSampler();
            if (sampler.sampler) {
                // pure sampler
                spvType = builder.makeSamplerType();
            } else {
                // an image is present, make its type
                spvType = builder.makeImageType(getSampledType(sampler), TranslateDimensionality(sampler), sampler.shadow, sampler.arrayed, sampler.ms,
                                                sampler.image ? 2 : 1, TranslateImageFormat(type));
                if (sampler.combined) {
                    // already has both image and sampler, make the combined type
                    spvType = builder.makeSampledImageType(spvType);
                }
            }
        }
        break;
    case glslang::EbtStruct:
    case glslang::EbtBlock:
        {
            // If we've seen this struct type, return it
            const glslang::TTypeList* glslangStruct = type.getStruct();
            std::vector<spv::Id> structFields;

            // Try to share structs for different layouts, but not yet for other
            // kinds of qualification (primarily not yet including interpolant qualification).
            if (! HasNonLayoutQualifiers(qualifier))
                spvType = structMap[explicitLayout][qualifier.layoutMatrix][glslangStruct];
            if (spvType != spv::NoResult)
                break;

            // else, we haven't seen it...

            // Create a vector of struct types for SPIR-V to consume
            int memberDelta = 0;  // how much the member's index changes from glslang to SPIR-V, normally 0, except sometimes for blocks
            if (type.getBasicType() == glslang::EbtBlock)
                memberRemapper[glslangStruct].resize(glslangStruct->size());
            int locationOffset = 0;  // for use across struct members, when they are called recursively
            for (int i = 0; i < (int)glslangStruct->size(); i++) {
                glslang::TType& glslangType = *(*glslangStruct)[i].type;
                if (glslangType.hiddenMember()) {
                    ++memberDelta;
                    if (type.getBasicType() == glslang::EbtBlock)
                        memberRemapper[glslangStruct][i] = -1;
                } else {
                    if (type.getBasicType() == glslang::EbtBlock)
                        memberRemapper[glslangStruct][i] = i - memberDelta;
                    // modify just this child's view of the qualifier
                    glslang::TQualifier subQualifier = glslangType.getQualifier();
                    InheritQualifiers(subQualifier, qualifier);

                    // manually inherit location; it's more complex
                    if (! subQualifier.hasLocation() && qualifier.hasLocation())
                        subQualifier.layoutLocation = qualifier.layoutLocation + locationOffset;
                    if (qualifier.hasLocation())
                        locationOffset += glslangIntermediate->computeTypeLocationSize(glslangType);

                    // recurse
                    structFields.push_back(convertGlslangToSpvType(glslangType, explicitLayout, subQualifier));
                }
            }

            // Make the SPIR-V type
            spvType = builder.makeStructType(structFields, type.getTypeName().c_str());
            if (! HasNonLayoutQualifiers(qualifier))
                structMap[explicitLayout][qualifier.layoutMatrix][glslangStruct] = spvType;

            // Name and decorate the non-hidden members
            int offset = -1;
            locationOffset = 0;  // for use within the members of this struct, right now
            for (int i = 0; i < (int)glslangStruct->size(); i++) {
                glslang::TType& glslangType = *(*glslangStruct)[i].type;
                int member = i;
                if (type.getBasicType() == glslang::EbtBlock)
                    member = memberRemapper[glslangStruct][i];

                // modify just this child's view of the qualifier
                glslang::TQualifier subQualifier = glslangType.getQualifier();
                InheritQualifiers(subQualifier, qualifier);

                // using -1 above to indicate a hidden member
                if (member >= 0) {
                    builder.addMemberName(spvType, member, glslangType.getFieldName().c_str());
                    addMemberDecoration(spvType, member, TranslateLayoutDecoration(glslangType, subQualifier.layoutMatrix));
                    addMemberDecoration(spvType, member, TranslatePrecisionDecoration(glslangType));
                    addMemberDecoration(spvType, member, TranslateInterpolationDecoration(subQualifier));
                    addMemberDecoration(spvType, member, TranslateInvariantDecoration(subQualifier));

                    if (qualifier.storage == glslang::EvqBuffer) {
                        std::vector<spv::Decoration> memory;
                        TranslateMemoryDecoration(subQualifier, memory);
                        for (unsigned int i = 0; i < memory.size(); ++i)
                            addMemberDecoration(spvType, member, memory[i]);
                    }

                    // compute location decoration; tricky based on whether inheritance is at play
                    // TODO: This algorithm (and it's cousin above doing almost the same thing) should
                    //       probably move to the linker stage of the front end proper, and just have the
                    //       answer sitting already distributed throughout the individual member locations.
                    int location = -1;                // will only decorate if present or inherited
                    if (subQualifier.hasLocation())   // no inheritance, or override of inheritance
                        location = subQualifier.layoutLocation;
                    else if (qualifier.hasLocation()) // inheritance
                        location = qualifier.layoutLocation + locationOffset;
                    if (qualifier.hasLocation())      // track for upcoming inheritance
                        locationOffset += glslangIntermediate->computeTypeLocationSize(glslangType);
                    if (location >= 0)
                        builder.addMemberDecoration(spvType, member, spv::DecorationLocation, location);

                    // component, XFB, others
                    if (glslangType.getQualifier().hasComponent())
                        builder.addMemberDecoration(spvType, member, spv::DecorationComponent, glslangType.getQualifier().layoutComponent);
                    if (glslangType.getQualifier().hasXfbOffset())
                        builder.addMemberDecoration(spvType, member, spv::DecorationOffset, glslangType.getQualifier().layoutXfbOffset);
                    else if (explicitLayout != glslang::ElpNone) {
                        // figure out what to do with offset, which is accumulating
                        int nextOffset;
                        updateMemberOffset(type, glslangType, offset, nextOffset, explicitLayout, subQualifier.layoutMatrix);
                        if (offset >= 0)
                            builder.addMemberDecoration(spvType, member, spv::DecorationOffset, offset);
                        offset = nextOffset;
                    }

                    if (glslangType.isMatrix() && explicitLayout != glslang::ElpNone)
                        builder.addMemberDecoration(spvType, member, spv::DecorationMatrixStride, getMatrixStride(glslangType, explicitLayout, subQualifier.layoutMatrix));

                    // built-in variable decorations
                    spv::BuiltIn builtIn = TranslateBuiltInDecoration(glslangType.getQualifier().builtIn);
                    if (builtIn != spv::BadValue)
                        addMemberDecoration(spvType, member, spv::DecorationBuiltIn, (int)builtIn);
                }
            }

            // Decorate the structure
            addDecoration(spvType, TranslateLayoutDecoration(type, qualifier.layoutMatrix));
            addDecoration(spvType, TranslateBlockDecoration(type));
            if (type.getQualifier().hasStream() && glslangIntermediate->isMultiStream()) {
                builder.addCapability(spv::CapabilityGeometryStreams);
                builder.addDecoration(spvType, spv::DecorationStream, type.getQualifier().layoutStream);
            }
            if (glslangIntermediate->getXfbMode()) {
                builder.addCapability(spv::CapabilityTransformFeedback);
                if (type.getQualifier().hasXfbStride())
                    builder.addDecoration(spvType, spv::DecorationXfbStride, type.getQualifier().layoutXfbStride);
                if (type.getQualifier().hasXfbBuffer())
                    builder.addDecoration(spvType, spv::DecorationXfbBuffer, type.getQualifier().layoutXfbBuffer);
            }
        }
        break;
    default:
        assert(0);
        break;
    }

    if (type.isMatrix())
        spvType = builder.makeMatrixType(spvType, type.getMatrixCols(), type.getMatrixRows());
    else {
        // If this variable has a vector element count greater than 1, create a SPIR-V vector
        if (type.getVectorSize() > 1)
            spvType = builder.makeVectorType(spvType, type.getVectorSize());
    }

    if (type.isArray()) {
        int stride = 0;  // keep this 0 unless doing an explicit layout; 0 will mean no decoration, no stride

        // Do all but the outer dimension
        if (type.getArraySizes()->getNumDims() > 1) {
            // We need to decorate array strides for types needing explicit layout, except blocks.
            if (explicitLayout != glslang::ElpNone && type.getBasicType() != glslang::EbtBlock) {
                // Use a dummy glslang type for querying internal strides of
                // arrays of arrays, but using just a one-dimensional array.
                glslang::TType simpleArrayType(type, 0); // deference type of the array
                while (simpleArrayType.getArraySizes().getNumDims() > 1)
                    simpleArrayType.getArraySizes().dereference();

                // Will compute the higher-order strides here, rather than making a whole
                // pile of types and doing repetitive recursion on their contents.
                stride = getArrayStride(simpleArrayType, explicitLayout, qualifier.layoutMatrix);
            }

            // make the arrays
            for (int dim = type.getArraySizes()->getNumDims() - 1; dim > 0; --dim) {
                spvType = builder.makeArrayType(spvType, makeArraySizeId(*type.getArraySizes(), dim), stride);
                if (stride > 0)
                    builder.addDecoration(spvType, spv::DecorationArrayStride, stride);
                stride *= type.getArraySizes()->getDimSize(dim);
            }
        } else {
            // single-dimensional array, and don't yet have stride

            // We need to decorate array strides for types needing explicit layout, except blocks.
            if (explicitLayout != glslang::ElpNone && type.getBasicType() != glslang::EbtBlock)
                stride = getArrayStride(type, explicitLayout, qualifier.layoutMatrix);
        }

        // Do the outer dimension, which might not be known for a runtime-sized array
        if (type.isRuntimeSizedArray()) {
            spvType = builder.makeRuntimeArray(spvType);
        } else {
            assert(type.getOuterArraySize() > 0);
            spvType = builder.makeArrayType(spvType, makeArraySizeId(*type.getArraySizes(), 0), stride);
        }
        if (stride > 0)
            builder.addDecoration(spvType, spv::DecorationArrayStride, stride);
    }

    return spvType;
}

// Turn the expression forming the array size into an id.
// This is not quite trivial, because of specialization constants.
// Sometimes, a raw constant is turned into an Id, and sometimes
// a specialization constant expression is.
spv::Id TGlslangToSpvTraverser::makeArraySizeId(const glslang::TArraySizes& arraySizes, int dim)
{
    // First, see if this is sized with a node, meaning a specialization constant:
    glslang::TIntermTyped* specNode = arraySizes.getDimNode(dim);
    if (specNode != nullptr) {
        builder.clearAccessChain();
        specNode->traverse(this);
        return accessChainLoad(specNode->getAsTyped()->getType());
    }

    // Otherwise, need a compile-time (front end) size, get it:
    int size = arraySizes.getDimSize(dim);
    assert(size > 0);
    return builder.makeUintConstant(size);
}

// Wrap the builder's accessChainLoad to:
//  - localize handling of RelaxedPrecision
//  - use the SPIR-V inferred type instead of another conversion of the glslang type
//    (avoids unnecessary work and possible type punning for structures)
//  - do conversion of concrete to abstract type
spv::Id TGlslangToSpvTraverser::accessChainLoad(const glslang::TType& type)
{
    spv::Id nominalTypeId = builder.accessChainGetInferredType();
    spv::Id loadedId = builder.accessChainLoad(TranslatePrecisionDecoration(type), nominalTypeId);

    // Need to convert to abstract types when necessary
    if (type.getBasicType() == glslang::EbtBool) {
        if (builder.isScalarType(nominalTypeId)) {
            // Conversion for bool
            spv::Id boolType = builder.makeBoolType();
            if (nominalTypeId != boolType)
                loadedId = builder.createBinOp(spv::OpINotEqual, boolType, loadedId, builder.makeUintConstant(0));
        } else if (builder.isVectorType(nominalTypeId)) {
            // Conversion for bvec
            int vecSize = builder.getNumTypeComponents(nominalTypeId);
            spv::Id bvecType = builder.makeVectorType(builder.makeBoolType(), vecSize);
            if (nominalTypeId != bvecType)
                loadedId = builder.createBinOp(spv::OpINotEqual, bvecType, loadedId, makeSmearedConstant(builder.makeUintConstant(0), vecSize));
        }
    }

    return loadedId;
}

// Wrap the builder's accessChainStore to:
//  - do conversion of concrete to abstract type
void TGlslangToSpvTraverser::accessChainStore(const glslang::TType& type, spv::Id rvalue)
{
    // Need to convert to abstract types when necessary
    if (type.getBasicType() == glslang::EbtBool) {
        spv::Id nominalTypeId = builder.accessChainGetInferredType();

        if (builder.isScalarType(nominalTypeId)) {
            // Conversion for bool
            spv::Id boolType = builder.makeBoolType();
            if (nominalTypeId != boolType) {
                spv::Id zero = builder.makeUintConstant(0);
                spv::Id one  = builder.makeUintConstant(1);
                rvalue = builder.createTriOp(spv::OpSelect, nominalTypeId, rvalue, one, zero);
            }
        } else if (builder.isVectorType(nominalTypeId)) {
            // Conversion for bvec
            int vecSize = builder.getNumTypeComponents(nominalTypeId);
            spv::Id bvecType = builder.makeVectorType(builder.makeBoolType(), vecSize);
            if (nominalTypeId != bvecType) {
                spv::Id zero = makeSmearedConstant(builder.makeUintConstant(0), vecSize);
                spv::Id one  = makeSmearedConstant(builder.makeUintConstant(1), vecSize);
                rvalue = builder.createTriOp(spv::OpSelect, nominalTypeId, rvalue, one, zero);
            }
        }
    }

    builder.accessChainStore(rvalue);
}

// Decide whether or not this type should be
// decorated with offsets and strides, and if so
// whether std140 or std430 rules should be applied.
glslang::TLayoutPacking TGlslangToSpvTraverser::getExplicitLayout(const glslang::TType& type) const
{
    // has to be a block
    if (type.getBasicType() != glslang::EbtBlock)
        return glslang::ElpNone;

    // has to be a uniform or buffer block
    if (type.getQualifier().storage != glslang::EvqUniform &&
        type.getQualifier().storage != glslang::EvqBuffer)
        return glslang::ElpNone;

    // return the layout to use
    switch (type.getQualifier().layoutPacking) {
    case glslang::ElpStd140:
    case glslang::ElpStd430:
        return type.getQualifier().layoutPacking;
    default:
        return glslang::ElpNone;
    }
}

// Given an array type, returns the integer stride required for that array
int TGlslangToSpvTraverser::getArrayStride(const glslang::TType& arrayType, glslang::TLayoutPacking explicitLayout, glslang::TLayoutMatrix matrixLayout)
{
    int size;
    int stride;
    glslangIntermediate->getBaseAlignment(arrayType, size, stride, explicitLayout == glslang::ElpStd140, matrixLayout == glslang::ElmRowMajor);

    return stride;
}

// Given a matrix type, or array (of array) of matrixes type, returns the integer stride required for that matrix
// when used as a member of an interface block
int TGlslangToSpvTraverser::getMatrixStride(const glslang::TType& matrixType, glslang::TLayoutPacking explicitLayout, glslang::TLayoutMatrix matrixLayout)
{
    glslang::TType elementType;
    elementType.shallowCopy(matrixType);
    elementType.clearArraySizes();

    int size;
    int stride;
    glslangIntermediate->getBaseAlignment(elementType, size, stride, explicitLayout == glslang::ElpStd140, matrixLayout == glslang::ElmRowMajor);

    return stride;
}

// Given a member type of a struct, realign the current offset for it, and compute
// the next (not yet aligned) offset for the next member, which will get aligned
// on the next call.
// 'currentOffset' should be passed in already initialized, ready to modify, and reflecting
// the migration of data from nextOffset -> currentOffset.  It should be -1 on the first call.
// -1 means a non-forced member offset (no decoration needed).
void TGlslangToSpvTraverser::updateMemberOffset(const glslang::TType& /*structType*/, const glslang::TType& memberType, int& currentOffset, int& nextOffset,
                                                glslang::TLayoutPacking explicitLayout, glslang::TLayoutMatrix matrixLayout)
{
    // this will get a positive value when deemed necessary
    nextOffset = -1;

    // override anything in currentOffset with user-set offset
    if (memberType.getQualifier().hasOffset())
        currentOffset = memberType.getQualifier().layoutOffset;

    // It could be that current linker usage in glslang updated all the layoutOffset,
    // in which case the following code does not matter.  But, that's not quite right
    // once cross-compilation unit GLSL validation is done, as the original user
    // settings are needed in layoutOffset, and then the following will come into play.

    if (explicitLayout == glslang::ElpNone) {
        if (! memberType.getQualifier().hasOffset())
            currentOffset = -1;

        return;
    }

    // Getting this far means we need explicit offsets
    if (currentOffset < 0)
        currentOffset = 0;

    // Now, currentOffset is valid (either 0, or from a previous nextOffset),
    // but possibly not yet correctly aligned.

    int memberSize;
    int dummyStride;
    int memberAlignment = glslangIntermediate->getBaseAlignment(memberType, memberSize, dummyStride, explicitLayout == glslang::ElpStd140, matrixLayout == glslang::ElmRowMajor);
    glslang::RoundToPow2(currentOffset, memberAlignment);
    nextOffset = currentOffset + memberSize;
}

bool TGlslangToSpvTraverser::isShaderEntrypoint(const glslang::TIntermAggregate* node)
{
    // have to ignore mangling and just look at the base name
    size_t firstOpen = node->getName().find('(');
    return node->getName().compare(0, firstOpen, glslangIntermediate->getEntryPoint().c_str()) == 0;
}

// Make all the functions, skeletally, without actually visiting their bodies.
void TGlslangToSpvTraverser::makeFunctions(const glslang::TIntermSequence& glslFunctions)
{
    for (int f = 0; f < (int)glslFunctions.size(); ++f) {
        glslang::TIntermAggregate* glslFunction = glslFunctions[f]->getAsAggregate();
        if (! glslFunction || glslFunction->getOp() != glslang::EOpFunction || isShaderEntrypoint(glslFunction))
            continue;

        // We're on a user function.  Set up the basic interface for the function now,
        // so that it's available to call.
        // Translating the body will happen later.
        //
        // Typically (except for a "const in" parameter), an address will be passed to the
        // function.  What it is an address of varies:
        //
        // - "in" parameters not marked as "const" can be written to without modifying the argument,
        //  so that write needs to be to a copy, hence the address of a copy works.
        //
        // - "const in" parameters can just be the r-value, as no writes need occur.
        //
        // - "out" and "inout" arguments can't be done as direct pointers, because GLSL has
        // copy-in/copy-out semantics.  They can be handled though with a pointer to a copy.

        std::vector<spv::Id> paramTypes;
        std::vector<spv::Decoration> paramPrecisions;
        glslang::TIntermSequence& parameters = glslFunction->getSequence()[0]->getAsAggregate()->getSequence();

        for (int p = 0; p < (int)parameters.size(); ++p) {
            const glslang::TType& paramType = parameters[p]->getAsTyped()->getType();
            spv::Id typeId = convertGlslangToSpvType(paramType);
            if (paramType.getQualifier().storage != glslang::EvqConstReadOnly)
                typeId = builder.makePointer(spv::StorageClassFunction, typeId);
            else
                constReadOnlyParameters.insert(parameters[p]->getAsSymbolNode()->getId());
            paramPrecisions.push_back(TranslatePrecisionDecoration(paramType));
            paramTypes.push_back(typeId);
        }

        spv::Block* functionBlock;
        spv::Function *function = builder.makeFunctionEntry(TranslatePrecisionDecoration(glslFunction->getType()),
                                                            convertGlslangToSpvType(glslFunction->getType()),
                                                            glslFunction->getName().c_str(), paramTypes, paramPrecisions, &functionBlock);

        // Track function to emit/call later
        functionMap[glslFunction->getName().c_str()] = function;

        // Set the parameter id's
        for (int p = 0; p < (int)parameters.size(); ++p) {
            symbolValues[parameters[p]->getAsSymbolNode()->getId()] = function->getParamId(p);
            // give a name too
            builder.addName(function->getParamId(p), parameters[p]->getAsSymbolNode()->getName().c_str());
        }
    }
}

// Process all the initializers, while skipping the functions and link objects
void TGlslangToSpvTraverser::makeGlobalInitializers(const glslang::TIntermSequence& initializers)
{
    builder.setBuildPoint(shaderEntry->getLastBlock());
    for (int i = 0; i < (int)initializers.size(); ++i) {
        glslang::TIntermAggregate* initializer = initializers[i]->getAsAggregate();
        if (initializer && initializer->getOp() != glslang::EOpFunction && initializer->getOp() != glslang::EOpLinkerObjects) {

            // We're on a top-level node that's not a function.  Treat as an initializer, whose
            // code goes into the beginning of main.
            initializer->traverse(this);
        }
    }
}

// Process all the functions, while skipping initializers.
void TGlslangToSpvTraverser::visitFunctions(const glslang::TIntermSequence& glslFunctions)
{
    for (int f = 0; f < (int)glslFunctions.size(); ++f) {
        glslang::TIntermAggregate* node = glslFunctions[f]->getAsAggregate();
        if (node && (node->getOp() == glslang::EOpFunction || node->getOp() == glslang ::EOpLinkerObjects))
            node->traverse(this);
    }
}

void TGlslangToSpvTraverser::handleFunctionEntry(const glslang::TIntermAggregate* node)
{
    // SPIR-V functions should already be in the functionMap from the prepass
    // that called makeFunctions().
    spv::Function* function = functionMap[node->getName().c_str()];
    spv::Block* functionBlock = function->getEntryBlock();
    builder.setBuildPoint(functionBlock);
}

void TGlslangToSpvTraverser::translateArguments(const glslang::TIntermAggregate& node, std::vector<spv::Id>& arguments)
{
    const glslang::TIntermSequence& glslangArguments = node.getSequence();

    glslang::TSampler sampler = {};
    bool cubeCompare = false;
    if (node.isTexture() || node.isImage()) {
        sampler = glslangArguments[0]->getAsTyped()->getType().getSampler();
        cubeCompare = sampler.dim == glslang::EsdCube && sampler.arrayed && sampler.shadow;
    }

    for (int i = 0; i < (int)glslangArguments.size(); ++i) {
        builder.clearAccessChain();
        glslangArguments[i]->traverse(this);

        // Special case l-value operands
        bool lvalue = false;
        switch (node.getOp()) {
        case glslang::EOpImageAtomicAdd:
        case glslang::EOpImageAtomicMin:
        case glslang::EOpImageAtomicMax:
        case glslang::EOpImageAtomicAnd:
        case glslang::EOpImageAtomicOr:
        case glslang::EOpImageAtomicXor:
        case glslang::EOpImageAtomicExchange:
        case glslang::EOpImageAtomicCompSwap:
            if (i == 0)
                lvalue = true;
            break;
        case glslang::EOpSparseImageLoad:
            if ((sampler.ms && i == 3) || (! sampler.ms && i == 2))
                lvalue = true;
            break;
        case glslang::EOpSparseTexture:
            if ((cubeCompare && i == 3) || (! cubeCompare && i == 2))
                lvalue = true;
            break;
        case glslang::EOpSparseTextureClamp:
            if ((cubeCompare && i == 4) || (! cubeCompare && i == 3))
                lvalue = true;
            break;
        case glslang::EOpSparseTextureLod:
        case glslang::EOpSparseTextureOffset:
            if (i == 3)
                lvalue = true;
            break;
        case glslang::EOpSparseTextureFetch:
            if ((sampler.dim != glslang::EsdRect && i == 3) || (sampler.dim == glslang::EsdRect && i == 2))
                lvalue = true;
            break;
        case glslang::EOpSparseTextureFetchOffset:
            if ((sampler.dim != glslang::EsdRect && i == 4) || (sampler.dim == glslang::EsdRect && i == 3))
                lvalue = true;
            break;
        case glslang::EOpSparseTextureLodOffset:
        case glslang::EOpSparseTextureGrad:
        case glslang::EOpSparseTextureOffsetClamp:
            if (i == 4)
                lvalue = true;
            break;
        case glslang::EOpSparseTextureGradOffset:
        case glslang::EOpSparseTextureGradClamp:
            if (i == 5)
                lvalue = true;
            break;
        case glslang::EOpSparseTextureGradOffsetClamp:
            if (i == 6)
                lvalue = true;
            break;
         case glslang::EOpSparseTextureGather:
            if ((sampler.shadow && i == 3) || (! sampler.shadow && i == 2))
                lvalue = true;
            break;
        case glslang::EOpSparseTextureGatherOffset:
        case glslang::EOpSparseTextureGatherOffsets:
            if ((sampler.shadow && i == 4) || (! sampler.shadow && i == 3))
                lvalue = true;
            break;
        default:
            break;
        }

        if (lvalue)
            arguments.push_back(builder.accessChainGetLValue());
        else
            arguments.push_back(accessChainLoad(glslangArguments[i]->getAsTyped()->getType()));
    }
}

void TGlslangToSpvTraverser::translateArguments(glslang::TIntermUnary& node, std::vector<spv::Id>& arguments)
{
    builder.clearAccessChain();
    node.getOperand()->traverse(this);
    arguments.push_back(accessChainLoad(node.getOperand()->getType()));
}

spv::Id TGlslangToSpvTraverser::createImageTextureFunctionCall(glslang::TIntermOperator* node)
{
    if (! node->isImage() && ! node->isTexture()) {
        return spv::NoResult;
    }

    // Process a GLSL texturing op (will be SPV image)
    const glslang::TSampler sampler = node->getAsAggregate() ? node->getAsAggregate()->getSequence()[0]->getAsTyped()->getType().getSampler()
                                                             : node->getAsUnaryNode()->getOperand()->getAsTyped()->getType().getSampler();
    std::vector<spv::Id> arguments;
    if (node->getAsAggregate())
        translateArguments(*node->getAsAggregate(), arguments);
    else
        translateArguments(*node->getAsUnaryNode(), arguments);
    spv::Decoration precision = TranslatePrecisionDecoration(node->getType());

    spv::Builder::TextureParameters params = { };
    params.sampler = arguments[0];

    glslang::TCrackedTextureOp cracked;
    node->crackTexture(sampler, cracked);

    // Check for queries
    if (cracked.query) {
        // a sampled image needs to have the image extracted first
        if (builder.isSampledImage(params.sampler))
            params.sampler = builder.createUnaryOp(spv::OpImage, builder.getImageType(params.sampler), params.sampler);
        switch (node->getOp()) {
        case glslang::EOpImageQuerySize:
        case glslang::EOpTextureQuerySize:
            if (arguments.size() > 1) {
                params.lod = arguments[1];
                return builder.createTextureQueryCall(spv::OpImageQuerySizeLod, params);
            } else
                return builder.createTextureQueryCall(spv::OpImageQuerySize, params);
        case glslang::EOpImageQuerySamples:
        case glslang::EOpTextureQuerySamples:
            return builder.createTextureQueryCall(spv::OpImageQuerySamples, params);
        case glslang::EOpTextureQueryLod:
            params.coords = arguments[1];
            return builder.createTextureQueryCall(spv::OpImageQueryLod, params);
        case glslang::EOpTextureQueryLevels:
            return builder.createTextureQueryCall(spv::OpImageQueryLevels, params);
        case glslang::EOpSparseTexelsResident:
            return builder.createUnaryOp(spv::OpImageSparseTexelsResident, builder.makeBoolType(), arguments[0]);
        default:
            assert(0);
            break;
        }
    }

    // Check for image functions other than queries
    if (node->isImage()) {
        std::vector<spv::Id> operands;
        auto opIt = arguments.begin();
        operands.push_back(*(opIt++));

        // Handle subpass operations
        // TODO: GLSL should change to have the "MS" only on the type rather than the
        // built-in function.
        if (cracked.subpass) {
            // add on the (0,0) coordinate
            spv::Id zero = builder.makeIntConstant(0);
            std::vector<spv::Id> comps;
            comps.push_back(zero);
            comps.push_back(zero);
            operands.push_back(builder.makeCompositeConstant(builder.makeVectorType(builder.makeIntType(32), 2), comps));
            if (sampler.ms) {
                operands.push_back(spv::ImageOperandsSampleMask);
                operands.push_back(*(opIt++));
            }
            return builder.createOp(spv::OpImageRead, convertGlslangToSpvType(node->getType()), operands);
        }

        operands.push_back(*(opIt++));
        if (node->getOp() == glslang::EOpImageLoad) {
            if (sampler.ms) {
                operands.push_back(spv::ImageOperandsSampleMask);
                operands.push_back(*opIt);
            }
            if (builder.getImageTypeFormat(builder.getImageType(operands.front())) == spv::ImageFormatUnknown)
                builder.addCapability(spv::CapabilityStorageImageReadWithoutFormat);
            return builder.createOp(spv::OpImageRead, convertGlslangToSpvType(node->getType()), operands);
        } else if (node->getOp() == glslang::EOpImageStore) {
            if (sampler.ms) {
                operands.push_back(*(opIt + 1));
                operands.push_back(spv::ImageOperandsSampleMask);
                operands.push_back(*opIt);
            } else
                operands.push_back(*opIt);
            builder.createNoResultOp(spv::OpImageWrite, operands);
            if (builder.getImageTypeFormat(builder.getImageType(operands.front())) == spv::ImageFormatUnknown)
                builder.addCapability(spv::CapabilityStorageImageWriteWithoutFormat);
            return spv::NoResult;
        } else if (node->getOp() == glslang::EOpSparseImageLoad) {
            builder.addCapability(spv::CapabilitySparseResidency);
            if (builder.getImageTypeFormat(builder.getImageType(operands.front())) == spv::ImageFormatUnknown)
                builder.addCapability(spv::CapabilityStorageImageReadWithoutFormat);

            if (sampler.ms) {
                operands.push_back(spv::ImageOperandsSampleMask);
                operands.push_back(*opIt++);
            }

            // Create the return type that was a special structure
            spv::Id texelOut = *opIt;
            spv::Id typeId0 = convertGlslangToSpvType(node->getType());
            spv::Id typeId1 = builder.getDerefTypeId(texelOut);
            spv::Id resultTypeId = builder.makeStructResultType(typeId0, typeId1);

            spv::Id resultId = builder.createOp(spv::OpImageSparseRead, resultTypeId, operands);

            // Decode the return type
            builder.createStore(builder.createCompositeExtract(resultId, typeId1, 1), texelOut);
            return builder.createCompositeExtract(resultId, typeId0, 0);
        } else {
            // Process image atomic operations

            // GLSL "IMAGE_PARAMS" will involve in constructing an image texel pointer and this pointer,
            // as the first source operand, is required by SPIR-V atomic operations.
            operands.push_back(sampler.ms ? *(opIt++) : builder.makeUintConstant(0)); // For non-MS, the value should be 0

            spv::Id resultTypeId = builder.makePointer(spv::StorageClassImage, convertGlslangToSpvType(node->getType()));
            spv::Id pointer = builder.createOp(spv::OpImageTexelPointer, resultTypeId, operands);

            std::vector<spv::Id> operands;
            operands.push_back(pointer);
            for (; opIt != arguments.end(); ++opIt)
                operands.push_back(*opIt);

            return createAtomicOperation(node->getOp(), precision, convertGlslangToSpvType(node->getType()), operands, node->getBasicType());
        }
    }

    // Check for texture functions other than queries
    bool sparse = node->isSparseTexture();
    bool cubeCompare = sampler.dim == glslang::EsdCube && sampler.arrayed && sampler.shadow;

    // check for bias argument
    bool bias = false;
    if (! cracked.lod && ! cracked.gather && ! cracked.grad && ! cracked.fetch && ! cubeCompare) {
        int nonBiasArgCount = 2;
        if (cracked.offset)
            ++nonBiasArgCount;
        if (cracked.grad)
            nonBiasArgCount += 2;
        if (cracked.lodClamp)
            ++nonBiasArgCount;
        if (sparse)
            ++nonBiasArgCount;

        if ((int)arguments.size() > nonBiasArgCount)
            bias = true;
    }

    // set the rest of the arguments

    params.coords = arguments[1];
    int extraArgs = 0;
    bool noImplicitLod = false;

    // sort out where Dref is coming from
    if (cubeCompare) {
        params.Dref = arguments[2];
        ++extraArgs;
    } else if (sampler.shadow && cracked.gather) {
        params.Dref = arguments[2];
        ++extraArgs;
    } else if (sampler.shadow) {
        std::vector<spv::Id> indexes;
        int comp;
        if (cracked.proj)
            comp = 2;  // "The resulting 3rd component of P in the shadow forms is used as Dref"
        else
            comp = builder.getNumComponents(params.coords) - 1;
        indexes.push_back(comp);
        params.Dref = builder.createCompositeExtract(params.coords, builder.getScalarTypeId(builder.getTypeId(params.coords)), indexes);
    }
    if (cracked.lod) {
        params.lod = arguments[2];
        ++extraArgs;
    } else if (glslangIntermediate->getStage() != EShLangFragment) {
        // we need to invent the default lod for an explicit lod instruction for a non-fragment stage
        noImplicitLod = true;
    }
    if (sampler.ms) {
        params.sample = arguments[2]; // For MS, "sample" should be specified
        ++extraArgs;
    }
    if (cracked.grad) {
        params.gradX = arguments[2 + extraArgs];
        params.gradY = arguments[3 + extraArgs];
        extraArgs += 2;
    }
    if (cracked.offset) {
        params.offset = arguments[2 + extraArgs];
        ++extraArgs;
    } else if (cracked.offsets) {
        params.offsets = arguments[2 + extraArgs];
        ++extraArgs;
    }
    if (cracked.lodClamp) {
        params.lodClamp = arguments[2 + extraArgs];
        ++extraArgs;
    }
    if (sparse) {
        params.texelOut = arguments[2 + extraArgs];
        ++extraArgs;
    }
    if (bias) {
        params.bias = arguments[2 + extraArgs];
        ++extraArgs;
    }
    if (cracked.gather && ! sampler.shadow) {
        // default component is 0, if missing, otherwise an argument
        if (2 + extraArgs < (int)arguments.size()) {
            params.comp = arguments[2 + extraArgs];
            ++extraArgs;
        } else {
            params.comp = builder.makeIntConstant(0);
        }
    }

    return builder.createTextureCall(precision, convertGlslangToSpvType(node->getType()), sparse, cracked.fetch, cracked.proj, cracked.gather, noImplicitLod, params);
}

spv::Id TGlslangToSpvTraverser::handleUserFunctionCall(const glslang::TIntermAggregate* node)
{
    // Grab the function's pointer from the previously created function
    spv::Function* function = functionMap[node->getName().c_str()];
    if (! function)
        return 0;

    const glslang::TIntermSequence& glslangArgs = node->getSequence();
    const glslang::TQualifierList& qualifiers = node->getQualifierList();

    //  See comments in makeFunctions() for details about the semantics for parameter passing.
    //
    // These imply we need a four step process:
    // 1. Evaluate the arguments
    // 2. Allocate and make copies of in, out, and inout arguments
    // 3. Make the call
    // 4. Copy back the results

    // 1. Evaluate the arguments
    std::vector<spv::Builder::AccessChain> lValues;
    std::vector<spv::Id> rValues;
    std::vector<const glslang::TType*> argTypes;
    for (int a = 0; a < (int)glslangArgs.size(); ++a) {
        // build l-value
        builder.clearAccessChain();
        glslangArgs[a]->traverse(this);
        argTypes.push_back(&glslangArgs[a]->getAsTyped()->getType());
        // keep outputs as l-values, evaluate input-only as r-values
        if (qualifiers[a] != glslang::EvqConstReadOnly) {
            // save l-value
            lValues.push_back(builder.getAccessChain());
        } else {
            // process r-value
            rValues.push_back(accessChainLoad(*argTypes.back()));
        }
    }

    // 2. Allocate space for anything needing a copy, and if it's "in" or "inout"
    // copy the original into that space.
    //
    // Also, build up the list of actual arguments to pass in for the call
    int lValueCount = 0;
    int rValueCount = 0;
    std::vector<spv::Id> spvArgs;
    for (int a = 0; a < (int)glslangArgs.size(); ++a) {
        spv::Id arg;
        if (qualifiers[a] != glslang::EvqConstReadOnly) {
            // need space to hold the copy
            const glslang::TType& paramType = glslangArgs[a]->getAsTyped()->getType();
            arg = builder.createVariable(spv::StorageClassFunction, convertGlslangToSpvType(paramType), "param");
            if (qualifiers[a] == glslang::EvqIn || qualifiers[a] == glslang::EvqInOut) {
                // need to copy the input into output space
                builder.setAccessChain(lValues[lValueCount]);
                spv::Id copy = accessChainLoad(*argTypes[a]);
                builder.createStore(copy, arg);
            }
            ++lValueCount;
        } else {
            arg = rValues[rValueCount];
            ++rValueCount;
        }
        spvArgs.push_back(arg);
    }

    // 3. Make the call.
    spv::Id result = builder.createFunctionCall(function, spvArgs);
    builder.setPrecision(result, TranslatePrecisionDecoration(node->getType()));

    // 4. Copy back out an "out" arguments.
    lValueCount = 0;
    for (int a = 0; a < (int)glslangArgs.size(); ++a) {
        if (qualifiers[a] != glslang::EvqConstReadOnly) {
            if (qualifiers[a] == glslang::EvqOut || qualifiers[a] == glslang::EvqInOut) {
                spv::Id copy = builder.createLoad(spvArgs[a]);
                builder.setAccessChain(lValues[lValueCount]);
                accessChainStore(glslangArgs[a]->getAsTyped()->getType(), copy);
            }
            ++lValueCount;
        }
    }

    return result;
}

// Translate AST operation to SPV operation, already having SPV-based operands/types.
spv::Id TGlslangToSpvTraverser::createBinaryOperation(glslang::TOperator op, spv::Decoration precision,
                                                      spv::Decoration noContraction,
                                                      spv::Id typeId, spv::Id left, spv::Id right,
                                                      glslang::TBasicType typeProxy, bool reduceComparison)
{
    bool isUnsigned = typeProxy == glslang::EbtUint || typeProxy == glslang::EbtUint64;
    bool isFloat = typeProxy == glslang::EbtFloat || typeProxy == glslang::EbtDouble;
    bool isBool = typeProxy == glslang::EbtBool;

    spv::Op binOp = spv::OpNop;
    bool needMatchingVectors = true;  // for non-matrix ops, would a scalar need to smear to match a vector?
    bool comparison = false;

    switch (op) {
    case glslang::EOpAdd:
    case glslang::EOpAddAssign:
        if (isFloat)
            binOp = spv::OpFAdd;
        else
            binOp = spv::OpIAdd;
        break;
    case glslang::EOpSub:
    case glslang::EOpSubAssign:
        if (isFloat)
            binOp = spv::OpFSub;
        else
            binOp = spv::OpISub;
        break;
    case glslang::EOpMul:
    case glslang::EOpMulAssign:
        if (isFloat)
            binOp = spv::OpFMul;
        else
            binOp = spv::OpIMul;
        break;
    case glslang::EOpVectorTimesScalar:
    case glslang::EOpVectorTimesScalarAssign:
        if (isFloat) {
            if (builder.isVector(right))
                std::swap(left, right);
            assert(builder.isScalar(right));
            needMatchingVectors = false;
            binOp = spv::OpVectorTimesScalar;
        } else
            binOp = spv::OpIMul;
        break;
    case glslang::EOpVectorTimesMatrix:
    case glslang::EOpVectorTimesMatrixAssign:
        binOp = spv::OpVectorTimesMatrix;
        break;
    case glslang::EOpMatrixTimesVector:
        binOp = spv::OpMatrixTimesVector;
        break;
    case glslang::EOpMatrixTimesScalar:
    case glslang::EOpMatrixTimesScalarAssign:
        binOp = spv::OpMatrixTimesScalar;
        break;
    case glslang::EOpMatrixTimesMatrix:
    case glslang::EOpMatrixTimesMatrixAssign:
        binOp = spv::OpMatrixTimesMatrix;
        break;
    case glslang::EOpOuterProduct:
        binOp = spv::OpOuterProduct;
        needMatchingVectors = false;
        break;

    case glslang::EOpDiv:
    case glslang::EOpDivAssign:
        if (isFloat)
            binOp = spv::OpFDiv;
        else if (isUnsigned)
            binOp = spv::OpUDiv;
        else
            binOp = spv::OpSDiv;
        break;
    case glslang::EOpMod:
    case glslang::EOpModAssign:
        if (isFloat)
            binOp = spv::OpFMod;
        else if (isUnsigned)
            binOp = spv::OpUMod;
        else
            binOp = spv::OpSMod;
        break;
    case glslang::EOpRightShift:
    case glslang::EOpRightShiftAssign:
        if (isUnsigned)
            binOp = spv::OpShiftRightLogical;
        else
            binOp = spv::OpShiftRightArithmetic;
        break;
    case glslang::EOpLeftShift:
    case glslang::EOpLeftShiftAssign:
        binOp = spv::OpShiftLeftLogical;
        break;
    case glslang::EOpAnd:
    case glslang::EOpAndAssign:
        binOp = spv::OpBitwiseAnd;
        break;
    case glslang::EOpLogicalAnd:
        needMatchingVectors = false;
        binOp = spv::OpLogicalAnd;
        break;
    case glslang::EOpInclusiveOr:
    case glslang::EOpInclusiveOrAssign:
        binOp = spv::OpBitwiseOr;
        break;
    case glslang::EOpLogicalOr:
        needMatchingVectors = false;
        binOp = spv::OpLogicalOr;
        break;
    case glslang::EOpExclusiveOr:
    case glslang::EOpExclusiveOrAssign:
        binOp = spv::OpBitwiseXor;
        break;
    case glslang::EOpLogicalXor:
        needMatchingVectors = false;
        binOp = spv::OpLogicalNotEqual;
        break;

    case glslang::EOpLessThan:
    case glslang::EOpGreaterThan:
    case glslang::EOpLessThanEqual:
    case glslang::EOpGreaterThanEqual:
    case glslang::EOpEqual:
    case glslang::EOpNotEqual:
    case glslang::EOpVectorEqual:
    case glslang::EOpVectorNotEqual:
        comparison = true;
        break;
    default:
        break;
    }

    // handle mapped binary operations (should be non-comparison)
    if (binOp != spv::OpNop) {
        assert(comparison == false);
        if (builder.isMatrix(left) || builder.isMatrix(right))
            return createBinaryMatrixOperation(binOp, precision, noContraction, typeId, left, right);

        // No matrix involved; make both operands be the same number of components, if needed
        if (needMatchingVectors)
            builder.promoteScalar(precision, left, right);

        spv::Id result = builder.createBinOp(binOp, typeId, left, right);
        addDecoration(result, noContraction);
        return builder.setPrecision(result, precision);
    }

    if (! comparison)
        return 0;

    // Handle comparison instructions

    if (reduceComparison && (builder.isVector(left) || builder.isMatrix(left) || builder.isAggregate(left))) {
        assert(op == glslang::EOpEqual || op == glslang::EOpNotEqual);

        return builder.createCompositeCompare(precision, left, right, op == glslang::EOpEqual);
    }

    switch (op) {
    case glslang::EOpLessThan:
        if (isFloat)
            binOp = spv::OpFOrdLessThan;
        else if (isUnsigned)
            binOp = spv::OpULessThan;
        else
            binOp = spv::OpSLessThan;
        break;
    case glslang::EOpGreaterThan:
        if (isFloat)
            binOp = spv::OpFOrdGreaterThan;
        else if (isUnsigned)
            binOp = spv::OpUGreaterThan;
        else
            binOp = spv::OpSGreaterThan;
        break;
    case glslang::EOpLessThanEqual:
        if (isFloat)
            binOp = spv::OpFOrdLessThanEqual;
        else if (isUnsigned)
            binOp = spv::OpULessThanEqual;
        else
            binOp = spv::OpSLessThanEqual;
        break;
    case glslang::EOpGreaterThanEqual:
        if (isFloat)
            binOp = spv::OpFOrdGreaterThanEqual;
        else if (isUnsigned)
            binOp = spv::OpUGreaterThanEqual;
        else
            binOp = spv::OpSGreaterThanEqual;
        break;
    case glslang::EOpEqual:
    case glslang::EOpVectorEqual:
        if (isFloat)
            binOp = spv::OpFOrdEqual;
        else if (isBool)
            binOp = spv::OpLogicalEqual;
        else
            binOp = spv::OpIEqual;
        break;
    case glslang::EOpNotEqual:
    case glslang::EOpVectorNotEqual:
        if (isFloat)
            binOp = spv::OpFOrdNotEqual;
        else if (isBool)
            binOp = spv::OpLogicalNotEqual;
        else
            binOp = spv::OpINotEqual;
        break;
    default:
        break;
    }

    if (binOp != spv::OpNop) {
        spv::Id result = builder.createBinOp(binOp, typeId, left, right);
        addDecoration(result, noContraction);
        return builder.setPrecision(result, precision);
    }

    return 0;
}

//
// Translate AST matrix operation to SPV operation, already having SPV-based operands/types.
// These can be any of:
//
//   matrix * scalar
//   scalar * matrix
//   matrix * matrix     linear algebraic
//   matrix * vector
//   vector * matrix
//   matrix * matrix     componentwise
//   matrix op matrix    op in {+, -, /}
//   matrix op scalar    op in {+, -, /}
//   scalar op matrix    op in {+, -, /}
//
spv::Id TGlslangToSpvTraverser::createBinaryMatrixOperation(spv::Op op, spv::Decoration precision, spv::Decoration noContraction, spv::Id typeId, spv::Id left, spv::Id right)
{
    bool firstClass = true;

    // First, handle first-class matrix operations (* and matrix/scalar)
    switch (op) {
    case spv::OpFDiv:
        if (builder.isMatrix(left) && builder.isScalar(right)) {
            // turn matrix / scalar into a multiply...
            right = builder.createBinOp(spv::OpFDiv, builder.getTypeId(right), builder.makeFloatConstant(1.0F), right);
            op = spv::OpMatrixTimesScalar;
        } else
            firstClass = false;
        break;
    case spv::OpMatrixTimesScalar:
        if (builder.isMatrix(right))
            std::swap(left, right);
        assert(builder.isScalar(right));
        break;
    case spv::OpVectorTimesMatrix:
        assert(builder.isVector(left));
        assert(builder.isMatrix(right));
        break;
    case spv::OpMatrixTimesVector:
        assert(builder.isMatrix(left));
        assert(builder.isVector(right));
        break;
    case spv::OpMatrixTimesMatrix:
        assert(builder.isMatrix(left));
        assert(builder.isMatrix(right));
        break;
    default:
        firstClass = false;
        break;
    }

    if (firstClass) {
        spv::Id result = builder.createBinOp(op, typeId, left, right);
        addDecoration(result, noContraction);
        return builder.setPrecision(result, precision);
    }

    // Handle component-wise +, -, *, and / for all combinations of type.
    // The result type of all of them is the same type as the (a) matrix operand.
    // The algorithm is to:
    //   - break the matrix(es) into vectors
    //   - smear any scalar to a vector
    //   - do vector operations
    //   - make a matrix out the vector results
    switch (op) {
    case spv::OpFAdd:
    case spv::OpFSub:
    case spv::OpFDiv:
    case spv::OpFMul:
    {
        // one time set up...
        bool  leftMat = builder.isMatrix(left);
        bool rightMat = builder.isMatrix(right);
        unsigned int numCols = leftMat ? builder.getNumColumns(left) : builder.getNumColumns(right);
        int numRows = leftMat ? builder.getNumRows(left) : builder.getNumRows(right);
        spv::Id scalarType = builder.getScalarTypeId(typeId);
        spv::Id vecType = builder.makeVectorType(scalarType, numRows);
        std::vector<spv::Id> results;
        spv::Id smearVec = spv::NoResult;
        if (builder.isScalar(left))
            smearVec = builder.smearScalar(precision, left, vecType);
        else if (builder.isScalar(right))
            smearVec = builder.smearScalar(precision, right, vecType);

        // do each vector op
        for (unsigned int c = 0; c < numCols; ++c) {
            std::vector<unsigned int> indexes;
            indexes.push_back(c);
            spv::Id  leftVec =  leftMat ? builder.createCompositeExtract( left, vecType, indexes) : smearVec;
            spv::Id rightVec = rightMat ? builder.createCompositeExtract(right, vecType, indexes) : smearVec;
            spv::Id result = builder.createBinOp(op, vecType, leftVec, rightVec);
            addDecoration(result, noContraction);
            results.push_back(builder.setPrecision(result, precision));
        }

        // put the pieces together
        return  builder.setPrecision(builder.createCompositeConstruct(typeId, results), precision);
    }
    default:
        assert(0);
        return spv::NoResult;
    }
}

spv::Id TGlslangToSpvTraverser::createUnaryOperation(glslang::TOperator op, spv::Decoration precision, spv::Decoration noContraction, spv::Id typeId, spv::Id operand, glslang::TBasicType typeProxy)
{
    spv::Op unaryOp = spv::OpNop;
    int libCall = -1;
    bool isUnsigned = typeProxy == glslang::EbtUint || typeProxy == glslang::EbtUint64;
    bool isFloat = typeProxy == glslang::EbtFloat || typeProxy == glslang::EbtDouble;

    switch (op) {
    case glslang::EOpNegative:
        if (isFloat) {
            unaryOp = spv::OpFNegate;
            if (builder.isMatrixType(typeId))
                return createUnaryMatrixOperation(unaryOp, precision, noContraction, typeId, operand, typeProxy);
        } else
            unaryOp = spv::OpSNegate;
        break;

    case glslang::EOpLogicalNot:
    case glslang::EOpVectorLogicalNot:
        unaryOp = spv::OpLogicalNot;
        break;
    case glslang::EOpBitwiseNot:
        unaryOp = spv::OpNot;
        break;

    case glslang::EOpDeterminant:
        libCall = spv::GLSLstd450Determinant;
        break;
    case glslang::EOpMatrixInverse:
        libCall = spv::GLSLstd450MatrixInverse;
        break;
    case glslang::EOpTranspose:
        unaryOp = spv::OpTranspose;
        break;

    case glslang::EOpRadians:
        libCall = spv::GLSLstd450Radians;
        break;
    case glslang::EOpDegrees:
        libCall = spv::GLSLstd450Degrees;
        break;
    case glslang::EOpSin:
        libCall = spv::GLSLstd450Sin;
        break;
    case glslang::EOpCos:
        libCall = spv::GLSLstd450Cos;
        break;
    case glslang::EOpTan:
        libCall = spv::GLSLstd450Tan;
        break;
    case glslang::EOpAcos:
        libCall = spv::GLSLstd450Acos;
        break;
    case glslang::EOpAsin:
        libCall = spv::GLSLstd450Asin;
        break;
    case glslang::EOpAtan:
        libCall = spv::GLSLstd450Atan;
        break;

    case glslang::EOpAcosh:
        libCall = spv::GLSLstd450Acosh;
        break;
    case glslang::EOpAsinh:
        libCall = spv::GLSLstd450Asinh;
        break;
    case glslang::EOpAtanh:
        libCall = spv::GLSLstd450Atanh;
        break;
    case glslang::EOpTanh:
        libCall = spv::GLSLstd450Tanh;
        break;
    case glslang::EOpCosh:
        libCall = spv::GLSLstd450Cosh;
        break;
    case glslang::EOpSinh:
        libCall = spv::GLSLstd450Sinh;
        break;

    case glslang::EOpLength:
        libCall = spv::GLSLstd450Length;
        break;
    case glslang::EOpNormalize:
        libCall = spv::GLSLstd450Normalize;
        break;

    case glslang::EOpExp:
        libCall = spv::GLSLstd450Exp;
        break;
    case glslang::EOpLog:
        libCall = spv::GLSLstd450Log;
        break;
    case glslang::EOpExp2:
        libCall = spv::GLSLstd450Exp2;
        break;
    case glslang::EOpLog2:
        libCall = spv::GLSLstd450Log2;
        break;
    case glslang::EOpSqrt:
        libCall = spv::GLSLstd450Sqrt;
        break;
    case glslang::EOpInverseSqrt:
        libCall = spv::GLSLstd450InverseSqrt;
        break;

    case glslang::EOpFloor:
        libCall = spv::GLSLstd450Floor;
        break;
    case glslang::EOpTrunc:
        libCall = spv::GLSLstd450Trunc;
        break;
    case glslang::EOpRound:
        libCall = spv::GLSLstd450Round;
        break;
    case glslang::EOpRoundEven:
        libCall = spv::GLSLstd450RoundEven;
        break;
    case glslang::EOpCeil:
        libCall = spv::GLSLstd450Ceil;
        break;
    case glslang::EOpFract:
        libCall = spv::GLSLstd450Fract;
        break;

    case glslang::EOpIsNan:
        unaryOp = spv::OpIsNan;
        break;
    case glslang::EOpIsInf:
        unaryOp = spv::OpIsInf;
        break;

    case glslang::EOpFloatBitsToInt:
    case glslang::EOpFloatBitsToUint:
    case glslang::EOpIntBitsToFloat:
    case glslang::EOpUintBitsToFloat:
    case glslang::EOpDoubleBitsToInt64:
    case glslang::EOpDoubleBitsToUint64:
    case glslang::EOpInt64BitsToDouble:
    case glslang::EOpUint64BitsToDouble:
        unaryOp = spv::OpBitcast;
        break;

    case glslang::EOpPackSnorm2x16:
        libCall = spv::GLSLstd450PackSnorm2x16;
        break;
    case glslang::EOpUnpackSnorm2x16:
        libCall = spv::GLSLstd450UnpackSnorm2x16;
        break;
    case glslang::EOpPackUnorm2x16:
        libCall = spv::GLSLstd450PackUnorm2x16;
        break;
    case glslang::EOpUnpackUnorm2x16:
        libCall = spv::GLSLstd450UnpackUnorm2x16;
        break;
    case glslang::EOpPackHalf2x16:
        libCall = spv::GLSLstd450PackHalf2x16;
        break;
    case glslang::EOpUnpackHalf2x16:
        libCall = spv::GLSLstd450UnpackHalf2x16;
        break;
    case glslang::EOpPackSnorm4x8:
        libCall = spv::GLSLstd450PackSnorm4x8;
        break;
    case glslang::EOpUnpackSnorm4x8:
        libCall = spv::GLSLstd450UnpackSnorm4x8;
        break;
    case glslang::EOpPackUnorm4x8:
        libCall = spv::GLSLstd450PackUnorm4x8;
        break;
    case glslang::EOpUnpackUnorm4x8:
        libCall = spv::GLSLstd450UnpackUnorm4x8;
        break;
    case glslang::EOpPackDouble2x32:
        libCall = spv::GLSLstd450PackDouble2x32;
        break;
    case glslang::EOpUnpackDouble2x32:
        libCall = spv::GLSLstd450UnpackDouble2x32;
        break;

    case glslang::EOpPackInt2x32:
    case glslang::EOpUnpackInt2x32:
    case glslang::EOpPackUint2x32:
    case glslang::EOpUnpackUint2x32:
        logger->missingFunctionality("shader int64");
        libCall = spv::GLSLstd450Bad; // TODO: This is a placeholder.
        break;

    case glslang::EOpDPdx:
        unaryOp = spv::OpDPdx;
        break;
    case glslang::EOpDPdy:
        unaryOp = spv::OpDPdy;
        break;
    case glslang::EOpFwidth:
        unaryOp = spv::OpFwidth;
        break;
    case glslang::EOpDPdxFine:
        builder.addCapability(spv::CapabilityDerivativeControl);
        unaryOp = spv::OpDPdxFine;
        break;
    case glslang::EOpDPdyFine:
        builder.addCapability(spv::CapabilityDerivativeControl);
        unaryOp = spv::OpDPdyFine;
        break;
    case glslang::EOpFwidthFine:
        builder.addCapability(spv::CapabilityDerivativeControl);
        unaryOp = spv::OpFwidthFine;
        break;
    case glslang::EOpDPdxCoarse:
        builder.addCapability(spv::CapabilityDerivativeControl);
        unaryOp = spv::OpDPdxCoarse;
        break;
    case glslang::EOpDPdyCoarse:
        builder.addCapability(spv::CapabilityDerivativeControl);
        unaryOp = spv::OpDPdyCoarse;
        break;
    case glslang::EOpFwidthCoarse:
        builder.addCapability(spv::CapabilityDerivativeControl);
        unaryOp = spv::OpFwidthCoarse;
        break;
    case glslang::EOpInterpolateAtCentroid:
        builder.addCapability(spv::CapabilityInterpolationFunction);
        libCall = spv::GLSLstd450InterpolateAtCentroid;
        break;
    case glslang::EOpAny:
        unaryOp = spv::OpAny;
        break;
    case glslang::EOpAll:
        unaryOp = spv::OpAll;
        break;

    case glslang::EOpAbs:
        if (isFloat)
            libCall = spv::GLSLstd450FAbs;
        else
            libCall = spv::GLSLstd450SAbs;
        break;
    case glslang::EOpSign:
        if (isFloat)
            libCall = spv::GLSLstd450FSign;
        else
            libCall = spv::GLSLstd450SSign;
        break;

    case glslang::EOpAtomicCounterIncrement:
    case glslang::EOpAtomicCounterDecrement:
    case glslang::EOpAtomicCounter:
    {
        // Handle all of the atomics in one place, in createAtomicOperation()
        std::vector<spv::Id> operands;
        operands.push_back(operand);
        return createAtomicOperation(op, precision, typeId, operands, typeProxy);
    }

    case glslang::EOpBitFieldReverse:
        unaryOp = spv::OpBitReverse;
        break;
    case glslang::EOpBitCount:
        unaryOp = spv::OpBitCount;
        break;
    case glslang::EOpFindLSB:
        libCall = spv::GLSLstd450FindILsb;
        break;
    case glslang::EOpFindMSB:
        if (isUnsigned)
            libCall = spv::GLSLstd450FindUMsb;
        else
            libCall = spv::GLSLstd450FindSMsb;
        break;

    case glslang::EOpBallot:
    case glslang::EOpReadFirstInvocation:
        logger->missingFunctionality("shader ballot");
        libCall = spv::GLSLstd450Bad;
        break;

    case glslang::EOpAnyInvocation:
    case glslang::EOpAllInvocations:
    case glslang::EOpAllInvocationsEqual:
        return createInvocationsOperation(op, typeId, operand);

    default:
        return 0;
    }

    spv::Id id;
    if (libCall >= 0) {
        std::vector<spv::Id> args;
        args.push_back(operand);
        id = builder.createBuiltinCall(typeId, stdBuiltins, libCall, args);
    } else {
        id = builder.createUnaryOp(unaryOp, typeId, operand);
    }

    addDecoration(id, noContraction);
    return builder.setPrecision(id, precision);
}

// Create a unary operation on a matrix
spv::Id TGlslangToSpvTraverser::createUnaryMatrixOperation(spv::Op op, spv::Decoration precision, spv::Decoration noContraction, spv::Id typeId, spv::Id operand, glslang::TBasicType /* typeProxy */)
{
    // Handle unary operations vector by vector.
    // The result type is the same type as the original type.
    // The algorithm is to:
    //   - break the matrix into vectors
    //   - apply the operation to each vector
    //   - make a matrix out the vector results

    // get the types sorted out
    int numCols = builder.getNumColumns(operand);
    int numRows = builder.getNumRows(operand);
    spv::Id scalarType = builder.getScalarTypeId(typeId);
    spv::Id vecType = builder.makeVectorType(scalarType, numRows);
    std::vector<spv::Id> results;

    // do each vector op
    for (int c = 0; c < numCols; ++c) {
        std::vector<unsigned int> indexes;
        indexes.push_back(c);
        spv::Id vec =  builder.createCompositeExtract(operand, vecType, indexes);
        spv::Id vec_result = builder.createUnaryOp(op, vecType, vec);
        addDecoration(vec_result, noContraction);
        results.push_back(builder.setPrecision(vec_result, precision));
    }

    // put the pieces together
    return builder.setPrecision(builder.createCompositeConstruct(typeId, results), precision);
}

spv::Id TGlslangToSpvTraverser::createConversion(glslang::TOperator op, spv::Decoration precision, spv::Id destType, spv::Id operand)
{
    spv::Op convOp = spv::OpNop;
    spv::Id zero = 0;
    spv::Id one = 0;
    spv::Id type = 0;

    int vectorSize = builder.isVectorType(destType) ? builder.getNumTypeComponents(destType) : 0;

    switch (op) {
    case glslang::EOpConvIntToBool:
    case glslang::EOpConvUintToBool:
    case glslang::EOpConvInt64ToBool:
    case glslang::EOpConvUint64ToBool:
        zero = (op == glslang::EOpConvInt64ToBool ||
                op == glslang::EOpConvUint64ToBool) ? builder.makeUint64Constant(0) : builder.makeUintConstant(0);
        zero = makeSmearedConstant(zero, vectorSize);
        return builder.createBinOp(spv::OpINotEqual, destType, operand, zero);

    case glslang::EOpConvFloatToBool:
        zero = builder.makeFloatConstant(0.0F);
        zero = makeSmearedConstant(zero, vectorSize);
        return builder.createBinOp(spv::OpFOrdNotEqual, destType, operand, zero);

    case glslang::EOpConvDoubleToBool:
        zero = builder.makeDoubleConstant(0.0);
        zero = makeSmearedConstant(zero, vectorSize);
        return builder.createBinOp(spv::OpFOrdNotEqual, destType, operand, zero);

    case glslang::EOpConvBoolToFloat:
        convOp = spv::OpSelect;
        zero = builder.makeFloatConstant(0.0);
        one  = builder.makeFloatConstant(1.0);
        break;
    case glslang::EOpConvBoolToDouble:
        convOp = spv::OpSelect;
        zero = builder.makeDoubleConstant(0.0);
        one  = builder.makeDoubleConstant(1.0);
        break;
    case glslang::EOpConvBoolToInt:
    case glslang::EOpConvBoolToInt64:
        zero = (op == glslang::EOpConvBoolToInt64) ? builder.makeInt64Constant(0) : builder.makeIntConstant(0);
        one  = (op == glslang::EOpConvBoolToInt64) ? builder.makeInt64Constant(1) : builder.makeIntConstant(1);
        convOp = spv::OpSelect;
        break;
    case glslang::EOpConvBoolToUint:
    case glslang::EOpConvBoolToUint64:
        zero = (op == glslang::EOpConvBoolToUint64) ? builder.makeUint64Constant(0) : builder.makeUintConstant(0);
        one  = (op == glslang::EOpConvBoolToUint64) ? builder.makeUint64Constant(1) : builder.makeUintConstant(1);
        convOp = spv::OpSelect;
        break;

    case glslang::EOpConvIntToFloat:
    case glslang::EOpConvIntToDouble:
    case glslang::EOpConvInt64ToFloat:
    case glslang::EOpConvInt64ToDouble:
        convOp = spv::OpConvertSToF;
        break;

    case glslang::EOpConvUintToFloat:
    case glslang::EOpConvUintToDouble:
    case glslang::EOpConvUint64ToFloat:
    case glslang::EOpConvUint64ToDouble:
        convOp = spv::OpConvertUToF;
        break;

    case glslang::EOpConvDoubleToFloat:
    case glslang::EOpConvFloatToDouble:
        convOp = spv::OpFConvert;
        break;

    case glslang::EOpConvFloatToInt:
    case glslang::EOpConvDoubleToInt:
    case glslang::EOpConvFloatToInt64:
    case glslang::EOpConvDoubleToInt64:
        convOp = spv::OpConvertFToS;
        break;

    case glslang::EOpConvUintToInt:
    case glslang::EOpConvIntToUint:
    case glslang::EOpConvUint64ToInt64:
    case glslang::EOpConvInt64ToUint64:
        if (builder.isInSpecConstCodeGenMode()) {
            // Build zero scalar or vector for OpIAdd.
            zero = (op == glslang::EOpConvUintToInt64 ||
                    op == glslang::EOpConvIntToUint64) ? builder.makeUint64Constant(0) : builder.makeUintConstant(0);
            zero = makeSmearedConstant(zero, vectorSize);
            // Use OpIAdd, instead of OpBitcast to do the conversion when
            // generating for OpSpecConstantOp instruction.
            return builder.createBinOp(spv::OpIAdd, destType, operand, zero);
        }
        // For normal run-time conversion instruction, use OpBitcast.
        convOp = spv::OpBitcast;
        break;

    case glslang::EOpConvFloatToUint:
    case glslang::EOpConvDoubleToUint:
    case glslang::EOpConvFloatToUint64:
    case glslang::EOpConvDoubleToUint64:
        convOp = spv::OpConvertFToU;
        break;

    case glslang::EOpConvIntToInt64:
    case glslang::EOpConvInt64ToInt:
        convOp = spv::OpSConvert;
        break;

    case glslang::EOpConvUintToUint64:
    case glslang::EOpConvUint64ToUint:
        convOp = spv::OpUConvert;
        break;

    case glslang::EOpConvIntToUint64:
    case glslang::EOpConvInt64ToUint:
    case glslang::EOpConvUint64ToInt:
    case glslang::EOpConvUintToInt64:
        // OpSConvert/OpUConvert + OpBitCast
        switch (op) {
        case glslang::EOpConvIntToUint64:
            convOp = spv::OpSConvert;
            type   = builder.makeIntType(64);
            break;
        case glslang::EOpConvInt64ToUint:
            convOp = spv::OpSConvert;
            type   = builder.makeIntType(32);
            break;
        case glslang::EOpConvUint64ToInt:
            convOp = spv::OpUConvert;
            type   = builder.makeUintType(32);
            break;
        case glslang::EOpConvUintToInt64:
            convOp = spv::OpUConvert;
            type   = builder.makeUintType(64);
            break;
        default:
            assert(0);
            break;
        }

        if (vectorSize > 0)
            type = builder.makeVectorType(type, vectorSize);

        operand = builder.createUnaryOp(convOp, type, operand);

        if (builder.isInSpecConstCodeGenMode()) {
            // Build zero scalar or vector for OpIAdd.
            zero = (op == glslang::EOpConvIntToUint64 ||
                    op == glslang::EOpConvUintToInt64) ? builder.makeUint64Constant(0) : builder.makeUintConstant(0);
            zero = makeSmearedConstant(zero, vectorSize);
            // Use OpIAdd, instead of OpBitcast to do the conversion when
            // generating for OpSpecConstantOp instruction.
            return builder.createBinOp(spv::OpIAdd, destType, operand, zero);
        }
        // For normal run-time conversion instruction, use OpBitcast.
        convOp = spv::OpBitcast;
        break;
    default:
        break;
    }

    spv::Id result = 0;
    if (convOp == spv::OpNop)
        return result;

    if (convOp == spv::OpSelect) {
        zero = makeSmearedConstant(zero, vectorSize);
        one  = makeSmearedConstant(one, vectorSize);
        result = builder.createTriOp(convOp, destType, operand, one, zero);
    } else
        result = builder.createUnaryOp(convOp, destType, operand);

    return builder.setPrecision(result, precision);
}

spv::Id TGlslangToSpvTraverser::makeSmearedConstant(spv::Id constant, int vectorSize)
{
    if (vectorSize == 0)
        return constant;

    spv::Id vectorTypeId = builder.makeVectorType(builder.getTypeId(constant), vectorSize);
    std::vector<spv::Id> components;
    for (int c = 0; c < vectorSize; ++c)
        components.push_back(constant);
    return builder.makeCompositeConstant(vectorTypeId, components);
}

// For glslang ops that map to SPV atomic opCodes
spv::Id TGlslangToSpvTraverser::createAtomicOperation(glslang::TOperator op, spv::Decoration /*precision*/, spv::Id typeId, std::vector<spv::Id>& operands, glslang::TBasicType typeProxy)
{
    spv::Op opCode = spv::OpNop;

    switch (op) {
    case glslang::EOpAtomicAdd:
    case glslang::EOpImageAtomicAdd:
        opCode = spv::OpAtomicIAdd;
        break;
    case glslang::EOpAtomicMin:
    case glslang::EOpImageAtomicMin:
        opCode = typeProxy == glslang::EbtUint ? spv::OpAtomicUMin : spv::OpAtomicSMin;
        break;
    case glslang::EOpAtomicMax:
    case glslang::EOpImageAtomicMax:
        opCode = typeProxy == glslang::EbtUint ? spv::OpAtomicUMax : spv::OpAtomicSMax;
        break;
    case glslang::EOpAtomicAnd:
    case glslang::EOpImageAtomicAnd:
        opCode = spv::OpAtomicAnd;
        break;
    case glslang::EOpAtomicOr:
    case glslang::EOpImageAtomicOr:
        opCode = spv::OpAtomicOr;
        break;
    case glslang::EOpAtomicXor:
    case glslang::EOpImageAtomicXor:
        opCode = spv::OpAtomicXor;
        break;
    case glslang::EOpAtomicExchange:
    case glslang::EOpImageAtomicExchange:
        opCode = spv::OpAtomicExchange;
        break;
    case glslang::EOpAtomicCompSwap:
    case glslang::EOpImageAtomicCompSwap:
        opCode = spv::OpAtomicCompareExchange;
        break;
    case glslang::EOpAtomicCounterIncrement:
        opCode = spv::OpAtomicIIncrement;
        break;
    case glslang::EOpAtomicCounterDecrement:
        opCode = spv::OpAtomicIDecrement;
        break;
    case glslang::EOpAtomicCounter:
        opCode = spv::OpAtomicLoad;
        break;
    default:
        assert(0);
        break;
    }

    // Sort out the operands
    //  - mapping from glslang -> SPV
    //  - there are extra SPV operands with no glslang source
    //  - compare-exchange swaps the value and comparator
    //  - compare-exchange has an extra memory semantics
    std::vector<spv::Id> spvAtomicOperands;  // hold the spv operands
    auto opIt = operands.begin();            // walk the glslang operands
    spvAtomicOperands.push_back(*(opIt++));
    spvAtomicOperands.push_back(builder.makeUintConstant(spv::ScopeDevice));     // TBD: what is the correct scope?
    spvAtomicOperands.push_back(builder.makeUintConstant(spv::MemorySemanticsMaskNone)); // TBD: what are the correct memory semantics?
    if (opCode == spv::OpAtomicCompareExchange) {
        // There are 2 memory semantics for compare-exchange. And the operand order of "comparator" and "new value" in GLSL
        // differs from that in SPIR-V. Hence, special processing is required.
        spvAtomicOperands.push_back(builder.makeUintConstant(spv::MemorySemanticsMaskNone));
        spvAtomicOperands.push_back(*(opIt + 1));
        spvAtomicOperands.push_back(*opIt);
        opIt += 2;
    }

    // Add the rest of the operands, skipping any that were dealt with above.
    for (; opIt != operands.end(); ++opIt)
        spvAtomicOperands.push_back(*opIt);

    return builder.createOp(opCode, typeId, spvAtomicOperands);
}

// Create group invocation operations.
spv::Id TGlslangToSpvTraverser::createInvocationsOperation(glslang::TOperator op, spv::Id typeId, spv::Id operand)
{
    builder.addCapability(spv::CapabilityGroups);

    std::vector<spv::Id> operands;
    operands.push_back(builder.makeUintConstant(spv::ScopeSubgroup));
    operands.push_back(operand);

    switch (op) {
    case glslang::EOpAnyInvocation:
    case glslang::EOpAllInvocations:
        return builder.createOp(op == glslang::EOpAnyInvocation ? spv::OpGroupAny : spv::OpGroupAll, typeId, operands);

    case glslang::EOpAllInvocationsEqual:
    {
        spv::Id groupAll = builder.createOp(spv::OpGroupAll, typeId, operands);
        spv::Id groupAny = builder.createOp(spv::OpGroupAny, typeId, operands);

        return builder.createBinOp(spv::OpLogicalOr, typeId, groupAll,
                                   builder.createUnaryOp(spv::OpLogicalNot, typeId, groupAny));
    }
    default:
        logger->missingFunctionality("invocation operation");
        return spv::NoResult;
    }
}

spv::Id TGlslangToSpvTraverser::createMiscOperation(glslang::TOperator op, spv::Decoration precision, spv::Id typeId, std::vector<spv::Id>& operands, glslang::TBasicType typeProxy)
{
    bool isUnsigned = typeProxy == glslang::EbtUint || typeProxy == glslang::EbtUint64;
    bool isFloat = typeProxy == glslang::EbtFloat || typeProxy == glslang::EbtDouble;

    spv::Op opCode = spv::OpNop;
    int libCall = -1;
    size_t consumedOperands = operands.size();
    spv::Id typeId0 = 0;
    if (consumedOperands > 0)
        typeId0 = builder.getTypeId(operands[0]);
    spv::Id frexpIntType = 0;

    switch (op) {
    case glslang::EOpMin:
        if (isFloat)
            libCall = spv::GLSLstd450FMin;
        else if (isUnsigned)
            libCall = spv::GLSLstd450UMin;
        else
            libCall = spv::GLSLstd450SMin;
        builder.promoteScalar(precision, operands.front(), operands.back());
        break;
    case glslang::EOpModf:
        libCall = spv::GLSLstd450Modf;
        break;
    case glslang::EOpMax:
        if (isFloat)
            libCall = spv::GLSLstd450FMax;
        else if (isUnsigned)
            libCall = spv::GLSLstd450UMax;
        else
            libCall = spv::GLSLstd450SMax;
        builder.promoteScalar(precision, operands.front(), operands.back());
        break;
    case glslang::EOpPow:
        libCall = spv::GLSLstd450Pow;
        break;
    case glslang::EOpDot:
        opCode = spv::OpDot;
        break;
    case glslang::EOpAtan:
        libCall = spv::GLSLstd450Atan2;
        break;

    case glslang::EOpClamp:
        if (isFloat)
            libCall = spv::GLSLstd450FClamp;
        else if (isUnsigned)
            libCall = spv::GLSLstd450UClamp;
        else
            libCall = spv::GLSLstd450SClamp;
        builder.promoteScalar(precision, operands.front(), operands[1]);
        builder.promoteScalar(precision, operands.front(), operands[2]);
        break;
    case glslang::EOpMix:
        if (! builder.isBoolType(builder.getScalarTypeId(builder.getTypeId(operands.back())))) {
            assert(isFloat);
            libCall = spv::GLSLstd450FMix;
        } else {
            opCode = spv::OpSelect;
            std::swap(operands.front(), operands.back());
        }
        builder.promoteScalar(precision, operands.front(), operands.back());
        break;
    case glslang::EOpStep:
        libCall = spv::GLSLstd450Step;
        builder.promoteScalar(precision, operands.front(), operands.back());
        break;
    case glslang::EOpSmoothStep:
        libCall = spv::GLSLstd450SmoothStep;
        builder.promoteScalar(precision, operands[0], operands[2]);
        builder.promoteScalar(precision, operands[1], operands[2]);
        break;

    case glslang::EOpDistance:
        libCall = spv::GLSLstd450Distance;
        break;
    case glslang::EOpCross:
        libCall = spv::GLSLstd450Cross;
        break;
    case glslang::EOpFaceForward:
        libCall = spv::GLSLstd450FaceForward;
        break;
    case glslang::EOpReflect:
        libCall = spv::GLSLstd450Reflect;
        break;
    case glslang::EOpRefract:
        libCall = spv::GLSLstd450Refract;
        break;
    case glslang::EOpInterpolateAtSample:
        builder.addCapability(spv::CapabilityInterpolationFunction);
        libCall = spv::GLSLstd450InterpolateAtSample;
        break;
    case glslang::EOpInterpolateAtOffset:
        builder.addCapability(spv::CapabilityInterpolationFunction);
        libCall = spv::GLSLstd450InterpolateAtOffset;
        break;
    case glslang::EOpAddCarry:
        opCode = spv::OpIAddCarry;
        typeId = builder.makeStructResultType(typeId0, typeId0);
        consumedOperands = 2;
        break;
    case glslang::EOpSubBorrow:
        opCode = spv::OpISubBorrow;
        typeId = builder.makeStructResultType(typeId0, typeId0);
        consumedOperands = 2;
        break;
    case glslang::EOpUMulExtended:
        opCode = spv::OpUMulExtended;
        typeId = builder.makeStructResultType(typeId0, typeId0);
        consumedOperands = 2;
        break;
    case glslang::EOpIMulExtended:
        opCode = spv::OpSMulExtended;
        typeId = builder.makeStructResultType(typeId0, typeId0);
        consumedOperands = 2;
        break;
    case glslang::EOpBitfieldExtract:
        if (isUnsigned)
            opCode = spv::OpBitFieldUExtract;
        else
            opCode = spv::OpBitFieldSExtract;
        break;
    case glslang::EOpBitfieldInsert:
        opCode = spv::OpBitFieldInsert;
        break;

    case glslang::EOpFma:
        libCall = spv::GLSLstd450Fma;
        break;
    case glslang::EOpFrexp:
        libCall = spv::GLSLstd450FrexpStruct;
        if (builder.getNumComponents(operands[0]) == 1)
            frexpIntType = builder.makeIntegerType(32, true);
        else
            frexpIntType = builder.makeVectorType(builder.makeIntegerType(32, true), builder.getNumComponents(operands[0]));
        typeId = builder.makeStructResultType(typeId0, frexpIntType);
        consumedOperands = 1;
        break;
    case glslang::EOpLdexp:
        libCall = spv::GLSLstd450Ldexp;
        break;

    case glslang::EOpReadInvocation:
        logger->missingFunctionality("shader ballot");
        libCall = spv::GLSLstd450Bad;
        break;

    default:
        return 0;
    }

    spv::Id id = 0;
    if (libCall >= 0) {
        // Use an extended instruction from the standard library.
        // Construct the call arguments, without modifying the original operands vector.
        // We might need the remaining arguments, e.g. in the EOpFrexp case.
        std::vector<spv::Id> callArguments(operands.begin(), operands.begin() + consumedOperands);
        id = builder.createBuiltinCall(typeId, stdBuiltins, libCall, callArguments);
    } else {
        switch (consumedOperands) {
        case 0:
            // should all be handled by visitAggregate and createNoArgOperation
            assert(0);
            return 0;
        case 1:
            // should all be handled by createUnaryOperation
            assert(0);
            return 0;
        case 2:
            id = builder.createBinOp(opCode, typeId, operands[0], operands[1]);
            break;
        default:
            // anything 3 or over doesn't have l-value operands, so all should be consumed
            assert(consumedOperands == operands.size());
            id = builder.createOp(opCode, typeId, operands);
            break;
        }
    }

    // Decode the return types that were structures
    switch (op) {
    case glslang::EOpAddCarry:
    case glslang::EOpSubBorrow:
        builder.createStore(builder.createCompositeExtract(id, typeId0, 1), operands[2]);
        id = builder.createCompositeExtract(id, typeId0, 0);
        break;
    case glslang::EOpUMulExtended:
    case glslang::EOpIMulExtended:
        builder.createStore(builder.createCompositeExtract(id, typeId0, 0), operands[3]);
        builder.createStore(builder.createCompositeExtract(id, typeId0, 1), operands[2]);
        break;
    case glslang::EOpFrexp:
        assert(operands.size() == 2);
        builder.createStore(builder.createCompositeExtract(id, frexpIntType, 1), operands[1]);
        id = builder.createCompositeExtract(id, typeId0, 0);
        break;
    default:
        break;
    }

    return builder.setPrecision(id, precision);
}

// Intrinsics with no arguments, no return value, and no precision.
spv::Id TGlslangToSpvTraverser::createNoArgOperation(glslang::TOperator op)
{
    // TODO: get the barrier operands correct

    switch (op) {
    case glslang::EOpEmitVertex:
        builder.createNoResultOp(spv::OpEmitVertex);
        return 0;
    case glslang::EOpEndPrimitive:
        builder.createNoResultOp(spv::OpEndPrimitive);
        return 0;
    case glslang::EOpBarrier:
        builder.createMemoryBarrier(spv::ScopeDevice, spv::MemorySemanticsAllMemory);
        builder.createControlBarrier(spv::ScopeDevice, spv::ScopeDevice, spv::MemorySemanticsMaskNone);
        return 0;
    case glslang::EOpMemoryBarrier:
        builder.createMemoryBarrier(spv::ScopeDevice, spv::MemorySemanticsAllMemory);
        return 0;
    case glslang::EOpMemoryBarrierAtomicCounter:
        builder.createMemoryBarrier(spv::ScopeDevice, spv::MemorySemanticsAtomicCounterMemoryMask);
        return 0;
    case glslang::EOpMemoryBarrierBuffer:
        builder.createMemoryBarrier(spv::ScopeDevice, spv::MemorySemanticsUniformMemoryMask);
        return 0;
    case glslang::EOpMemoryBarrierImage:
        builder.createMemoryBarrier(spv::ScopeDevice, spv::MemorySemanticsImageMemoryMask);
        return 0;
    case glslang::EOpMemoryBarrierShared:
        builder.createMemoryBarrier(spv::ScopeDevice, spv::MemorySemanticsWorkgroupMemoryMask);
        return 0;
    case glslang::EOpGroupMemoryBarrier:
        builder.createMemoryBarrier(spv::ScopeDevice, spv::MemorySemanticsCrossWorkgroupMemoryMask);
        return 0;
    default:
        logger->missingFunctionality("unknown operation with no arguments");
        return 0;
    }
}

spv::Id TGlslangToSpvTraverser::getSymbolId(const glslang::TIntermSymbol* symbol)
{
    auto iter = symbolValues.find(symbol->getId());
    spv::Id id;
    if (symbolValues.end() != iter) {
        id = iter->second;
        return id;
    }

    // it was not found, create it
    id = createSpvVariable(symbol);
    symbolValues[symbol->getId()] = id;

    if (! symbol->getType().isStruct()) {
        addDecoration(id, TranslatePrecisionDecoration(symbol->getType()));
        addDecoration(id, TranslateInterpolationDecoration(symbol->getType().getQualifier()));
        if (symbol->getType().getQualifier().hasSpecConstantId())
            addDecoration(id, spv::DecorationSpecId, symbol->getType().getQualifier().layoutSpecConstantId);
        if (symbol->getQualifier().hasLocation())
            builder.addDecoration(id, spv::DecorationLocation, symbol->getQualifier().layoutLocation);
        if (symbol->getQualifier().hasIndex())
            builder.addDecoration(id, spv::DecorationIndex, symbol->getQualifier().layoutIndex);
        if (symbol->getQualifier().hasComponent())
            builder.addDecoration(id, spv::DecorationComponent, symbol->getQualifier().layoutComponent);
        if (glslangIntermediate->getXfbMode()) {
            builder.addCapability(spv::CapabilityTransformFeedback);
            if (symbol->getQualifier().hasXfbStride())
                builder.addDecoration(id, spv::DecorationXfbStride, symbol->getQualifier().layoutXfbStride);
            if (symbol->getQualifier().hasXfbBuffer())
                builder.addDecoration(id, spv::DecorationXfbBuffer, symbol->getQualifier().layoutXfbBuffer);
            if (symbol->getQualifier().hasXfbOffset())
                builder.addDecoration(id, spv::DecorationOffset, symbol->getQualifier().layoutXfbOffset);
        }
    }

    addDecoration(id, TranslateInvariantDecoration(symbol->getType().getQualifier()));
    if (symbol->getQualifier().hasStream() && glslangIntermediate->isMultiStream()) {
        builder.addCapability(spv::CapabilityGeometryStreams);
        builder.addDecoration(id, spv::DecorationStream, symbol->getQualifier().layoutStream);
    }
    if (symbol->getQualifier().hasSet())
        builder.addDecoration(id, spv::DecorationDescriptorSet, symbol->getQualifier().layoutSet);
    else if (IsDescriptorResource(symbol->getType())) {
        // default to 0
        builder.addDecoration(id, spv::DecorationDescriptorSet, 0);
    }
    if (symbol->getQualifier().hasBinding())
        builder.addDecoration(id, spv::DecorationBinding, symbol->getQualifier().layoutBinding);
    if (symbol->getQualifier().hasAttachment())
        builder.addDecoration(id, spv::DecorationInputAttachmentIndex, symbol->getQualifier().layoutAttachment);
    if (glslangIntermediate->getXfbMode()) {
        builder.addCapability(spv::CapabilityTransformFeedback);
        if (symbol->getQualifier().hasXfbStride())
            builder.addDecoration(id, spv::DecorationXfbStride, symbol->getQualifier().layoutXfbStride);
        if (symbol->getQualifier().hasXfbBuffer())
            builder.addDecoration(id, spv::DecorationXfbBuffer, symbol->getQualifier().layoutXfbBuffer);
    }

    if (symbol->getType().isImage()) {
        std::vector<spv::Decoration> memory;
        TranslateMemoryDecoration(symbol->getType().getQualifier(), memory);
        for (unsigned int i = 0; i < memory.size(); ++i)
            addDecoration(id, memory[i]);
    }

    // built-in variable decorations
    spv::BuiltIn builtIn = TranslateBuiltInDecoration(symbol->getQualifier().builtIn);
    if (builtIn != spv::BadValue)
        addDecoration(id, spv::DecorationBuiltIn, (int)builtIn);

    return id;
}

// If 'dec' is valid, add no-operand decoration to an object
void TGlslangToSpvTraverser::addDecoration(spv::Id id, spv::Decoration dec)
{
    if (dec != spv::BadValue)
        builder.addDecoration(id, dec);
}

// If 'dec' is valid, add a one-operand decoration to an object
void TGlslangToSpvTraverser::addDecoration(spv::Id id, spv::Decoration dec, unsigned value)
{
    if (dec != spv::BadValue)
        builder.addDecoration(id, dec, value);
}

// If 'dec' is valid, add a no-operand decoration to a struct member
void TGlslangToSpvTraverser::addMemberDecoration(spv::Id id, int member, spv::Decoration dec)
{
    if (dec != spv::BadValue)
        builder.addMemberDecoration(id, (unsigned)member, dec);
}

// If 'dec' is valid, add a one-operand decoration to a struct member
void TGlslangToSpvTraverser::addMemberDecoration(spv::Id id, int member, spv::Decoration dec, unsigned value)
{
    if (dec != spv::BadValue)
        builder.addMemberDecoration(id, (unsigned)member, dec, value);
}

// Make a full tree of instructions to build a SPIR-V specialization constant,
// or regular constant if possible.
//
// TBD: this is not yet done, nor verified to be the best design, it does do the leaf symbols though
//
// Recursively walk the nodes.  The nodes form a tree whose leaves are
// regular constants, which themselves are trees that createSpvConstant()
// recursively walks.  So, this function walks the "top" of the tree:
//  - emit specialization constant-building instructions for specConstant
//  - when running into a non-spec-constant, switch to createSpvConstant()
spv::Id TGlslangToSpvTraverser::createSpvConstant(const glslang::TIntermTyped& node)
{
    assert(node.getQualifier().isConstant());

    // Handle front-end constants first (non-specialization constants).
    if (! node.getQualifier().specConstant) {
        // hand off to the non-spec-constant path
        assert(node.getAsConstantUnion() != nullptr || node.getAsSymbolNode() != nullptr);
        int nextConst = 0;
        return createSpvConstantFromConstUnionArray(node.getType(), node.getAsConstantUnion() ? node.getAsConstantUnion()->getConstArray() : node.getAsSymbolNode()->getConstArray(),
                                 nextConst, false);
    }

    // We now know we have a specialization constant to build

    // gl_WorkgroupSize is a special case until the front-end handles hierarchical specialization constants,
    // even then, it's specialization ids are handled by special case syntax in GLSL: layout(local_size_x = ...
    if (node.getType().getQualifier().builtIn == glslang::EbvWorkGroupSize) {
        std::vector<spv::Id> dimConstId;
        for (int dim = 0; dim < 3; ++dim) {
            bool specConst = (glslangIntermediate->getLocalSizeSpecId(dim) != glslang::TQualifier::layoutNotSet);
            dimConstId.push_back(builder.makeUintConstant(glslangIntermediate->getLocalSize(dim), specConst));
            if (specConst)
                addDecoration(dimConstId.back(), spv::DecorationSpecId, glslangIntermediate->getLocalSizeSpecId(dim));
        }
        return builder.makeCompositeConstant(builder.makeVectorType(builder.makeUintType(32), 3), dimConstId, true);
    }

    // An AST node labelled as specialization constant should be a symbol node.
    // Its initializer should either be a sub tree with constant nodes, or a constant union array.
    if (auto* sn = node.getAsSymbolNode()) {
        if (auto* sub_tree = sn->getConstSubtree()) {
            // Traverse the constant constructor sub tree like generating normal run-time instructions.
            // During the AST traversal, if the node is marked as 'specConstant', SpecConstantOpModeGuard
            // will set the builder into spec constant op instruction generating mode.
            sub_tree->traverse(this);
            return accessChainLoad(sub_tree->getType());
        } else if (auto* const_union_array = &sn->getConstArray()){
            int nextConst = 0;
            return createSpvConstantFromConstUnionArray(sn->getType(), *const_union_array, nextConst, true);
        }
    }

    // Neither a front-end constant node, nor a specialization constant node with constant union array or
    // constant sub tree as initializer.
    logger->missingFunctionality("Neither a front-end constant nor a spec constant.");
    exit(1);
    return spv::NoResult;
}

// Use 'consts' as the flattened glslang source of scalar constants to recursively
// build the aggregate SPIR-V constant.
//
// If there are not enough elements present in 'consts', 0 will be substituted;
// an empty 'consts' can be used to create a fully zeroed SPIR-V constant.
//
spv::Id TGlslangToSpvTraverser::createSpvConstantFromConstUnionArray(const glslang::TType& glslangType, const glslang::TConstUnionArray& consts, int& nextConst, bool specConstant)
{
    // vector of constants for SPIR-V
    std::vector<spv::Id> spvConsts;

    // Type is used for struct and array constants
    spv::Id typeId = convertGlslangToSpvType(glslangType);

    if (glslangType.isArray()) {
        glslang::TType elementType(glslangType, 0);
        for (int i = 0; i < glslangType.getOuterArraySize(); ++i)
            spvConsts.push_back(createSpvConstantFromConstUnionArray(elementType, consts, nextConst, false));
    } else if (glslangType.isMatrix()) {
        glslang::TType vectorType(glslangType, 0);
        for (int col = 0; col < glslangType.getMatrixCols(); ++col)
            spvConsts.push_back(createSpvConstantFromConstUnionArray(vectorType, consts, nextConst, false));
    } else if (glslangType.getStruct()) {
        glslang::TVector<glslang::TTypeLoc>::const_iterator iter;
        for (iter = glslangType.getStruct()->begin(); iter != glslangType.getStruct()->end(); ++iter)
            spvConsts.push_back(createSpvConstantFromConstUnionArray(*iter->type, consts, nextConst, false));
    } else if (glslangType.isVector()) {
        for (unsigned int i = 0; i < (unsigned int)glslangType.getVectorSize(); ++i) {
            bool zero = nextConst >= consts.size();
            switch (glslangType.getBasicType()) {
            case glslang::EbtInt:
                spvConsts.push_back(builder.makeIntConstant(zero ? 0 : consts[nextConst].getIConst()));
                break;
            case glslang::EbtUint:
                spvConsts.push_back(builder.makeUintConstant(zero ? 0 : consts[nextConst].getUConst()));
                break;
            case glslang::EbtInt64:
                spvConsts.push_back(builder.makeInt64Constant(zero ? 0 : consts[nextConst].getI64Const()));
                break;
            case glslang::EbtUint64:
                spvConsts.push_back(builder.makeUint64Constant(zero ? 0 : consts[nextConst].getU64Const()));
                break;
            case glslang::EbtFloat:
                spvConsts.push_back(builder.makeFloatConstant(zero ? 0.0F : (float)consts[nextConst].getDConst()));
                break;
            case glslang::EbtDouble:
                spvConsts.push_back(builder.makeDoubleConstant(zero ? 0.0 : consts[nextConst].getDConst()));
                break;
            case glslang::EbtBool:
                spvConsts.push_back(builder.makeBoolConstant(zero ? false : consts[nextConst].getBConst()));
                break;
            default:
                assert(0);
                break;
            }
            ++nextConst;
        }
    } else {
        // we have a non-aggregate (scalar) constant
        bool zero = nextConst >= consts.size();
        spv::Id scalar = 0;
        switch (glslangType.getBasicType()) {
        case glslang::EbtInt:
            scalar = builder.makeIntConstant(zero ? 0 : consts[nextConst].getIConst(), specConstant);
            break;
        case glslang::EbtUint:
            scalar = builder.makeUintConstant(zero ? 0 : consts[nextConst].getUConst(), specConstant);
            break;
        case glslang::EbtInt64:
            scalar = builder.makeInt64Constant(zero ? 0 : consts[nextConst].getI64Const(), specConstant);
            break;
        case glslang::EbtUint64:
            scalar = builder.makeUint64Constant(zero ? 0 : consts[nextConst].getU64Const(), specConstant);
            break;
        case glslang::EbtFloat:
            scalar = builder.makeFloatConstant(zero ? 0.0F : (float)consts[nextConst].getDConst(), specConstant);
            break;
        case glslang::EbtDouble:
            scalar = builder.makeDoubleConstant(zero ? 0.0 : consts[nextConst].getDConst(), specConstant);
            break;
        case glslang::EbtBool:
            scalar = builder.makeBoolConstant(zero ? false : consts[nextConst].getBConst(), specConstant);
            break;
        default:
            assert(0);
            break;
        }
        ++nextConst;
        return scalar;
    }

    return builder.makeCompositeConstant(typeId, spvConsts);
}

// Return true if the node is a constant or symbol whose reading has no
// non-trivial observable cost or effect.
bool TGlslangToSpvTraverser::isTrivialLeaf(const glslang::TIntermTyped* node)
{
    // don't know what this is
    if (node == nullptr)
        return false;

    // a constant is safe
    if (node->getAsConstantUnion() != nullptr)
        return true;

    // not a symbol means non-trivial
    if (node->getAsSymbolNode() == nullptr)
        return false;

    // a symbol, depends on what's being read
    switch (node->getType().getQualifier().storage) {
    case glslang::EvqTemporary:
    case glslang::EvqGlobal:
    case glslang::EvqIn:
    case glslang::EvqInOut:
    case glslang::EvqConst:
    case glslang::EvqConstReadOnly:
    case glslang::EvqUniform:
        return true;
    default:
        return false;
    }
}

// A node is trivial if it is a single operation with no side effects.
// Error on the side of saying non-trivial.
// Return true if trivial.
bool TGlslangToSpvTraverser::isTrivial(const glslang::TIntermTyped* node)
{
    if (node == nullptr)
        return false;

    // symbols and constants are trivial
    if (isTrivialLeaf(node))
        return true;

    // otherwise, it needs to be a simple operation or one or two leaf nodes

    // not a simple operation
    const glslang::TIntermBinary* binaryNode = node->getAsBinaryNode();
    const glslang::TIntermUnary* unaryNode = node->getAsUnaryNode();
    if (binaryNode == nullptr && unaryNode == nullptr)
        return false;

    // not on leaf nodes
    if (binaryNode && (! isTrivialLeaf(binaryNode->getLeft()) || ! isTrivialLeaf(binaryNode->getRight())))
        return false;

    if (unaryNode && ! isTrivialLeaf(unaryNode->getOperand())) {
        return false;
    }

    switch (node->getAsOperator()->getOp()) {
    case glslang::EOpLogicalNot:
    case glslang::EOpConvIntToBool:
    case glslang::EOpConvUintToBool:
    case glslang::EOpConvFloatToBool:
    case glslang::EOpConvDoubleToBool:
    case glslang::EOpEqual:
    case glslang::EOpNotEqual:
    case glslang::EOpLessThan:
    case glslang::EOpGreaterThan:
    case glslang::EOpLessThanEqual:
    case glslang::EOpGreaterThanEqual:
    case glslang::EOpIndexDirect:
    case glslang::EOpIndexDirectStruct:
    case glslang::EOpLogicalXor:
    case glslang::EOpAny:
    case glslang::EOpAll:
        return true;
    default:
        return false;
    }
}

// Emit short-circuiting code, where 'right' is never evaluated unless
// the left side is true (for &&) or false (for ||).
spv::Id TGlslangToSpvTraverser::createShortCircuit(glslang::TOperator op, glslang::TIntermTyped& left, glslang::TIntermTyped& right)
{
    spv::Id boolTypeId = builder.makeBoolType();

    // emit left operand
    builder.clearAccessChain();
    left.traverse(this);
    spv::Id leftId = accessChainLoad(left.getType());

    // Operands to accumulate OpPhi operands
    std::vector<spv::Id> phiOperands;
    // accumulate left operand's phi information
    phiOperands.push_back(leftId);
    phiOperands.push_back(builder.getBuildPoint()->getId());

    // Make the two kinds of operation symmetric with a "!"
    //   || => emit "if (! left) result = right"
    //   && => emit "if (  left) result = right"
    //
    // TODO: this runtime "not" for || could be avoided by adding functionality
    // to 'builder' to have an "else" without an "then"
    if (op == glslang::EOpLogicalOr)
        leftId = builder.createUnaryOp(spv::OpLogicalNot, boolTypeId, leftId);

    // make an "if" based on the left value
    spv::Builder::If ifBuilder(leftId, builder);

    // emit right operand as the "then" part of the "if"
    builder.clearAccessChain();
    right.traverse(this);
    spv::Id rightId = accessChainLoad(right.getType());

    // accumulate left operand's phi information
    phiOperands.push_back(rightId);
    phiOperands.push_back(builder.getBuildPoint()->getId());

    // finish the "if"
    ifBuilder.makeEndIf();

    // phi together the two results
    return builder.createOp(spv::OpPhi, boolTypeId, phiOperands);
}

};  // end anonymous namespace

namespace glslang {

void GetSpirvVersion(std::string& version)
{
    const int bufSize = 100;
    char buf[bufSize];
    snprintf(buf, bufSize, "0x%08x, Revision %d", spv::Version, spv::Revision);
    version = buf;
}

// Write SPIR-V out to a binary file
void OutputSpv(const std::vector<unsigned int>& spirv, const char* baseName)
{
    std::ofstream out;
    out.open(baseName, std::ios::binary | std::ios::out);
    for (int i = 0; i < (int)spirv.size(); ++i) {
        unsigned int word = spirv[i];
        out.write((const char*)&word, 4);
    }
    out.close();
}

//
// Set up the glslang traversal
//
void GlslangToSpv(const glslang::TIntermediate& intermediate, std::vector<unsigned int>& spirv)
{
    spv::SpvBuildLogger logger;
    GlslangToSpv(intermediate, spirv, &logger);
}

void GlslangToSpv(const glslang::TIntermediate& intermediate, std::vector<unsigned int>& spirv, spv::SpvBuildLogger* logger)
{
    TIntermNode* root = intermediate.getTreeRoot();

    if (root == 0)
        return;

    glslang::GetThreadPoolAllocator().push();

    TGlslangToSpvTraverser it(&intermediate, logger);

    root->traverse(&it);

    it.dumpSpv(spirv);

    glslang::GetThreadPoolAllocator().pop();
}

}; // end namespace glslang
