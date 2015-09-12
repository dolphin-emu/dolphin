// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once
#include <vector>

namespace DebugAPI
{
	class DebugHandler
	{
	public:
		virtual bool IsAvailable() { return true; }
		virtual std::string GetArch() = 0;
		virtual u32 GetCoreState() = 0;
		virtual void SetCoreState(u32 state) = 0;
		virtual std::vector<std::pair<std::string, bool>> GetCoreFeatures() = 0;
		virtual void SetCoreFeature(std::string& core_part, bool disabled) = 0;
		virtual void SetProfile(bool enable) = 0;
		virtual void GetProfileResults() = 0;
		virtual void DumpCode(u32 address, std::vector<u32>* ppc, std::vector<u8>* host, u64* host_addr) = 0;
	};

	class DebugHandlerServerBase
	{
	public:
		virtual bool IsAvailable() = 0;
	};

	// Clients
	std::unique_ptr<DebugHandler> InitLocal();
	std::unique_ptr<DebugHandler> InitClient(const std::string& ip);

	// Server
	std::unique_ptr<DebugHandlerServerBase> InitServer();
}
