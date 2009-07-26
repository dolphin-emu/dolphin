// Copyright (C) 2003-2009 Dolphin Project.

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
#include "GCMemcard.h"
#include "ColorUtil.h"

// i think there is support for this stuff in the common lib... if not there should be support

// undefined functions... prolly it means something like that
void ByteSwap(u8 *valueA, u8 *valueB)
{
	u8 tmp = *valueA;
	*valueA = *valueB;
	*valueB = tmp;
}

void decode5A3image(u32* dst, u16* src, int width, int height)
{
	for (int y = 0; y < height; y += 4)
	{
		for (int x = 0; x < width; x += 4)
		{
			for (int iy = 0; iy < 4; iy++, src += 4)
			{
				for (int ix = 0; ix < 4; ix++)
				{
					u32 RGBA = ColorUtil::Decode5A3(Common::swap16(src[ix]));
					dst[(y + iy) * width + (x + ix)] = RGBA;
				}
			}
		}
	}
}

void decodeCI8image(u32* dst, u8* src, u16* pal, int width, int height)
{
	for (int y = 0; y < height; y += 4)
	{
		for (int x = 0; x < width; x += 8)
		{
			for (int iy = 0; iy < 4; iy++, src += 8)
			{
				u32 *tdst = dst+(y+iy)*width+x;
				for (int ix = 0; ix < 8; ix++)
				{
					// huh, this seems wrong. CI8, not 5A3, no?
					tdst[ix] = ColorUtil::Decode5A3(Common::swap16(pal[src[ix]]));
				}
			}
		}
	}
}

GCMemcard::GCMemcard(const char *filename)
{
	FILE *mcd = fopen(filename, "r+b");
	mcdFile = mcd;
	fail = false;
	if (!mcd)
	{
		if (!PanicYesNo("\"%s\" does not exist.\n Create a new 16MB Memcard?", filename))
		{
			fail = true;
			return;
		}
		mcd = fopen(filename, "wb");
		if (!mcd)
		{
			fail = true;
			return;
		}
		mcdFile = mcd;
		Format();
		fclose(mcd);
		mcd = fopen(filename, "r+b");
	}
	else
	{
	//This function can be removed once more about hdr is known and we can check for a valid header
		std::string fileType;
		SplitPath(filename, NULL, NULL, &fileType);
		if (strcasecmp(fileType.c_str(), ".raw") && strcasecmp(fileType.c_str(), ".gcp"))
		{
			fail = true;
			PanicAlert("File has the extension \"%s\"\nvalid extensions are (.raw/.gcp)", fileType.c_str());
			return;
		}
	}

	fseek(mcd, 0, SEEK_SET);
	if (fread(&hdr, 1, BLOCK_SIZE, mcd) != BLOCK_SIZE)
	{
		fail = true;
		PanicAlert("Failed to read header correctly\n(0x0000-0x1FFF)");
		return;
	}
	if (fread(&dir, 1, BLOCK_SIZE, mcd) != BLOCK_SIZE)
	{
		fail = true;
		PanicAlert("Failed to read directory correctly\n(0x2000-0x3FFF)");
		return;
	}
	if (fread(&dir_backup, 1, BLOCK_SIZE, mcd) != BLOCK_SIZE)
	{
		fail = true;
		PanicAlert("Failed to read directory backup correctly\n(0x4000-0x5FFF)");
		return;
	}
	if (fread(&bat, 1, BLOCK_SIZE, mcd) != BLOCK_SIZE)
	{
		fail = true;
		PanicAlert("Failed to read block allocation table correctly\n(0x6000-0x7FFF)");
		return;
	}
	if (fread(&bat_backup, 1, BLOCK_SIZE, mcd) != BLOCK_SIZE)
	{
		fail = true;
		PanicAlert("Failed to read block allocation table backup correctly\n(0x8000-0x9FFF)");
		return;
	}

	u32 csums = TestChecksums();
	
	if (csums&1)
	{
		// header checksum error!
		// invalid files do not always get here
		fail = true;
		PanicAlert("Header checksum failed");
		return;
	}

	if (csums&2) // directory checksum error!
	{
		if (csums&4)
		{
			// backup is also wrong!
			fail = true;
			PanicAlert("Directory checksum failed\n and Directory backup checksum failed");
			return;
		}
		else
		{
			// backup is correct, restore
			dir = dir_backup;
			bat = bat_backup;

			// update checksums
			csums = TestChecksums();
		}
	}

	if (csums&8) // BAT checksum error!
	{
		if (csums&16)
		{
			// backup is also wrong!
			fail = true;
			PanicAlert("Block Allocation Table checksum failed");
			return;
		}
		else
		{
			// backup is correct, restore
			dir = dir_backup;
			bat = bat_backup;

			// update checksums
			csums = TestChecksums();
		}
// It seems that the backup having a larger counter doesn't necessarily mean
// the backup should be copied?
//	}
//
//	if(BE16(dir_backup.UpdateCounter) > BE16(dir.UpdateCounter)) //check if the backup is newer
//	{
//		dir = dir_backup;
//		bat = bat_backup; // needed?
	}

	fseek(mcd, 0xa000, SEEK_SET);

	u16 sizeMb = BE16(hdr.SizeMb);
	switch (sizeMb)
	{
	case MemCard59Mb:
	case MemCard123Mb:
	case MemCard251Mb:
	case Memcard507Mb:
	case MemCard1019Mb:
	case MemCard2043Mb:
	{
		maxBlock = (u32)sizeMb * MBIT_TO_BLOCKS;
		mc_data_size = (maxBlock - MC_FST_BLOCKS) * BLOCK_SIZE;
		mc_data = new u8[mc_data_size];

		size_t read = fread(mc_data, 1, mc_data_size, mcd);
		if (mc_data_size != read)
		{
			fail = true;
			PanicAlert("Failed to read save data\n(0xA000-)\nMemcard may be truncated");
		}
		break;
	}
	default:
		fail = true;
		PanicAlert("Memcard failed to load\n Card size is invalid (%04X)", sizeMb);
	}
}

GCMemcard::~GCMemcard()
{
	if (!mcdFile) return;
	fclose((FILE*)mcdFile);
}

bool GCMemcard::IsOpen()
{
	return (mcdFile!=NULL);
}

bool GCMemcard::Save()
{
	bool completeWrite = true;
	FILE *mcd=(FILE*)mcdFile;
	fseek(mcd, 0, SEEK_SET);
	if (fwrite(&hdr,		1, BLOCK_SIZE, mcd) != BLOCK_SIZE) completeWrite = false;
	if (fwrite(&dir,		1, BLOCK_SIZE, mcd) != BLOCK_SIZE) completeWrite = false;
	if (fwrite(&dir_backup, 1, BLOCK_SIZE, mcd) != BLOCK_SIZE) completeWrite = false;
	if (fwrite(&bat,		1, BLOCK_SIZE ,mcd) != BLOCK_SIZE) completeWrite = false;
	if (fwrite(&bat_backup, 1, BLOCK_SIZE, mcd) != BLOCK_SIZE) completeWrite = false;
	if (fwrite(mc_data, 1, mc_data_size, mcd) != mc_data_size) completeWrite = false;
	return completeWrite;
}

void GCMemcard::calc_checksumsBE(u16 *buf, u32 num, u16 *c1, u16 *c2)
{
	*c1 = 0;*c2 = 0;
	for (u32 i = 0; i < num; ++i)
	{
		//weird warnings here
		*c1 += Common::swap16(buf[i]);
		*c2 += Common::swap16((u16)(buf[i] ^ 0xffff));
	}
	if (*c1 == 0xffff)
	{
		*c1 = 0;
	}
	if (*c2 == 0xffff)
	{
		*c2 = 0;
	}
}

u32  GCMemcard::TestChecksums()
{
	if (!mcdFile) return 0xFFFFFFFF;

	u16 csum1=0,
		csum2=0;

	u32 results = 0;

	calc_checksumsBE((u16*)&hdr, 0xFE , &csum1, &csum2);
	if (BE16(hdr.CheckSum1) != csum1) results |= 1;
	if (BE16(hdr.CheckSum2) != csum2) results |= 1;

	calc_checksumsBE((u16*)&dir, 0xFFE, &csum1, &csum2);
	if (BE16(dir.CheckSum1) != csum1) results |= 2;
	if (BE16(dir.CheckSum2) != csum2) results |= 2;

	calc_checksumsBE((u16*)&dir_backup, 0xFFE, &csum1, &csum2);
	if (BE16(dir_backup.CheckSum1) != csum1) results |= 4;
	if (BE16(dir_backup.CheckSum2) != csum2) results |= 4;

	calc_checksumsBE((u16*)(((u8*)&bat)+4), 0xFFE, &csum1, &csum2);
	if (BE16(bat.CheckSum1) != csum1) results |= 8;
	if (BE16(bat.CheckSum2) != csum2) results |= 8;

	calc_checksumsBE((u16*)(((u8*)&bat_backup)+4), 0xFFE, &csum1, &csum2);
	if (BE16(bat_backup.CheckSum1) != csum1) results |= 16;
	if (BE16(bat_backup.CheckSum2) != csum2) results |= 16;

	return 0;
}

bool GCMemcard::FixChecksums()
{
	if (!mcdFile) return false;

	u16 csum1=0,
		csum2=0;

	calc_checksumsBE((u16*)&hdr, 0xFE, &csum1, &csum2);
	hdr.CheckSum1[0] = u8(csum1 >> 8);
	hdr.CheckSum1[1] = u8(csum1);
	hdr.CheckSum2[0] = u8(csum2 >> 8);
	hdr.CheckSum2[1] = u8(csum2);

	calc_checksumsBE((u16*)&dir, 0xFFE, &csum1, &csum2);
	dir.CheckSum1[0] = u8(csum1 >> 8);
	dir.CheckSum1[1] = u8(csum1);
	dir.CheckSum2[0] = u8(csum2 >> 8);
	dir.CheckSum2[1] = u8(csum2);

	calc_checksumsBE((u16*)&dir_backup, 0xFFE, &csum1, &csum2);
	dir_backup.CheckSum1[0] = u8(csum1 >> 8);
	dir_backup.CheckSum1[1] = u8(csum1);
	dir_backup.CheckSum2[0] = u8(csum2 >> 8);
	dir_backup.CheckSum2[1] = u8(csum2);

	calc_checksumsBE((u16*)(((u8*)&bat)+4), 0xFFE, &csum1, &csum2);
	bat.CheckSum1[0] = u8(csum1 >> 8);
	bat.CheckSum1[1] = u8(csum1);
	bat.CheckSum2[0] = u8(csum2 >> 8);
	bat.CheckSum2[1] = u8(csum2);

	calc_checksumsBE((u16*)(((u8*)&bat_backup)+4), 0xFFE, &csum1, &csum2);
	bat_backup.CheckSum1[0] = u8(csum1 >> 8);
	bat_backup.CheckSum1[1] = u8(csum1);
	bat_backup.CheckSum2[0] = u8(csum2 >> 8);
	bat_backup.CheckSum2[1] = u8(csum2);

	return true;
}

u8 GCMemcard::GetNumFiles()
{
	if (!mcdFile) return 0;
	u8 j = 0;
	for (int i = 0; i < DIRLEN; i++)
	{
		if (BE32(dir.Dir[i].Gamecode)!= 0xFFFFFFFF)
			 j++;
	}
	return j;
}

u16 GCMemcard::GetFreeBlocks()
{
	if (!mcdFile) return 0;
	return BE16(bat.FreeBlocks);
}

u8 GCMemcard::TitlePresent(DEntry d)
{
	if (!mcdFile) return DIRLEN;

	u8 i = 0;
	while(i < DIRLEN)
	{
		if ((BE32(dir.Dir[i].Gamecode) == BE32(d.Gamecode)) &&
			(!memcmp(dir.Dir[i].Filename, d.Filename, 32)))
		{
			break;
		}
		i++;
	}
	return i;
}

// DEntry functions, all take u8 index < DIRLEN (127)
// Functions that have ascii output take a char *buffer

bool GCMemcard::DEntry_GameCode(u8 index, char *buffer)
{
	if (!mcdFile) return false;

	memcpy(buffer, dir.Dir[index].Gamecode, 4);
	buffer[4] = 0;
	return true;
}

bool GCMemcard::DEntry_Markercode(u8 index, char *buffer)
{
	if (!mcdFile) return false;
	memcpy(buffer, dir.Dir[index].Markercode, 2);
	buffer[2] = 0;
	return true;
}
bool GCMemcard::DEntry_BIFlags(u8 index, char *buffer)
{
	if (!mcdFile) return false;

	int x = dir.Dir[index].BIFlags;
	for (int i = 0; i < 8; i++)
	{
		buffer[i] = (x & 0x80) ? '1' : '0';
		x = x << 1;
	}
	buffer[8] = 0;
	return true;
}

bool GCMemcard::DEntry_FileName(u8 index, char *buffer)
{
	if (!mcdFile) return false;
	memcpy (buffer, (const char*)dir.Dir[index].Filename, DENTRY_STRLEN);
	buffer[31] = 0;
	return true;
}

u32 GCMemcard::DEntry_ModTime(u8 index)
{
	return BE32(dir.Dir[index].ModTime);
}

u32 GCMemcard::DEntry_ImageOffset(u8 index)
{
	return BE32(dir.Dir[index].ImageOffset);
}

bool GCMemcard::DEntry_IconFmt(u8 index, char *buffer)
{
	if (!mcdFile) return false;

	int x = dir.Dir[index].IconFmt[0];
	for(int i = 0; i < 16; i++)
	{
		if (i == 8) x = dir.Dir[index].IconFmt[1];
		buffer[i] = (x & 0x80) ? '1' : '0';
		x = x << 1;
	}
	buffer[16] = 0;
	return true;
	
}

u16 GCMemcard::DEntry_AnimSpeed(u8 index)
{
	return BE16(dir.Dir[index].AnimSpeed);
}

bool GCMemcard::DEntry_Permissions(u8 index, char *fn)
{
	if (!mcdFile) return false;
	fn[0] = (dir.Dir[index].Permissions & 16) ? 'x' : 'M';
	fn[1] = (dir.Dir[index].Permissions &  8) ? 'x' : 'C';
	fn[2] = (dir.Dir[index].Permissions &  4) ? 'P' : 'x';
	fn[3] = 0;
	return true;
}

u8 GCMemcard::DEntry_CopyCounter(u8 index)
{
	return dir.Dir[index].CopyCounter;
}

u16 GCMemcard::DEntry_FirstBlock(u8 index)
{
	if (!mcdFile) return 0xFFFF;
	u16 block = BE16(dir.Dir[index].FirstBlock);
	if (block > (u16) maxBlock) return 0xFFFF;
	return block;
}

u16 GCMemcard::DEntry_BlockCount(u8 index)
{
	if (!mcdFile) return 0xFFFF;

	u16 blocks = BE16(dir.Dir[index].BlockCount);
	if (blocks > (u16) maxBlock) return 0xFFFF;
	return blocks;
}

u32 GCMemcard::DEntry_CommentsAddress(u8 index)
{
	return BE32(dir.Dir[index].CommentsAddr);
}

bool GCMemcard::DEntry_Comment1(u8 index, char* buffer)
{
	if (!mcdFile) return false;

	u32 Comment1 = BE32(dir.Dir[index].CommentsAddr);
	u32 DataBlock = BE16(dir.Dir[index].FirstBlock) - MC_FST_BLOCKS;
	if ((DataBlock > maxBlock) || (Comment1 == 0xFFFFFFFF))
	{
		buffer[0] = 0;
		return false;
	}
	memcpy(buffer, mc_data + (DataBlock * BLOCK_SIZE) + Comment1, DENTRY_STRLEN);
	buffer[31] = 0;
	return true;
}

bool GCMemcard::DEntry_Comment2(u8 index, char* buffer)
{
	if (!mcdFile) return false;

	u32 Comment1 = BE32(dir.Dir[index].CommentsAddr);
	u32 Comment2 = Comment1 + DENTRY_STRLEN;
	u32 DataBlock = BE16(dir.Dir[index].FirstBlock) - MC_FST_BLOCKS;
	if ((DataBlock > maxBlock) || (Comment1 == 0xFFFFFFFF))
	{
		buffer[0] = 0;
		return false;
	}
	memcpy(buffer, mc_data + (DataBlock * BLOCK_SIZE) + Comment2, DENTRY_STRLEN);
	buffer[31] = 0;
	return true;
}

bool GCMemcard::DEntry_Copy(u8 index, GCMemcard::DEntry& info)
{
	if (!mcdFile) return false;

	info = dir.Dir[index];
	return true;
}

u32 GCMemcard::DEntry_GetSaveData(u8 index, u8* dest, bool old)
{
	if (!mcdFile) return NOMEMCARD;

	u16 block = DEntry_FirstBlock(index);
	u16 saveLength = DEntry_BlockCount(index);
	u16 memcardSize = BE16(hdr.SizeMb) * MBIT_TO_BLOCKS;

	if ((block == 0xFFFF) || (saveLength == 0xFFFF)
		|| (block + saveLength > memcardSize))
	{
		return FAIL;
	}

	if (!old)
	{
		memcpy(dest, mc_data + BLOCK_SIZE * (block - MC_FST_BLOCKS), saveLength * BLOCK_SIZE);
	}
	else
	{
		if (block == 0) return FAIL;
		while (block != 0xffff) 
		{
			memcpy(dest, mc_data + BLOCK_SIZE * (block - MC_FST_BLOCKS), BLOCK_SIZE);
			dest += BLOCK_SIZE;

			u16 nextblock = Common::swap16(bat.Map[block - MC_FST_BLOCKS]);
			if (block + saveLength != memcardSize && nextblock > 0)
			{//Fixes for older memcards that were not initialized with FF
				block = nextblock;
			}
			else break;
		}
	}
	return SUCCESS;
}
// End DEntry functions

u32  GCMemcard::ImportFile(DEntry& direntry, u8* contents, int remove)
{
	if (!mcdFile) return NOMEMCARD;

	if (GetNumFiles() >= DIRLEN)
	{
		return OUTOFDIRENTRIES;
	}
	if (BE16(bat.FreeBlocks) < BE16(direntry.BlockCount))
	{
		return OUTOFBLOCKS;
	}
	if (!remove && (TitlePresent(direntry) != DIRLEN))
	{
		return TITLEPRESENT;
	}

	// find first free data block -- assume no freespace fragmentation
	int totalspace = (((u32)BE16(hdr.SizeMb) * MBIT_TO_BLOCKS) - MC_FST_BLOCKS);
	int firstFree1 = BE16(bat.LastAllocated) + 1;
	int firstFree2 = 0;
	for (int i = 0; i < DIRLEN; i++)
	{
		if (BE32(dir.Dir[i].Gamecode) == 0xFFFFFFFF)
		{
			break;
		}
		else
		{
			firstFree2 = max<int>(firstFree2,
				(int)(BE16(dir.Dir[i].FirstBlock) + BE16(dir.Dir[i].BlockCount)));
		}
	}
	firstFree1 = max<int>(firstFree1, firstFree2);

	// find first free dir entry
	int index = -1;
	for (int i=0; i < DIRLEN; i++)
	{
		if (BE32(dir.Dir[i].Gamecode) == 0xFFFFFFFF)
		{
			index = i;
			dir.Dir[i] = direntry;
			dir.Dir[i].FirstBlock[0] = u8(firstFree1 >> 8);
			dir.Dir[i].FirstBlock[1] = u8(firstFree1);
			if (!remove)
			{
				dir.Dir[i].CopyCounter = dir.Dir[i].CopyCounter+1;
			}
			dir_backup = dir;
			break;
		}
	}

	// keep assuming no freespace fragmentation, and copy over all the data
	u8 *destination = mc_data + (firstFree1 - MC_FST_BLOCKS) * BLOCK_SIZE;

	int fileBlocks = BE16(direntry.BlockCount);
	memcpy(destination, contents, BLOCK_SIZE * fileBlocks);
	bat_backup = bat;
	u16 last = BE16(bat_backup.LastAllocated);
	u16 i = (last - 4);
	int j = 2;
	while(j < BE16(direntry.BlockCount) + 1)
	{
		bat_backup.Map[i] = Common::swap16(last + (u16)j);
		i++;
		j++;
	}
	bat_backup.Map[i++] = 0xFFFF;
	//Set bat.map to 0 for each block that was removed
	for (int k = 0; k < remove; k++)
	{
		bat_backup.Map[i++] = 0x0000;
	}

	//update last allocated block
	int lastallocated = BE16(bat_backup.LastAllocated) + j - 1;
	bat_backup.LastAllocated[0] = u8(lastallocated >> 8);
	bat_backup.LastAllocated[1] = u8(lastallocated);

	//update freespace counter
	int freespace1 = totalspace - firstFree1 - fileBlocks + MC_FST_BLOCKS;
	bat_backup.FreeBlocks[0] = u8(freespace1 >> 8);
	bat_backup.FreeBlocks[1] = u8(freespace1);

	
	if (!remove)
	{	// ... and dir update counter
		int updateCtr = BE16(dir_backup.UpdateCounter) + 1;
		dir_backup.UpdateCounter[0] = u8(updateCtr >> 8);
		dir_backup.UpdateCounter[1] = u8(updateCtr);
		// ... and bat update counter
		updateCtr = BE16(bat_backup.UpdateCounter) + 1;
		bat_backup.UpdateCounter[0] = u8(updateCtr >> 8);
		bat_backup.UpdateCounter[1] = u8(updateCtr);
	}
	bat = bat_backup;
	return SUCCESS;
}

u32 GCMemcard::RemoveFile(u8 index) //index in the directory array
{
	if (!mcdFile) return NOMEMCARD;


	//error checking
	u16 startingblock = 0;
	for (int i = 0; i < DIRLEN; i++)
	{
		if (startingblock > BE16(dir.Dir[i].FirstBlock))
			return DELETE_FAIL;
		startingblock = BE16(dir.Dir[i].FirstBlock);
	}

	//backup the directory and bat (not really needed here but meh :P
	dir_backup = dir;
	bat_backup = bat;

	//free the blocks
	int blocks_left = BE16(dir.Dir[index].BlockCount);
	int block = BE16(dir.Dir[index].FirstBlock) - 1;
	bat.LastAllocated[0] = (u8)(block  >> 8);
	bat.LastAllocated[1] = (u8)block;

	u8 nextIndex = index + 1;
	memset(&(dir.Dir[index]), 0xFF, DENTRY_SIZE);

	while (nextIndex < DIRLEN)
	{
		DEntry * tempDEntry = new DEntry;
		DEntry_Copy(nextIndex, *tempDEntry);
		u8 * tempSaveData = NULL;
		//Only get file data if it is a valid dir entry
		if (BE16(tempDEntry->FirstBlock) != 0xFFFF)
		{
			u16 freeBlock= BE16(bat.FreeBlocks) - BE16(tempDEntry->BlockCount);
			bat.FreeBlocks[0] = u8(freeBlock >> 8);
			bat.FreeBlocks[1] = u8(freeBlock);

			u16 size = DEntry_BlockCount(nextIndex);
			if (size != 0xFFFF)
			{
				tempSaveData = new u8[size * BLOCK_SIZE];
				switch (DEntry_GetSaveData(nextIndex, tempSaveData, true))
				{
				case NOMEMCARD:
					delete[] tempSaveData;
					break;
				case FAIL:
					delete[] tempSaveData;
					return FAIL;
					break;
				}
			}
		}
		memset(&(dir.Dir[nextIndex]), 0xFF, DENTRY_SIZE);
		//Only call import file if DEntry_GetSaveData returns SUCCESS
		if (tempSaveData != NULL)
		{
			ImportFile(*tempDEntry, tempSaveData, blocks_left);
			delete[] tempSaveData;
		}
		delete tempDEntry;
		nextIndex++;

	}
	//Added to clean up if removing last file
	if (BE16(bat.LastAllocated) == (u16)4)
	{
		for (int j = 0; j < blocks_left; j++)
		{
			bat.Map[j] = 0x0000;
		}
	}
	// increment update counter
	int updateCtr = BE16(dir.UpdateCounter) + 1;
	dir.UpdateCounter[0] = u8(updateCtr >> 8);
	dir.UpdateCounter[1] = u8(updateCtr);

	return SUCCESS;
}

u32 GCMemcard::CopyFrom(GCMemcard& source, u8 index)
{
	if (!mcdFile) return NOMEMCARD;

	DEntry tempDEntry;
	if (!source.DEntry_Copy(index, tempDEntry)) return NOMEMCARD;
	
	u32 size = source.DEntry_BlockCount(index);
	if (size == 0xFFFF) return INVALIDFILESIZE;
	u8 *tempSaveData = new u8[size * BLOCK_SIZE];

	switch (source.DEntry_GetSaveData(index, tempSaveData, true))
	{
	case FAIL:
		delete[] tempSaveData;
		return FAIL;
	case NOMEMCARD:
		delete[] tempSaveData;
		return NOMEMCARD;
	default:
		u32 ret = ImportFile(tempDEntry, tempSaveData, 0);
		delete[] tempSaveData;
		return ret;
	}
}

u32 GCMemcard::ImportGci(const char *fileName, std::string fileName2)
{
	if (fileName2.empty() && !mcdFile) return OPENFAIL;

	FILE *gci = fopen(fileName, "rb");
	if (!gci) return OPENFAIL;

	int offset;
	char * tmp = new char[0xD];
	std::string fileType;
	SplitPath(fileName, NULL, NULL, &fileType);

	if( !strcasecmp(fileType.c_str(), ".gci"))
		offset = GCI;
	else
	{
		fread(tmp, 1, 0xD, gci);
		if (!strcasecmp(fileType.c_str(), ".gcs"))
		{
			if (!memcmp(tmp, "GCSAVE", 6))	// Header must be uppercase
				offset = GCS;
			else
			{
				return GCSFAIL;
			}
		}
		else{
			if (!strcasecmp(fileType.c_str(), ".sav"))
			{
				if (!memcmp(tmp, "DATELGC_SAVE", 0xC)) // Header must be uppercase
					offset = SAV;
				else
				{
					return SAVFAIL;
				}
			}
			else
			{
				return OPENFAIL;
			}
		}
	}
	delete []tmp;
	fseek(gci, offset, SEEK_SET);


	DEntry *tempDEntry = new DEntry;
	fread(tempDEntry, 1, DENTRY_SIZE, gci);
	int fStart = (int) ftell(gci);
	fseek(gci, 0, SEEK_END);
	int length = (int) ftell(gci) - fStart;
	fseek(gci, offset + DENTRY_SIZE, SEEK_SET);

	Gcs_SavConvert(tempDEntry, offset, length);

	if (length != BE16(tempDEntry->BlockCount) * BLOCK_SIZE)
	{
		return LENGTHFAIL;
	}
	if (ftell(gci)  != offset + DENTRY_SIZE) // Verify correct file position
	{
		return OPENFAIL;
	}
	u32 size = BE16((tempDEntry->BlockCount)) * BLOCK_SIZE;
	u8 *tempSaveData = new u8[size];
	fread(tempSaveData, 1, size, gci);
	fclose(gci);
	u32 ret;
	if(!fileName2.empty())
	{
		FILE * gci2 = fopen(fileName2.c_str(), "wb");
		bool completeWrite = true;
		if (!gci2) return OPENFAIL;
		fseek(gci2, 0, SEEK_SET);

		if (fwrite(tempDEntry, 1, DENTRY_SIZE, gci2) != DENTRY_SIZE) completeWrite = false;
		int fileBlocks = BE16(tempDEntry->BlockCount);
		fseek(gci2, DENTRY_SIZE, SEEK_SET);

		if (fwrite(tempSaveData, 1, BLOCK_SIZE * fileBlocks, gci2) != (unsigned) (BLOCK_SIZE * fileBlocks))
			completeWrite = false;
		fclose(gci2);
		if (completeWrite) ret = GCS;
		else ret = WRITEFAIL;
	}
	else ret= ImportFile(*tempDEntry, tempSaveData,0);

	delete []tempSaveData;
	delete tempDEntry;
	return ret;
}

u32 GCMemcard::ExportGci(u8 index, const char *fileName, std::string *fileName2)
{
	FILE *gci;
	int offset = GCI;
	if (!strcasecmp(fileName,"."))
	{
		if (BE32(dir.Dir[index].Gamecode) == 0xFFFFFFFF) return SUCCESS;

		size_t length = fileName2->length() + 42;
		char *filename = new char[length];
		char GameCode[5];

		DEntry_GameCode(index, GameCode);
		
		sprintf(filename, "%s/%s_%s.gci", fileName2->c_str(), GameCode, dir.Dir[index].Filename);
		gci = fopen((const char *)filename, "wb");
	}
	else
	{
		gci = fopen(fileName, "wb");

		std::string fileType;
		SplitPath(fileName, NULL, NULL, &fileType);
		if (!strcasecmp(fileType.c_str(), ".gcs"))
		{
			offset = GCS;
		}
		else if (!strcasecmp(fileType.c_str(), ".sav"))
		{
			offset = SAV;
		}
	}

	if (!gci) return OPENFAIL;
	bool completeWrite = true;

	fseek(gci, 0, SEEK_SET);
	
	switch(offset)
	{
	case GCS:
	{
		u8 gcsHDR[GCS];
		memset(gcsHDR, 0, GCS);
		memcpy(gcsHDR, "GCSAVE", 6);
		if (fwrite(gcsHDR, 1, GCS, gci) != GCS)	completeWrite = false;
		break;
	}
	case SAV:
	{
		u8 savHDR[SAV];
		memset(savHDR, 0, SAV);
		memcpy(savHDR, "DATELGC_SAVE", 0xC);
		if (fwrite(savHDR, 1, SAV, gci) != SAV)	completeWrite = false;
	}
		break;
	default:
		break;
	}

	DEntry tempDEntry;
	if (!DEntry_Copy(index, tempDEntry)) return NOMEMCARD;


	Gcs_SavConvert(&tempDEntry, offset);
	if (fwrite(&tempDEntry, 1, DENTRY_SIZE, gci) != DENTRY_SIZE) completeWrite = false;

	u32 size = DEntry_BlockCount(index);
	if (size == 0xFFFF) return FAIL;
	size *= BLOCK_SIZE;
	u8 *tempSaveData = new u8[size];

	switch(DEntry_GetSaveData(index, tempSaveData, true))
	{
	case FAIL:
		fclose(gci);
		delete []tempSaveData;
		return FAIL;
	case NOMEMCARD:
		fclose(gci);
		delete []tempSaveData;
		return NOMEMCARD;
	default:
		break;
	}
	fseek(gci, DENTRY_SIZE + offset, SEEK_SET);
	if (fwrite(tempSaveData, 1, size, gci) != size)
		completeWrite = false;
	fclose(gci);
	delete [] tempSaveData;
	if (completeWrite) return SUCCESS;
	else return WRITEFAIL;
	
}

void  GCMemcard::Gcs_SavConvert(DEntry* tempDEntry, int saveType, int length)
{
	switch(saveType)
	{
	case GCS:
	{	// field containing the Block count as displayed within
		// the GameSaves software is not stored in the GCS file.
		// It is stored only within the corresponding GSV file.
		// If the GCS file is added without using the GameSaves software,
		// the value stored is always "1"
		int blockCount = length / BLOCK_SIZE;
		tempDEntry->BlockCount[0] = u8(blockCount >> 8);
		tempDEntry->BlockCount[1] = u8(blockCount);
	}
		break;
	case SAV:
		// swap byte pairs
		// 0x2C and 0x2D, 0x2E and 0x2F, 0x30 and 0x31, 0x32 and 0x33,
		// 0x34 and 0x35, 0x36 and 0x37, 0x38 and 0x39, 0x3A and 0x3B,
		// 0x3C and 0x3D,0x3E and 0x3F.
		// It seems that sav files also swap the BIFlags...
		ByteSwap(&tempDEntry->Unused1, &tempDEntry->BIFlags);
		ArrayByteSwap((tempDEntry->ImageOffset));
		ArrayByteSwap(&(tempDEntry->ImageOffset[2]));
		ArrayByteSwap((tempDEntry->IconFmt));
		ArrayByteSwap((tempDEntry->AnimSpeed));
		ByteSwap(&tempDEntry->Permissions, &tempDEntry->CopyCounter);
		ArrayByteSwap((tempDEntry->FirstBlock));
		ArrayByteSwap((tempDEntry->BlockCount));
		ArrayByteSwap((tempDEntry->Unused2));
		ArrayByteSwap((tempDEntry->CommentsAddr));
		ArrayByteSwap(&(tempDEntry->CommentsAddr[2]));
		break;
	default:
		break;
	}
}

bool GCMemcard::ReadBannerRGBA8(u8 index, u32* buffer)
{
	if (!mcdFile) return false;

	int flags = dir.Dir[index].BIFlags;
	// Timesplitters 2 is the only game that I see this in
	// May be a hack
	if (flags == 0xFB) flags = ~flags;

	int  bnrFormat = (flags&3);

	if (bnrFormat == 0)
		return false;

	u32 DataOffset = BE32(dir.Dir[index].ImageOffset);
	u32 DataBlock = BE16(dir.Dir[index].FirstBlock) - MC_FST_BLOCKS;

	if ((DataBlock > maxBlock) || (DataOffset == 0xFFFFFFFF))
	{
		return false;
	}

	const int pixels = 96*32;

	if (bnrFormat&1)
	{
		u8  *pxdata  = (u8* )(mc_data + (DataBlock * BLOCK_SIZE) + DataOffset);
		u16 *paldata = (u16*)(mc_data + (DataBlock * BLOCK_SIZE) + DataOffset + pixels);

		decodeCI8image(buffer, pxdata, paldata, 96, 32);
	}
	else
	{
		u16 *pxdata = (u16*)(mc_data + (DataBlock * BLOCK_SIZE) + DataOffset);

		decode5A3image(buffer, pxdata, 96, 32);
	}
	return true;
}

u32 GCMemcard::ReadAnimRGBA8(u8 index, u32* buffer, u8 *delays)
{
	if (!mcdFile) return 0;

	// To ensure only one type of icon is used
	// Sonic Heroes it the only game I have seen that tries to use a CI8 and RGB5A3 icon
	int fmtCheck = 0; 

	int formats = BE16(dir.Dir[index].IconFmt);
	int fdelays  = BE16(dir.Dir[index].AnimSpeed);

	int flags = dir.Dir[index].BIFlags;
	// Timesplitters 2 is the only game that I see this in
	// May be a hack
	if (flags == 0xFB) flags = ~flags;
	int bnrFormat = (flags&3);

	u32 DataOffset = BE32(dir.Dir[index].ImageOffset);
	u32 DataBlock = BE16(dir.Dir[index].FirstBlock) - MC_FST_BLOCKS;

	if ((DataBlock > maxBlock) || (DataOffset == 0xFFFFFFFF))
	{
		return 0;
	}

	u8* animData = (u8*)(mc_data + (DataBlock * BLOCK_SIZE) + DataOffset);

	switch (bnrFormat)
	{
	case 1:
	case 3:
		animData += 96*32 + 2*256; // image+palette
		break;
	case 2:
		animData += 96*32*2;
		break;
	}

	int fmts[8];
	u8* data[8];
	int frames = 0;


	for (int i = 0; i < 8; i++)
	{
		fmts[i] = (formats >> (2*i))&3;
		delays[i] = ((fdelays >> (2*i))&3) << 2;
		data[i] = animData;

		if (!fmtCheck) fmtCheck = fmts[i];
		if (fmtCheck == fmts[i])
		{
			switch (fmts[i])
			{
			case CI8SHARED: // CI8 with shared palette
				animData += 32*32;
				frames++;
				break;
			case RGB5A3: // RGB5A3
				animData += 32*32*2;
				frames++;
				break;
			case CI8: // CI8 with own palette
				animData += 32*32 + 2*256;
				frames++;
				break;
			}
		}
	}

	u16* sharedPal = (u16*)(animData);

	for (int i = 0; i < 8; i++)
	{

		if (fmtCheck == fmts[i])
		{
			switch (fmts[i])
			{
			case CI8SHARED: // CI8 with shared palette
				decodeCI8image(buffer,data[i],sharedPal,32,32);
				buffer += 32*32;
				break;
			case RGB5A3: // RGB5A3
				decode5A3image(buffer, (u16*)(data[i]), 32, 32);
				break;
			case CI8: // CI8 with own palette
				u16 *paldata = (u16*)(data[i] + 32*32);
				decodeCI8image(buffer, data[i], paldata, 32, 32);
				buffer += 32*32;
				break;
			}
		}
	}

	return frames;
}

bool GCMemcard::Format(bool New, int slot, u16 SizeMb, bool sjis, bool hdrOnly)
{
	//Currently only formats cards for slot A
	u32 data_size = BLOCK_SIZE * (SizeMb * MBIT_TO_BLOCKS - MC_FST_BLOCKS);

	SRAM m_SRAM;
	FILE * pStream;
	u64 time, rand;

	if (New)
	{
		mc_data_size = data_size;
		mc_data = new u8[mc_data_size];
	}
	// Only Format 16MB memcards for now
	if ((SizeMb != MemCard2043Mb) || (data_size != mc_data_size)) return false;

	pStream = fopen(GC_SRAM_FILE, "rb");
	if (pStream == NULL)
	{
		PanicAlert("Could not open SRAM file");
		return false;
	}
	fread(&m_SRAM, 1, 64, pStream);
	fclose(pStream);

	time = CEXIIPL::GetGCTime();
	rand = Common::swap64(time);

	memset(&hdr, 0xFF, BLOCK_SIZE);
	for(int i = 0; i < 12; i++)
	{
		rand = (((rand * (u64)0x0000000041c64e6dULL) + (u64)0x0000000000003039ULL) >> 16);
		hdr.serial[i] = (u8)(m_SRAM.syssram.flash_id[slot][i] + (u32)rand);
		rand = (((rand * (u64)0x0000000041c64e6dULL) + (u64)0x0000000000003039ULL) >> 16);	
		rand &= (u64)0x0000000000007fffULL;
	}

	hdr.fmtTime.high = (time >> 32) & 0xFFFFFFFF;
	hdr.fmtTime.low = time & 0xFFFFFFFF;
	*(u32*)&(hdr.SramBias) = *(u32*)&(m_SRAM.syssram.counter_bias);
	*(u32*)&(hdr.SramLang) = m_SRAM.syssram.lang;
	*(u32*)&(hdr.Unk2) = Common::swap32(1);		// = _viReg[55];  static vu16* const _viReg = (u16*)0xCC002000;
	// TODO: find out why memcard cares if component cable used for now set to one like main app
	*(u16*)&(hdr.deviceID) = 0;
	*(u16*)&(hdr.SizeMb) = Common::swap16(SizeMb);
	*(u16*)&(hdr.Encoding) = Common::swap16(sjis ? 1 : 0);

	if (!hdrOnly)
	{
		memset(&dir, 0xFF, BLOCK_SIZE);
		memset(&dir_backup, 0xFF, BLOCK_SIZE);
		*(u16*)&dir.UpdateCounter = 0;
		*(u16*)&dir_backup.UpdateCounter = Common::swap16(1);
		memset(&bat, 0, BLOCK_SIZE);
		memset(&bat_backup, 0, BLOCK_SIZE);
		*(u16*)&bat.UpdateCounter = 0;
		*(u16*)&bat_backup.UpdateCounter = Common::swap16(1);
		*(u16*)&bat.FreeBlocks = *(u16*)&bat_backup.FreeBlocks = Common::swap16(SizeMb * MBIT_TO_BLOCKS - MC_FST_BLOCKS);
		*(u16*)&bat.LastAllocated = *(u16*)&bat_backup.LastAllocated = Common::swap16(4);
		memset(mc_data, 0xFF, mc_data_size);
	}
	
	FixChecksums();
	Save();
	return true;
}

