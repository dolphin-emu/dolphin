#ifndef _PERFQUERY_H_
#define _PERFQUERY_H_

#include "PerfQueryBase.h"

namespace DX9 {

class PerfQuery : public PerfQueryBase
{
public:
	PerfQuery();
	~PerfQuery();

	void EnableQuery(PerfQueryGroup type);
	void DisableQuery(PerfQueryGroup type);
	void ResetQuery();
	u32 GetQueryResult(PerfQueryType type);
	void FlushResults();
	bool IsFlushed() const;
	void CreateDeviceObjects();
	void DestroyDeviceObjects();

private:
	struct ActiveQuery
	{
		IDirect3DQuery9* query;
		PerfQueryGroup query_type;
	};

	void WeakFlush();	
	// Only use when non-empty
	void FlushOne();

	// when testing in SMS: 64 was too small, 128 was ok
	static const int PERF_QUERY_BUFFER_SIZE = 512;

	ActiveQuery m_query_buffer[PERF_QUERY_BUFFER_SIZE];
	int m_query_read_pos;

	// TODO: sloppy
	volatile int m_query_count;
	volatile u32 m_results[PQG_NUM_MEMBERS];
};

} // namespace

#endif // _PERFQUERY_H_
