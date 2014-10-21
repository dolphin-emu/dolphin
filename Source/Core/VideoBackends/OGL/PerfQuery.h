#pragma once

#include <array>

#include "VideoBackends/OGL/GLExtensions/GLExtensions.h"
#include "VideoCommon/PerfQueryBase.h"

namespace OGL
{

class PerfQuery : public PerfQueryBase
{
public:
	PerfQuery();
	~PerfQuery();

	void EnableQuery(PerfQueryGroup type) override;
	void DisableQuery(PerfQueryGroup type) override;
	void ResetQuery() override;
	u32 GetQueryResult(PerfQueryType type) override;
	void FlushResults() override;
	bool IsFlushed() const override;

private:
	struct ActiveQuery
	{
		GLuint query_id;
		PerfQueryGroup query_type;
	};

	// when testing in SMS: 64 was too small, 128 was ok
	static const u32 PERF_QUERY_BUFFER_SIZE = 512;

	void WeakFlush();
	// Only use when non-empty
	void FlushOne();

	// This contains gl query objects with unretrieved results.
	std::array<ActiveQuery, PERF_QUERY_BUFFER_SIZE> m_query_buffer;
	u32 m_query_read_pos;

	// TODO: sloppy
	volatile u32 m_query_count;
	volatile u32 m_results[PQG_NUM_MEMBERS];
};

} // namespace
