//
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

//
// Create strings that declare built-in definitions, add built-ins programmatically 
// that cannot be expressed in the strings, and establish mappings between
// built-in functions and operators.
//
// Where to put a built-in:
//   TBuiltInParseablesHlsl::initialize(version,profile) context-independent textual built-ins; add them to the right string
//   TBuiltInParseablesHlsl::initialize(resources,...)   context-dependent textual built-ins; add them to the right string
//   TBuiltInParseablesHlsl::identifyBuiltIns(...,symbolTable) context-independent programmatic additions/mappings to the symbol table,
//                                                including identifying what extensions are needed if a version does not allow a symbol
//   TBuiltInParseablesHlsl::identifyBuiltIns(...,symbolTable, resources) context-dependent programmatic additions/mappings to the
//                                                symbol table, including identifying what extensions are needed if a version does
//                                                not allow a symbol
//

#include "hlslParseables.h"
#include <cctype>
#include <utility>

namespace {  // anonymous namespace functions

const char* BaseTypeName(const char* argOrder, const char* scalarName, const char* vecName, const char* matName)
{
    switch (*argOrder) {
    case 'S': return scalarName;
    case 'V': return vecName;
    case 'M': return matName;
    default:  return "UNKNOWN_TYPE";
    }
}
    
// Create and return a type name.  This is done in GLSL, not HLSL conventions, until such
// time as builtins are parsed using the HLSL parser.
//
//    order:   S = scalar, V = vector, M = matrix
//    argType: F = float, D = double, I = int, U = uint, B = bool, S = sampler
//    dim0 = vector dimension, or matrix 1st dimension
//    dim1 = matrix 2nd dimension
glslang::TString& AppendTypeName(glslang::TString& s, const char* argOrder, const char* argType, int dim0, int dim1)
{
    const bool transpose = (argOrder[0] == '^');

    // Take transpose of matrix dimensions
    if (transpose) {
        std::swap(dim0, dim1);
        ++argOrder;
    }

    switch (*argType) {
    case '-': s += "void"; break;
    case 'F': s += BaseTypeName(argOrder, "float",   "vec",     "mat");  break;
    case 'D': s += BaseTypeName(argOrder, "double",  "dvec",    "dmat"); break;
    case 'I': s += BaseTypeName(argOrder, "int",     "ivec",    "imat"); break;
    case 'U': s += BaseTypeName(argOrder, "uint",    "uvec",    "umat"); break;
    case 'B': s += BaseTypeName(argOrder, "bool",    "bvec",    "bmat"); break;
    case 'S': s += BaseTypeName(argOrder, "sampler", "sampler", "sampler"); break; // TODO: 
    default:  s += "UNKNOWN_TYPE"; break;
    }

    // handle fixed vector sizes, such as float3, and only ever 3.
    const int fixedVecSize = isdigit(argOrder[1]) ? (argOrder[1] - '0') : 0;
    if (fixedVecSize != 0)
        dim0 = dim1 = fixedVecSize;

    // Add sampler dimensions
    if (*argType == 'S') {
        switch (dim0) {
        case 1: s += "1D";   break;
        case 2: s += "2D";   break;
        case 3: s += "3D";   break;
        case 4: s += "Cube"; break;
        default: s += "UNKNOWN_SAMPLER"; break;
        }
    }

    // verify dimensions
    if ((*argOrder == 'V' || *argOrder == 'M') && (dim0 < 1 || dim0 > 4) ||
        (*argOrder == 'M' && (dim1 < 1 || dim1 > 4))) {
        s += "UNKNOWN_DIMENSION";
        return s;
    }

    switch (*argOrder) {
    case '-': break;  // no dimensions for voids
    case 'S': break;  // no dimensions on scalars
    case 'V': s += ('0' + dim0); break;
    case 'M': s += ('0' + dim0); s += 'x'; s += ('0' + dim1); break;
    }

    return s;
}

// TODO: the GLSL parser is currently used to parse HLSL prototypes.  However, many valid HLSL prototypes
// are not valid GLSL prototypes.  This rejects the invalid ones.  Thus, there is a single switch below
// to enable creation of the entire HLSL space.
inline bool IsValidGlsl(const char* cname, char retOrder, char retType, char argOrder, char argType,
                        int dim0, int dim1, int dim0Max, int dim1Max)
{
    const bool isVec = dim0Max > 1 || argType == 'V';
    const bool isMat = dim1Max > 1 || argType == 'M';

    if (argType == 'D'                  ||  // avoid double args
        retType == 'D'                  ||  // avoid double return
        (isVec && dim0 == 1)            ||  // avoid vec1
        (isMat && dim0 == 1 && dim1 == 1))  // avoid mat1x1
        return false;

    const std::string name(cname);  // for ease of comparison. slow, but temporary, until HLSL parser is online.
                                
    if (isMat && dim0 != dim1)  // TODO: avoid mats until we find the right GLSL profile
        return false;

    if (isMat && (argType == 'I' || argType == 'U' || argType == 'B') ||
        retOrder == 'M' && (retType == 'I' || retType == 'U' || retType == 'B'))
        return false;

    if (name == "GetRenderTargetSamplePosition" ||
        name == "tex1D" ||
        name == "tex1Dgrad")
        return false;

    return true;
}


// Return true for the end of a single argument key, which can be the end of the string, or
// the comma separator.
inline bool IsEndOfArg(const char* arg)
{
    return arg == nullptr || *arg == '\0' || *arg == ',';
}


// return position of end of argument specifier
inline const char* FindEndOfArg(const char* arg)
{
    while (!IsEndOfArg(arg))
        ++arg;

    return *arg == '\0' ? nullptr : arg;
}


// Return pointer to beginning of Nth argument specifier in the string.
inline const char* NthArg(const char* arg, int n)
{
    for (int x=0; x<n && arg; ++x)
        if ((arg = FindEndOfArg(arg)) != nullptr)
            ++arg;  // skip arg separator

    return arg;
}

inline void FindVectorMatrixBounds(const char* argOrder, int fixedVecSize, int& dim0Min, int& dim0Max, int& dim1Min, int& dim1Max)
{
    for (int arg = 0; ; ++arg) {
        const char* nthArgOrder(NthArg(argOrder, arg));
        if (nthArgOrder == nullptr)
            break;
        else if (*nthArgOrder == 'V')
            dim0Max = 4;
        else if (*nthArgOrder == 'M')
            dim0Max = dim1Max = 4;
    }

    if (fixedVecSize > 0) // handle fixed sized vectors
        dim0Min = dim0Max = fixedVecSize;
}
    
} // end anonymous namespace

namespace glslang {

TBuiltInParseablesHlsl::TBuiltInParseablesHlsl()
{
}

//
// Add all context-independent built-in functions and variables that are present
// for the given version and profile.  Share common ones across stages, otherwise
// make stage-specific entries.
//
// Most built-ins variables can be added as simple text strings.  Some need to
// be added programmatically, which is done later in IdentifyBuiltIns() below.
//
void TBuiltInParseablesHlsl::initialize(int version, EProfile profile, const SpvVersion& spvVersion)
{
    static const EShLanguageMask EShLangAll = EShLanguageMask(EShLangCount - 1);

    // This structure encodes the prototype information for each HLSL intrinsic.
    // Because explicit enumeration would be cumbersome, it's procedurally generated.
    // orderKey can be:
    //   S = scalar, V = vector, M = matrix, - = void
    // typekey can be:
    //   D = double, F = float, U = uint, I = int, B = bool, S = sampler, - = void
    // An empty order or type key repeats the first one.  E.g: SVM,, means 3 args each of SVM.
    // '>' as first letter of order creates an output parameter
    // '<' as first letter of order creates an input parameter
    // '^' as first letter of order takes transpose dimensions

    static const struct {
        const char*   name;      // intrinsic name
        const char*   retOrder;  // return type key: empty matches order of 1st argument
        const char*   retType;   // return type key: empty matches type of 1st argument
        const char*   argOrder;  // argument order key
        const char*   argType;   // argument type key
        unsigned int  stage;     // stage mask
    } hlslIntrinsics[] = {
        // name                               retOrd   retType    argOrder      argType   stage mask
        // -----------------------------------------------------------------------------------------------
        { "abort",                            nullptr, nullptr,   "-",          "-",      EShLangAll },
        { "abs",                              nullptr, nullptr,   "SVM",        "DFUI",   EShLangAll },
        { "acos",                             nullptr, nullptr,   "SVM",        "F",      EShLangAll },
        { "all",                              "S",    "B",        "SVM",        "BFI",    EShLangAll },
        { "AllMemoryBarrier",                 nullptr, nullptr,   "-",          "-",      EShLangComputeMask },
        { "AllMemoryBarrierWithGroupSync",    nullptr, nullptr,   "-",          "-",      EShLangComputeMask },
        { "any",                              "S",     "B",       "SVM",        "BFI",    EShLangAll },
        { "asdouble",                         "S",     "D",       "S,",         "U,",     EShLangAll },
        { "asdouble",                         "V2",    "D",       "V2,",        "U,",     EShLangAll },
        { "asfloat",                          nullptr, "F",       "SVM",        "BFIU",   EShLangAll },
        { "asin",                             nullptr, nullptr,   "SVM",        "F",      EShLangAll },
        { "asint",                            nullptr, "I",       "SVM",        "FU",     EShLangAll },
        { "asuint",                           nullptr, "U",       "SVM",        "FU",     EShLangAll },
        { "atan",                             nullptr, nullptr,   "SVM",        "F",      EShLangAll },
        { "atan2",                            nullptr, nullptr,   "SVM,",       "F,",     EShLangAll },
        { "ceil",                             nullptr, nullptr,   "SVM",        "F",      EShLangAll },
        { "CheckAccessFullyMapped",           "S",     "B" ,      "S",          "U",      EShLangFragmentMask | EShLangComputeMask },
        { "clamp",                            nullptr, nullptr,   "SVM,,",      "FUI,,",  EShLangAll },
        { "clip",                             "-",     "-",       "SVM",        "F",      EShLangFragmentMask },
        { "cos",                              nullptr, nullptr,   "SVM",        "F",      EShLangAll },
        { "cosh",                             nullptr, nullptr,   "SVM",        "F",      EShLangAll },
        { "countbits",                        nullptr, nullptr,   "SV",         "U",      EShLangAll },
        { "cross",                            nullptr, nullptr,   "V3,",        "F,",     EShLangAll },
        { "D3DCOLORtoUBYTE4",                 "V4",    "I",       "V4",         "F",      EShLangAll },
        { "ddx",                              nullptr, nullptr,   "SVM",        "F",      EShLangFragmentMask },
        { "ddx_coarse",                       nullptr, nullptr,   "SVM",        "F",      EShLangFragmentMask },
        { "ddx_fine",                         nullptr, nullptr,   "SVM",        "F",      EShLangFragmentMask },
        { "ddy",                              nullptr, nullptr,   "SVM",        "F",      EShLangFragmentMask },
        { "ddy_coarse",                       nullptr, nullptr,   "SVM",        "F",      EShLangFragmentMask },
        { "ddy_fine",                         nullptr, nullptr,   "SVM",        "F",      EShLangFragmentMask },
        { "degrees",                          nullptr, nullptr,   "SVM",        "F",      EShLangAll },
        { "determinant",                      "S",     "F",       "M",          "F",      EShLangAll },
        { "DeviceMemoryBarrier",              nullptr, nullptr,   "-",          "-",      EShLangFragmentMask | EShLangComputeMask },
        { "DeviceMemoryBarrierWithGroupSync", nullptr, nullptr,   "-",          "-",      EShLangComputeMask },
        { "distance",                         "S",     "F",       "V,",         "F,",     EShLangAll },
        { "dot",                              "S",     nullptr,   "V,",         "FI,",    EShLangAll },
        { "dst",                              nullptr, nullptr,   "V4,V4",      "F,",     EShLangAll },
        // { "errorf",                           "-",     "-",       "",         "",     EShLangAll }, TODO: varargs
        { "EvaluateAttributeAtCentroid",      nullptr, nullptr,   "SVM",        "F",      EShLangFragmentMask },
        { "EvaluateAttributeAtSample",        nullptr, nullptr,   "SVM,S",      "F,U",    EShLangFragmentMask },
        { "EvaluateAttributeSnapped",         nullptr, nullptr,   "SVM,V2",     "F,I",    EShLangFragmentMask },
        { "exp",                              nullptr, nullptr,   "SVM",        "F",      EShLangAll },
        { "exp2",                             nullptr, nullptr,   "SVM",        "F",      EShLangAll },
        { "f16tof32",                         nullptr, "F",       "SV",         "U",      EShLangAll },
        { "f32tof16",                         nullptr, "U",       "SV",         "F",      EShLangAll },
        { "faceforward",                      nullptr, nullptr,   "V,,",        "F,,",    EShLangAll },
        { "firstbithigh",                     nullptr, nullptr,   "SV",         "UI",     EShLangAll },
        { "firstbitlow",                      nullptr, nullptr,   "SV",         "UI",     EShLangAll },
        { "floor",                            nullptr, nullptr,   "SVM",        "F",      EShLangAll },
        { "fma",                              nullptr, nullptr,   "SVM,,",      "D,,",    EShLangAll },
        { "fmod",                             nullptr, nullptr,   "SVM,",       "F,",     EShLangAll },
        { "frac",                             nullptr, nullptr,   "SVM",        "F",      EShLangAll },
        { "frexp",                            nullptr, nullptr,   "SVM,",       "F,",     EShLangAll },
        { "fwidth",                           nullptr, nullptr,   "SVM",        "F",      EShLangFragmentMask },
        { "GetRenderTargetSampleCount",       "S",     "U",       "-",          "-",      EShLangAll },
        { "GetRenderTargetSamplePosition",    "V2",    "F",       "V1",         "I",      EShLangAll },
        { "GroupMemoryBarrier",               nullptr, nullptr,   "-",          "-",      EShLangComputeMask },
        { "GroupMemoryBarrierWithGroupSync",  nullptr, nullptr,   "-",          "-",      EShLangComputeMask },
        { "InterlockedAdd",                   "-",     "-",       "SVM,,>",     "UI,,",   EShLangFragmentMask | EShLangComputeMask },
        { "InterlockedAdd",                   "-",     "-",       "SVM,",       "UI,",    EShLangFragmentMask | EShLangComputeMask },
        { "InterlockedAnd",                   "-",     "-",       "SVM,,>",     "UI,,",   EShLangFragmentMask | EShLangComputeMask },
        { "InterlockedAnd",                   "-",     "-",       "SVM,",       "UI,",    EShLangFragmentMask | EShLangComputeMask },
        { "InterlockedCompareExchange",       "-",     "-",       "SVM,,,>",    "UI,,,",  EShLangFragmentMask | EShLangComputeMask },
        { "InterlockedCompareStore",          "-",     "-",       "SVM,,",      "UI,,",   EShLangFragmentMask | EShLangComputeMask },
        { "InterlockedExchange",              "-",     "-",       "SVM,,>",     "UI,,",   EShLangFragmentMask | EShLangComputeMask },
        { "InterlockedMax",                   "-",     "-",       "SVM,,>",     "UI,,",   EShLangFragmentMask | EShLangComputeMask },
        { "InterlockedMax",                   "-",     "-",       "SVM,",       "UI,",    EShLangFragmentMask | EShLangComputeMask },
        { "InterlockedMin",                   "-",     "-",       "SVM,,>",     "UI,,",   EShLangFragmentMask | EShLangComputeMask },
        { "InterlockedMin",                   "-",     "-",       "SVM,",       "UI,",    EShLangFragmentMask | EShLangComputeMask },
        { "InterlockedOr",                    "-",     "-",       "SVM,,>",     "UI,,",   EShLangFragmentMask | EShLangComputeMask },
        { "InterlockedOr",                    "-",     "-",       "SVM,",       "UI,",    EShLangFragmentMask | EShLangComputeMask },
        { "InterlockedXor",                   "-",     "-",       "SVM,,>",     "UI,,",   EShLangFragmentMask | EShLangComputeMask },
        { "InterlockedXor",                   "-",     "-",       "SVM,",       "UI,",    EShLangFragmentMask | EShLangComputeMask },
        { "isfinite",                         nullptr, "B" ,      "SVM",        "F",      EShLangAll },
        { "isinf",                            nullptr, "B" ,      "SVM",        "F",      EShLangAll },
        { "isnan",                            nullptr, "B" ,      "SVM",        "F",      EShLangAll },
        { "ldexp",                            nullptr, nullptr,   "SVM,",       "F,",     EShLangAll },
        { "length",                           "S",     "F",       "V",          "F",      EShLangAll },
        { "lit",                              "V4",    "F",       "S,,",        "F,,",    EShLangAll },
        { "log",                              nullptr, nullptr,   "SVM",        "F",      EShLangAll },
        { "log10",                            nullptr, nullptr,   "SVM",        "F",      EShLangAll },
        { "log2",                             nullptr, nullptr,   "SVM",        "F",      EShLangAll },
        { "mad",                              nullptr, nullptr,   "SVM,,",      "DFUI,,", EShLangAll },
        { "max",                              nullptr, nullptr,   "SVM,",       "FI,",    EShLangAll },
        { "min",                              nullptr, nullptr,   "SVM,",       "FI,",    EShLangAll },
        { "modf",                             nullptr, nullptr,   "SVM,>",      "FI,",    EShLangAll },
        { "msad4",                            "V4",    "U",       "S,V2,V4",    "U,,",    EShLangAll },
        // TODO: fix matrix return size for non-square mats used with mul opcode
        { "mul",                              "S",     nullptr,   "S,S",        "FI,",    EShLangAll },
        { "mul",                              "V",     nullptr,   "S,V",        "FI,",    EShLangAll },
        { "mul",                              "M",     nullptr,   "S,M",        "FI,",    EShLangAll },
        { "mul",                              "V",     nullptr,   "V,S",        "FI,",    EShLangAll },
        { "mul",                              "S",     nullptr,   "V,V",        "FI,",    EShLangAll },
        { "mul",                              "V",     nullptr,   "V,M",        "FI,",    EShLangAll },
        { "mul",                              "M",     nullptr,   "M,S",        "FI,",    EShLangAll },
        { "mul",                              "V",     nullptr,   "M,V",        "FI,",    EShLangAll },
        { "mul",                              "M",     nullptr,   "M,M",        "FI,",    EShLangAll },
        { "noise",                            "S",     "F",       "V",          "F",      EShLangFragmentMask },
        { "normalize",                        nullptr, nullptr,   "V",          "F",      EShLangAll },
        { "pow",                              nullptr, nullptr,   "SVM,",       "F,",     EShLangAll },
        // { "printf",                           "-",     "-",       "",        "",     EShLangAll }, TODO: varargs
        { "Process2DQuadTessFactorsAvg",      "-",     "-",       "V4,V2,>V4,>V2,>V2", "F,,,,", EShLangTessControlMask },
        { "Process2DQuadTessFactorsMax",      "-",     "-",       "V4,V2,>V4,>V2,>V2", "F,,,,", EShLangTessControlMask },
        { "Process2DQuadTessFactorsMin",      "-",     "-",       "V4,V2,>V4,>V2,>V2", "F,,,,", EShLangTessControlMask },
        { "ProcessIsolineTessFactors",        "-",     "-",       "S,,>,>",  "F,,,",   EShLangTessControlMask },
        { "ProcessQuadTessFactorsAvg",        "-",     "-",       "V4,S,>V4,>V2,>V2", "F,,,,",  EShLangTessControlMask },
        { "ProcessQuadTessFactorsMax",        "-",     "-",       "V4,S,>V4,>V2,>V2", "F,,,,",  EShLangTessControlMask },
        { "ProcessQuadTessFactorsMin",        "-",     "-",       "V4,S,>V4,>V2,>V2", "F,,,,",  EShLangTessControlMask },
        { "ProcessTriTessFactorsAvg",         "-",     "-",       "V3,S,>V3,>S,>S",   "F,,,,",  EShLangTessControlMask },
        { "ProcessTriTessFactorsMax",         "-",     "-",       "V3,S,>V3,>S,>S",   "F,,,,",  EShLangTessControlMask },
        { "ProcessTriTessFactorsMin",         "-",     "-",       "V3,S,>V3,>S,>S",   "F,,,,",  EShLangTessControlMask },
        { "radians",                          nullptr, nullptr,   "SVM",        "F",      EShLangAll },
        { "rcp",                              nullptr, nullptr,   "SVM",        "FD",     EShLangAll },
        { "reflect",                          nullptr, nullptr,   "V,",         "F,",     EShLangAll },
        { "refract",                          nullptr, nullptr,   "V,V,S",      "F,,",    EShLangAll },
        { "reversebits",                      nullptr, nullptr,   "SV",         "U",      EShLangAll },
        { "round",                            nullptr, nullptr,   "SVM",        "F",      EShLangAll },
        { "rsqrt",                            nullptr, nullptr,   "SVM",        "F",      EShLangAll },
        { "saturate",                         nullptr, nullptr ,  "SVM",        "F",      EShLangAll },
        { "sign",                             nullptr, nullptr,   "SVM",        "FI",     EShLangAll },
        { "sin",                              nullptr, nullptr,   "SVM",        "F",      EShLangAll },
        { "sincos",                           "-",     "-",       "SVM,>,>",    "F,,",    EShLangAll },
        { "sinh",                             nullptr, nullptr,   "SVM",        "F",      EShLangAll },
        { "smoothstep",                       nullptr, nullptr,   "SVM,,",      "F,,",    EShLangAll },
        { "sqrt",                             nullptr, nullptr,   "SVM",        "F",      EShLangAll },
        { "step",                             nullptr, nullptr,   "SVM,",       "F,",     EShLangAll },
        { "tan",                              nullptr, nullptr,   "SVM",        "F",      EShLangAll },
        { "tanh",                             nullptr, nullptr,   "SVM",        "F",      EShLangAll },
        { "tex1D",                            "V4",    "F",       "S1,S",       "S,F",    EShLangFragmentMask },
        { "tex1D",                            "V4",    "F",       "S1,S,V1,V1", "S,F,F,F",EShLangFragmentMask },
        { "tex1Dbias",                        "V4",    "F",       "S1,V4",      "S,F",    EShLangFragmentMask },
        { "tex1Dgrad",                        "V4",    "F",       "S1,V1,V1,V1","S,F,F,F",EShLangFragmentMask },
        { "tex1Dlod",                         "V4",    "F",       "S1,V4",      "S,F",    EShLangFragmentMask },
        { "tex1Dproj",                        "V4",    "F",       "S1,V4",      "S,F",    EShLangFragmentMask },
        { "tex2D",                            "V4",    "F",       "S2,V2",      "S,F",    EShLangFragmentMask },
        { "tex2D",                            "V4",    "F",       "S2,V2,V2,V2","S,F,F,F",EShLangFragmentMask },
        { "tex2Dbias",                        "V4",    "F",       "S2,V4",      "S,F",    EShLangFragmentMask },
        { "tex2Dgrad",                        "V4",    "F",       "S2,V2,V2,V2","S,F,F,F",EShLangFragmentMask },
        { "tex2Dlod",                         "V4",    "F",       "S2,V4",      "S,F",    EShLangFragmentMask },
        { "tex2Dproj",                        "V4",    "F",       "S2,V4",      "S,F",    EShLangFragmentMask },
        { "tex3D",                            "V4",    "F",       "S3,V3",      "S,F",    EShLangFragmentMask },
        { "tex3D",                            "V4",    "F",       "S3,V3,V3,V3","S,F,F,F",EShLangFragmentMask },
        { "tex3Dbias",                        "V4",    "F",       "S3,V4",      "S,F",    EShLangFragmentMask },
        { "tex3Dgrad",                        "V4",    "F",       "S3,V3,V3,V3","S,F,F,F",EShLangFragmentMask },
        { "tex3Dlod",                         "V4",    "F",       "S3,V4",      "S,F",    EShLangFragmentMask },
        { "tex3Dproj",                        "V4",    "F",       "S3,V4",      "S,F",    EShLangFragmentMask },
        { "texCUBE",                          "V4",    "F",       "S4,V3",      "S,F",    EShLangFragmentMask },
        { "texCUBE",                          "V4",    "F",       "S4,V3,V3,V3","S,F,F,F",EShLangFragmentMask },
        { "texCUBEbias",                      "V4",    "F",       "S4,V4",      "S,F",    EShLangFragmentMask },
        { "texCUBEgrad",                      "V4",    "F",       "S4,V3,V3,V3","S,F,F,F",EShLangFragmentMask },
        { "texCUBElod",                       "V4",    "F",       "S4,V4",      "S,F",    EShLangFragmentMask },
        { "texCUBEproj",                      "V4",    "F",       "S4,V4",      "S,F",    EShLangFragmentMask },
        { "transpose",                        "^M",    nullptr,   "M",          "F",      EShLangAll },
        { "trunc",                            nullptr, nullptr,   "SVM",        "F",      EShLangAll },

        // Mark end of list, since we want to avoid a range-based for, as some compilers don't handle it yet.
        { nullptr,                            nullptr, nullptr,   nullptr,      nullptr,  0 },
    };

    // Set this to true to avoid generating prototypes that will be invalid for the GLSL parser.
    // TODO: turn it off (and remove the code) when the HLSL parser can be used to parse builtins.
    static const bool skipInvalidGlsl = true;
    
    // Create prototypes for the intrinsics.  TODO: Avoid ranged based for until all compilers can handle it.
    for (int icount = 0; hlslIntrinsics[icount].name; ++icount) {
        const auto& intrinsic = hlslIntrinsics[icount];

        for (int stage = 0; stage < EShLangCount; ++stage) {                                // for each stage...
            if ((intrinsic.stage & (1<<stage)) == 0) // skip inapplicable stages
                continue;

            // reference to either the common builtins, or stage specific builtins.
            TString& s = (intrinsic.stage == EShLangAll) ? commonBuiltins : stageBuiltins[stage];

            for (const char* argOrder = intrinsic.argOrder; !IsEndOfArg(argOrder); ++argOrder) { // for each order...
                const int fixedVecSize = isdigit(argOrder[1]) ? (argOrder[1] - '0') : 0;

                // calculate min and max vector and matrix dimensions
                int dim0Min = 1;
                int dim0Max = 1;
                int dim1Min = 1;
                int dim1Max = 1;

                FindVectorMatrixBounds(argOrder, fixedVecSize, dim0Min, dim0Max, dim1Min, dim1Max);

                for (const char* argType = intrinsic.argType; !IsEndOfArg(argType); ++argType) { // for each type...
                    for (int dim0 = dim0Min; dim0 <= dim0Max; ++dim0) {          // for each dim 0...
                        for (int dim1 = dim1Min; dim1 <= dim1Max; ++dim1) {      // for each dim 1...
                            const char* retOrder = intrinsic.retOrder ? intrinsic.retOrder : argOrder;
                            const char* retType  = intrinsic.retType  ? intrinsic.retType  : argType;

                            if (skipInvalidGlsl && !IsValidGlsl(intrinsic.name, *retOrder, *retType, *argOrder, *argType,
                                                                dim0, dim1, dim0Max, dim1Max))
                                continue;

                            AppendTypeName(s, retOrder, retType, dim0, dim1);  // add return type
                            s.append(" ");                                     // space between type and name
                            s.append(intrinsic.name);                          // intrinsic name
                            s.append("(");                                     // open paren

                            // Append argument types, if any.
                            for (int arg = 0; ; ++arg) {
                                const char* nthArgOrder(NthArg(argOrder, arg));
                                const char* nthArgType(NthArg(argType, arg));

                                if (nthArgOrder == nullptr || nthArgType == nullptr)
                                    break;

                                s.append(arg > 0 ? ", ": "");  // comma separator if needed
                                
                                if (*nthArgOrder == '>') {           // output params
                                    ++nthArgOrder;
                                    s.append("out ");
                                } else if (*nthArgOrder == '<') {    // input params
                                    ++nthArgOrder;
                                    s.append("in ");
                                }

                                // Comma means use the 1st argument order and type.
                                if (*nthArgOrder == ',' || *nthArgOrder == '\0') nthArgOrder = argOrder;
                                if (*nthArgType == ',' || *nthArgType == '\0') nthArgType = argType;

                                AppendTypeName(s, nthArgOrder, nthArgType, dim0, dim1); // Add first argument
                            }
                            
                            s.append(");\n");            // close paren and trailing semicolon
                        }
                    }
                }

                if (fixedVecSize > 0)  // skip over number for fixed size vectors
                    ++argOrder;
            }
            
            if (intrinsic.stage == EShLangAll) // common builtins are only added once.
                break;
        }
    }

    // printf("Common:\n%s\n",   getCommonString().c_str());
    // printf("Frag:\n%s\n",     getStageString(EShLangFragment).c_str());
    // printf("Vertex:\n%s\n",   getStageString(EShLangVertex).c_str());
    // printf("Geo:\n%s\n",      getStageString(EShLangGeometry).c_str());
    // printf("TessCtrl:\n%s\n", getStageString(EShLangTessControl).c_str());
    // printf("TessEval:\n%s\n", getStageString(EShLangTessEvaluation).c_str());
    // printf("Compute:\n%s\n",  getStageString(EShLangCompute).c_str());
}

//
// Add context-dependent built-in functions and variables that are present
// for the given version and profile.  All the results are put into just the
// commonBuiltins, because it is called for just a specific stage.  So,
// add stage-specific entries to the commonBuiltins, and only if that stage
// was requested.
//
void TBuiltInParseablesHlsl::initialize(const TBuiltInResource &resources, int version, EProfile profile,
                                        const SpvVersion& spvVersion, EShLanguage language)
{
}


//
// Finish adding/processing context-independent built-in symbols.
// 1) Programmatically add symbols that could not be added by simple text strings above.
// 2) Map built-in functions to operators, for those that will turn into an operation node
//    instead of remaining a function call.
// 3) Tag extension-related symbols added to their base version with their extensions, so
//    that if an early version has the extension turned off, there is an error reported on use.
//
void TBuiltInParseablesHlsl::identifyBuiltIns(int version, EProfile profile, const SpvVersion& spvVersion, EShLanguage language,
                                              TSymbolTable& symbolTable)
{
    // symbolTable.relateToOperator("abort",                       EOpAbort);
    symbolTable.relateToOperator("abs",                         EOpAbs);
    symbolTable.relateToOperator("acos",                        EOpAcos);
    symbolTable.relateToOperator("all",                         EOpAll);
    symbolTable.relateToOperator("AllMemoryBarrier",            EOpMemoryBarrier);
    symbolTable.relateToOperator("AllMemoryBarrierWithGroupSync", EOpAllMemoryBarrierWithGroupSync);
    symbolTable.relateToOperator("any",                         EOpAny);
    symbolTable.relateToOperator("asdouble",                    EOpUint64BitsToDouble);
    symbolTable.relateToOperator("asfloat",                     EOpIntBitsToFloat);
    symbolTable.relateToOperator("asin",                        EOpAsin);
    symbolTable.relateToOperator("asint",                       EOpFloatBitsToInt);
    symbolTable.relateToOperator("asuint",                      EOpFloatBitsToUint);
    symbolTable.relateToOperator("atan",                        EOpAtan);
    symbolTable.relateToOperator("atan2",                       EOpAtan);
    symbolTable.relateToOperator("ceil",                        EOpCeil);
    // symbolTable.relateToOperator("CheckAccessFullyMapped");
    symbolTable.relateToOperator("clamp",                       EOpClamp);
    symbolTable.relateToOperator("clip",                        EOpClip);
    symbolTable.relateToOperator("cos",                         EOpCos);
    symbolTable.relateToOperator("cosh",                        EOpCosh);
    symbolTable.relateToOperator("countbits",                   EOpBitCount);
    symbolTable.relateToOperator("cross",                       EOpCross);
    // symbolTable.relateToOperator("D3DCOLORtoUBYTE4",            EOpD3DCOLORtoUBYTE4);
    symbolTable.relateToOperator("ddx",                         EOpDPdx);
    symbolTable.relateToOperator("ddx_coarse",                  EOpDPdxCoarse);
    symbolTable.relateToOperator("ddx_fine",                    EOpDPdxFine);
    symbolTable.relateToOperator("ddy",                         EOpDPdy);
    symbolTable.relateToOperator("ddy_coarse",                  EOpDPdyCoarse);
    symbolTable.relateToOperator("ddy_fine",                    EOpDPdyFine);
    symbolTable.relateToOperator("degrees",                     EOpDegrees);
    symbolTable.relateToOperator("determinant",                 EOpDeterminant);
    symbolTable.relateToOperator("DeviceMemoryBarrier",         EOpGroupMemoryBarrier); // == ScopeDevice+CrossWorkGroup
    symbolTable.relateToOperator("DeviceMemoryBarrierWithGroupSync", EOpGroupMemoryBarrierWithGroupSync); // ...
    symbolTable.relateToOperator("distance",                    EOpDistance);
    symbolTable.relateToOperator("dot",                         EOpDot);
    symbolTable.relateToOperator("dst",                         EOpDst);
    // symbolTable.relateToOperator("errorf",                      EOpErrorf);
    symbolTable.relateToOperator("EvaluateAttributeAtCentroid", EOpInterpolateAtCentroid);
    symbolTable.relateToOperator("EvaluateAttributeAtSample",   EOpInterpolateAtSample);
    symbolTable.relateToOperator("EvaluateAttributeSnapped",    EOpEvaluateAttributeSnapped);
    symbolTable.relateToOperator("exp",                         EOpExp);
    symbolTable.relateToOperator("exp2",                        EOpExp2);
    symbolTable.relateToOperator("f16tof32",                    EOpF16tof32);
    symbolTable.relateToOperator("f32tof16",                    EOpF32tof16);
    symbolTable.relateToOperator("faceforward",                 EOpFaceForward);
    symbolTable.relateToOperator("firstbithigh",                EOpFindMSB);
    symbolTable.relateToOperator("firstbitlow",                 EOpFindLSB);
    symbolTable.relateToOperator("floor",                       EOpFloor);
    symbolTable.relateToOperator("fma",                         EOpFma);
    symbolTable.relateToOperator("fmod",                        EOpMod);
    symbolTable.relateToOperator("frac",                        EOpFract);
    symbolTable.relateToOperator("frexp",                       EOpFrexp);
    symbolTable.relateToOperator("fwidth",                      EOpFwidth);
    // symbolTable.relateToOperator("GetRenderTargetSampleCount");
    // symbolTable.relateToOperator("GetRenderTargetSamplePosition");
    symbolTable.relateToOperator("GroupMemoryBarrier",          EOpWorkgroupMemoryBarrier);
    symbolTable.relateToOperator("GroupMemoryBarrierWithGroupSync", EOpWorkgroupMemoryBarrierWithGroupSync);
    symbolTable.relateToOperator("InterlockedAdd",              EOpInterlockedAdd);
    symbolTable.relateToOperator("InterlockedAnd",              EOpInterlockedAnd);
    symbolTable.relateToOperator("InterlockedCompareExchange",  EOpInterlockedCompareExchange);
    symbolTable.relateToOperator("InterlockedCompareStore",     EOpInterlockedCompareStore);
    symbolTable.relateToOperator("InterlockedExchange",         EOpInterlockedExchange);
    symbolTable.relateToOperator("InterlockedMax",              EOpInterlockedMax);
    symbolTable.relateToOperator("InterlockedMin",              EOpInterlockedMin);
    symbolTable.relateToOperator("InterlockedOr",               EOpInterlockedOr);
    symbolTable.relateToOperator("InterlockedXor",              EOpInterlockedXor);
    symbolTable.relateToOperator("isfinite",                    EOpIsFinite);
    symbolTable.relateToOperator("isinf",                       EOpIsInf);
    symbolTable.relateToOperator("isnan",                       EOpIsNan);
    symbolTable.relateToOperator("ldexp",                       EOpLdexp);
    symbolTable.relateToOperator("length",                      EOpLength);
    symbolTable.relateToOperator("lit",                         EOpLit);
    symbolTable.relateToOperator("log",                         EOpLog);
    symbolTable.relateToOperator("log10",                       EOpLog10);
    symbolTable.relateToOperator("log2",                        EOpLog2);
    // symbolTable.relateToOperator("mad");
    symbolTable.relateToOperator("max",                         EOpMax);
    symbolTable.relateToOperator("min",                         EOpMin);
    symbolTable.relateToOperator("modf",                        EOpModf);
    // symbolTable.relateToOperator("msad4",                       EOpMsad4);
    symbolTable.relateToOperator("mul",                         EOpGenMul);
    // symbolTable.relateToOperator("noise",                    EOpNoise); // TODO: check return type
    symbolTable.relateToOperator("normalize",                   EOpNormalize);
    symbolTable.relateToOperator("pow",                         EOpPow);
    // symbolTable.relateToOperator("printf",                     EOpPrintf);
    // symbolTable.relateToOperator("Process2DQuadTessFactorsAvg");
    // symbolTable.relateToOperator("Process2DQuadTessFactorsMax");
    // symbolTable.relateToOperator("Process2DQuadTessFactorsMin");
    // symbolTable.relateToOperator("ProcessIsolineTessFactors");
    // symbolTable.relateToOperator("ProcessQuadTessFactorsAvg");
    // symbolTable.relateToOperator("ProcessQuadTessFactorsMax");
    // symbolTable.relateToOperator("ProcessQuadTessFactorsMin");
    // symbolTable.relateToOperator("ProcessTriTessFactorsAvg");
    // symbolTable.relateToOperator("ProcessTriTessFactorsMax");
    // symbolTable.relateToOperator("ProcessTriTessFactorsMin");
    symbolTable.relateToOperator("radians",                     EOpRadians);
    symbolTable.relateToOperator("rcp",                         EOpRcp);
    symbolTable.relateToOperator("reflect",                     EOpReflect);
    symbolTable.relateToOperator("refract",                     EOpRefract);
    symbolTable.relateToOperator("reversebits",                 EOpBitFieldReverse);
    symbolTable.relateToOperator("round",                       EOpRoundEven);
    symbolTable.relateToOperator("rsqrt",                       EOpInverseSqrt);
    symbolTable.relateToOperator("saturate",                    EOpSaturate);
    symbolTable.relateToOperator("sign",                        EOpSign);
    symbolTable.relateToOperator("sin",                         EOpSin);
    symbolTable.relateToOperator("sincos",                      EOpSinCos);
    symbolTable.relateToOperator("sinh",                        EOpSinh);
    symbolTable.relateToOperator("smoothstep",                  EOpSmoothStep);
    symbolTable.relateToOperator("sqrt",                        EOpSqrt);
    symbolTable.relateToOperator("step",                        EOpStep);
    symbolTable.relateToOperator("tan",                         EOpTan);
    symbolTable.relateToOperator("tanh",                        EOpTanh);
    symbolTable.relateToOperator("tex1D",                       EOpTexture);
    // symbolTable.relateToOperator("tex1Dbias",                  // TODO:
    symbolTable.relateToOperator("tex1Dgrad",                   EOpTextureGrad);
    symbolTable.relateToOperator("tex1Dlod",                    EOpTextureLod);
    symbolTable.relateToOperator("tex1Dproj",                   EOpTextureProj);
    symbolTable.relateToOperator("tex2D",                       EOpTexture);
    // symbolTable.relateToOperator("tex2Dbias",                  // TODO:
    symbolTable.relateToOperator("tex2Dgrad",                   EOpTextureGrad);
    symbolTable.relateToOperator("tex2Dlod",                    EOpTextureLod);
    // symbolTable.relateToOperator("tex2Dproj",                   EOpTextureProj);
    symbolTable.relateToOperator("tex3D",                       EOpTexture);
    // symbolTable.relateToOperator("tex3Dbias");                // TODO
    symbolTable.relateToOperator("tex3Dgrad",                   EOpTextureGrad);
    symbolTable.relateToOperator("tex3Dlod",                    EOpTextureLod);
    // symbolTable.relateToOperator("tex3Dproj",                   EOpTextureProj);
    symbolTable.relateToOperator("texCUBE",                     EOpTexture);
    // symbolTable.relateToOperator("texCUBEbias",              // TODO
    symbolTable.relateToOperator("texCUBEgrad",                 EOpTextureGrad);
    symbolTable.relateToOperator("texCUBElod",                  EOpTextureLod);
    // symbolTable.relateToOperator("texCUBEproj",                 EOpTextureProj);
    symbolTable.relateToOperator("transpose",                   EOpTranspose);
    symbolTable.relateToOperator("trunc",                       EOpTrunc);
}

//
// Add context-dependent (resource-specific) built-ins not handled by the above.  These
// would be ones that need to be programmatically added because they cannot 
// be added by simple text strings.  For these, also
// 1) Map built-in functions to operators, for those that will turn into an operation node
//    instead of remaining a function call.
// 2) Tag extension-related symbols added to their base version with their extensions, so
//    that if an early version has the extension turned off, there is an error reported on use.
//
void TBuiltInParseablesHlsl::identifyBuiltIns(int version, EProfile profile, const SpvVersion& spvVersion, EShLanguage language,
                                              TSymbolTable& symbolTable, const TBuiltInResource &resources)
{
}


} // end namespace glslang
