// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/Metal/MTLUtil.h"

#include <fstream>
#include <string>

#include <TargetConditionals.h>
#include <spirv_msl.hpp>

#include "Common/MsgHandler.h"

#include "VideoCommon/DriverDetails.h"
#include "VideoCommon/Spirv.h"

Metal::DeviceFeatures Metal::g_features;

std::vector<MRCOwned<id<MTLDevice>>> Metal::Util::GetAdapterList()
{
  std::vector<MRCOwned<id<MTLDevice>>> list;
  id<MTLDevice> default_dev = MTLCreateSystemDefaultDevice();
  if (default_dev)
    list.push_back(MRCTransfer(default_dev));

#if TARGET_OS_OSX
  auto devices = MRCTransfer(MTLCopyAllDevices());
  for (id<MTLDevice> device in devices.Get())
  {
    if (device != default_dev)
      list.push_back(MRCRetain(device));
  }
#endif

  return list;
}

void Metal::Util::PopulateBackendInfo(VideoConfig* config)
{
  config->backend_info.api_type = APIType::Metal;
  config->backend_info.bUsesLowerLeftOrigin = false;
  config->backend_info.bSupportsExclusiveFullscreen = false;
  config->backend_info.bSupportsDualSourceBlend = true;
  config->backend_info.bSupportsPrimitiveRestart = true;
  config->backend_info.bSupportsGeometryShaders = false;
  config->backend_info.bSupportsComputeShaders = true;
  config->backend_info.bSupports3DVision = false;
  config->backend_info.bSupportsEarlyZ = true;
  config->backend_info.bSupportsBindingLayout = true;
  config->backend_info.bSupportsBBox = true;
  config->backend_info.bSupportsGSInstancing = false;
  config->backend_info.bSupportsPostProcessing = true;
  config->backend_info.bSupportsPaletteConversion = true;
  config->backend_info.bSupportsClipControl = true;
  config->backend_info.bSupportsSSAA = true;
  config->backend_info.bSupportsFragmentStoresAndAtomics = true;
  config->backend_info.bSupportsReversedDepthRange = false;
  config->backend_info.bSupportsLogicOp = false;
  config->backend_info.bSupportsMultithreading = false;
  config->backend_info.bSupportsGPUTextureDecoding = true;
  config->backend_info.bSupportsCopyToVram = true;
  config->backend_info.bSupportsBitfield = true;
  config->backend_info.bSupportsDynamicSamplerIndexing = true;
  config->backend_info.bSupportsFramebufferFetch = false;
  config->backend_info.bSupportsBackgroundCompiling = true;
  config->backend_info.bSupportsLargePoints = true;
  config->backend_info.bSupportsPartialDepthCopies = true;
  config->backend_info.bSupportsDepthReadback = true;
  config->backend_info.bSupportsShaderBinaries = false;
  config->backend_info.bSupportsPipelineCacheData = false;
  config->backend_info.bSupportsCoarseDerivatives = false;
  config->backend_info.bSupportsTextureQueryLevels = true;
  config->backend_info.bSupportsLodBiasInSampler = false;
  config->backend_info.bSupportsSettingObjectNames = true;
  // Metal requires multisample resolve to be done on a render pass
  config->backend_info.bSupportsPartialMultisampleResolve = false;
  config->backend_info.bSupportsDynamicVertexLoader = true;
  config->backend_info.bSupportsVSLinePointExpand = true;
}

void Metal::Util::PopulateBackendInfoAdapters(VideoConfig* config,
                                              const std::vector<MRCOwned<id<MTLDevice>>>& adapters)
{
  config->backend_info.Adapters.clear();
  for (id<MTLDevice> adapter : adapters)
  {
    config->backend_info.Adapters.push_back([[adapter name] UTF8String]);
  }
}

static bool DetectIntelGPUFBFetch(id<MTLDevice> dev)
{
  // Even though it's nowhere in the feature set tables, some Intel GPUs support fbfetch!
  // Annoyingly, the Haswell compiler successfully makes a pipeline but actually miscompiles it and
  // doesn't insert any fbfetch instructions.
  // The Broadwell compiler inserts the Skylake fbfetch instruction,
  // but Broadwell doesn't support that.  It seems to make the shader not do anything.
  // So we actually have to test the thing

  static constexpr const char* shader = R"(
vertex float4 fs_triangle(uint vid [[vertex_id]]) {
  return float4(vid & 1 ? 3 : -1, vid & 2 ? 3 : -1, 0, 1);
}
fragment float4 fbfetch_test(float4 in [[color(0), raster_order_group(0)]]) {
  return in * 2;
}
)";
  auto lib = MRCTransfer([dev newLibraryWithSource:[NSString stringWithUTF8String:shader]
                                           options:nil
                                             error:nil]);
  if (!lib)
    return false;
  auto pdesc = MRCTransfer([MTLRenderPipelineDescriptor new]);
  [pdesc setVertexFunction:MRCTransfer([lib newFunctionWithName:@"fs_triangle"])];
  [pdesc setFragmentFunction:MRCTransfer([lib newFunctionWithName:@"fbfetch_test"])];
  [[pdesc colorAttachments][0] setPixelFormat:MTLPixelFormatRGBA8Unorm];
  auto pipe = MRCTransfer([dev newRenderPipelineStateWithDescriptor:pdesc error:nil]);
  if (!pipe)
    return false;
  auto buf = MRCTransfer([dev newBufferWithLength:4 options:MTLResourceStorageModeShared]);
  auto tdesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                  width:1
                                                                 height:1
                                                              mipmapped:false];
  [tdesc setUsage:MTLTextureUsageRenderTarget];
  auto tex = MRCTransfer([dev newTextureWithDescriptor:tdesc]);
  auto q = MRCTransfer([dev newCommandQueue]);
  u32 px = 0x11223344;
  memcpy([buf contents], &px, 4);
  id<MTLCommandBuffer> cmdbuf = [q commandBuffer];
  id<MTLBlitCommandEncoder> upload_encoder = [cmdbuf blitCommandEncoder];
  [upload_encoder copyFromBuffer:buf
                    sourceOffset:0
               sourceBytesPerRow:4
             sourceBytesPerImage:4
                      sourceSize:MTLSizeMake(1, 1, 1)
                       toTexture:tex
                destinationSlice:0
                destinationLevel:0
               destinationOrigin:MTLOriginMake(0, 0, 0)];
  [upload_encoder endEncoding];
  auto rpdesc = MRCTransfer([MTLRenderPassDescriptor new]);
  [[rpdesc colorAttachments][0] setTexture:tex];
  [[rpdesc colorAttachments][0] setLoadAction:MTLLoadActionLoad];
  [[rpdesc colorAttachments][0] setStoreAction:MTLStoreActionStore];
  id<MTLRenderCommandEncoder> renc = [cmdbuf renderCommandEncoderWithDescriptor:rpdesc];
  [renc setRenderPipelineState:pipe];
  [renc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
  [renc endEncoding];
  id<MTLBlitCommandEncoder> download_encoder = [cmdbuf blitCommandEncoder];
  [download_encoder copyFromTexture:tex
                        sourceSlice:0
                        sourceLevel:0
                       sourceOrigin:MTLOriginMake(0, 0, 0)
                         sourceSize:MTLSizeMake(1, 1, 1)
                           toBuffer:buf
                  destinationOffset:0
             destinationBytesPerRow:4
           destinationBytesPerImage:4];
  [download_encoder endEncoding];
  [cmdbuf commit];
  [cmdbuf waitUntilCompleted];
  u32 outpx;
  memcpy(&outpx, [buf contents], 4);
  // Proper fbfetch will double contents, Haswell will return black, and Broadwell will do nothing
  if (outpx == 0x22446688)
    return true;  // Skylake+
  else if (outpx == 0x11223344)
    return false;  // Broadwell
  else
    return false;  // Haswell
}

void Metal::Util::PopulateBackendInfoFeatures(VideoConfig* config, id<MTLDevice> device)
{
  // Initialize DriverDetails first so we can use it later
  DriverDetails::Vendor vendor = DriverDetails::VENDOR_UNKNOWN;
  if ([[device name] containsString:@"NVIDIA"])
    vendor = DriverDetails::VENDOR_NVIDIA;
  else if ([[device name] containsString:@"AMD"])
    vendor = DriverDetails::VENDOR_ATI;
  else if ([[device name] containsString:@"Intel"])
    vendor = DriverDetails::VENDOR_INTEL;
  else if ([[device name] containsString:@"Apple"])
    vendor = DriverDetails::VENDOR_APPLE;
  const NSOperatingSystemVersion cocoa_ver = [[NSProcessInfo processInfo] operatingSystemVersion];
  double version = cocoa_ver.majorVersion * 100 + cocoa_ver.minorVersion;
  DriverDetails::Init(DriverDetails::API_METAL, vendor, DriverDetails::DRIVER_APPLE, version,
                      DriverDetails::Family::UNKNOWN);

#if TARGET_OS_OSX
  config->backend_info.bSupportsDepthClamp = true;
  config->backend_info.bSupportsST3CTextures = true;
  config->backend_info.bSupportsBPTCTextures = true;
#else
  bool supports_apple4 = false;
  bool supports_bcn = false;
  if (@available(iOS 13, *))
    supports_apple4 = [device supportsFamily:MTLGPUFamilyApple4];
  else
    supports_apple4 = [device supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily4_v1];
  if (@available(iOS 16.4, *))
    supports_bcn = [device supportsBCTextureCompression];
  config->backend_info.bSupportsDepthClamp = supports_apple4;
  config->backend_info.bSupportsST3CTextures = supports_bcn;
  config->backend_info.bSupportsBPTCTextures = supports_bcn;

  config->backend_info.bSupportsFramebufferFetch = true;
#endif

  config->backend_info.AAModes.clear();
  for (u32 i = 1; i <= 64; i <<= 1)
  {
    if ([device supportsTextureSampleCount:i])
      config->backend_info.AAModes.push_back(i);
  }

  switch (config->iManuallyUploadBuffers)
  {
  case TriState::Off:
    g_features.manual_buffer_upload = false;
    break;
  case TriState::On:
    g_features.manual_buffer_upload = true;
    break;
  case TriState::Auto:
#if TARGET_OS_OSX
    g_features.manual_buffer_upload = false;
    if (@available(macOS 10.15, *))
      if (![device hasUnifiedMemory])
        g_features.manual_buffer_upload = true;
#else
    // All iOS devices have unified memory
    g_features.manual_buffer_upload = false;
#endif
    break;
  }

  g_features.subgroup_ops = false;
  if (@available(macOS 10.15, iOS 13, *))
  {
    // Requires SIMD-scoped reduction operations
    g_features.subgroup_ops =
        [device supportsFamily:MTLGPUFamilyMac2] || [device supportsFamily:MTLGPUFamilyApple6];
    config->backend_info.bSupportsFramebufferFetch = [device supportsFamily:MTLGPUFamilyApple1];
  }
  if (DriverDetails::HasBug(DriverDetails::BUG_BROKEN_SUBGROUP_OPS))
    g_features.subgroup_ops = false;
#if TARGET_OS_OSX
  if (@available(macOS 11, *))
    if (vendor == DriverDetails::VENDOR_INTEL)
      config->backend_info.bSupportsFramebufferFetch |= DetectIntelGPUFBFetch(device);
#endif
  if (DriverDetails::HasBug(DriverDetails::BUG_BROKEN_DYNAMIC_SAMPLER_INDEXING))
    config->backend_info.bSupportsDynamicSamplerIndexing = false;
}

// clang-format off

AbstractTextureFormat Metal::Util::ToAbstract(MTLPixelFormat format)
{
  switch (format)
  {
  case MTLPixelFormatRGBA8Unorm:            return AbstractTextureFormat::RGBA8;
  case MTLPixelFormatBGRA8Unorm:            return AbstractTextureFormat::BGRA8;
  case MTLPixelFormatBC1_RGBA:              return AbstractTextureFormat::DXT1;
  case MTLPixelFormatBC2_RGBA:              return AbstractTextureFormat::DXT3;
  case MTLPixelFormatBC3_RGBA:              return AbstractTextureFormat::DXT5;
  case MTLPixelFormatBC7_RGBAUnorm:         return AbstractTextureFormat::BPTC;
  case MTLPixelFormatR16Unorm:              return AbstractTextureFormat::R16;
  case MTLPixelFormatDepth16Unorm:          return AbstractTextureFormat::D16;
#if TARGET_OS_OSX
  case MTLPixelFormatDepth24Unorm_Stencil8: return AbstractTextureFormat::D24_S8;
#endif
  case MTLPixelFormatR32Float:              return AbstractTextureFormat::R32F;
  case MTLPixelFormatDepth32Float:          return AbstractTextureFormat::D32F;
  case MTLPixelFormatDepth32Float_Stencil8: return AbstractTextureFormat::D32F_S8;
  default:                                  return AbstractTextureFormat::Undefined;
  }
}

// Don't complain about BCn formats requiring iOS 16.4, these are just enum conversions
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability"

MTLPixelFormat Metal::Util::FromAbstract(AbstractTextureFormat format)
{
  switch (format)
  {
  case AbstractTextureFormat::RGBA8:     return MTLPixelFormatRGBA8Unorm;
  case AbstractTextureFormat::BGRA8:     return MTLPixelFormatBGRA8Unorm;
  case AbstractTextureFormat::DXT1:      return MTLPixelFormatBC1_RGBA;
  case AbstractTextureFormat::DXT3:      return MTLPixelFormatBC2_RGBA;
  case AbstractTextureFormat::DXT5:      return MTLPixelFormatBC3_RGBA;
  case AbstractTextureFormat::BPTC:      return MTLPixelFormatBC7_RGBAUnorm;
  case AbstractTextureFormat::R16:       return MTLPixelFormatR16Unorm;
  case AbstractTextureFormat::D16:       return MTLPixelFormatDepth16Unorm;
#if TARGET_OS_OSX
  case AbstractTextureFormat::D24_S8:    return MTLPixelFormatDepth24Unorm_Stencil8;
#endif
  case AbstractTextureFormat::R32F:      return MTLPixelFormatR32Float;
  case AbstractTextureFormat::D32F:      return MTLPixelFormatDepth32Float;
  case AbstractTextureFormat::D32F_S8:   return MTLPixelFormatDepth32Float_Stencil8;
  default:                               return MTLPixelFormatInvalid;
  }
}

#pragma clang diagnostic pop

// clang-format on

// MARK: Shader Translation

static const std::string_view SHADER_HEADER = R"(
// Target GLSL 4.5.
#version 450 core
// Always available on Metal
#extension GL_EXT_shader_8bit_storage : require
#extension GL_EXT_shader_16bit_storage : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require

#define ATTRIBUTE_LOCATION(x) layout(location = x)
#define FRAGMENT_OUTPUT_LOCATION(x) layout(location = x)
#define FRAGMENT_OUTPUT_LOCATION_INDEXED(x, y) layout(location = x, index = y)
#define UBO_BINDING(packing, x) layout(packing, set = 0, binding = (x - 1))
#define SAMPLER_BINDING(x) layout(set = 1, binding = x)
#define TEXEL_BUFFER_BINDING(x) layout(set = 1, binding = (x + 8))
#define SSBO_BINDING(x) layout(std430, set = 2, binding = x)
#define INPUT_ATTACHMENT_BINDING(x, y, z) layout(set = x, binding = y, input_attachment_index = z)
#define VARYING_LOCATION(x) layout(location = x)
#define FORCE_EARLY_Z layout(early_fragment_tests) in

// Metal framebuffer fetch helpers.
#define FB_FETCH_VALUE subpassLoad(in_ocol0)

// hlsl to glsl function translation
#define API_METAL 1
#define float2 vec2
#define float3 vec3
#define float4 vec4
#define uint2 uvec2
#define uint3 uvec3
#define uint4 uvec4
#define int2 ivec2
#define int3 ivec3
#define int4 ivec4
#define frac fract
#define lerp mix

// These were changed in Vulkan
#define gl_VertexID gl_VertexIndex
#define gl_InstanceID gl_InstanceIndex
)";
static const std::string_view COMPUTE_SHADER_HEADER = R"(
// Target GLSL 4.5.
#version 450 core
// Always available on Metal
#extension GL_EXT_shader_8bit_storage : require
#extension GL_EXT_shader_16bit_storage : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require

// All resources are packed into one descriptor set for compute.
#define UBO_BINDING(packing, x) layout(packing, set = 0, binding = (x - 1))
#define SSBO_BINDING(x) layout(std430, set = 2, binding = x)
#define IMAGE_BINDING(format, x) layout(format, set = 1, binding = x)

// hlsl to glsl function translation
#define API_METAL 1
#define float2 vec2
#define float3 vec3
#define float4 vec4
#define uint2 uvec2
#define uint3 uvec3
#define uint4 uvec4
#define int2 ivec2
#define int3 ivec3
#define int4 ivec4
#define frac fract
#define lerp mix
)";
static const std::string_view SUBGROUP_HELPER_HEADER = R"(
#extension GL_KHR_shader_subgroup_basic : enable
#extension GL_KHR_shader_subgroup_arithmetic : enable
#extension GL_KHR_shader_subgroup_ballot : enable

#define SUPPORTS_SUBGROUP_REDUCTION 1
#define IS_HELPER_INVOCATION gl_HelperInvocation
#define IS_FIRST_ACTIVE_INVOCATION (subgroupElect())
#define SUBGROUP_MIN(value) value = subgroupMin(value)
#define SUBGROUP_MAX(value) value = subgroupMax(value)
)";

static const std::string_view MSL_HEADER =
    // We know our shader generator leaves unused variables.
    "#pragma clang diagnostic ignored \"-Wunused-variable\"\n"
    // These are usually when the compiler doesn't think a switch is exhaustive
    "#pragma clang diagnostic ignored \"-Wreturn-type\"\n";

static constexpr std::pair<std::string_view, std::string_view> MSL_FIXUPS[] = {
    // Force-unroll the lighting loop in ubershaders, which greatly reduces register pressure on AMD
    {"for (uint chan = 0u; chan < 2u; chan++)",
     "_Pragma(\"unroll\") for (uint chan = 0u; chan < 2u; chan++)"},
};

static constexpr spirv_cross::MSLResourceBinding
MakeResourceBinding(spv::ExecutionModel stage, u32 set, u32 binding,  //
                    u32 msl_buffer, u32 msl_texture, u32 msl_sampler)
{
  spirv_cross::MSLResourceBinding resource;
  resource.stage = stage;
  resource.desc_set = set;
  resource.binding = binding;
  resource.msl_buffer = msl_buffer;
  resource.msl_texture = msl_texture;
  resource.msl_sampler = msl_sampler;
  return resource;
}

std::optional<std::string> Metal::Util::TranslateShaderToMSL(ShaderStage stage,
                                                             std::string_view source)
{
  std::string full_source;

  std::string_view header = stage == ShaderStage::Compute ? COMPUTE_SHADER_HEADER : SHADER_HEADER;
  full_source.reserve(header.size() + SUBGROUP_HELPER_HEADER.size() + source.size());

  full_source.append(header);
  if (Metal::g_features.subgroup_ops)
    full_source.append(SUBGROUP_HELPER_HEADER);
  full_source.append(source);

  std::optional<SPIRV::CodeVector> code;
  switch (stage)
  {
  case ShaderStage::Vertex:
    code = SPIRV::CompileVertexShader(full_source, APIType::Metal, glslang::EShTargetSpv_1_3);
    break;
  case ShaderStage::Geometry:
    PanicAlertFmt("Tried to compile geometry shader for Metal, but Metal doesn't support them!");
    break;
  case ShaderStage::Pixel:
    code = SPIRV::CompileFragmentShader(full_source, APIType::Metal, glslang::EShTargetSpv_1_3);
    break;
  case ShaderStage::Compute:
    code = SPIRV::CompileComputeShader(full_source, APIType::Metal, glslang::EShTargetSpv_1_3);
    break;
  }
  if (!code.has_value())
    return std::nullopt;

  // clang-format off
  static const spirv_cross::MSLResourceBinding resource_bindings[] = {
      MakeResourceBinding(spv::ExecutionModelVertex,    0, 0, 1, 0, 0), // vs/ubo
      MakeResourceBinding(spv::ExecutionModelVertex,    0, 1, 1, 0, 0), // vs/ubo
      MakeResourceBinding(spv::ExecutionModelVertex,    0, 2, 2, 0, 0), // vs/ubo
      MakeResourceBinding(spv::ExecutionModelVertex,    2, 1, 0, 0, 0), // vs/ssbo
      MakeResourceBinding(spv::ExecutionModelFragment,  0, 0, 0, 0, 0), // vs/ubo
      MakeResourceBinding(spv::ExecutionModelFragment,  0, 1, 1, 0, 0), // vs/ubo
      MakeResourceBinding(spv::ExecutionModelFragment,  1, 0, 0, 0, 0), // ps/samp0
      MakeResourceBinding(spv::ExecutionModelFragment,  1, 1, 0, 1, 1), // ps/samp1
      MakeResourceBinding(spv::ExecutionModelFragment,  1, 2, 0, 2, 2), // ps/samp2
      MakeResourceBinding(spv::ExecutionModelFragment,  1, 3, 0, 3, 3), // ps/samp3
      MakeResourceBinding(spv::ExecutionModelFragment,  1, 4, 0, 4, 4), // ps/samp4
      MakeResourceBinding(spv::ExecutionModelFragment,  1, 5, 0, 5, 5), // ps/samp5
      MakeResourceBinding(spv::ExecutionModelFragment,  1, 6, 0, 6, 6), // ps/samp6
      MakeResourceBinding(spv::ExecutionModelFragment,  1, 7, 0, 7, 7), // ps/samp7
      MakeResourceBinding(spv::ExecutionModelFragment,  1, 8, 0, 8, 8), // ps/samp8
      MakeResourceBinding(spv::ExecutionModelFragment,  2, 0, 2, 0, 0), // ps/ssbo
      MakeResourceBinding(spv::ExecutionModelGLCompute, 0, 1, 0, 0, 0), // cs/ubo
      MakeResourceBinding(spv::ExecutionModelGLCompute, 1, 0, 0, 0, 0), // cs/output_image
      MakeResourceBinding(spv::ExecutionModelGLCompute, 2, 0, 2, 0, 0), // cs/ssbo
      MakeResourceBinding(spv::ExecutionModelGLCompute, 2, 1, 3, 0, 0), // cs/ssbo
  };

  spirv_cross::CompilerMSL::Options options;
#if TARGET_OS_OSX
  options.platform = spirv_cross::CompilerMSL::Options::macOS;
#elif TARGET_OS_IOS
  options.platform = spirv_cross::CompilerMSL::Options::iOS;
#else
  #error What platform is this?
#endif
  // clang-format on

  spirv_cross::CompilerMSL compiler(std::move(*code));

  if (@available(macOS 11, iOS 14, *))
    options.set_msl_version(2, 3);
  else if (@available(macOS 10.15, iOS 13, *))
    options.set_msl_version(2, 2);
  else if (@available(macOS 10.14, iOS 12, *))
    options.set_msl_version(2, 1);
  else
    options.set_msl_version(2, 0);
  options.use_framebuffer_fetch_subpasses = true;
  compiler.set_msl_options(options);

  for (auto& binding : resource_bindings)
    compiler.add_msl_resource_binding(binding);

  std::string output(MSL_HEADER);
  std::string compiled = compiler.compile();
  std::string_view remaining = compiled;
  while (!remaining.empty())
  {
    // Apply fixups
    std::string_view piece = remaining;
    std::string_view fixup_piece = {};
    size_t next = piece.size();
    for (const auto& fixup : MSL_FIXUPS)
    {
      size_t found = piece.find(fixup.first);
      if (found == std::string_view::npos)
        continue;
      piece = piece.substr(0, found);
      fixup_piece = fixup.second;
      next = found + fixup.first.size();
    }
    output += piece;
    output += fixup_piece;
    remaining = remaining.substr(next);
  }
  return output;
}
