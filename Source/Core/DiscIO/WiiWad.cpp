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
	std::unique_ptr<IBlobReader> reader(CreateBlobReader(name));
	if (reader == nullptr || File::IsDirectory(name))
	{
		m_valid = false;
		return;
	}

	m_valid = ParseWAD(*reader);
}

WiiWAD::~WiiWAD()
{
}

std::vector<u8> WiiWAD::CreateWADEntry(IBlobReader& reader, u32 size, u64 offset)
{
	if (size == 0)
		return {};

	std::vector<u8> buffer(size);

	if (!reader.Read(offset, size, buffer.data()))
	{
		ERROR_LOG(DISCIO, "WiiWAD: Could not read from file");
		PanicAlertT("WiiWAD: Could not read from file");
	}

	return buffer;
}


bool WiiWAD::ParseWAD(IBlobReader& reader)
{
	CBlobBigEndianReader big_endian_reader(reader);

	if (!IsWiiWAD(big_endian_reader))
		return false;

	u32 certificate_chain_size;
	u32 reserved;
	u32 ticket_size;
	u32 tmd_size;
	u32 data_app_size;
	u32 footer_size;

	if (!big_endian_reader.ReadSwapped(0x08, &certificate_chain_size) ||
	    !big_endian_reader.ReadSwapped(0x0C, &reserved) ||
	    !big_endian_reader.ReadSwapped(0x10, &ticket_size) ||
	    !big_endian_reader.ReadSwapped(0x14, &tmd_size) ||
	    !big_endian_reader.ReadSwapped(0x18, &data_app_size) ||
	    !big_endian_reader.ReadSwapped(0x1C, &footer_size))
		return false;

	if (MAX_LOGLEVEL >= LogTypes::LOG_LEVELS::LDEBUG)
		_dbg_assert_msg_(BOOT, reserved == 0x00, "WiiWAD: Reserved must be 0x00");

	u32 offset = 0x40;
	m_certificate_chain = CreateWADEntry(reader, certificate_chain_size, offset); offset += ROUND_UP(certificate_chain_size, 0x40);
	m_ticket            = CreateWADEntry(reader, ticket_size, offset);            offset += ROUND_UP(ticket_size, 0x40);
	m_tmd               = CreateWADEntry(reader, tmd_size, offset);               offset += ROUND_UP(tmd_size, 0x40);
	m_data_app          = CreateWADEntry(reader, data_app_size, offset);          offset += ROUND_UP(data_app_size, 0x40);
	m_footer            = CreateWADEntry(reader, footer_size, offset);            offset += ROUND_UP(footer_size, 0x40);

	return true;
}

bool WiiWAD::IsWiiWAD(const CBlobBigEndianReader& reader)
{
	u32 header_size = 0;
	u32 header_type = 0;
	reader.ReadSwapped(0x0, &header_size);
	reader.ReadSwapped(0x4, &header_type);
	return header_size == 0x20 && (header_type == 0x49730000 || header_type == 0x69620000);
}

} // namespace end
