//
// Copyright (C) 2016 Google, Inc.
// Copyright (C) 2016 LunarG, Inc.
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

#include <gtest/gtest.h>

#include "TestFixture.h"

namespace glslangtest {
namespace {

struct FileNameEntryPointPair {
  const char* fileName;
  const char* entryPoint;
};

// We are using FileNameEntryPointPair objects as parameters for instantiating
// the template, so the global FileNameAsCustomTestSuffix() won't work since
// it assumes std::string as parameters. Thus, an overriding one here.
std::string FileNameAsCustomTestSuffix(
    const ::testing::TestParamInfo<FileNameEntryPointPair>& info) {
    std::string name = info.param.fileName;
    // A valid test case suffix cannot have '.' and '-' inside.
    std::replace(name.begin(), name.end(), '.', '_');
    std::replace(name.begin(), name.end(), '-', '_');
    return name;
}

using HlslCompileTest = GlslangTest<::testing::TestWithParam<FileNameEntryPointPair>>;

// Compiling HLSL to SPIR-V under Vulkan semantics. Expected to successfully
// generate both AST and SPIR-V.
TEST_P(HlslCompileTest, FromFile)
{
    loadFileCompileAndCheck(GLSLANG_TEST_DIRECTORY, GetParam().fileName,
                            Source::HLSL, Semantics::Vulkan,
                            Target::BothASTAndSpv, GetParam().entryPoint);
}

// clang-format off
INSTANTIATE_TEST_CASE_P(
    ToSpirv, HlslCompileTest,
    ::testing::ValuesIn(std::vector<FileNameEntryPointPair>{
        {"hlsl.array.frag", "PixelShaderFunction"},
        {"hlsl.assoc.frag", "PixelShaderFunction"},
        {"hlsl.attribute.frag", "PixelShaderFunction"},
        {"hlsl.cast.frag", "PixelShaderFunction"},
        {"hlsl.discard.frag", "PixelShaderFunction"},
        {"hlsl.doLoop.frag", "PixelShaderFunction"},
        {"hlsl.float1.frag", "PixelShaderFunction"},
        {"hlsl.float4.frag", "PixelShaderFunction"},
        {"hlsl.forLoop.frag", "PixelShaderFunction"},
        {"hlsl.if.frag", "PixelShaderFunction"},
        {"hlsl.inoutquals.frag", "main"},
        {"hlsl.init.frag", "ShaderFunction"},
        {"hlsl.intrinsics.barriers.comp", "ComputeShaderFunction"},
        {"hlsl.intrinsics.comp", "ComputeShaderFunction"},
        {"hlsl.intrinsics.evalfns.frag", "main"},
        {"hlsl.intrinsics.double.frag", "PixelShaderFunction"},
        {"hlsl.intrinsics.f1632.frag", "PixelShaderFunction"},
        {"hlsl.intrinsics.frag", "PixelShaderFunction"},
        {"hlsl.intrinsics.lit.frag", "PixelShaderFunction"},
        {"hlsl.intrinsics.negative.comp", "ComputeShaderFunction"},
        {"hlsl.intrinsics.negative.frag", "PixelShaderFunction"},
        {"hlsl.intrinsics.negative.vert", "VertexShaderFunction"},
        {"hlsl.sample.array.dx10.frag", "main"},
        {"hlsl.sample.basic.dx10.frag", "main"},
        {"hlsl.sample.offset.dx10.frag", "main"},
        {"hlsl.sample.offsetarray.dx10.frag", "main"},
        {"hlsl.samplebias.array.dx10.frag", "main"},
        {"hlsl.samplebias.basic.dx10.frag", "main"},
        {"hlsl.samplebias.offset.dx10.frag", "main"},
        {"hlsl.samplebias.offsetarray.dx10.frag", "main"},
        {"hlsl.samplegrad.array.dx10.frag", "main"},
        {"hlsl.samplegrad.basic.dx10.frag", "main"},
        {"hlsl.samplegrad.basic.dx10.vert", "main"},
        {"hlsl.samplegrad.offset.dx10.frag", "main"},
        {"hlsl.samplegrad.offsetarray.dx10.frag", "main"},
        {"hlsl.intrinsics.vert", "VertexShaderFunction"},
        {"hlsl.matType.frag", "PixelShaderFunction"},
        {"hlsl.max.frag", "PixelShaderFunction"},
        {"hlsl.precedence.frag", "PixelShaderFunction"},
        {"hlsl.precedence2.frag", "PixelShaderFunction"},
        {"hlsl.scope.frag", "PixelShaderFunction"},
        {"hlsl.sin.frag", "PixelShaderFunction"},
        {"hlsl.struct.frag", "PixelShaderFunction"},
        {"hlsl.switch.frag", "PixelShaderFunction"},
        {"hlsl.swizzle.frag", "PixelShaderFunction"},
        {"hlsl.templatetypes.frag", "PixelShaderFunction"},
        {"hlsl.typedef.frag", "PixelShaderFunction"},
        {"hlsl.whileLoop.frag", "PixelShaderFunction"},
        {"hlsl.void.frag", "PixelShaderFunction"},
    }),
    FileNameAsCustomTestSuffix
);
// clang-format on

}  // anonymous namespace
}  // namespace glslangtest
