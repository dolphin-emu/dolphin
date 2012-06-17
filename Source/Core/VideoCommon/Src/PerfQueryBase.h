#ifndef _PERFQUERY_BASE_H_
#define _PERFQUERY_BASE_H_

#include "CommonTypes.h"

enum PerfQueryType
{
	PQ_ZCOMP_INPUT_ZCOMPLOC = 0,
	PQ_ZCOMP_OUTPUT_ZCOMPLOC,
	PQ_ZCOMP_INPUT,
	PQ_ZCOMP_OUTPUT,
	PQ_BLEND_INPUT,
	PQ_EFB_COPY_CLOCKS,
	PQ_NUM_MEMBERS
};

enum PerfQueryGroup
{
	PQG_ZCOMP_ZCOMPLOC,
	PQG_ZCOMP,
	PQG_EFB_COPY_CLOCKS,
	PQG_NUM_MEMBERS,
};

class PerfQueryBase
{
public:
	PerfQueryBase() {};
	virtual ~PerfQueryBase() {}

	virtual void EnableQuery(PerfQueryGroup type) {}
	virtual void DisableQuery(PerfQueryGroup type) {}
	virtual void ResetQuery() {}
	virtual u32 GetQueryResult(PerfQueryType type) { return 0; }
};

extern PerfQueryBase* g_perf_query;

#endif // _PERFQUERY_H_
