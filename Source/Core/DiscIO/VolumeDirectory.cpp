// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/MathUtil.h"
#include "Core/VolumeHandler.h"
#include "DiscIO/FileBlob.h"
#include "DiscIO/FileMonitor.h"
#include "DiscIO/Volume.h"
#include "DiscIO/VolumeDirectory.h"

namespace DiscIO
{

CVolumeDirectory::CVolumeDirectory(const std::string& _rDirectory, bool _bIsWii,
								   const std::string& _rApploader, const std::string& _rDOL)
	: m_total_name_size(0)
	, m_data_start_address(-1)
	, m_disk_header(DISKHEADERINFO_ADDRESS)
	, m_disk_header_info(new SDiskHeaderInfo())
	, m_FST_address(0)
	, m_DOL_address(0)
{
	m_root_directory = ExtractDirectoryName(_rDirectory);

	// create the default disk header
	SetUniqueID("AGBJ01");
	SetName("Default name");

	if (_bIsWii)
	{
		SetDiskTypeWii();
	}
	else
	{
		SetDiskTypeGC();
	}

	// Don't load the dol if we've no apploader...
	if (SetApploader(_rApploader))
		SetDOL(_rDOL);

	BuildFST();
}

CVolumeDirectory::~CVolumeDirectory()
{
}

bool CVolumeDirectory::IsValidDirectory(const std::string& _rDirectory)
{
	std::string directory_name = ExtractDirectoryName(_rDirectory);
	return File::IsDirectory(directory_name);
}

bool CVolumeDirectory::Read(u64 _offset, u64 _length, u8* _pBuffer, bool decrypt) const
{
	if (!decrypt && (_offset + _length >= 0x400) && m_is_wii)
	{
		// Fully supporting this would require re-encrypting every file that's read.
		// Only supporting the areas that IOS allows software to read could be more feasible.
		// Currently, only the header (up to 0x400) is supported, though we're cheating a bit
		// with it by reading the header inside the current partition instead. Supporting the
		// header is enough for booting games, but not for running things like the Disc Channel.
		return false;
	}

	if (decrypt && !m_is_wii)
		PanicAlertT("Tried to decrypt data from a non-Wii volume");

	// header
	if (_offset < DISKHEADERINFO_ADDRESS)
	{
		WriteToBuffer(DISKHEADER_ADDRESS, DISKHEADERINFO_ADDRESS, m_disk_header.data(), _offset, _length, _pBuffer);
	}
	// header info
	if (_offset >= DISKHEADERINFO_ADDRESS && _offset < APPLOADER_ADDRESS)
	{
		WriteToBuffer(DISKHEADERINFO_ADDRESS, sizeof(m_disk_header_info), (u8*)m_disk_header_info.get(), _offset, _length, _pBuffer);
	}
	// apploader
	if (_offset >= APPLOADER_ADDRESS && _offset < APPLOADER_ADDRESS + m_apploader.size())
	{
		WriteToBuffer(APPLOADER_ADDRESS, m_apploader.size(), m_apploader.data(), _offset, _length, _pBuffer);
	}
	// dol
	if (_offset >= m_DOL_address && _offset < m_DOL_address + m_DOL.size())
	{
		WriteToBuffer(m_DOL_address, m_DOL.size(), m_DOL.data(), _offset, _length, _pBuffer);
	}
	// fst
	if (_offset >= m_FST_address && _offset < m_data_start_address)
	{
		WriteToBuffer(m_FST_address, m_FST_data.size(), m_FST_data.data(), _offset, _length, _pBuffer);
	}

	if (m_virtual_disk.empty())
		return true;

	// Determine which file the offset refers to
	std::map<u64, std::string>::const_iterator fileIter = m_virtual_disk.lower_bound(_offset);
	if (fileIter->first > _offset && fileIter != m_virtual_disk.begin())
		--fileIter;

	// zero fill to start of file data
	PadToAddress(fileIter->first, _offset, _length, _pBuffer);

	while (fileIter != m_virtual_disk.end() && _length > 0)
	{
		_dbg_assert_(DVDINTERFACE, fileIter->first <= _offset);
		u64 file_offset = _offset - fileIter->first;
		const std::string file_name = fileIter->second;

		std::unique_ptr<PlainFileReader> reader(PlainFileReader::Create(file_name));
		if (reader == nullptr)
			return false;

		u64 file_size = reader->GetDataSize();

		FileMon::CheckFile(file_name, file_size);

		if (file_offset < file_size)
		{
			u64 fileBytes = file_size - file_offset;
			if (_length < fileBytes)
				fileBytes = _length;

			if (!reader->Read(file_offset, fileBytes, _pBuffer))
				return false;

			_length  -= fileBytes;
			_pBuffer += fileBytes;
			_offset  += fileBytes;
		}

		++fileIter;

		if (fileIter != m_virtual_disk.end())
		{
			_dbg_assert_(DVDINTERFACE, fileIter->first >= _offset);
			PadToAddress(fileIter->first, _offset, _length, _pBuffer);
		}
	}

	return true;
}

std::string CVolumeDirectory::GetUniqueID() const
{
	static const size_t ID_LENGTH = 6;
	return std::string(m_disk_header.begin(), m_disk_header.begin() + ID_LENGTH);
}

void CVolumeDirectory::SetUniqueID(const std::string& id)
{
	size_t length = id.length();
	if (length > 6)
		length = 6;

	memcpy(m_disk_header.data(), id.c_str(), length);
}

IVolume::ECountry CVolumeDirectory::GetCountry() const
{
	u8 country_code = m_disk_header[3];

	return CountrySwitch(country_code);
}

std::string CVolumeDirectory::GetMakerID() const
{
	return "VOID";
}

std::vector<std::string> CVolumeDirectory::GetNames() const
{
	return std::vector<std::string>(1, (char*)(&m_disk_header[0x20]));
}

void CVolumeDirectory::SetName(const std::string& name)
{
	size_t length = name.length();
	if (length > MAX_NAME_LENGTH)
	    length = MAX_NAME_LENGTH;

	memcpy(&m_disk_header[0x20], name.c_str(), length);
	m_disk_header[length + 0x20] = 0;
}

u32 CVolumeDirectory::GetFSTSize() const
{
	return 0;
}

std::string CVolumeDirectory::GetApploaderDate() const
{
	return "VOID";
}

bool CVolumeDirectory::IsWiiDisc() const
{
	return m_is_wii;
}

u64 CVolumeDirectory::GetSize() const
{
	return 0;
}

u64 CVolumeDirectory::GetRawSize() const
{
	return GetSize();
}

std::string CVolumeDirectory::ExtractDirectoryName(const std::string& _rDirectory)
{
	std::string directory_name = _rDirectory;

	size_t lastSep = directory_name.find_last_of(DIR_SEP_CHR);

	if (lastSep != directory_name.size() - 1)
	{
		// TODO: This assumes that file names will always have a dot in them
		//       and directory names never will; both assumptions are often
		//       right but in general wrong.
		size_t extension_start = directory_name.find_last_of('.');
		if (extension_start != std::string::npos && extension_start > lastSep)
		{
			directory_name.resize(lastSep);
		}
	}
	else
	{
		directory_name.resize(lastSep);
	}

	return directory_name;
}

void CVolumeDirectory::SetDiskTypeWii()
{
	m_disk_header[0x18] = 0x5d;
	m_disk_header[0x19] = 0x1c;
	m_disk_header[0x1a] = 0x9e;
	m_disk_header[0x1b] = 0xa3;
	memset(&m_disk_header[0x1c], 0, 4);

	m_is_wii = true;
	m_address_shift = 2;
}

void CVolumeDirectory::SetDiskTypeGC()
{
	memset(&m_disk_header[0x18], 0, 4);
	m_disk_header[0x1c] = 0xc2;
	m_disk_header[0x1d] = 0x33;
	m_disk_header[0x1e] = 0x9f;
	m_disk_header[0x1f] = 0x3d;

	m_is_wii = false;
	m_address_shift = 0;
}

bool CVolumeDirectory::SetApploader(const std::string& _rApploader)
{
	if (!_rApploader.empty())
	{
		std::string data;
		if (!File::ReadFileToString(_rApploader, data))
		{
			PanicAlertT("Apploader unable to load from file");
			return false;
		}
		size_t apploaderSize = 0x20 + Common::swap32(*(u32*)&data.data()[0x14]) + Common::swap32(*(u32*)&data.data()[0x18]);
		if (apploaderSize != data.size())
		{
			PanicAlertT("Apploader is the wrong size...is it really an apploader?");
			return false;
		}
		m_apploader.resize(apploaderSize);
		std::copy(data.begin(), data.end(), m_apploader.begin());

		// 32byte aligned (plus 0x20 padding)
		m_DOL_address = ROUND_UP(APPLOADER_ADDRESS + m_apploader.size() + 0x20, 0x20ull);
		return true;
	}
	else
	{
		m_apploader.resize(0x20);
		// Make sure BS2 HLE doesn't try to run the apploader
		*(u32*)&m_apploader[0x10] = (u32)-1;
		return false;
	}
}

void CVolumeDirectory::SetDOL(const std::string& rDOL)
{
	if (!rDOL.empty())
	{
		std::string data;
		File::ReadFileToString(rDOL, data);
		m_DOL.resize(data.size());
		std::copy(data.begin(), data.end(), m_DOL.begin());

		Write32((u32)(m_DOL_address >> m_address_shift), 0x0420, &m_disk_header);

		// 32byte aligned (plus 0x20 padding)
		m_FST_address = ROUND_UP(m_DOL_address + m_DOL.size() + 0x20, 0x20ull);
	}
}

void CVolumeDirectory::BuildFST()
{
	m_FST_data.clear();

	File::FSTEntry rootEntry;

	// read data from physical disk to rootEntry
	u32 total_entries = AddDirectoryEntries(m_root_directory, rootEntry) + 1;

	m_FST_name_offset = total_entries * ENTRY_SIZE; // offset in FST nameTable
	m_FST_data.resize(m_FST_name_offset + m_total_name_size);

	// if FST hasn't been assigned (ie no apploader/dol setup), set to default
	if (m_FST_address == 0)
		m_FST_address = APPLOADER_ADDRESS + 0x2000;

	// 4 byte aligned start of data on disk
	m_data_start_address = ROUND_UP(m_FST_address + m_FST_data.size(), 0x8000ull);
	u64 cur_data_address = m_data_start_address;

	u32 FST_offset = 0;   // Offset within FST data
	u32 name_offset = 0;  // Offset within name table
	u32 root_offset = 0;  // Offset of root of FST

	// write root entry
	WriteEntryData(FST_offset, DIRECTORY_ENTRY, 0, 0, total_entries);

	for (auto& entry : rootEntry.children)
	{
		WriteEntry(entry, FST_offset, name_offset, cur_data_address, root_offset);
	}

	// overflow check
	_dbg_assert_(DVDINTERFACE, name_offset == m_total_name_size);

	// write FST size and location
	Write32((u32)(m_FST_address     >> m_address_shift), 0x0424, &m_disk_header);
	Write32((u32)(m_FST_data.size() >> m_address_shift), 0x0428, &m_disk_header);
	Write32((u32)(m_FST_data.size() >> m_address_shift), 0x042c, &m_disk_header);
}

void CVolumeDirectory::WriteToBuffer(u64 _src_start_address, u64 _src_length, const u8* _src,
									 u64& _address, u64& _length, u8*& _pBuffer) const
{
	if (_length == 0)
		return;

	_dbg_assert_(DVDINTERFACE, _address >= _src_start_address);

	u64 src_offset = _address - _src_start_address;

	if (src_offset < _src_length)
	{
		u64 src_bytes = _src_length - src_offset;
		if (_length < src_bytes)
			src_bytes = _length;

		memcpy(_pBuffer, _src + src_offset, (size_t)src_bytes);

		_length  -= src_bytes;
		_pBuffer += src_bytes;
		_address += src_bytes;
	}
}

void CVolumeDirectory::PadToAddress(u64 _start_address, u64& _address, u64& _length, u8*& _pBuffer) const
{
	if (_start_address <= _address)
		return;

	u64 pad_bytes = _start_address - _address;
	if (pad_bytes > _length)
		pad_bytes = _length;

	if (_length > 0)
	{
		memset(_pBuffer, 0, (size_t)pad_bytes);
		_length  -= pad_bytes;
		_pBuffer += pad_bytes;
		_address += pad_bytes;
	}
}

void CVolumeDirectory::Write32(u32 data, u32 offset, std::vector<u8>* const buffer)
{
	(*buffer)[offset++] = (data >> 24);
	(*buffer)[offset++] = (data >> 16) & 0xff;
	(*buffer)[offset++] = (data >> 8) & 0xff;
	(*buffer)[offset]   = (data) & 0xff;
}

void CVolumeDirectory::WriteEntryData(u32& entry_offset, u8 type, u32 name_offset, u64 data_offset, u32 length)
{
	m_FST_data[entry_offset++] = type;

	m_FST_data[entry_offset++] = (name_offset >> 16) & 0xff;
	m_FST_data[entry_offset++] = (name_offset >> 8) & 0xff;
	m_FST_data[entry_offset++] = (name_offset) & 0xff;

	Write32((u32)(data_offset >> m_address_shift), entry_offset, &m_FST_data);
	entry_offset += 4;

	Write32((u32)length, entry_offset, &m_FST_data);
	entry_offset += 4;
}

void CVolumeDirectory::WriteEntryName(u32& name_offset, const std::string& name)
{
	strncpy((char*)&m_FST_data[name_offset + m_FST_name_offset], name.c_str(), name.length() + 1);

	name_offset += (u32)(name.length() + 1);
}

void CVolumeDirectory::WriteEntry(const File::FSTEntry& entry, u32& FST_offset, u32& name_offset, u64& data_offset, u32 parent_entry_num)
{
	if (entry.isDirectory)
	{
		u32 myOffset = FST_offset;
		u32 myEntryNum = myOffset / ENTRY_SIZE;
		WriteEntryData(FST_offset, DIRECTORY_ENTRY, name_offset, parent_entry_num, (u32)(myEntryNum + entry.size + 1));
		WriteEntryName(name_offset, entry.virtualName);

		for (const auto& child : entry.children)
		{
			WriteEntry(child, FST_offset, name_offset, data_offset, myEntryNum);
		}
	}
	else
	{
		// put entry in FST
		WriteEntryData(FST_offset, FILE_ENTRY, name_offset, data_offset, (u32)entry.size);
		WriteEntryName(name_offset, entry.virtualName);

		// write entry to virtual disk
		_dbg_assert_(DVDINTERFACE, m_virtual_disk.find(data_offset) == m_virtual_disk.end());
		m_virtual_disk.insert(make_pair(data_offset, entry.physicalName));

		// 4 byte aligned
		data_offset = ROUND_UP(data_offset + entry.size, 0x8000ull);
	}
}

static u32 Computename_size(const File::FSTEntry& parent_entry)
{
	u32 name_size = 0;
	const std::vector<File::FSTEntry>& children = parent_entry.children;
	for (auto it = children.cbegin(); it != children.cend(); ++it)
	{
		const File::FSTEntry& entry = *it;
		if (entry.isDirectory)
		{
			name_size += Computename_size(entry);
		}
		name_size += (u32)entry.virtualName.length() + 1;
	}
	return name_size;
}

u32 CVolumeDirectory::AddDirectoryEntries(const std::string& _directory, File::FSTEntry& parent_entry)
{
	u32 found_entries = ScanDirectoryTree(_directory, parent_entry);
	m_total_name_size += Computename_size(parent_entry);
	return found_entries;
}

} // namespace
