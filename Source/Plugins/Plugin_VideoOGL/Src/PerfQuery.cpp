#include "RenderBase.h"
#include "GLUtil.h"
#include "PerfQuery.h"

namespace OGL
{

PerfQuery::PerfQuery()
	: m_query_read_pos()
	, m_query_count()
{
	for (u32 i = 0; i != ARRAYSIZE(m_query_buffer); ++i)
		glGenQueries(1, &m_query_buffer[i].query_id);
	
	ResetQuery();
}

PerfQuery::~PerfQuery()
{
	for (u32 i = 0; i != ARRAYSIZE(m_query_buffer); ++i)
		glDeleteQueries(1, &m_query_buffer[i].query_id);
}

void PerfQuery::EnableQuery(PerfQueryGroup type)
{
	if (!ShouldEmulate())
		return;

	// Is this sane?
	if (m_query_count > ARRAYSIZE(m_query_buffer) / 2)
		WeakFlush();
	
	if (ARRAYSIZE(m_query_buffer) == m_query_count)
	{
		FlushOne();
		//ERROR_LOG(VIDEO, "Flushed query buffer early!");
	}
	
	// start query
	if (type == PQG_ZCOMP_ZCOMPLOC || type == PQG_ZCOMP)
	{
		auto& entry = m_query_buffer[(m_query_read_pos + m_query_count) % ARRAYSIZE(m_query_buffer)];
		
		glBeginQuery(GL_SAMPLES_PASSED, entry.query_id);
		entry.query_type = type;
		
		++m_query_count;
	}
}

void PerfQuery::DisableQuery(PerfQueryGroup type)
{
	if (!ShouldEmulate())
		return;

	// stop query
	if (type == PQG_ZCOMP_ZCOMPLOC || type == PQG_ZCOMP)
	{
		glEndQuery(GL_SAMPLES_PASSED);
	}
}

bool PerfQuery::IsFlushed() const
{
	if (!ShouldEmulate())
		return true;

	return 0 == m_query_count;
}

void PerfQuery::FlushOne()
{
	if (!ShouldEmulate())
		return;

	auto& entry = m_query_buffer[m_query_read_pos];

	GLuint result = 0;
	glGetQueryObjectuiv(entry.query_id, GL_QUERY_RESULT, &result);

	// NOTE: Reported pixel metrics should be referenced to native resolution
	m_results[entry.query_type] += (u64)result * EFB_WIDTH / g_renderer->GetTargetWidth() * EFB_HEIGHT / g_renderer->GetTargetHeight();

	m_query_read_pos = (m_query_read_pos + 1) % ARRAYSIZE(m_query_buffer);
	--m_query_count;
}

// TODO: could selectively flush things, but I don't think that will do much
void PerfQuery::FlushResults()
{
	if (!ShouldEmulate())
		return;

	while (!IsFlushed())
		FlushOne();
}

void PerfQuery::WeakFlush()
{
	if (!ShouldEmulate())
		return;

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

void PerfQuery::ResetQuery()
{
	m_query_count = 0;
	std::fill_n(m_results, ARRAYSIZE(m_results), 0);
}

u32 PerfQuery::GetQueryResult(PerfQueryType type)
{
	if (!ShouldEmulate())
		return 0;

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

} // namespace
