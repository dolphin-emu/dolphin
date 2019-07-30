#pragma once
#include <string>
#include "VideoCommon/VideoCommon.h"

enum class EFBReinterpretType;
enum class TextureFormat;

namespace FramebufferShaderGen
{
struct Config
{
  Config(APIType api_type_, u32 efb_samples_, u32 efb_layers_, bool ssaa_)
      : api_type(api_type_), efb_samples(efb_samples_), efb_layers(efb_layers_), ssaa(ssaa_)
  {
  }

  APIType api_type;
  u32 efb_samples;
  u32 efb_layers;
  bool ssaa;
};

std::string GenerateScreenQuadVertexShader();
std::string GenerateTextureCopyVertexShader();
std::string GenerateTextureCopyPixelShader();
std::string GenerateResolveDepthPixelShader(u32 samples);
std::string GenerateClearVertexShader();
std::string GenerateEFBPokeVertexShader();
std::string GenerateColorPixelShader();
std::string GenerateFormatConversionShader(EFBReinterpretType convtype, u32 samples);
std::string GenerateTextureReinterpretShader(TextureFormat from_format, TextureFormat to_format);

}  // namespace FramebufferShaderGen
