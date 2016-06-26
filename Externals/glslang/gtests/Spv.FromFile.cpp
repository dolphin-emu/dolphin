//
// Copyright (C) 2016 Google, Inc.
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
//    Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//
//    Redistributions in binary form must reproduce the above
//    copyright notice, this list of conditions and the following
//    disclaimer in the documentation and/or other materials provided
//    with the distribution.
//
//    Neither the name of Google Inc. nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
// FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
// COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
// BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
// ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#include <algorithm>

#include <gtest/gtest.h>

#include "TestFixture.h"

namespace glslangtest {
namespace {

using CompileToSpirvTest = GlslangTest<::testing::TestWithParam<std::string>>;
using VulkanSemantics = GlslangTest<::testing::TestWithParam<std::string>>;

// Compiling GLSL to SPIR-V under Vulkan semantics. Expected to successfully
// generate SPIR-V.
TEST_P(CompileToSpirvTest, FromFile)
{
    loadFileCompileAndCheck(GLSLANG_TEST_DIRECTORY, GetParam(),
                            Semantics::Vulkan, Target::Spirv);
}

// GLSL-level Vulkan semantics test. Expected to error out before generating
// SPIR-V.
TEST_P(VulkanSemantics, FromFile)
{
    loadFileCompileAndCheck(GLSLANG_TEST_DIRECTORY, GetParam(),
                            Semantics::Vulkan, Target::Spirv);
}

// clang-format off
INSTANTIATE_TEST_CASE_P(
    Glsl, CompileToSpirvTest,
    ::testing::ValuesIn(std::vector<std::string>({
        // Test looping constructs.
        // No tests yet for making sure break and continue from a nested loop
        // goes to the innermost target.
        "spv.do-simple.vert",
        "spv.do-while-continue-break.vert",
        "spv.for-complex-condition.vert",
        "spv.for-continue-break.vert",
        "spv.for-simple.vert",
        "spv.for-notest.vert",
        "spv.for-nobody.vert",
        "spv.while-continue-break.vert",
        "spv.while-simple.vert",
        // vulkan-specific tests
        "spv.set.vert",
        "spv.double.comp",
        "spv.100ops.frag",
        "spv.130.frag",
        "spv.140.frag",
        "spv.150.geom",
        "spv.150.vert",
        "spv.300BuiltIns.vert",
        "spv.300layout.frag",
        "spv.300layout.vert",
        "spv.300layoutp.vert",
        "spv.310.comp",
        "spv.330.geom",
        "spv.400.frag",
        "spv.400.tesc",
        "spv.400.tese",
        "spv.420.geom",
        "spv.430.vert",
        "spv.accessChain.frag",
        "spv.aggOps.frag",
        "spv.always-discard.frag",
        "spv.always-discard2.frag",
        "spv.bitCast.frag",
        "spv.bool.vert",
        "spv.boolInBlock.frag",
        "spv.branch-return.vert",
        "spv.conditionalDiscard.frag",
        "spv.conversion.frag",
        "spv.dataOut.frag",
        "spv.dataOutIndirect.frag",
        "spv.dataOutIndirect.vert",
        "spv.deepRvalue.frag",
        "spv.depthOut.frag",
        "spv.discard-dce.frag",
        "spv.doWhileLoop.frag",
        "spv.earlyReturnDiscard.frag",
        "spv.flowControl.frag",
        "spv.forLoop.frag",
        "spv.forwardFun.frag",
        "spv.functionCall.frag",
        "spv.functionSemantics.frag",
        "spv.interpOps.frag",
        "spv.int64.frag",
        "spv.layoutNested.vert",
        "spv.length.frag",
        "spv.localAggregates.frag",
        "spv.loops.frag",
        "spv.loopsArtificial.frag",
        "spv.matFun.vert",
        "spv.matrix.frag",
        "spv.matrix2.frag",
        "spv.memoryQualifier.frag",
        "spv.merge-unreachable.frag",
        "spv.newTexture.frag",
        "spv.noDeadDecorations.vert",
        "spv.nonSquare.vert",
        "spv.Operations.frag",
        "spv.intOps.vert",
        "spv.precision.frag",
        "spv.prepost.frag",
        "spv.qualifiers.vert",
        "spv.shaderBallot.comp",
        "spv.shaderGroupVote.comp",
        "spv.shiftOps.frag",
        "spv.simpleFunctionCall.frag",
        "spv.simpleMat.vert",
        "spv.sparseTexture.frag",
        "spv.sparseTextureClamp.frag",
        "spv.structAssignment.frag",
        "spv.structDeref.frag",
        "spv.structure.frag",
        "spv.switch.frag",
        "spv.swizzle.frag",
        "spv.test.frag",
        "spv.test.vert",
        "spv.texture.frag",
        "spv.texture.vert",
        "spv.image.frag",
        "spv.types.frag",
        "spv.uint.frag",
        "spv.uniformArray.frag",
        "spv.variableArrayIndex.frag",
        "spv.varyingArray.frag",
        "spv.varyingArrayIndirect.frag",
        "spv.voidFunction.frag",
        "spv.whileLoop.frag",
        "spv.AofA.frag",
        "spv.queryL.frag",
        "spv.separate.frag",
        "spv.shortCircuit.frag",
        "spv.pushConstant.vert",
        "spv.subpass.frag",
        "spv.specConstant.vert",
        "spv.specConstant.comp",
        "spv.specConstantComposite.vert",
    })),
    FileNameAsCustomTestName
);

INSTANTIATE_TEST_CASE_P(
    Glsl, VulkanSemantics,
    ::testing::ValuesIn(std::vector<std::string>({
        "vulkan.frag",
        "vulkan.vert",
        "vulkan.comp",
    })),
    FileNameAsCustomTestName
);
// clang-format on

}  // anonymous namespace
}  // namespace glslangtest
