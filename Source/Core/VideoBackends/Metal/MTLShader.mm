
#include "VideoBackends/Metal/MTLShader.h"

#include "VideoBackends/Metal/MTLObjectCache.h"

Metal::Shader::Shader(ShaderStage stage, std::string msl, MRCOwned<id<MTLFunction>> shader)
    : AbstractShader(stage), m_msl(std::move(msl)), m_shader(std::move(shader))
{
}

Metal::Shader::~Shader()
{
  g_object_cache->ShaderDestroyed(this);
}

AbstractShader::BinaryData Metal::Shader::GetBinary() const
{
  return BinaryData(m_msl.begin(), m_msl.end());
}
