// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <vector>
#include <polarssl/aes.h>

#include "Common/CommonTypes.h"

// --- this is used for encrypted Wii save files


class CWiiSaveCrypted
{
public:
	bool static ImportWiiSave(const char* FileName);
	bool static ExportWiiSave(u64 TitleID);
	void static ExportAllSaves();

private:
	CWiiSaveCrypted(const char* FileName, u64 TitleID = 0);
	~CWiiSaveCrypted();
	void ReadHDR();
	void ReadBKHDR();
	void WriteHDR();
	void WriteBKHDR();
	void Extract(){;}
	void ImportWiiSaveFiles();
	void ExportWiiSaveFiles(); // To data.bin
	void do_sig();
	void make_ec_cert(u8 *cert, u8 *sig, char *signer, char *name, u8 *priv, u32 key_id);
	bool getPaths(bool forExport = false);
	void ScanForFiles(std::string savDir, std::vector<std::string>&FilesList, u32 *_numFiles, u32 *_sizeFiles);

	aes_context m_AES_ctx;
	u8 SD_IV[0x10];
	std::vector<std::string> FilesList;

	std::string encryptedSavePath;

	std::string WiiTitlePath;

	u8  IV[0x10];

	u32 //_bannerSize,
		_numberOfFiles,
		_sizeOfFiles,
		_totalSize;

	u64 m_TitleID;

	bool b_valid;

	enum
	{
		BLOCK_SZ       = 0x40,
		HDR_SZ         = 0x20,
		ICON_SZ        = 0x1200,
		BNR_SZ         = 0x60a0,
		FULL_BNR_MIN   = 0x72a0, // BNR_SZ + 1*ICON_SZ
		FULL_BNR_MAX   = 0xF0A0, // BNR_SZ + 8*ICON_SZ
		HEADER_SZ      = 0xF0C0, // HDR_SZ + FULL_BNR_MAX
		BK_LISTED_SZ   = 0x70,   // Size before rounding to nearest block
		BK_SZ          = 0x80,
		FILE_HDR_SZ    = 0x80,

		SIG_SZ         = 0x40,
		NG_CERT_SZ     = 0x180,
		AP_CERT_SZ     = 0x180,
		FULL_CERT_SZ   = 0x3C0,  // SIG_SZ + NG_CERT_SZ + AP_CERT_SZ + 0x80?

		BK_HDR_MAGIC   = 0x426B0001,
		FILE_HDR_MAGIC = 0x03adf17e
	};

#pragma pack(push,1)

	struct Data_Bin_HDR // encrypted
	{
		u64 SaveGameTitle;
		u32 BannerSize; // (0x72A0 or 0xF0A0, also seen 0xBAA0)
		u8 Permissions;
		u8 unk1; // maybe permissions is a be16
		u8 Md5[0x10]; // md5 of plaintext header with md5 blanker applied
		u16 unk2;
	};

	struct HEADER
	{
		Data_Bin_HDR hdr;
		u8 BNR[FULL_BNR_MAX];
	}_header, _encryptedHeader;

	struct BK_Header // Not encrypted
	{
		u32 size;   // 0x00000070
		// u16 magic;  // 'Bk'
		// u16 magic2; // or version (0x0001)
		u32 magic;  // 0x426B0001
		u32 NGid;
		u32 numberOfFiles;
		u32 sizeOfFiles;
		u32 unk1;
		u32 unk2;
		u32 totalSize;
		u8 unk3[64];
		u64 SaveGameTitle;
		u8 MACaddress[6];
		u8 padding[0x12];
	}bkhdr;

	struct FileHDR // encrypted
	{
		u32 magic; //0x03adf17e
		u32 size;
		u8 Permissions;
		u8 attrib;
		u8 type; // (1=file, 2=directory)
		u8 name[0x45];
		u8 IV[0x10];
		u8 unk[0x20];
	};
#pragma pack(pop)
};
