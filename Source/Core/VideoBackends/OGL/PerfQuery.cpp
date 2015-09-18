// Copyright 2012 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/GL/GLInterfaceBase.h"
#include "Common/GL/GLUtil.h"

#include "VideoBackends/OGL/PerfQuery.h"
#include "VideoCommon/RenderBase.h"

namespace OGL
{
PerfQueryBase* GetPerfQuery()
{
	if (GLInterface->GetMode() == GLInterfaceMode::MODE_OPENGLES3 &&
	    GLExtensions::Supports("GL_NV_occlusion_query_samples"))
		return new PerfQueryGLESNV();
	else if (GLInterface->GetMode() == GLInterfaceMode::MODE_OPENGLES3)
		return new PerfQueryGL(GL_ANY_SAMPLES_PASSED);
	else
		return new PerfQueryGL(GL_SAMPLES_PASSED);
}

PerfQuery::PerfQuery()
	: m_query_read_pos()
{
	ResetQuery();
}

void PerfQuery::EnableQuery(PerfQueryGroup type)
{
	m_query->EnableQuery(type);
}

void PerfQuery::DisableQuery(PerfQueryGroup type)
{
	m_query->DisableQuery(type);
}

bool PerfQuery::IsFlushed() const
{
	return 0 == m_query_count;
}

// TODO: could selectively flush things, but I don't think that will do much
void PerfQuery::FlushResults()
{
	m_query->FlushResults();
}

void PerfQuery::ResetQuery()
{
	m_query_count = 0;
	std::fill_n(m_results, ArraySize(m_results), 0);
}

u32 PerfQuery::GetQueryResult(PerfQueryType type)
{
	u32 result = 0;

	if (type == PQ_ZCOMP_INPUT_ZCOMPLOC || type == PQ_ZCOMP_OUTPUT_ZCOMPLOC)
	{
		result = m_results[PQG_ZCOMP_ZCOMPLOC];
	}
	else if (type == PQ_ZCOMP_INPUT || type == PQ_ZCOMP_OUTPUT)
	{
		result = m_results[PQG_ZCOMP];
	}
	else if (type == PQ_BLEND_INPUT)
	{
		result = m_results[PQG_ZCOMP] + m_results[PQG_ZCOMP_ZCOMPLOC];
	}
	else if (type == PQ_EFB_COPY_CLOCKS)
	{
		result = m_results[PQG_EFB_COPY_CLOCKS];
	}

	return result / 4;
}

// Implementations
PerfQueryGL::PerfQueryGL(GLenum query_type)
	: m_query_type(query_type)
{
	for (ActiveQuery& query : m_query_buffer)
		glGenQueries(1, &query.query_id);
}

PerfQueryGL::~PerfQueryGL()
{
	for (ActiveQuery& query : m_query_buffer)
		glDeleteQueries(1, &query.query_id);
}

void PerfQueryGL::EnableQuery(PerfQueryGroup type)
{
	// Is this sane?
	if (m_query_count > m_query_buffer.size() / 2)
		WeakFlush();

	if (m_query_buffer.size() == m_query_count)
	{
		FlushOne();
		//ERROR_LOG(VIDEO, "Flushed query buffer early!");
	}

	// start query
	if (type == PQG_ZCOMP_ZCOMPLOC || type == PQG_ZCOMP)
	{
		auto& entry = m_query_buffer[(m_query_read_pos + m_query_count) % m_query_buffer.size()];

		glBeginQuery(m_query_type, entry.query_id);
		entry.query_type = type;

		++m_query_count;
	}
}
void PerfQueryGL::DisableQuery(PerfQueryGroup type)
{
	// stop query
	if (type == PQG_ZCOMP_ZCOMPLOC || type == PQG_ZCOMP)
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
	m_results[entry.query_type] += (u64)result * EFB_WIDTH / g_renderer->GetTargetWidth() * EFB_HEIGHT / g_renderer->GetTargetHeight();

	m_query_read_pos = (m_query_read_pos + 1) % m_query_buffer.size();
	--m_query_count;
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

void PerfQueryGLESNV::EnableQuery(PerfQueryGroup type)
{
	// Is this sane?
	if (m_query_count > m_query_buffer.size() / 2)
		WeakFlush();

	if (m_query_buffer.size() == m_query_count)
	{
		FlushOne();
		//ERROR_LOG(VIDEO, "Flushed query buffer early!");
	}

	// start query
	if (type == PQG_ZCOMP_ZCOMPLOC || type == PQG_ZCOMP)
	{
		auto& entry = m_query_buffer[(m_query_read_pos + m_query_count) % m_query_buffer.size()];

		glBeginOcclusionQueryNV(entry.query_id);
		entry.query_type = type;

		++m_query_count;
	}
}
void PerfQueryGLESNV::DisableQuery(PerfQueryGroup type)
{
	// stop query
	if (type == PQG_ZCOMP_ZCOMPLOC || type == PQG_ZCOMP)
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
	m_results[entry.query_type] += (u64)result * EFB_WIDTH / g_renderer->GetTargetWidth() * EFB_HEIGHT / g_renderer->GetTargetHeight();

	m_query_read_pos = (m_query_read_pos + 1) % m_query_buffer.size();
	--m_query_count;
}

// TODO: could selectively flush things, but I don't think that will do much
void PerfQueryGLESNV::FlushResults()
{
	while (!IsFlushed())
		FlushOne();
}

} // namespace
