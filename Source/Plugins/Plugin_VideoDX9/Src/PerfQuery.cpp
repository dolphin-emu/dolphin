#include "RenderBase.h"

#include "D3DBase.h"
#include "PerfQuery.h"

namespace DX9 {

PerfQuery::PerfQuery()
	: m_query_read_pos()
	, m_query_count()
{
	
}

PerfQuery::~PerfQuery()
{
	
}

void PerfQuery::CreateDeviceObjects()
{
	for (int i = 0; i != ARRAYSIZE(m_query_buffer); ++i)
	{
		D3D::dev->CreateQuery(D3DQUERYTYPE_OCCLUSION, &m_query_buffer[i].query);
	}
	ResetQuery();
}
void PerfQuery::DestroyDeviceObjects()
{
	for (int i = 0; i != ARRAYSIZE(m_query_buffer); ++i)
	{
		m_query_buffer[i].query->Release();
	}
}

void PerfQuery::EnableQuery(PerfQueryGroup type)
{
	// Is this sane?
	if (m_query_count > ARRAYSIZE(m_query_buffer) / 2)
		WeakFlush();

	if (ARRAYSIZE(m_query_buffer) == m_query_count)
	{
		// TODO
		FlushOne();
		ERROR_LOG(VIDEO, "Flushed query buffer early!");
	}

	// start query
	if (type == PQG_ZCOMP_ZCOMPLOC || type == PQG_ZCOMP)
	{
		auto& entry = m_query_buffer[(m_query_read_pos + m_query_count) % ARRAYSIZE(m_query_buffer)];
		entry.query->Issue(D3DISSUE_BEGIN);
		entry.query_type = type;
		++m_query_count;
	}
}

void PerfQuery::DisableQuery(PerfQueryGroup type)
{
	// stop query
	if (type == PQG_ZCOMP_ZCOMPLOC || type == PQG_ZCOMP)
	{
		auto& entry = m_query_buffer[(m_query_read_pos + m_query_count + ARRAYSIZE(m_query_buffer)-1) % ARRAYSIZE(m_query_buffer)];
		entry.query->Issue(D3DISSUE_END);
	}
}

void PerfQuery::ResetQuery()
{
	m_query_count = 0;
	std::fill_n(m_results, ARRAYSIZE(m_results), 0);
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

void PerfQuery::FlushOne()
{
	auto& entry = m_query_buffer[m_query_read_pos];

	DWORD result = 0;
	HRESULT hr = S_FALSE;
	while (hr != S_OK && hr != D3DERR_DEVICELOST)
	{
		// TODO: Might cause us to be stuck in an infinite loop!
		hr = entry.query->GetData(&result, sizeof(result), D3DGETDATA_FLUSH);
	}

	// NOTE: Reported pixel metrics should be referenced to native resolution
	m_results[entry.query_type] += (u32)((u64)result * EFB_WIDTH / g_renderer->GetTargetWidth() * EFB_HEIGHT / g_renderer->GetTargetHeight());

	m_query_read_pos = (m_query_read_pos + 1) % ARRAYSIZE(m_query_buffer);
	--m_query_count;
}

// TODO: could selectively flush things, but I don't think that will do much
void PerfQuery::FlushResults()
{
	while (!IsFlushed())
		FlushOne();
}

void PerfQuery::WeakFlush()
{
	while (!IsFlushed())
	{
		auto& entry = m_query_buffer[m_query_read_pos];

		DWORD result = 0;
		HRESULT hr = entry.query->GetData(&result, sizeof(result), 0);

		if (hr == S_OK)
		{
			// NOTE: Reported pixel metrics should be referenced to native resolution
			m_results[entry.query_type] += (u32)((u64)result * EFB_WIDTH / g_renderer->GetTargetWidth() * EFB_HEIGHT / g_renderer->GetTargetHeight());

			m_query_read_pos = (m_query_read_pos + 1) % ARRAYSIZE(m_query_buffer);
			--m_query_count;
		}
		else
		{
			break;
		}
	}
}

bool PerfQuery::IsFlushed() const
{
	return 0 == m_query_count;
}


} // namespace
