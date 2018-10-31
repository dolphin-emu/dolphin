// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/GL/GLExtensions/GLExtensions.h"

#ifndef GL_TIME_ELAPSED
#define GL_TIME_ELAPSED 0x88BF
#endif

namespace OGL
{
/*
 * This class can be used to measure the time it takes for the GPU to perform a draw call
 * or compute dispatch. To use:
 *
 *   - Create an instance of GPUTimer before issuing the draw call.
 *     (this can be before or after any binding that needs to be done)
 *
 *   - (optionally) call Begin(). This is not needed for a single draw call.
 *
 *   - Issue the draw call or compute dispatch as normal.
 *
 *   - (optionally) call End(). This is not necessary for a single draw call.
 *
 *   - Call GetTime{Seconds,Milliseconds,Nanoseconds} to determine how long the operation
 *     took to execute on the GPU.
 *
 * NOTE: When the timer is read back, this will force a GL flush, so the more often a timer is used,
 * the larger of a performance impact it will have. Only one timer can be active at any time, due to
 * using GL_TIME_ELAPSED. This is not enforced by the class, however.
 *
 */
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
