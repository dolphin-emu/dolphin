// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Common/CommonTypes.h"

namespace DiscIO
{

class IBlobReader;

class WiiWAD
{
public:

	WiiWAD(const std::string& _rName);

	~WiiWAD();

	bool IsValid() const { return m_Valid; }
	u32 GetCertificateChainSize() const { return m_CertificateChainSize; }
	u32 GetTicketSize() const { return m_TicketSize; }
	u32 GetTMDSize() const { return m_TMDSize; }
	u32 GetDataAppSize() const { return m_DataAppSize; }
	u32 GetFooterSize() const { return m_FooterSize; }

	u8* GetCertificateChain() const { return m_pCertificateChain; }
	u8* GetTicket() const { return m_pTicket; }
	u8* GetTMD() const { return m_pTMD; }
	u8* GetDataApp() const { return m_pDataApp; }
	u8* GetFooter() const { return m_pFooter; }

	static bool IsWiiWAD(const std::string& _rName);

private:

	bool m_Valid;

	u32 m_CertificateChainSize;
	u32 m_TicketSize;
	u32 m_TMDSize;
	u32 m_DataAppSize;
	u32 m_FooterSize;

	u8* m_pCertificateChain;
	u8* m_pTicket;
	u8* m_pTMD;
	u8* m_pDataApp;
	u8* m_pFooter;

	u8* CreateWADEntry(DiscIO::IBlobReader& _rReader, u32 _Size, u64 _Offset);
	bool ParseWAD(DiscIO::IBlobReader& _rReader);
};

}
