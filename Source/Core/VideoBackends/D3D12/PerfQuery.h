// Copyright 2012 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>

#include "VideoCommon/PerfQueryBase.h"

namespace DX12
{

class PerfQuery final : public PerfQueryBase
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
		ID3D11Query* query;
		PerfQueryGroup query_type;
	};

	void WeakFlush();

	// Only use when non-empty
	void FlushOne();

	// when testing in SMS: 64 was too small, 128 was ok
	static const int s_perf_query_buffer_size = 512;

	std::array<ActiveQuery, s_perf_query_buffer_size> m_query_buffer;
	int m_query_read_pos = 0;
};

} // namespace
