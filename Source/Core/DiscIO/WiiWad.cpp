// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.


#include <cstddef>
#include <memory>
#include <string>

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/MathUtil.h"
#include "Common/Logging/Log.h"
#include "DiscIO/Blob.h"
#include "DiscIO/WiiWad.h"

namespace DiscIO
{

WiiWAD::WiiWAD(const std::string& name)
{
	std::unique_ptr<IBlobReader> reader(DiscIO::CreateBlobReader(name));
	if (reader == nullptr || File::IsDirectory(name))
	{
		m_Valid = false;
		return;
	}

	m_Valid = ParseWAD(*reader);
}

WiiWAD::~WiiWAD()
{
	if (m_Valid)
	{
		delete[] m_pCertificateChain;
		delete[] m_pTicket;
		delete[] m_pTMD;
		delete[] m_pDataApp;
		delete[] m_pFooter;
	}
}

u8* WiiWAD::CreateWADEntry(DiscIO::IBlobReader& _rReader, u32 _Size, u64 _Offset)
{
	if (_Size > 0)
	{
		u8* pTmpBuffer = new u8[_Size];
		_dbg_assert_msg_(BOOT, pTmpBuffer!=nullptr, "WiiWAD: Can't allocate memory for WAD entry");

		if (!_rReader.Read(_Offset, _Size, pTmpBuffer))
		{
			ERROR_LOG(DISCIO, "WiiWAD: Could not read from file");
			PanicAlertT("WiiWAD: Could not read from file");
		}
		return pTmpBuffer;
	}
	return nullptr;
}


bool WiiWAD::ParseWAD(DiscIO::IBlobReader& _rReader)
{
	CBlobBigEndianReader ReaderBig(_rReader);

	if (!IsWiiWAD(ReaderBig))
		return false;

	u32 Reserved;
	if (!ReaderBig.ReadSwapped(0x8, &m_CertificateChainSize) ||
	    !ReaderBig.ReadSwapped(0xC, &Reserved) ||
	    !ReaderBig.ReadSwapped(0x10, &m_TicketSize) ||
	    !ReaderBig.ReadSwapped(0x14, &m_TMDSize) ||
	    !ReaderBig.ReadSwapped(0x18, &m_DataAppSize) ||
	    !ReaderBig.ReadSwapped(0x1C, &m_FooterSize))
		return false;

	if (MAX_LOGLEVEL >= LogTypes::LOG_LEVELS::LDEBUG)
		_dbg_assert_msg_(BOOT, Reserved==0x00, "WiiWAD: Reserved must be 0x00");

	u32 Offset = 0x40;
	m_pCertificateChain   = CreateWADEntry(_rReader, m_CertificateChainSize, Offset);  Offset += ROUND_UP(m_CertificateChainSize, 0x40);
	m_pTicket             = CreateWADEntry(_rReader, m_TicketSize, Offset);            Offset += ROUND_UP(m_TicketSize, 0x40);
	m_pTMD                = CreateWADEntry(_rReader, m_TMDSize, Offset);               Offset += ROUND_UP(m_TMDSize, 0x40);
	m_pDataApp            = CreateWADEntry(_rReader, m_DataAppSize, Offset);           Offset += ROUND_UP(m_DataAppSize, 0x40);
	m_pFooter             = CreateWADEntry(_rReader, m_FooterSize, Offset);            Offset += ROUND_UP(m_FooterSize, 0x40);

	return true;
}

bool WiiWAD::IsWiiWAD(const DiscIO::CBlobBigEndianReader& reader)
{
	u32 header_size = 0;
	u32 header_type = 0;
	reader.ReadSwapped(0x0, &header_size);
	reader.ReadSwapped(0x4, &header_type);
	return header_size == 0x20 && (header_type == 0x49730000 || header_type == 0x69620000);
}

} // namespace end
