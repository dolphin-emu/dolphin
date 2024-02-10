// Copyright 2012 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/OGL/OGLPerfQuery.h"

#include <memory>

#include "Common/CommonTypes.h"
#include "Common/GL/GLExtensions/GLExtensions.h"

#include "VideoBackends/OGL/OGLGfx.h"
#include "VideoCommon/FramebufferManager.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"

namespace OGL
{
std::unique_ptr<PerfQueryBase> GetPerfQuery(bool is_gles)
{
  if (is_gles && GLExtensions::Supports("GL_NV_occlusion_query_samples"))
    return std::make_unique<PerfQueryGLESNV>();
  else if (is_gles)
    return std::make_unique<PerfQueryGL>(GL_ANY_SAMPLES_PASSED);
  else
    return std::make_unique<PerfQueryGL>(GL_SAMPLES_PASSED);
}

PerfQuery::PerfQuery() : m_query_read_pos()
{
  ResetQuery();
}

void PerfQuery::EnableQuery(PerfQueryGroup group)
{
  m_query->EnableQuery(group);
}

void PerfQuery::DisableQuery(PerfQueryGroup group)
{
  m_query->DisableQuery(group);
}

bool PerfQuery::IsFlushed() const
{
  return m_query_count.load(std::memory_order_relaxed) == 0;
}

// TODO: could selectively flush things, but I don't think that will do much
void PerfQuery::FlushResults()
{
  m_query->FlushResults();
}

void PerfQuery::ResetQuery()
{
  m_query_count.store(0, std::memory_order_relaxed);
  for (size_t i = 0; i < m_results.size(); ++i)
    m_results[i].store(0, std::memory_order_relaxed);
}

u32 PerfQuery::GetQueryResult(PerfQueryType type)
{
  u32 result = 0;

  if (type == PQ_ZCOMP_INPUT_ZCOMPLOC || type == PQ_ZCOMP_OUTPUT_ZCOMPLOC)
  {
    result = m_results[PQG_ZCOMP_ZCOMPLOC].load(std::memory_order_relaxed);
  }
  else if (type == PQ_ZCOMP_INPUT || type == PQ_ZCOMP_OUTPUT)
  {
    result = m_results[PQG_ZCOMP].load(std::memory_order_relaxed);
  }
  else if (type == PQ_BLEND_INPUT)
  {
    result = m_results[PQG_ZCOMP].load(std::memory_order_relaxed) +
             m_results[PQG_ZCOMP_ZCOMPLOC].load(std::memory_order_relaxed);
  }
  else if (type == PQ_EFB_COPY_CLOCKS)
  {
    result = m_results[PQG_EFB_COPY_CLOCKS].load(std::memory_order_relaxed);
  }

  return result / 4;
}

// Implementations
PerfQueryGL::PerfQueryGL(GLenum query_type) : m_query_type(query_type)
{
  for (ActiveQuery& query : m_query_buffer)
    glGenQueries(1, &query.query_id);
}

PerfQueryGL::~PerfQueryGL()
{
  for (ActiveQuery& query : m_query_buffer)
    glDeleteQueries(1, &query.query_id);
}

void PerfQueryGL::EnableQuery(PerfQueryGroup group)
{
  u32 query_count = m_query_count.load(std::memory_order_relaxed);

  // Is this sane?
  if (query_count > m_query_buffer.size() / 2)
  {
    WeakFlush();
    query_count = m_query_count.load(std::memory_order_relaxed);
  }

  if (m_query_buffer.size() == query_count)
  {
    FlushOne();
    query_count = m_query_count.load(std::memory_order_relaxed);
    // ERROR_LOG_FMT(VIDEO, "Flushed query buffer early!");
  }

  // start query
  if (group == PQG_ZCOMP_ZCOMPLOC || group == PQG_ZCOMP)
  {
    auto& entry = m_query_buffer[(m_query_read_pos + query_count) % m_query_buffer.size()];

    glBeginQuery(m_query_type, entry.query_id);
    entry.query_group = group;

    m_query_count.fetch_add(1, std::memory_order_relaxed);
  }
}
void PerfQueryGL::DisableQuery(PerfQueryGroup group)
{
  // stop query
  if (group == PQG_ZCOMP_ZCOMPLOC || group == PQG_ZCOMP)
  {
    glEndQuery(m_query_type);
  }
}

void PerfQueryGL::WeakFlush()
{
  while (!IsFlushed())
  {
    auto& entry = m_query_buffer[m_query_read_pos];

    GLuint result = GL_FALSE;
    glGetQueryObjectuiv(entry.query_id, GL_QUERY_RESULT_AVAILABLE, &result);

    if (GL_TRUE == result)
    {
      FlushOne();
    }
    else
    {
      break;
    }
  }
}

void PerfQueryGL::FlushOne()
{
  auto& entry = m_query_buffer[m_query_read_pos];

  GLuint result = 0;
  glGetQueryObjectuiv(entry.query_id, GL_QUERY_RESULT, &result);

  // NOTE: Reported pixel metrics should be referenced to native resolution
  // TODO: Dropping the lower 2 bits from this count should be closer to actual
  // hardware behavior when drawing triangles.
  result = static_cast<u64>(result) * EFB_WIDTH * EFB_HEIGHT /
           (g_framebuffer_manager->GetEFBWidth() * g_framebuffer_manager->GetEFBHeight());

  // Adjust for multisampling
  if (g_ActiveConfig.iMultisamples > 1)
    result /= g_ActiveConfig.iMultisamples;

  m_results[entry.query_group].fetch_add(result, std::memory_order_relaxed);

  m_query_read_pos = (m_query_read_pos + 1) % m_query_buffer.size();
  m_query_count.fetch_sub(1, std::memory_order_relaxed);
}

// TODO: could selectively flush things, but I don't think that will do much
void PerfQueryGL::FlushResults()
{
  while (!IsFlushed())
    FlushOne();
}

PerfQueryGLESNV::PerfQueryGLESNV()
{
  for (ActiveQuery& query : m_query_buffer)
    glGenOcclusionQueriesNV(1, &query.query_id);
}

PerfQueryGLESNV::~PerfQueryGLESNV()
{
  for (ActiveQuery& query : m_query_buffer)
    glDeleteOcclusionQueriesNV(1, &query.query_id);
}

void PerfQueryGLESNV::EnableQuery(PerfQueryGroup group)
{
  u32 query_count = m_query_count.load(std::memory_order_relaxed);

  // Is this sane?
  if (query_count > m_query_buffer.size() / 2)
  {
    WeakFlush();
    query_count = m_query_count.load(std::memory_order_relaxed);
  }

  if (m_query_buffer.size() == query_count)
  {
    FlushOne();
    query_count = m_query_count.load(std::memory_order_relaxed);
    // ERROR_LOG_FMT(VIDEO, "Flushed query buffer early!");
  }

  // start query
  if (group == PQG_ZCOMP_ZCOMPLOC || group == PQG_ZCOMP)
  {
    auto& entry = m_query_buffer[(m_query_read_pos + query_count) % m_query_buffer.size()];

    glBeginOcclusionQueryNV(entry.query_id);
    entry.query_group = group;

    m_query_count.fetch_add(1, std::memory_order_relaxed);
  }
}
void PerfQueryGLESNV::DisableQuery(PerfQueryGroup group)
{
  // stop query
  if (group == PQG_ZCOMP_ZCOMPLOC || group == PQG_ZCOMP)
  {
    glEndOcclusionQueryNV();
  }
}

void PerfQueryGLESNV::WeakFlush()
{
  while (!IsFlushed())
  {
    auto& entry = m_query_buffer[m_query_read_pos];

    GLuint result = GL_FALSE;
    glGetOcclusionQueryuivNV(entry.query_id, GL_PIXEL_COUNT_AVAILABLE_NV, &result);

    if (GL_TRUE == result)
    {
      FlushOne();
    }
    else
    {
      break;
    }
  }
}

void PerfQueryGLESNV::FlushOne()
{
  auto& entry = m_query_buffer[m_query_read_pos];

  GLuint result = 0;
  glGetOcclusionQueryuivNV(entry.query_id, GL_OCCLUSION_TEST_RESULT_HP, &result);

  // NOTE: Reported pixel metrics should be referenced to native resolution
  // TODO: Dropping the lower 2 bits from this count should be closer to actual
  // hardware behavior when drawing triangles.
  const u64 native_res_result =
      static_cast<u64>(result) * EFB_WIDTH * EFB_HEIGHT /
      (g_framebuffer_manager->GetEFBWidth() * g_framebuffer_manager->GetEFBHeight());
  m_results[entry.query_group].fetch_add(static_cast<u32>(native_res_result),
                                         std::memory_order_relaxed);

  m_query_read_pos = (m_query_read_pos + 1) % m_query_buffer.size();
  m_query_count.fetch_sub(1, std::memory_order_relaxed);
}

// TODO: could selectively flush things, but I don't think that will do much
void PerfQueryGLESNV::FlushResults()
{
  while (!IsFlushed())
    FlushOne();
}

}  // namespace OGL
