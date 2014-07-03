// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

// Based off of tachtig/twintig http://git.infradead.org/?p=users/segher/wii.git
// Copyright 2007,2008  Segher Boessenkool  <segher@kernel.crashing.org>
// Licensed under the terms of the GNU GPL, version 2
// http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt

#include <cinttypes>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <polarssl/aes.h>
#include <polarssl/md5.h>
#include <polarssl/sha1.h>

#include "Common/Common.h"
#include "Common/FileUtil.h"
#include "Common/MathUtil.h"
#include "Common/NandPaths.h"
#include "Common/StringUtil.h"
#include "Common/Crypto/ec.h"
#include "DolphinWX/MemoryCards/WiiSaveCrypted.h"

static Common::replace_v replacements;

const u8 SDKey[16] = {
	0xAB, 0x01, 0xB9, 0xD8, 0xE1, 0x62, 0x2B, 0x08,
	0xAF, 0xBA, 0xD8, 0x4D, 0xBF, 0xC2, 0xA5, 0x5D
};
const u8 MD5_BLANKER[0x10] = {
	0x0E, 0x65, 0x37, 0x81, 0x99, 0xBE, 0x45, 0x17,
	0xAB, 0x06, 0xEC, 0x22, 0x45, 0x1A, 0x57, 0x93
};
const u32 NG_id = 0x0403AC68;

bool CWiiSaveCrypted::ImportWiiSave(const char* FileName)
{
	CWiiSaveCrypted saveFile(FileName);
	return saveFile.b_valid;
}

bool CWiiSaveCrypted::ExportWiiSave(u64 TitleID)
{
	CWiiSaveCrypted exportSave("", TitleID);
	return exportSave.b_valid;
}

void CWiiSaveCrypted::ExportAllSaves()
{
	std::string titleFolder = File::GetUserPath(D_WIIUSER_IDX) + "title";
	std::vector<u64> titles;
	u32 pathMask = 0x00010000;
	for (int i = 0; i < 8; ++i)
	{
		File::FSTEntry FST_Temp;
		std::string folder = StringFromFormat("%s/%08x/", titleFolder.c_str(), pathMask | i);
		File::ScanDirectoryTree(folder, FST_Temp);

		for (const File::FSTEntry& entry : FST_Temp.children)
		{
			if (entry.isDirectory)
			{
				u32 gameid;
				if (AsciiToHex(entry.virtualName, gameid))
				{
					std::string bannerPath = StringFromFormat("%s%08x/data/banner.bin", folder.c_str(), gameid);
					if (File::Exists(bannerPath))
					{
						u64 titleID = (((u64)pathMask | i) << 32) | gameid;
						titles.push_back(titleID);
					}
				}
			}
		}
	}
	SuccessAlertT("Found %x save files", (unsigned int) titles.size());
	for (const u64& title : titles)
	{
		CWiiSaveCrypted* exportSave = new CWiiSaveCrypted("", title);
		delete exportSave;
	}
}
CWiiSaveCrypted::CWiiSaveCrypted(const char* FileName, u64 TitleID)
 : m_TitleID(TitleID)
{
	Common::ReadReplacements(replacements);
	encryptedSavePath = std::string(FileName);
	memcpy(SD_IV, "\x21\x67\x12\xE6\xAA\x1F\x68\x9F\x95\xC5\xA2\x23\x24\xDC\x6A\x98", 0x10);

	if (!TitleID) // Import
	{
		aes_setkey_dec(&m_AES_ctx, SDKey, 128);
			b_valid = true;
			ReadHDR();
			ReadBKHDR();
			ImportWiiSaveFiles();
			// TODO: check_sig()
			if (b_valid)
			{
				SuccessAlertT("Successfully imported save files");
			}
			else
			{
				PanicAlertT("Import failed");
			}
	}
	else
	{
		aes_setkey_enc(&m_AES_ctx, SDKey, 128);

		if (getPaths(true))
		{
			b_valid = true;
			WriteHDR();
			WriteBKHDR();
			ExportWiiSaveFiles();
			do_sig();
			if (b_valid)
			{
				SuccessAlertT("Successfully exported file to %s", encryptedSavePath.c_str());
			}
			else
			{
				 PanicAlertT("Export failed");
			}
		}
	}
}

void CWiiSaveCrypted::ReadHDR()
{
	File::IOFile fpData_bin(encryptedSavePath, "rb");
	if (!fpData_bin)
	{
		PanicAlertT("Cannot open %s", encryptedSavePath.c_str());
		b_valid = false;
		return;
	}
	if (!fpData_bin.ReadBytes(&_encryptedHeader, HEADER_SZ))
	{
		PanicAlertT("Failed to read header");
		b_valid = false;
		return;
	}
	fpData_bin.Close();

	aes_crypt_cbc(&m_AES_ctx, AES_DECRYPT, HEADER_SZ, SD_IV, (const u8*)&_encryptedHeader, (u8*)&_header);
	u32 bannerSize = Common::swap32(_header.hdr.BannerSize);
	if ((bannerSize < FULL_BNR_MIN) || (bannerSize > FULL_BNR_MAX) ||
		(((bannerSize - BNR_SZ) % ICON_SZ) != 0))
	{
		PanicAlertT("Not a Wii save or read failure for file header size %x", bannerSize);
		b_valid = false;
		return;
	}
	m_TitleID = Common::swap64(_header.hdr.SaveGameTitle);


	u8 md5_file[16];
	u8 md5_calc[16];
	memcpy(md5_file, _header.hdr.Md5, 0x10);
	memcpy(_header.hdr.Md5, MD5_BLANKER, 0x10);
	md5((u8*)&_header, HEADER_SZ, md5_calc);
	if (memcmp(md5_file, md5_calc, 0x10))
	{
		PanicAlertT("MD5 mismatch\n %016" PRIx64 "%016" PRIx64 " != %016" PRIx64 "%016" PRIx64, Common::swap64(md5_file),Common::swap64(md5_file+8), Common::swap64(md5_calc), Common::swap64(md5_calc+8));
		b_valid= false;
	}

	if (!getPaths())
	{
		b_valid = false;
		return;
	}
	std::string BannerFilePath = WiiTitlePath + "banner.bin";
	if (!File::Exists(BannerFilePath) || AskYesNoT("%s already exists, overwrite?", BannerFilePath.c_str()))
	{
		INFO_LOG(CONSOLE, "Creating file %s", BannerFilePath.c_str());
		File::IOFile fpBanner_bin(BannerFilePath, "wb");
		fpBanner_bin.WriteBytes(_header.BNR, bannerSize);
	}
}

void CWiiSaveCrypted::WriteHDR()
{
	if (!b_valid) return;
	memset(&_header, 0, HEADER_SZ);

	std::string BannerFilePath = WiiTitlePath + "banner.bin";
	u32 bannerSize = File::GetSize(BannerFilePath);
	_header.hdr.BannerSize =  Common::swap32(bannerSize);

	_header.hdr.SaveGameTitle = Common::swap64(m_TitleID);
	memcpy(_header.hdr.Md5, MD5_BLANKER, 0x10);
	_header.hdr.Permissions = 0x3C;

	File::IOFile fpBanner_bin(BannerFilePath, "rb");
	if (!fpBanner_bin.ReadBytes(_header.BNR, bannerSize))
	{
		PanicAlertT("Failed to read banner.bin");
		b_valid = false;
		return;
	}
	// remove nocopy flag
	_header.BNR[7] &= ~1;

	u8 md5_calc[16];
	md5((u8*)&_header, HEADER_SZ, md5_calc);
	memcpy(_header.hdr.Md5, md5_calc, 0x10);

	aes_crypt_cbc(&m_AES_ctx, AES_ENCRYPT, HEADER_SZ, SD_IV, (const u8*)&_header, (u8*)&_encryptedHeader);

	File::IOFile fpData_bin(encryptedSavePath, "wb");
	if (!fpData_bin.WriteBytes(&_encryptedHeader, HEADER_SZ))
	{
		PanicAlertT("Failed to write header for %s", encryptedSavePath.c_str());
		b_valid = false;
	}
}



void CWiiSaveCrypted::ReadBKHDR()
{
	if (!b_valid) return;

	File::IOFile fpData_bin(encryptedSavePath, "rb");
	if (!fpData_bin)
	{
		PanicAlertT("Cannot open %s", encryptedSavePath.c_str());
		b_valid = false;
		return;
	}
	fpData_bin.Seek(HEADER_SZ, SEEK_SET);
	if (!fpData_bin.ReadBytes(&bkhdr, BK_SZ))
	{
		PanicAlertT("Failed to read bk header");
		b_valid = false;
		return;
	}
	fpData_bin.Close();

	if (bkhdr.size  != Common::swap32(BK_LISTED_SZ) ||
		bkhdr.magic != Common::swap32(BK_HDR_MAGIC))
	{
		PanicAlertT("Invalid Size(%x) or Magic word (%x)", bkhdr.size, bkhdr.magic);
		b_valid = false;
		return;
	}

	_numberOfFiles = Common::swap32(bkhdr.numberOfFiles);
	_sizeOfFiles = Common::swap32(bkhdr.sizeOfFiles);
	_totalSize = Common::swap32(bkhdr.totalSize);

	if (_sizeOfFiles + FULL_CERT_SZ != _totalSize)
		WARN_LOG(CONSOLE, "Size(%x) + cert(%x) does not equal totalsize(%x)", _sizeOfFiles, FULL_CERT_SZ, _totalSize);
	if (m_TitleID != Common::swap64(bkhdr.SaveGameTitle))
		WARN_LOG(CONSOLE, "Encrypted title (%" PRIx64 ") does not match unencrypted title (%" PRIx64 ")", m_TitleID,  Common::swap64(bkhdr.SaveGameTitle));
}

void CWiiSaveCrypted::WriteBKHDR()
{
	if (!b_valid) return;
	_numberOfFiles = 0;
	_sizeOfFiles = 0;

	ScanForFiles(WiiTitlePath, FilesList, &_numberOfFiles, &_sizeOfFiles);
	memset(&bkhdr, 0, BK_SZ);
	bkhdr.size = Common::swap32(BK_LISTED_SZ);
	bkhdr.magic = Common::swap32(BK_HDR_MAGIC);
	bkhdr.NGid = NG_id;
	bkhdr.numberOfFiles = Common::swap32(_numberOfFiles);
	bkhdr.sizeOfFiles = Common::swap32(_sizeOfFiles);
	bkhdr.totalSize = Common::swap32(_sizeOfFiles + FULL_CERT_SZ);
	bkhdr.SaveGameTitle = Common::swap64(m_TitleID);

	File::IOFile fpData_bin(encryptedSavePath, "ab");
	if (!fpData_bin.WriteBytes(&bkhdr, BK_SZ))
	{
		PanicAlertT("Failed to write bkhdr");
		b_valid = false;
	}
}

void CWiiSaveCrypted::ImportWiiSaveFiles()
{
	if (!b_valid) return;

	File::IOFile fpData_bin(encryptedSavePath, "rb");
	if (!fpData_bin)
	{
		PanicAlertT("Cannot open %s", encryptedSavePath.c_str());
		b_valid = false;
		return;
	}

	fpData_bin.Seek(HEADER_SZ + BK_SZ, SEEK_SET);


	FileHDR _tmpFileHDR;

	for (u32 i = 0; i < _numberOfFiles; i++)
	{
		memset(&_tmpFileHDR, 0, FILE_HDR_SZ);
		memset(IV, 0, 0x10);
		u32 _fileSize = 0;

		if (!fpData_bin.ReadBytes(&_tmpFileHDR, FILE_HDR_SZ))
		{
			PanicAlertT("Failed to read header for file %d", i);
			b_valid = false;
		}

		if (Common::swap32(_tmpFileHDR.magic) != FILE_HDR_MAGIC)
		{
			PanicAlertT("Bad File Header");
			break;
		}
		else
		{
			std::string fileName ((char*)_tmpFileHDR.name);
			for (Common::replace_v::const_iterator iter = replacements.begin(); iter != replacements.end(); ++iter)
			{
				for (size_t j = 0; (j = fileName.find(iter->first, j)) != fileName.npos; ++j)
					fileName.replace(j, 1, iter->second);
			}

			std::string fullFilePath = WiiTitlePath + fileName;
			File::CreateFullPath(fullFilePath);
			if (_tmpFileHDR.type == 1)
			{
				_fileSize = Common::swap32(_tmpFileHDR.size);
				u32 RoundedFileSize = ROUND_UP(_fileSize, BLOCK_SZ);
				std::vector<u8> _data,_encryptedData;
				_data.reserve(RoundedFileSize);
				_encryptedData.reserve(RoundedFileSize);
				if (!fpData_bin.ReadBytes(&_encryptedData[0], RoundedFileSize))
				{
					PanicAlertT("Failed to read data from file %d", i);
					b_valid = false;
					break;
				}


				memcpy(IV, _tmpFileHDR.IV, 0x10);
				aes_crypt_cbc(&m_AES_ctx, AES_DECRYPT, RoundedFileSize, IV, (const u8*)&_encryptedData[0], &_data[0]);

				if (!File::Exists(fullFilePath) || AskYesNoT("%s already exists, overwrite?", fullFilePath.c_str()))
				{
					INFO_LOG(CONSOLE, "Creating file %s", fullFilePath.c_str());

					File::IOFile fpRawSaveFile(fullFilePath, "wb");
					fpRawSaveFile.WriteBytes(&_data[0], _fileSize);
				}
			}
		}
	}
}

void CWiiSaveCrypted::ExportWiiSaveFiles()
{
	if (!b_valid) return;

	for (u32 i = 0; i < _numberOfFiles; i++)
	{
		FileHDR tmpFileHDR;
		std::string __name;
		memset(&tmpFileHDR, 0, FILE_HDR_SZ);

		u32 _fileSize =  0;
		if (File::IsDirectory(FilesList[i]))
		{
			tmpFileHDR.type = 2;
		}
		else
		{
			_fileSize = File::GetSize(FilesList[i]);
			tmpFileHDR.type = 1;
		}

		u32 _roundedfileSize = ROUND_UP(_fileSize, BLOCK_SZ);
		tmpFileHDR.magic = Common::swap32(FILE_HDR_MAGIC);
		tmpFileHDR.size = Common::swap32(_fileSize);
		tmpFileHDR.Permissions = 0x3c;

		__name = FilesList[i].substr(WiiTitlePath.length()+1);


		for (const Common::replace_t& repl : replacements)
		{
			for (size_t j = 0; (j = __name.find(repl.second, j)) != __name.npos; ++j)
			{
				__name.replace(j, repl.second.length(), 1, repl.first);
			}
		}

		if (__name.length() > 0x44)
		{
			PanicAlertT("\"%s\" is too long for the filename, max length is 0x44 + \\0", __name.c_str());
			b_valid = false;
			return;
		}
		strncpy((char *)tmpFileHDR.name, __name.c_str(), sizeof(tmpFileHDR.name));

		{
		File::IOFile fpData_bin(encryptedSavePath, "ab");
		fpData_bin.WriteBytes(&tmpFileHDR, FILE_HDR_SZ);
		}

		if (tmpFileHDR.type == 1)
		{
			if (_fileSize == 0)
			{
				PanicAlertT("%s is a 0 byte file", FilesList[i].c_str());
				b_valid = false;
				return;
			}
			File::IOFile fpRawSaveFile(FilesList[i], "rb");
			if (!fpRawSaveFile)
			{
				PanicAlertT("%s failed to open", FilesList[i].c_str());
				b_valid = false;
			}

				std::vector<u8> _data,_encryptedData;
				_data.reserve(_roundedfileSize);
				_encryptedData.reserve(_roundedfileSize);
				memset(&_data[0], 0, _roundedfileSize);
			if (!fpRawSaveFile.ReadBytes(&_data[0], _fileSize))
			{
				PanicAlertT("Failed to read data from file: %s", FilesList[i].c_str());
				b_valid = false;
			}

			aes_crypt_cbc(&m_AES_ctx, AES_ENCRYPT, _roundedfileSize, tmpFileHDR.IV, (const u8*)&_data[0], &_encryptedData[0]);

			File::IOFile fpData_bin(encryptedSavePath, "ab");
			if (!fpData_bin.WriteBytes(&_encryptedData[0], _roundedfileSize))
				PanicAlertT("Failed to write data to file: %s", encryptedSavePath.c_str());


		}
	}
}

void CWiiSaveCrypted::do_sig()
{
	if (!b_valid) return;
	u8 sig[0x40];
	u8 ng_cert[0x180];
	u8 ap_cert[0x180];
	u8 hash[0x14];
	u8 ap_priv[30];
	u8 ap_sig[60];
	char signer[64];
	char name[64];
	u8 *data;
	u32 data_size;

	u32 NG_key_id = 0x6AAB8C59;

	u8 NG_priv[30] = {
		0, 0xAB, 0xEE, 0xC1, 0xDD, 0xB4, 0xA6, 0x16, 0x6B, 0x70, 0xFD, 0x7E, 0x56, 0x67, 0x70,
		0x57, 0x55, 0x27, 0x38, 0xA3, 0x26, 0xC5, 0x46, 0x16, 0xF7, 0x62, 0xC9, 0xED, 0x73, 0xF2
	};

	u8 NG_sig[0x3C] = {
		0, 0xD8, 0x81, 0x63, 0xB2, 0x00, 0x6B, 0x0B, 0x54, 0x82, 0x88, 0x63, 0x81, 0x1C, 0x00,
		0x71, 0x12, 0xED, 0xB7, 0xFD, 0x21, 0xAB, 0x0E, 0x50, 0x0E, 0x1F, 0xBF, 0x78, 0xAD, 0x37,
		0x00, 0x71, 0x8D, 0x82, 0x41, 0xEE, 0x45, 0x11, 0xC7, 0x3B, 0xAC, 0x08, 0xB6, 0x83, 0xDC,
		0x05, 0xB8, 0xA8, 0x90, 0x1F, 0xA8, 0x2A, 0x0E, 0x4E, 0x76, 0xEF, 0x44, 0x72, 0x99, 0xF8
	};

	sprintf(signer, "Root-CA00000001-MS00000002");
	sprintf(name, "NG%08x", NG_id);
	make_ec_cert(ng_cert, NG_sig, signer, name, NG_priv, NG_key_id);


	memset(ap_priv, 0, sizeof ap_priv);
	ap_priv[10] = 1;

	memset(ap_sig, 81, sizeof ap_sig); // temp

	sprintf(signer, "Root-CA00000001-MS00000002-NG%08x", NG_id);
	sprintf(name, "AP%08x%08x", 1, 2);
	make_ec_cert(ap_cert, ap_sig, signer, name, ap_priv, 0);

	sha1(ap_cert + 0x80, 0x100, hash);
	generate_ecdsa(ap_sig, ap_sig + 30, NG_priv, hash);
	make_ec_cert(ap_cert, ap_sig, signer, name, ap_priv, 0);

	data_size = Common::swap32(bkhdr.sizeOfFiles)  + 0x80;

	File::IOFile fpData_bin(encryptedSavePath, "rb");
	if (!fpData_bin)
	{
		b_valid = false;
		return;
	}
	data = new u8[data_size];

	fpData_bin.Seek(0xf0c0, SEEK_SET);
	if (!fpData_bin.ReadBytes(data, data_size))
	{
		b_valid = false;
		return;
	}

	sha1(data, data_size, hash);
	sha1(hash, 20, hash);
	delete []data;

	fpData_bin.Open(encryptedSavePath, "ab");
	if (!fpData_bin)
	{
		b_valid = false;
		return;
	}
	generate_ecdsa(sig, sig + 30, ap_priv, hash);
	*(u32*)(sig + 60) = Common::swap32(0x2f536969);

	fpData_bin.WriteArray(sig, sizeof(sig));
	fpData_bin.WriteArray(ng_cert, sizeof(ng_cert));
	fpData_bin.WriteArray(ap_cert, sizeof(ap_cert));

	b_valid = fpData_bin.IsGood();
}


void CWiiSaveCrypted::make_ec_cert(u8 *cert, u8 *sig, char *signer, char *name, u8 *priv, u32 key_id)
{
	memset(cert, 0, 0x180);
	*(u32*)cert = Common::swap32(0x10002);

	memcpy(cert + 4, sig, 60);
	strcpy((char*)cert + 0x80, signer);
	*(u32*)(cert + 0xc0) = Common::swap32(2);
	strcpy((char*)cert + 0xc4, name);
	*(u32*)(cert + 0x104) = Common::swap32(key_id);
	ec_priv_to_pub(priv, cert + 0x108);
}

bool CWiiSaveCrypted::getPaths(bool forExport)
{
	if (m_TitleID)
	{
		WiiTitlePath = Common::GetTitleDataPath(m_TitleID);
	}

	if (forExport)
	{
		char GameID[5];
		sprintf(GameID, "%c%c%c%c",
			(u8)(m_TitleID >> 24) & 0xFF, (u8)(m_TitleID >> 16) & 0xFF,
			(u8)(m_TitleID >>  8) & 0xFF, (u8)m_TitleID & 0xFF);

		if (!File::IsDirectory(WiiTitlePath))
		{
			b_valid = false;
			PanicAlertT("No save folder found for title %s", GameID);
			return false;
		}

		if (!File::Exists(WiiTitlePath + "banner.bin"))
		{
			b_valid = false;
			PanicAlertT("No banner file found for title  %s", GameID);
			return false;
		}
		if (encryptedSavePath.length() == 0)
		{
			encryptedSavePath = File::GetUserPath(D_USER_IDX); // If no path was passed, use User folder
		}
		encryptedSavePath += StringFromFormat("/private/wii/title/%s/data.bin", GameID);
		File::CreateFullPath(encryptedSavePath);
	}
	else
	{
		File::CreateFullPath(WiiTitlePath);
		if (!AskYesNoT("Warning! it is advised to backup all files in the folder:\n%s\nDo you wish to continue?", WiiTitlePath.c_str()))
			return false;
	}
	return true;
}

void CWiiSaveCrypted::ScanForFiles(std::string savDir, std::vector<std::string>& FileList, u32 *_numFiles, u32 *_sizeFiles)
{
	std::vector<std::string> Directories;
	*_numFiles = *_sizeFiles = 0;

	Directories.push_back(savDir);
	for (u32 i = 0; i < Directories.size(); i++)
	{
		if (i != 0)
		{
			FileList.push_back(Directories[i]);//add dir to fst
		}

		File::FSTEntry FST_Temp;
		File::ScanDirectoryTree(Directories[i], FST_Temp);
		for (const File::FSTEntry& elem : FST_Temp.children)
		{
			if (strncmp(elem.virtualName.c_str(), "banner.bin", 10) != 0)
			{
				(*_numFiles)++;
				*_sizeFiles += FILE_HDR_SZ;
				if (elem.isDirectory)
				{
					if ((elem.virtualName == "nocopy") || elem.virtualName == "nomove")
					{
						PanicAlertT("This save will likely require homebrew tools to copy to a real Wii.");
					}

					Directories.push_back(elem.physicalName);
				}
				else
				{
					FileList.push_back(elem.physicalName);
					*_sizeFiles += ROUND_UP(elem.size, BLOCK_SZ);
				}
			}
		}
	}
}

CWiiSaveCrypted::~CWiiSaveCrypted()
{
}

