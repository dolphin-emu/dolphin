#pragma once
#include <array>
#include "Common/GL/GLExtensions/GLExtensions.h"

#ifndef GL_TIME_ELAPSED
#define GL_TIME_ELAPSED 0x88BF
#endif

namespace OGL
{
// NOTE: Only one timer can be active at any time, due to using GL_TIME_ELAPSED.
// This is not enforced by the class, however.
class GPUTimer final
{
public:
  GPUTimer()
  {
    glGenQueries(1, &m_query_id);
    Begin();
  }

  ~GPUTimer()
  {
    End();
    glDeleteQueries(1, &m_query_id);
  }

  void Begin()
  {
    if (m_started)
      glEndQuery(GL_TIME_ELAPSED);

    glBeginQuery(GL_TIME_ELAPSED, m_query_id);
    m_started = true;
  }

  void End()
  {
    if (!m_started)
      return;

    glEndQuery(GL_TIME_ELAPSED);
    m_started = false;
  }

  double GetTimeSeconds()
  {
    GetResult();
    return static_cast<double>(m_result) / 1000000000.0;
  }

  double GetTimeMilliseconds()
  {
    GetResult();
    return static_cast<double>(m_result) / 1000000.0;
  }

  u32 GetTimeNanoseconds()
  {
    GetResult();
    return m_result;
  }

private:
  void GetResult()
  {
    if (m_has_result)
      return;

    if (m_started)
      End();

    glGetQueryObjectuiv(m_query_id, GL_QUERY_RESULT, &m_result);
    m_has_result = true;
  }

  GLuint m_query_id;
  GLuint m_result = 0;
  bool m_started = false;
  bool m_has_result = false;
};
}  // namespace OGL
