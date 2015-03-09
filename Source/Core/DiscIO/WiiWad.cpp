// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.


#include <cstddef>
#include <memory>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/MathUtil.h"
#include "Common/Logging/Log.h"
#include "DiscIO/Blob.h"
#include "DiscIO/WiiWad.h"

namespace DiscIO
{

class CBlobBigEndianReader
{
public:
	CBlobBigEndianReader(DiscIO::IBlobReader& _rReader) : m_rReader(_rReader) {}

	u32 Read32(u64 _offset)
	{
		u32 temp;
		m_rReader.Read(_offset, 4, (u8*)&temp);
		return(Common::swap32(temp));
	}

private:
	DiscIO::IBlobReader& m_rReader;
};

WiiWAD::WiiWAD(const std::string& name)
{
	std::unique_ptr<IBlobReader> reader(DiscIO::CreateBlobReader(name));
	if (reader == nullptr || File::IsDirectory(name))
	{
		m_valid = false;
		return;
	}

	m_valid = ParseWAD(*reader);
}

WiiWAD::~WiiWAD()
{
	if (m_valid)
	{
		delete m_pCertificateChain;
		delete m_pTicket;
		delete m_pTMD;
		delete m_pDataApp;
		delete m_pFooter;
	}
}

u8* WiiWAD::CreateWADEntry(DiscIO::IBlobReader& _rReader, u32 _size, u64 _offset)
{
	if (_size > 0)
	{
		u8* pTmpBuffer = new u8[_size];
		_dbg_assert_msg_(BOOT, pTmpBuffer!=nullptr, "WiiWAD: Can't allocate memory for WAD entry");

		if (!_rReader.Read(_offset, _size, pTmpBuffer))
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

	// get header size
	u32 header_size = ReaderBig.Read32(0);
	if (header_size != 0x20)
	{
		_dbg_assert_msg_(BOOT, (header_size==0x20), "WiiWAD: Header size != 0x20");
		return false;
	}

	// get header
	u8 header[0x20];
	_rReader.Read(0, header_size, header);
	u32 header_type = ReaderBig.Read32(0x4);
	if ((0x49730000 != header_type) && (0x69620000 != header_type))
		return false;

	m_cert_chain_size   = ReaderBig.Read32(0x08);
	u32 reserved               = ReaderBig.Read32(0x0C);
	m_ticket_size              = ReaderBig.Read32(0x10);
	m_TMD_size                 = ReaderBig.Read32(0x14);
	m_data_app_size            = ReaderBig.Read32(0x18);
	m_footer_size              = ReaderBig.Read32(0x1C);

	if (MAX_LOGLEVEL >= LogTypes::LOG_LEVELS::LDEBUG)
		_dbg_assert_msg_(BOOT, reserved==0x00, "WiiWAD: Reserved must be 0x00");

	u32 offset = 0x40;
	m_pCertificateChain   = CreateWADEntry(_rReader, m_cert_chain_size, offset);     offset += ROUND_UP(m_cert_chain_size, 0x40);
	m_pTicket             = CreateWADEntry(_rReader, m_ticket_size, offset);         offset += ROUND_UP(m_ticket_size, 0x40);
	m_pTMD                = CreateWADEntry(_rReader, m_TMD_size, offset);            offset += ROUND_UP(m_TMD_size, 0x40);
	m_pDataApp            = CreateWADEntry(_rReader, m_data_app_size, offset);       offset += ROUND_UP(m_data_app_size, 0x40);
	m_pFooter             = CreateWADEntry(_rReader, m_footer_size, offset);         offset += ROUND_UP(m_footer_size, 0x40);

	return true;
}

bool WiiWAD::IsWiiWAD(const std::string& name)
{
	std::unique_ptr<IBlobReader> blob_reader(DiscIO::CreateBlobReader(name));
	if (blob_reader == nullptr)
		return false;

	CBlobBigEndianReader big_endian_reader(*blob_reader);
	bool result = false;

	// check for Wii wad
	if (big_endian_reader.Read32(0x00) == 0x20)
	{
		u32 wad_type = big_endian_reader.Read32(0x04);
		switch (wad_type)
		{
		case 0x49730000:
		case 0x69620000:
			result = true;
		}
	}

	return result;
}



} // namespace end

