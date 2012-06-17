#include "GLUtil.h"
#include "PerfQuery.h"

namespace OGL {

u32 results[PQG_NUM_MEMBERS] = { 0 };
GLuint query_id;

PerfQueryGroup active_query;

PerfQuery::PerfQuery()
{
	glGenQueries(1, &query_id);
}

PerfQuery::~PerfQuery()
{
	glDeleteQueries(1, &query_id);
}

void PerfQuery::EnableQuery(PerfQueryGroup type)
{
	// start query
	if (type == PQG_ZCOMP_ZCOMPLOC || type == PQG_ZCOMP)
	{
		glBeginQuery(GL_SAMPLES_PASSED, query_id);
	}
	active_query = type;
}

void PerfQuery::DisableQuery(PerfQueryGroup type)
{
	// stop query
	if (type == PQG_ZCOMP_ZCOMPLOC || type == PQG_ZCOMP)
	{
		glEndQuery(GL_SAMPLES_PASSED);

		GLuint query_result = GL_FALSE;
		while (query_result != GL_TRUE)
		{
			glGetQueryObjectuiv(query_id, GL_QUERY_RESULT_AVAILABLE, &query_result);
		}

		glGetQueryObjectuiv(query_id, GL_QUERY_RESULT, &query_result);

		results[active_query] += query_result;
	}
}

void PerfQuery::ResetQuery()
{
	memset(results, 0, sizeof(results));
}

u32 PerfQuery::GetQueryResult(PerfQueryType type)
{
	if (type == PQ_ZCOMP_INPUT_ZCOMPLOC || type == PQ_ZCOMP_OUTPUT_ZCOMPLOC || type == PQ_BLEND_INPUT)
	{

	}
	if (type == PQ_ZCOMP_INPUT || type == PQ_ZCOMP_OUTPUT || type == PQ_BLEND_INPUT)
	{

	}
	if (type == PQ_BLEND_INPUT)
	{
		results[PQ_BLEND_INPUT] = results[PQ_ZCOMP_OUTPUT] + results[PQ_ZCOMP_OUTPUT_ZCOMPLOC];
	}

	if (type == PQ_EFB_COPY_CLOCKS)
	{
		// TODO
	}

	return results[type];
}

} // namespace
