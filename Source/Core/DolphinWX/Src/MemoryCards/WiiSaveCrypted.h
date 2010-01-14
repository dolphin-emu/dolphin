// Copyright (C) 2003 Dolphin Project.

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

#ifndef _WII_SAVE_CRYPTED
#define _WII_SAVE_CRYPTED

#include "AES/aes.h"
#include "StringUtil.h"
#include "Crypto/tools.h"
#include "Crypto/md5.h"

// --- this is used for encrypted Wii save files


	
class CWiiSaveCrypted
{
public:
	CWiiSaveCrypted(const char* FileName, u64 title = 0);
	~CWiiSaveCrypted();
	void ReadHDR();
	void ReadBKHDR();
	void WriteHDR();
	void WriteBKHDR();
	void Extract(){;}
	void ImportWiiSaveFiles();
	void ExportWiiSaveFiles(); // To data.bin
	void do_sig();
	void make_ec_cert(u8 *cert, u8 *sig, char *signer, char *name, u8 *priv,
                         u32 key_id);
	bool getPaths(bool _export = false);
	void ScanForFiles(std::string savDir, std::vector<std::string>&FilesList, u32 *_numFiles, u32 *_sizeFiles);

private:
	AES_KEY m_AES_KEY;
	std::vector<std::string> FilesList; 

	FILE *fpData_bin,
		 *fpBanner_bin,
		 *fpRawSaveFile;
		 
	char pathData_bin[2048],
		 pathSavedir[2048],
		 pathBanner_bin[2048], //should always be FULL_WII_USER_DIR "title/%08x/%08x/data/"
		 pathRawSave[2048],
		_saveGameString[5];

	u8  IV[0x10],
		*_encryptedData,
		*_data,
		md5_file[16],
		md5_calc[16];

	u32 _bannerSize,
		_numberOfFiles,
		_sizeOfFiles,
		_totalSize,
		_fileSize,
		_roundedfileSize;

	u64 _saveGameTitle;

	bool b_valid,
		b_tryAgain;

	enum
	{
		BLOCK_SZ = 0x40,
		HDR_SZ = 0x20,
		ICON_SZ = 0x1200,
		BNR_SZ = 0x60a0,
		FULL_BNR_MIN = 0x72a0,	// BNR_SZ + 1*ICON_SZ
		FULL_BNR_MAX = 0xF0A0,	// BNR_SZ + 8*ICON_SZ
		HEADER_SZ = 0xF0C0,		// HDR_SZ + FULL_BNR_MAX
		BK_LISTED_SZ = 0x70,	// Size before rounding to nearest block
		BK_SZ  = 0x80,
		FILE_HDR_SZ = 0x80,
		
		SIG_SZ = 0x40,
		NG_CERT_SZ = 0x180,
		AP_CERT_SZ = 0x180,
		FULL_CERT_SZ =  0x3C0,	// SIG_SZ +	NG_CERT_SZ + AP_CERT_SZ + 0x80?
	
	
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
//		u16 magic;  // 'Bk'
//		u16 magic2; // or version (0x0001)
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

#endif
