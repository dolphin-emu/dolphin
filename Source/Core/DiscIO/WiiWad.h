// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <vector>

#include "Common/CommonTypes.h"

namespace DiscIO
{

class IBlobReader;
class CBlobBigEndianReader;

class WiiWAD
{
public:
	WiiWAD(const std::string& name);
	~WiiWAD();

	bool IsValid() const { return m_valid; }

	const std::vector<u8>& GetCertificateChain() const { return m_certificate_chain; }
	const std::vector<u8>& GetTicket() const { return m_ticket; }
	const std::vector<u8>& GetTMD() const { return m_tmd; }
	const std::vector<u8>& GetDataApp() const { return m_data_app; }
	const std::vector<u8>& GetFooter() const { return m_footer; }

private:
	bool ParseWAD(IBlobReader& reader);
	static std::vector<u8> CreateWADEntry(IBlobReader& reader, u32 size, u64 offset);
	static bool IsWiiWAD(const CBlobBigEndianReader& reader);

	bool m_valid;

	std::vector<u8> m_certificate_chain;
	std::vector<u8> m_ticket;
	std::vector<u8> m_tmd;
	std::vector<u8> m_data_app;
	std::vector<u8> m_footer;
};

}
