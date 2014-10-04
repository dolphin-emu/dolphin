// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <vector>
#include <polarssl/aes.h>

#include "Common/CommonTypes.h"

class CWiiSaveCrypted
{
public:
	bool static ImportWiiSave(const std::string& filename);
	bool static ExportWiiSave(u64 title_id);
	void static ExportAllSaves();

private:
	CWiiSaveCrypted(const std::string& filename, u64 title_id = 0);
	~CWiiSaveCrypted();
	void ReadHDR();
	void ReadBKHDR();
	void WriteHDR();
	void WriteBKHDR();
	void Extract(){}
	void ImportWiiSaveFiles();
	void ExportWiiSaveFiles();
	void do_sig();
	void make_ec_cert(u8 *cert, const u8 *sig, const char *signer, const char *name,
		const u8 *priv, const u32 key_id);
	bool getPaths(bool for_export = false);
	void ScanForFiles(const std::string& save_directory, std::vector<std::string>& file_list,
		u32 *num_files, u32 *size_files);

	static const u8 s_sd_key[16];
	static const u8 s_md5_blanker[16];
	static const u32 s_ng_id;

	aes_context m_aes_ctx;
	u8 m_sd_iv[0x10];
	std::vector<std::string> m_files_list;

	std::string m_encrypted_save_path;

	std::string m_wii_title_path;

	u8  m_iv[0x10];

	u32 m_files_list_size;
	u32 m_size_of_files;
	u32 m_total_size;

	u64 m_title_id;

	bool m_valid;

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
	};

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
	};

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

	HEADER m_header;
	HEADER m_encrypted_header;
	BK_Header m_bk_hdr;
};
