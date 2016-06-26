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

#include <sstream>

#include "DefaultResourceLimits.h"

namespace glslang {

const TBuiltInResource DefaultTBuiltInResource = {
    /* .MaxLights = */ 32,
    /* .MaxClipPlanes = */ 6,
    /* .MaxTextureUnits = */ 32,
    /* .MaxTextureCoords = */ 32,
    /* .MaxVertexAttribs = */ 64,
    /* .MaxVertexUniformComponents = */ 4096,
    /* .MaxVaryingFloats = */ 64,
    /* .MaxVertexTextureImageUnits = */ 32,
    /* .MaxCombinedTextureImageUnits = */ 80,
    /* .MaxTextureImageUnits = */ 32,
    /* .MaxFragmentUniformComponents = */ 4096,
    /* .MaxDrawBuffers = */ 32,
    /* .MaxVertexUniformVectors = */ 128,
    /* .MaxVaryingVectors = */ 8,
    /* .MaxFragmentUniformVectors = */ 16,
    /* .MaxVertexOutputVectors = */ 16,
    /* .MaxFragmentInputVectors = */ 15,
    /* .MinProgramTexelOffset = */ -8,
    /* .MaxProgramTexelOffset = */ 7,
    /* .MaxClipDistances = */ 8,
    /* .MaxComputeWorkGroupCountX = */ 65535,
    /* .MaxComputeWorkGroupCountY = */ 65535,
    /* .MaxComputeWorkGroupCountZ = */ 65535,
    /* .MaxComputeWorkGroupSizeX = */ 1024,
    /* .MaxComputeWorkGroupSizeY = */ 1024,
    /* .MaxComputeWorkGroupSizeZ = */ 64,
    /* .MaxComputeUniformComponents = */ 1024,
    /* .MaxComputeTextureImageUnits = */ 16,
    /* .MaxComputeImageUniforms = */ 8,
    /* .MaxComputeAtomicCounters = */ 8,
    /* .MaxComputeAtomicCounterBuffers = */ 1,
    /* .MaxVaryingComponents = */ 60,
    /* .MaxVertexOutputComponents = */ 64,
    /* .MaxGeometryInputComponents = */ 64,
    /* .MaxGeometryOutputComponents = */ 128,
    /* .MaxFragmentInputComponents = */ 128,
    /* .MaxImageUnits = */ 8,
    /* .MaxCombinedImageUnitsAndFragmentOutputs = */ 8,
    /* .MaxCombinedShaderOutputResources = */ 8,
    /* .MaxImageSamples = */ 0,
    /* .MaxVertexImageUniforms = */ 0,
    /* .MaxTessControlImageUniforms = */ 0,
    /* .MaxTessEvaluationImageUniforms = */ 0,
    /* .MaxGeometryImageUniforms = */ 0,
    /* .MaxFragmentImageUniforms = */ 8,
    /* .MaxCombinedImageUniforms = */ 8,
    /* .MaxGeometryTextureImageUnits = */ 16,
    /* .MaxGeometryOutputVertices = */ 256,
    /* .MaxGeometryTotalOutputComponents = */ 1024,
    /* .MaxGeometryUniformComponents = */ 1024,
    /* .MaxGeometryVaryingComponents = */ 64,
    /* .MaxTessControlInputComponents = */ 128,
    /* .MaxTessControlOutputComponents = */ 128,
    /* .MaxTessControlTextureImageUnits = */ 16,
    /* .MaxTessControlUniformComponents = */ 1024,
    /* .MaxTessControlTotalOutputComponents = */ 4096,
    /* .MaxTessEvaluationInputComponents = */ 128,
    /* .MaxTessEvaluationOutputComponents = */ 128,
    /* .MaxTessEvaluationTextureImageUnits = */ 16,
    /* .MaxTessEvaluationUniformComponents = */ 1024,
    /* .MaxTessPatchComponents = */ 120,
    /* .MaxPatchVertices = */ 32,
    /* .MaxTessGenLevel = */ 64,
    /* .MaxViewports = */ 16,
    /* .MaxVertexAtomicCounters = */ 0,
    /* .MaxTessControlAtomicCounters = */ 0,
    /* .MaxTessEvaluationAtomicCounters = */ 0,
    /* .MaxGeometryAtomicCounters = */ 0,
    /* .MaxFragmentAtomicCounters = */ 8,
    /* .MaxCombinedAtomicCounters = */ 8,
    /* .MaxAtomicCounterBindings = */ 1,
    /* .MaxVertexAtomicCounterBuffers = */ 0,
    /* .MaxTessControlAtomicCounterBuffers = */ 0,
    /* .MaxTessEvaluationAtomicCounterBuffers = */ 0,
    /* .MaxGeometryAtomicCounterBuffers = */ 0,
    /* .MaxFragmentAtomicCounterBuffers = */ 1,
    /* .MaxCombinedAtomicCounterBuffers = */ 1,
    /* .MaxAtomicCounterBufferSize = */ 16384,
    /* .MaxTransformFeedbackBuffers = */ 4,
    /* .MaxTransformFeedbackInterleavedComponents = */ 64,
    /* .MaxCullDistances = */ 8,
    /* .MaxCombinedClipAndCullDistances = */ 8,
    /* .MaxSamples = */ 4,
    /* .limits = */ {
        /* .nonInductiveForLoops = */ 1,
        /* .whileLoops = */ 1,
        /* .doWhileLoops = */ 1,
        /* .generalUniformIndexing = */ 1,
        /* .generalAttributeMatrixVectorIndexing = */ 1,
        /* .generalVaryingIndexing = */ 1,
        /* .generalSamplerIndexing = */ 1,
        /* .generalVariableIndexing = */ 1,
        /* .generalConstantMatrixVectorIndexing = */ 1,
    }};

std::string GetDefaultTBuiltInResourceString()
{
    std::ostringstream ostream;

    ostream << "MaxLights "                                 << DefaultTBuiltInResource.maxLights << "\n"
            << "MaxClipPlanes "                             << DefaultTBuiltInResource.maxClipPlanes << "\n"
            << "MaxTextureUnits "                           << DefaultTBuiltInResource.maxTextureUnits << "\n"
            << "MaxTextureCoords "                          << DefaultTBuiltInResource.maxTextureCoords << "\n"
            << "MaxVertexAttribs "                          << DefaultTBuiltInResource.maxVertexAttribs << "\n"
            << "MaxVertexUniformComponents "                << DefaultTBuiltInResource.maxVertexUniformComponents << "\n"
            << "MaxVaryingFloats "                          << DefaultTBuiltInResource.maxVaryingFloats << "\n"
            << "MaxVertexTextureImageUnits "                << DefaultTBuiltInResource.maxVertexTextureImageUnits << "\n"
            << "MaxCombinedTextureImageUnits "              << DefaultTBuiltInResource.maxCombinedTextureImageUnits << "\n"
            << "MaxTextureImageUnits "                      << DefaultTBuiltInResource.maxTextureImageUnits << "\n"
            << "MaxFragmentUniformComponents "              << DefaultTBuiltInResource.maxFragmentUniformComponents << "\n"
            << "MaxDrawBuffers "                            << DefaultTBuiltInResource.maxDrawBuffers << "\n"
            << "MaxVertexUniformVectors "                   << DefaultTBuiltInResource.maxVertexUniformVectors << "\n"
            << "MaxVaryingVectors "                         << DefaultTBuiltInResource.maxVaryingVectors << "\n"
            << "MaxFragmentUniformVectors "                 << DefaultTBuiltInResource.maxFragmentUniformVectors << "\n"
            << "MaxVertexOutputVectors "                    << DefaultTBuiltInResource.maxVertexOutputVectors << "\n"
            << "MaxFragmentInputVectors "                   << DefaultTBuiltInResource.maxFragmentInputVectors << "\n"
            << "MinProgramTexelOffset "                     << DefaultTBuiltInResource.minProgramTexelOffset << "\n"
            << "MaxProgramTexelOffset "                     << DefaultTBuiltInResource.maxProgramTexelOffset << "\n"
            << "MaxClipDistances "                          << DefaultTBuiltInResource.maxClipDistances << "\n"
            << "MaxComputeWorkGroupCountX "                 << DefaultTBuiltInResource.maxComputeWorkGroupCountX << "\n"
            << "MaxComputeWorkGroupCountY "                 << DefaultTBuiltInResource.maxComputeWorkGroupCountY << "\n"
            << "MaxComputeWorkGroupCountZ "                 << DefaultTBuiltInResource.maxComputeWorkGroupCountZ << "\n"
            << "MaxComputeWorkGroupSizeX "                  << DefaultTBuiltInResource.maxComputeWorkGroupSizeX << "\n"
            << "MaxComputeWorkGroupSizeY "                  << DefaultTBuiltInResource.maxComputeWorkGroupSizeY << "\n"
            << "MaxComputeWorkGroupSizeZ "                  << DefaultTBuiltInResource.maxComputeWorkGroupSizeZ << "\n"
            << "MaxComputeUniformComponents "               << DefaultTBuiltInResource.maxComputeUniformComponents << "\n"
            << "MaxComputeTextureImageUnits "               << DefaultTBuiltInResource.maxComputeTextureImageUnits << "\n"
            << "MaxComputeImageUniforms "                   << DefaultTBuiltInResource.maxComputeImageUniforms << "\n"
            << "MaxComputeAtomicCounters "                  << DefaultTBuiltInResource.maxComputeAtomicCounters << "\n"
            << "MaxComputeAtomicCounterBuffers "            << DefaultTBuiltInResource.maxComputeAtomicCounterBuffers << "\n"
            << "MaxVaryingComponents "                      << DefaultTBuiltInResource.maxVaryingComponents << "\n"
            << "MaxVertexOutputComponents "                 << DefaultTBuiltInResource.maxVertexOutputComponents << "\n"
            << "MaxGeometryInputComponents "                << DefaultTBuiltInResource.maxGeometryInputComponents << "\n"
            << "MaxGeometryOutputComponents "               << DefaultTBuiltInResource.maxGeometryOutputComponents << "\n"
            << "MaxFragmentInputComponents "                << DefaultTBuiltInResource.maxFragmentInputComponents << "\n"
            << "MaxImageUnits "                             << DefaultTBuiltInResource.maxImageUnits << "\n"
            << "MaxCombinedImageUnitsAndFragmentOutputs "   << DefaultTBuiltInResource.maxCombinedImageUnitsAndFragmentOutputs << "\n"
            << "MaxCombinedShaderOutputResources "          << DefaultTBuiltInResource.maxCombinedShaderOutputResources << "\n"
            << "MaxImageSamples "                           << DefaultTBuiltInResource.maxImageSamples << "\n"
            << "MaxVertexImageUniforms "                    << DefaultTBuiltInResource.maxVertexImageUniforms << "\n"
            << "MaxTessControlImageUniforms "               << DefaultTBuiltInResource.maxTessControlImageUniforms << "\n"
            << "MaxTessEvaluationImageUniforms "            << DefaultTBuiltInResource.maxTessEvaluationImageUniforms << "\n"
            << "MaxGeometryImageUniforms "                  << DefaultTBuiltInResource.maxGeometryImageUniforms << "\n"
            << "MaxFragmentImageUniforms "                  << DefaultTBuiltInResource.maxFragmentImageUniforms << "\n"
            << "MaxCombinedImageUniforms "                  << DefaultTBuiltInResource.maxCombinedImageUniforms << "\n"
            << "MaxGeometryTextureImageUnits "              << DefaultTBuiltInResource.maxGeometryTextureImageUnits << "\n"
            << "MaxGeometryOutputVertices "                 << DefaultTBuiltInResource.maxGeometryOutputVertices << "\n"
            << "MaxGeometryTotalOutputComponents "          << DefaultTBuiltInResource.maxGeometryTotalOutputComponents << "\n"
            << "MaxGeometryUniformComponents "              << DefaultTBuiltInResource.maxGeometryUniformComponents << "\n"
            << "MaxGeometryVaryingComponents "              << DefaultTBuiltInResource.maxGeometryVaryingComponents << "\n"
            << "MaxTessControlInputComponents "             << DefaultTBuiltInResource.maxTessControlInputComponents << "\n"
            << "MaxTessControlOutputComponents "            << DefaultTBuiltInResource.maxTessControlOutputComponents << "\n"
            << "MaxTessControlTextureImageUnits "           << DefaultTBuiltInResource.maxTessControlTextureImageUnits << "\n"
            << "MaxTessControlUniformComponents "           << DefaultTBuiltInResource.maxTessControlUniformComponents << "\n"
            << "MaxTessControlTotalOutputComponents "       << DefaultTBuiltInResource.maxTessControlTotalOutputComponents << "\n"
            << "MaxTessEvaluationInputComponents "          << DefaultTBuiltInResource.maxTessEvaluationInputComponents << "\n"
            << "MaxTessEvaluationOutputComponents "         << DefaultTBuiltInResource.maxTessEvaluationOutputComponents << "\n"
            << "MaxTessEvaluationTextureImageUnits "        << DefaultTBuiltInResource.maxTessEvaluationTextureImageUnits << "\n"
            << "MaxTessEvaluationUniformComponents "        << DefaultTBuiltInResource.maxTessEvaluationUniformComponents << "\n"
            << "MaxTessPatchComponents "                    << DefaultTBuiltInResource.maxTessPatchComponents << "\n"
            << "MaxPatchVertices "                          << DefaultTBuiltInResource.maxPatchVertices << "\n"
            << "MaxTessGenLevel "                           << DefaultTBuiltInResource.maxTessGenLevel << "\n"
            << "MaxViewports "                              << DefaultTBuiltInResource.maxViewports << "\n"
            << "MaxVertexAtomicCounters "                   << DefaultTBuiltInResource.maxVertexAtomicCounters << "\n"
            << "MaxTessControlAtomicCounters "              << DefaultTBuiltInResource.maxTessControlAtomicCounters << "\n"
            << "MaxTessEvaluationAtomicCounters "           << DefaultTBuiltInResource.maxTessEvaluationAtomicCounters << "\n"
            << "MaxGeometryAtomicCounters "                 << DefaultTBuiltInResource.maxGeometryAtomicCounters << "\n"
            << "MaxFragmentAtomicCounters "                 << DefaultTBuiltInResource.maxFragmentAtomicCounters << "\n"
            << "MaxCombinedAtomicCounters "                 << DefaultTBuiltInResource.maxCombinedAtomicCounters << "\n"
            << "MaxAtomicCounterBindings "                  << DefaultTBuiltInResource.maxAtomicCounterBindings << "\n"
            << "MaxVertexAtomicCounterBuffers "             << DefaultTBuiltInResource.maxVertexAtomicCounterBuffers << "\n"
            << "MaxTessControlAtomicCounterBuffers "        << DefaultTBuiltInResource.maxTessControlAtomicCounterBuffers << "\n"
            << "MaxTessEvaluationAtomicCounterBuffers "     << DefaultTBuiltInResource.maxTessEvaluationAtomicCounterBuffers << "\n"
            << "MaxGeometryAtomicCounterBuffers "           << DefaultTBuiltInResource.maxGeometryAtomicCounterBuffers << "\n"
            << "MaxFragmentAtomicCounterBuffers "           << DefaultTBuiltInResource.maxFragmentAtomicCounterBuffers << "\n"
            << "MaxCombinedAtomicCounterBuffers "           << DefaultTBuiltInResource.maxCombinedAtomicCounterBuffers << "\n"
            << "MaxAtomicCounterBufferSize "                << DefaultTBuiltInResource.maxAtomicCounterBufferSize << "\n"
            << "MaxTransformFeedbackBuffers "               << DefaultTBuiltInResource.maxTransformFeedbackBuffers << "\n"
            << "MaxTransformFeedbackInterleavedComponents " << DefaultTBuiltInResource.maxTransformFeedbackInterleavedComponents << "\n"
            << "MaxCullDistances "                          << DefaultTBuiltInResource.maxCullDistances << "\n"
            << "MaxCombinedClipAndCullDistances "           << DefaultTBuiltInResource.maxCombinedClipAndCullDistances << "\n"
            << "MaxSamples "                                << DefaultTBuiltInResource.maxSamples << "\n"

            << "nonInductiveForLoops "                      << DefaultTBuiltInResource.limits.nonInductiveForLoops << "\n"
            << "whileLoops "                                << DefaultTBuiltInResource.limits.whileLoops << "\n"
            << "doWhileLoops "                              << DefaultTBuiltInResource.limits.doWhileLoops << "\n"
            << "generalUniformIndexing "                    << DefaultTBuiltInResource.limits.generalUniformIndexing << "\n"
            << "generalAttributeMatrixVectorIndexing "      << DefaultTBuiltInResource.limits.generalAttributeMatrixVectorIndexing << "\n"
            << "generalVaryingIndexing "                    << DefaultTBuiltInResource.limits.generalVaryingIndexing << "\n"
            << "generalSamplerIndexing "                    << DefaultTBuiltInResource.limits.generalSamplerIndexing << "\n"
            << "generalVariableIndexing "                   << DefaultTBuiltInResource.limits.generalVariableIndexing << "\n"
            << "generalConstantMatrixVectorIndexing "       << DefaultTBuiltInResource.limits.generalConstantMatrixVectorIndexing << "\n"
      ;

    return ostream.str();
}

}  // end namespace glslang
