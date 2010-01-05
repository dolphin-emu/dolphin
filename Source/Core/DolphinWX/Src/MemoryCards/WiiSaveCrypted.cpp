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

// Based off of tachtig http://git.infradead.org/?p=users/segher/wii.git


#include "WiiSaveCrypted.h"
#include "FileUtil.h"

u8 SD_IV[0x10] = {0x21, 0x67, 0x12, 0xE6, 0xAA, 0x1F, 0x68, 0x9F,
				  0x95, 0xC5, 0xA2, 0x23, 0x24, 0xDC, 0x6A, 0x98};

CWiiSaveCrypted::CWiiSaveCrypted(const char* FileName)
{
	_filename = std::string(FileName);

	const u8 SDKey[16] =	{0xAB, 0x01, 0xB9, 0xD8, 0xE1, 0x62, 0x2B, 0x08,
							 0xAF, 0xBA, 0xD8, 0x4D, 0xBF, 0xC2, 0xA5, 0x5D};
	AES_set_decrypt_key(SDKey, 128, &m_AES_KEY);
	b_valid = true;
	ReadHDR();
	ReadBKHDR();
}

void CWiiSaveCrypted::ReadHDR()
{
	saveFileP = fopen(_filename.c_str(), "rb");
	if (!saveFileP)
	{
		PanicAlert("Cannot open %s", _filename.c_str());
		b_valid = false;
		return;
	}
	Data_Bin_HDR * tmpHDR = new Data_Bin_HDR;

	if (fread(tmpHDR, HDR_SZ, 1, saveFileP) != 1)
	{
		PanicAlert("failed to read header");
		b_valid = false;
		return;
	}
	AES_cbc_encrypt((const u8*)tmpHDR, (u8*)&hdr, HDR_SZ, &m_AES_KEY, SD_IV, AES_DECRYPT);
	delete tmpHDR;
	_bannerSize = Common::swap32(hdr.BannerSize);
	if ((_bannerSize < FULL_BNR_MIN) || (_bannerSize > FULL_BNR_MAX) ||
		(((_bannerSize - BNR_SZ) % ICON_SZ) != 0))
	{
		PanicAlert("not a wii save or read failure for file header size %x", _bannerSize);
		b_valid = false;
		return;
	}
	_saveGameTitle = Common::swap64(hdr.SaveGameTitle);
	fclose(saveFileP);
}
void CWiiSaveCrypted::ReadBKHDR()
{
	if (!b_valid) return;
	saveFileP = fopen(_filename.c_str(), "rb");
	if (!saveFileP)
	{
		PanicAlert("Cannot open %s", _filename.c_str());
		b_valid = false;
		return;
	}
	fseek(saveFileP, HDR_SZ + FULL_BNR_MAX/*_bannerSize*/, SEEK_SET);
	
	if (fread(&bkhdr, BK_SZ, 1, saveFileP) != 1)
	{
		PanicAlert("failed to read bk header");
		b_valid = false;
		return;
	}
	
	if (Common::swap64((u8*)&bkhdr) != 0x00000070426b0001ULL)
	{
		PanicAlert("Invalid Size or Magic word %016llx", bkhdr);
		b_valid = false;
		return;
	}
	if (_saveGameTitle != Common::swap64(bkhdr.SaveGameTitle))
		WARN_LOG(CONSOLE, "encrypted title (%x) does not match unencrypted title (%x)", _saveGameTitle,  Common::swap64(bkhdr.SaveGameTitle));

	_numberOfFiles = Common::swap32(bkhdr.numberOfFiles);
	_sizeOfFiles = Common::swap32(bkhdr.sizeOfFiles);
	_totalSize = Common::swap32(bkhdr.totalSize);
	fclose(saveFileP);
}

void CWiiSaveCrypted::Extract()
{
	if (!b_valid) return;

	saveFileP = fopen(_filename.c_str(), "rb");
	if (!saveFileP)
	{
		PanicAlert("Cannot open %s", _filename.c_str());
		b_valid = false;
		return;
	}

	sprintf(dir, FULL_WII_USER_DIR "title/%08x/%08x/data/", (u32)(_saveGameTitle>>32), (u32)_saveGameTitle);

	if (!PanicYesNo("Warning! it is advised to backup all files in the folder:\n%s\nDo you wish to continue?", dir))
		return;
	
	INFO_LOG(CONSOLE, "%s", dir);
	File::CreateFullPath(dir);
	
	fseek(saveFileP, HDR_SZ, SEEK_SET);
	
	_encryptedData = new u8[_bannerSize];
	_data = new u8[_bannerSize];
	if (fread(_encryptedData, _bannerSize, 1, saveFileP) != 1)
	{
		PanicAlert("failed to read banner");
		b_valid = false;
		return;
	}
	
	AES_cbc_encrypt((const u8*)_encryptedData, (u8*)_data, _bannerSize, &m_AES_KEY, SD_IV, AES_DECRYPT);
	delete []_encryptedData;
	
	sprintf(path, "%sbanner.bin", dir);

	// remove after code is more thoroughly tested
	sprintf(tmpPath, "%s.bak", path); 
	File::Copy(path, tmpPath);
	//

	if (!File::Exists(path) || PanicYesNo("%s already exists, overwrite?", path))
	{
		INFO_LOG(CONSOLE, "creating file %s", path);
		outFileP = fopen(path, "wb");
		if (outFileP)
		{
			fwrite(_data, _bannerSize, 1, outFileP);
			fclose(outFileP);
		}
	}			
	delete []_data;

	int lastpos = HDR_SZ + FULL_BNR_MAX /*_bannerSize */+ BK_SZ;




	FileHDR _tmpFileHDR;

	for(u32 i = 0; i < _numberOfFiles; i++)
	{
		fseek(saveFileP, lastpos, SEEK_SET);
		memset(&_tmpFileHDR, 0, FILE_HDR_SZ);
		memset(IV, 0, 0x10);
		u32 roundedsize;

		fread(&_tmpFileHDR, FILE_HDR_SZ, 1, saveFileP);
		lastpos += FILE_HDR_SZ;
		if(Common::swap32(_tmpFileHDR.magic) != 0x03adf17e)
		{
			PanicAlert("Bad File Header");
			break;
		}
		else
		{
			sprintf(path, "%s%s", dir, _tmpFileHDR.name);
			if (_tmpFileHDR.type == 2)
			{
				PanicAlert("savegame with a dir, report me :p, %s", path);
				// we should prolly write any future files to this new dir
				// but tachtig doesnt do this...
				File::CreateFullPath(path);
			}
			else
			{
				roundedsize = (Common::swap32(_tmpFileHDR.size));// + 63) & ~63; // rounding makes corrupted files for NSMBwii
				lastpos += roundedsize;
				_encryptedData = new u8[roundedsize];
				_data = new u8[roundedsize];
				fread(_encryptedData, roundedsize, 1, saveFileP);
				memcpy(IV, _tmpFileHDR.IV, 0x10);
				AES_cbc_encrypt((const unsigned char *)_encryptedData, _data, roundedsize, &m_AES_KEY, IV, AES_DECRYPT);
				delete []_encryptedData;
				
				// remove after code is more thoroughly tested
				sprintf(tmpPath, "%s.bak", path); 
				File::Copy(path, tmpPath);
				//
				if (!File::Exists(path) || PanicYesNo("%s already exists, overwrite?", path))
				{
					INFO_LOG(CONSOLE, "creating file %s", path);
	
					outFileP = fopen(path, "wb");
					if (outFileP)
					{
						fwrite(_data, roundedsize, 1, outFileP);
						fclose(outFileP);
					}
				}			
				delete []_data;
			}

		}	
	}
fclose(saveFileP);
}

CWiiSaveCrypted::~CWiiSaveCrypted()
{
}

