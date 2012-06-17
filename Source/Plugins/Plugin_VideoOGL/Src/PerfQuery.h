#ifndef _PERFQUERY_H_
#define _PERFQUERY_H_

#include "PerfQueryBase.h"

namespace OGL {

class PerfQuery : public PerfQueryBase
{
public:
	PerfQuery();
	~PerfQuery();

	void EnableQuery(PerfQueryGroup type);
	void DisableQuery(PerfQueryGroup type);
	void ResetQuery();
	u32 GetQueryResult(PerfQueryType type);
};

} // namespace

#endif // _PERFQUERY_H_
