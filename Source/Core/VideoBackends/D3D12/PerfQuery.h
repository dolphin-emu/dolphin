// Copyright 2012 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <d3d12.h>

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
		PerfQueryGroup query_type;
		UINT64 fence_value;
	};

	void WeakFlush();

	// Find the last fence value of all pending queries.
	UINT64 FindLastPendingFenceValue() const;

	// Only use when non-empty
	void FlushOne();

	static void QueueFenceCallback(void* owning_object, UINT64 fence_value);
	void QueueFence(UINT64 fence_value);

	// when testing in SMS: 64 was too small, 128 was ok
	static constexpr size_t PERF_QUERY_BUFFER_SIZE = 512;
	static constexpr size_t QUERY_READBACK_BUFFER_SIZE = PERF_QUERY_BUFFER_SIZE * sizeof(UINT64);

	std::array<ActiveQuery, PERF_QUERY_BUFFER_SIZE> m_query_buffer;
	int m_query_read_pos = 0;

	ID3D12QueryHeap* m_query_heap = nullptr;
	ID3D12Resource* m_query_readback_buffer = nullptr;

	ID3D12Fence* m_tracking_fence = nullptr;
	UINT64 m_next_fence_value = 0;
};

} // namespace
