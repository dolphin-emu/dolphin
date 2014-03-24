// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "DiscIO/Volume.h"

namespace File { struct FSTEntry; }

//
// --- this volume type is used for reading files directly from the hard drive ---
//

namespace DiscIO
{

class CVolumeDirectory : public IVolume
{
public:

	CVolumeDirectory(const std::string& rDirectory, bool bIsWii,
		const std::string& rApploader = "", const std::string& rDOL = "");

	~CVolumeDirectory();

	static bool IsValidDirectory(const std::string& rDirectory);

	bool Read(u64 Offset, u64 Length, u8* pBuffer) const override;
	bool RAWRead(u64 Offset, u64 Length, u8* pBuffer) const override;

	std::string GetUniqueID() const override;
	void SetUniqueID(std::string _ID);

	std::string GetMakerID() const override;

	std::vector<std::string> GetNames() const override;
	void SetName(std::string);

	u32 GetFSTSize() const override;

	std::string GetApploaderDate() const override;

	ECountry GetCountry() const override;

	u64 GetSize() const override;
	u64 GetRawSize() const override;

	void BuildFST();

private:
	static std::string ExtractDirectoryName(const std::string& rDirectory);

	void SetDiskTypeWii();
	void SetDiskTypeGC();

	bool SetApploader(const std::string& rApploader);

	void SetDOL(const std::string& rDOL);

	// writing to read buffer
	void WriteToBuffer(u64 SrcStartAddress, u64 SrcLength, u8* Src,
					   u64& Address, u64& Length, u8*& pBuffer) const;

	void PadToAddress(u64 StartAddress, u64& Address, u64& Length, u8*& pBuffer) const;

	void Write32(u32 data, u32 offset, u8* buffer);

	// FST creation
	void WriteEntryData(u32& entryOffset, u8 type, u32 nameOffset, u64 dataOffset, u32 length);
	void WriteEntryName(u32& nameOffset, const std::string& name);
	void WriteEntry(const File::FSTEntry& entry, u32& fstOffset, u32& nameOffset, u64& dataOffset, u32 parentEntryNum);

	// returns number of entries found in Directory
	u32 AddDirectoryEntries(const std::string& Directory, File::FSTEntry& parentEntry);

	std::string m_rootDirectory;

	std::map<u64, std::string> m_virtualDisk;

	u32 m_totalNameSize;

	// gc has no shift, wii has 2 bit shift
	u32 m_addressShift;

	// first address on disk containing file data
	u64 m_dataStartAddress;

	u64 m_fstNameOffset;
	u64 m_fstSize;
	u8* m_FSTData;

	u8* m_diskHeader;

	#pragma pack(push, 1)
	struct SDiskHeaderInfo
	{
		u32 debug_mntr_size;
		u32 simulated_mem_size;
		u32 arg_offset;
		u32 debug_flag;
		u32 track_location;
		u32 track_size;
		u32 countrycode;
		u32 unknown;
		u32 unknown2;

		// All the data is byteswapped
		SDiskHeaderInfo() {
			debug_mntr_size = 0;
			simulated_mem_size = 0;
			arg_offset = 0;
			debug_flag = 0;
			track_location = 0;
			track_size = 0;
			countrycode = 0;
			unknown = 0;
			unknown2 = 0;
		}
	};
	#pragma pack(pop)
	SDiskHeaderInfo* m_diskHeaderInfo;

	u64 m_apploaderSize;
	u8* m_apploader;

	u64 m_DOLSize;
	u8* m_DOL;

	static const u8 ENTRY_SIZE = 0x0c;
	static const u8 FILE_ENTRY = 0;
	static const u8 DIRECTORY_ENTRY = 1;
	static const u64 DISKHEADER_ADDRESS = 0;
	static const u64 DISKHEADERINFO_ADDRESS = 0x440;
	static const u64 APPLOADER_ADDRESS = 0x2440;
	static const u32 MAX_NAME_LENGTH = 0x3df;
	u64 FST_ADDRESS;
	u64 DOL_ADDRESS;
};

} // namespace
