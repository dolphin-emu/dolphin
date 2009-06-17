// Copyright (C) 2003-2009 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#ifndef _WII_WAD_H
#define _WII_WAD_H

#include <string>
#include <vector>
#include <map>

#include "Common.h"
#include "Blob.h"
#include "Volume.h"

namespace DiscIO
{

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

#endif

