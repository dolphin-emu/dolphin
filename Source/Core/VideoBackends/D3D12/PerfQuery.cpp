// Copyright 2012 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/D3D12/D3DBase.h"
#include "VideoBackends/D3D12/PerfQuery.h"
#include "VideoCommon/RenderBase.h"

//D3D12TODO: Implement PerfQuery class.

namespace DX12
{

PerfQuery::PerfQuery()
{
	//D3D12TODO: Add implementation
}

PerfQuery::~PerfQuery()
{
	//D3D12TODO: Add implementation
}

void PerfQuery::EnableQuery(PerfQueryGroup type)
{
	//D3D12TODO: Add implementation
}

void PerfQuery::DisableQuery(PerfQueryGroup type)
{
	//D3D12TODO: Add implementation
}

void PerfQuery::ResetQuery()
{
	//D3D12TODO: Add implementation
}

u32 PerfQuery::GetQueryResult(PerfQueryType type)
{
	//D3D12TODO: Add implementation
	return 0;
}

void PerfQuery::FlushOne()
{
	//D3D12TODO: Add implementation
}

void PerfQuery::FlushResults()
{
	//D3D12TODO: Add implementation
}

void PerfQuery::WeakFlush()
{
	//D3D12TODO: Add implementation
}

bool PerfQuery::IsFlushed() const
{
	//D3D12TODO: Add implementation
	return true;
}

} // namespace
