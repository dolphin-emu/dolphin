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

#include <cstdlib>
#include <cstring>
#include <sstream>

#include "ResourceLimits.h"

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

void DecodeResourceLimits(TBuiltInResource* resources, char* config)
{
    const char* delims = " \t\n\r";
    const char* token = strtok(config, delims);
    while (token) {
        const char* valueStr = strtok(0, delims);
        if (valueStr == 0 || ! (valueStr[0] == '-' || (valueStr[0] >= '0' && valueStr[0] <= '9'))) {
            printf("Error: '%s' bad .conf file.  Each name must be followed by one number.\n", valueStr ? valueStr : "");
            return;
        }
        int value = atoi(valueStr);

        if (strcmp(token, "MaxLights") == 0)
            resources->maxLights = value;
        else if (strcmp(token, "MaxClipPlanes") == 0)
            resources->maxClipPlanes = value;
        else if (strcmp(token, "MaxTextureUnits") == 0)
            resources->maxTextureUnits = value;
        else if (strcmp(token, "MaxTextureCoords") == 0)
            resources->maxTextureCoords = value;
        else if (strcmp(token, "MaxVertexAttribs") == 0)
            resources->maxVertexAttribs = value;
        else if (strcmp(token, "MaxVertexUniformComponents") == 0)
            resources->maxVertexUniformComponents = value;
        else if (strcmp(token, "MaxVaryingFloats") == 0)
            resources->maxVaryingFloats = value;
        else if (strcmp(token, "MaxVertexTextureImageUnits") == 0)
            resources->maxVertexTextureImageUnits = value;
        else if (strcmp(token, "MaxCombinedTextureImageUnits") == 0)
            resources->maxCombinedTextureImageUnits = value;
        else if (strcmp(token, "MaxTextureImageUnits") == 0)
            resources->maxTextureImageUnits = value;
        else if (strcmp(token, "MaxFragmentUniformComponents") == 0)
            resources->maxFragmentUniformComponents = value;
        else if (strcmp(token, "MaxDrawBuffers") == 0)
            resources->maxDrawBuffers = value;
        else if (strcmp(token, "MaxVertexUniformVectors") == 0)
            resources->maxVertexUniformVectors = value;
        else if (strcmp(token, "MaxVaryingVectors") == 0)
            resources->maxVaryingVectors = value;
        else if (strcmp(token, "MaxFragmentUniformVectors") == 0)
            resources->maxFragmentUniformVectors = value;
        else if (strcmp(token, "MaxVertexOutputVectors") == 0)
            resources->maxVertexOutputVectors = value;
        else if (strcmp(token, "MaxFragmentInputVectors") == 0)
            resources->maxFragmentInputVectors = value;
        else if (strcmp(token, "MinProgramTexelOffset") == 0)
            resources->minProgramTexelOffset = value;
        else if (strcmp(token, "MaxProgramTexelOffset") == 0)
            resources->maxProgramTexelOffset = value;
        else if (strcmp(token, "MaxClipDistances") == 0)
            resources->maxClipDistances = value;
        else if (strcmp(token, "MaxComputeWorkGroupCountX") == 0)
            resources->maxComputeWorkGroupCountX = value;
        else if (strcmp(token, "MaxComputeWorkGroupCountY") == 0)
            resources->maxComputeWorkGroupCountY = value;
        else if (strcmp(token, "MaxComputeWorkGroupCountZ") == 0)
            resources->maxComputeWorkGroupCountZ = value;
        else if (strcmp(token, "MaxComputeWorkGroupSizeX") == 0)
            resources->maxComputeWorkGroupSizeX = value;
        else if (strcmp(token, "MaxComputeWorkGroupSizeY") == 0)
            resources->maxComputeWorkGroupSizeY = value;
        else if (strcmp(token, "MaxComputeWorkGroupSizeZ") == 0)
            resources->maxComputeWorkGroupSizeZ = value;
        else if (strcmp(token, "MaxComputeUniformComponents") == 0)
            resources->maxComputeUniformComponents = value;
        else if (strcmp(token, "MaxComputeTextureImageUnits") == 0)
            resources->maxComputeTextureImageUnits = value;
        else if (strcmp(token, "MaxComputeImageUniforms") == 0)
            resources->maxComputeImageUniforms = value;
        else if (strcmp(token, "MaxComputeAtomicCounters") == 0)
            resources->maxComputeAtomicCounters = value;
        else if (strcmp(token, "MaxComputeAtomicCounterBuffers") == 0)
            resources->maxComputeAtomicCounterBuffers = value;
        else if (strcmp(token, "MaxVaryingComponents") == 0)
            resources->maxVaryingComponents = value;
        else if (strcmp(token, "MaxVertexOutputComponents") == 0)
            resources->maxVertexOutputComponents = value;
        else if (strcmp(token, "MaxGeometryInputComponents") == 0)
            resources->maxGeometryInputComponents = value;
        else if (strcmp(token, "MaxGeometryOutputComponents") == 0)
            resources->maxGeometryOutputComponents = value;
        else if (strcmp(token, "MaxFragmentInputComponents") == 0)
            resources->maxFragmentInputComponents = value;
        else if (strcmp(token, "MaxImageUnits") == 0)
            resources->maxImageUnits = value;
        else if (strcmp(token, "MaxCombinedImageUnitsAndFragmentOutputs") == 0)
            resources->maxCombinedImageUnitsAndFragmentOutputs = value;
        else if (strcmp(token, "MaxCombinedShaderOutputResources") == 0)
            resources->maxCombinedShaderOutputResources = value;
        else if (strcmp(token, "MaxImageSamples") == 0)
            resources->maxImageSamples = value;
        else if (strcmp(token, "MaxVertexImageUniforms") == 0)
            resources->maxVertexImageUniforms = value;
        else if (strcmp(token, "MaxTessControlImageUniforms") == 0)
            resources->maxTessControlImageUniforms = value;
        else if (strcmp(token, "MaxTessEvaluationImageUniforms") == 0)
            resources->maxTessEvaluationImageUniforms = value;
        else if (strcmp(token, "MaxGeometryImageUniforms") == 0)
            resources->maxGeometryImageUniforms = value;
        else if (strcmp(token, "MaxFragmentImageUniforms") == 0)
            resources->maxFragmentImageUniforms = value;
        else if (strcmp(token, "MaxCombinedImageUniforms") == 0)
            resources->maxCombinedImageUniforms = value;
        else if (strcmp(token, "MaxGeometryTextureImageUnits") == 0)
            resources->maxGeometryTextureImageUnits = value;
        else if (strcmp(token, "MaxGeometryOutputVertices") == 0)
            resources->maxGeometryOutputVertices = value;
        else if (strcmp(token, "MaxGeometryTotalOutputComponents") == 0)
            resources->maxGeometryTotalOutputComponents = value;
        else if (strcmp(token, "MaxGeometryUniformComponents") == 0)
            resources->maxGeometryUniformComponents = value;
        else if (strcmp(token, "MaxGeometryVaryingComponents") == 0)
            resources->maxGeometryVaryingComponents = value;
        else if (strcmp(token, "MaxTessControlInputComponents") == 0)
            resources->maxTessControlInputComponents = value;
        else if (strcmp(token, "MaxTessControlOutputComponents") == 0)
            resources->maxTessControlOutputComponents = value;
        else if (strcmp(token, "MaxTessControlTextureImageUnits") == 0)
            resources->maxTessControlTextureImageUnits = value;
        else if (strcmp(token, "MaxTessControlUniformComponents") == 0)
            resources->maxTessControlUniformComponents = value;
        else if (strcmp(token, "MaxTessControlTotalOutputComponents") == 0)
            resources->maxTessControlTotalOutputComponents = value;
        else if (strcmp(token, "MaxTessEvaluationInputComponents") == 0)
            resources->maxTessEvaluationInputComponents = value;
        else if (strcmp(token, "MaxTessEvaluationOutputComponents") == 0)
            resources->maxTessEvaluationOutputComponents = value;
        else if (strcmp(token, "MaxTessEvaluationTextureImageUnits") == 0)
            resources->maxTessEvaluationTextureImageUnits = value;
        else if (strcmp(token, "MaxTessEvaluationUniformComponents") == 0)
            resources->maxTessEvaluationUniformComponents = value;
        else if (strcmp(token, "MaxTessPatchComponents") == 0)
            resources->maxTessPatchComponents = value;
        else if (strcmp(token, "MaxPatchVertices") == 0)
            resources->maxPatchVertices = value;
        else if (strcmp(token, "MaxTessGenLevel") == 0)
            resources->maxTessGenLevel = value;
        else if (strcmp(token, "MaxViewports") == 0)
            resources->maxViewports = value;
        else if (strcmp(token, "MaxVertexAtomicCounters") == 0)
            resources->maxVertexAtomicCounters = value;
        else if (strcmp(token, "MaxTessControlAtomicCounters") == 0)
            resources->maxTessControlAtomicCounters = value;
        else if (strcmp(token, "MaxTessEvaluationAtomicCounters") == 0)
            resources->maxTessEvaluationAtomicCounters = value;
        else if (strcmp(token, "MaxGeometryAtomicCounters") == 0)
            resources->maxGeometryAtomicCounters = value;
        else if (strcmp(token, "MaxFragmentAtomicCounters") == 0)
            resources->maxFragmentAtomicCounters = value;
        else if (strcmp(token, "MaxCombinedAtomicCounters") == 0)
            resources->maxCombinedAtomicCounters = value;
        else if (strcmp(token, "MaxAtomicCounterBindings") == 0)
            resources->maxAtomicCounterBindings = value;
        else if (strcmp(token, "MaxVertexAtomicCounterBuffers") == 0)
            resources->maxVertexAtomicCounterBuffers = value;
        else if (strcmp(token, "MaxTessControlAtomicCounterBuffers") == 0)
            resources->maxTessControlAtomicCounterBuffers = value;
        else if (strcmp(token, "MaxTessEvaluationAtomicCounterBuffers") == 0)
            resources->maxTessEvaluationAtomicCounterBuffers = value;
        else if (strcmp(token, "MaxGeometryAtomicCounterBuffers") == 0)
            resources->maxGeometryAtomicCounterBuffers = value;
        else if (strcmp(token, "MaxFragmentAtomicCounterBuffers") == 0)
            resources->maxFragmentAtomicCounterBuffers = value;
        else if (strcmp(token, "MaxCombinedAtomicCounterBuffers") == 0)
            resources->maxCombinedAtomicCounterBuffers = value;
        else if (strcmp(token, "MaxAtomicCounterBufferSize") == 0)
            resources->maxAtomicCounterBufferSize = value;
        else if (strcmp(token, "MaxTransformFeedbackBuffers") == 0)
            resources->maxTransformFeedbackBuffers = value;
        else if (strcmp(token, "MaxTransformFeedbackInterleavedComponents") == 0)
            resources->maxTransformFeedbackInterleavedComponents = value;
        else if (strcmp(token, "MaxCullDistances") == 0)
            resources->maxCullDistances = value;
        else if (strcmp(token, "MaxCombinedClipAndCullDistances") == 0)
            resources->maxCombinedClipAndCullDistances = value;
        else if (strcmp(token, "MaxSamples") == 0)
            resources->maxSamples = value;

        else if (strcmp(token, "nonInductiveForLoops") == 0)
            resources->limits.nonInductiveForLoops = (value != 0);
        else if (strcmp(token, "whileLoops") == 0)
            resources->limits.whileLoops = (value != 0);
        else if (strcmp(token, "doWhileLoops") == 0)
            resources->limits.doWhileLoops = (value != 0);
        else if (strcmp(token, "generalUniformIndexing") == 0)
            resources->limits.generalUniformIndexing = (value != 0);
        else if (strcmp(token, "generalAttributeMatrixVectorIndexing") == 0)
            resources->limits.generalAttributeMatrixVectorIndexing = (value != 0);
        else if (strcmp(token, "generalVaryingIndexing") == 0)
            resources->limits.generalVaryingIndexing = (value != 0);
        else if (strcmp(token, "generalSamplerIndexing") == 0)
            resources->limits.generalSamplerIndexing = (value != 0);
        else if (strcmp(token, "generalVariableIndexing") == 0)
            resources->limits.generalVariableIndexing = (value != 0);
        else if (strcmp(token, "generalConstantMatrixVectorIndexing") == 0)
            resources->limits.generalConstantMatrixVectorIndexing = (value != 0);
        else
            printf("Warning: unrecognized limit (%s) in configuration file.\n", token);

        token = strtok(0, delims);
    }
}

}  // end namespace glslang
