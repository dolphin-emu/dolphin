// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Common/Common.h"
#include "Common/CommonPaths.h"
#include "Common/StringUtil.h"

#include "Core/HW/EXI_DeviceIPL.h"
#include "Core/HW/Sram.h"

#define BE64(x) (Common::swap64(x))
#define BE32(x) (Common::swap32(x))
#define BE16(x) (Common::swap16(x))
#define ArrayByteSwap(a) (ByteSwap(a, a+sizeof(u8)));
enum
{
	SLOT_A = 0,
	SLOT_B = 1,
	GCI = 0,
	SUCCESS,
	NOMEMCARD,
	OPENFAIL,
	OUTOFBLOCKS,
	OUTOFDIRENTRIES,
	LENGTHFAIL,
	INVALIDFILESIZE,
	TITLEPRESENT,
	DIRLEN = 0x7F,
	SAV = 0x80,
	SAVFAIL,
	GCS = 0x110,
	GCSFAIL,
	FAIL,
	WRITEFAIL,
	DELETE_FAIL,

	MC_FST_BLOCKS  = 0x05,
	MBIT_TO_BLOCKS = 0x10,
	DENTRY_STRLEN  = 0x20,
	DENTRY_SIZE    = 0x40,
	BLOCK_SIZE     = 0x2000,
	BAT_SIZE       = 0xFFB,

	MemCard59Mb   = 0x04,
	MemCard123Mb  = 0x08,
	MemCard251Mb  = 0x10,
	Memcard507Mb  = 0x20,
	MemCard1019Mb = 0x40,
	MemCard2043Mb = 0x80,

	CI8SHARED = 1,
	RGB5A3,
	CI8,
};

class GCMemcard : NonCopyable
{
private:
	friend class CMemcardManagerDebug;
	bool m_valid;
	std::string m_fileName;

	u32 maxBlock;
	u16 m_sizeMb;
	struct GCMBlock
	{
		GCMBlock(){erase();}
		void erase() {memset(block, 0xFF, BLOCK_SIZE);}
		u8 block[BLOCK_SIZE];
	};
	std::vector<GCMBlock> mc_data_blocks;
#pragma pack(push,1)
	struct Header {         //Offset    Size    Description
		 // Serial in libogc
		u8 serial[12];      //0x0000    12      ?
		u64 formatTime;     //0x000c    8       Time of format (OSTime value)
		u32 SramBias;       //0x0014    4       SRAM bias at time of format
		u32 SramLang;       //0x0018    4       SRAM language
		u8 Unk2[4];         //0x001c    4       ? almost always 0
		// end Serial in libogc
		u8 deviceID[2];     //0x0020    2       0 if formated in slot A 1 if formated in slot B
		u8 SizeMb[2];       //0x0022    2       Size of memcard in Mbits
		u16 Encoding;       //0x0024    2       Encoding (ASCII or japanese)
		u8 Unused1[468];    //0x0026    468     Unused (0xff)
		u16 UpdateCounter;  //0x01fa    2       Update Counter (?, probably unused)
		u16 Checksum;       //0x01fc    2       Additive Checksum
		u16 Checksum_Inv;   //0x01fe    2       Inverse Checksum
		u8 Unused2[7680];   //0x0200    0x1e00  Unused (0xff)
	} hdr;

	struct DEntry
	{
		u8 Gamecode[4];     //0x00       0x04    Gamecode
		u8 Makercode[2];    //0x04      0x02    Makercode
		u8 Unused1;         //0x06      0x01    reserved/unused (always 0xff, has no effect)
		u8 BIFlags;         //0x07      0x01    banner gfx format and icon animation (Image Key)
		                    //      Bit(s)  Description
		                    //      2       Icon Animation 0: forward 1: ping-pong
		                    //      1       [--0: No Banner 1: Banner present--] WRONG! YAGCD LIES!
		                    //      0       [--Banner Color 0: RGB5A3 1: CI8--]  WRONG! YAGCD LIES!
		                    //      bits 0 and 1: image format
		                    //      00 no banner
		                    //      01 CI8 banner
		                    //      10 RGB5A3 banner
		                    //      11 ? maybe ==00? Time Splitters 2 and 3 have it and don't have banner
		                    //
		u8 Filename[DENTRY_STRLEN]; //0x08      0x20     Filename
		u8 ModTime[4];      //0x28      0x04    Time of file's last modification in seconds since 12am, January 1st, 2000
		u8 ImageOffset[4];  //0x2c      0x04    image data offset
		u8 IconFmt[2];      //0x30      0x02    icon gfx format (2bits per icon)
		                    //      Bits    Description
		                    //      00      No icon
		                    //      01      CI8 with a shared color palette after the last frame
		                    //      10      RGB5A3
		                    //      11      CI8 with a unique color palette after itself
		                    //
		u8 AnimSpeed[2];    //0x32      0x02    Animation speed (2bits per icon) (*1)
		                    //      Bits    Description
		                    //      00      No icon
		                    //      01      Icon lasts for 4 frames
		                    //      10      Icon lasts for 8 frames
		                    //      11      Icon lasts for 12 frames
		                    //
		u8 Permissions;     //0x34      0x01    File-permissions
		                    //      Bit Permission  Description
		                    //      4   no move     File cannot be moved by the IPL
		                    //      3   no copy     File cannot be copied by the IPL
		                    //      2   public      Can be read by any game
		                    //
		u8 CopyCounter;     //0x35      0x01    Copy counter (*2)
		u8 FirstBlock[2];   //0x36      0x02    Block no of first block of file (0 == offset 0)
		u8 BlockCount[2];   //0x38      0x02    File-length (number of blocks in file)
		u8 Unused2[2];      //0x3a      0x02    Reserved/unused (always 0xffff, has no effect)
		u8 CommentsAddr[4]; //0x3c      0x04    Address of the two comments within the file data (*3)
	};

	struct Directory
	{
		DEntry Dir[DIRLEN]; //0x0000            Directory Entries (max 127)
		u8 Padding[0x3a];
		u16 UpdateCounter;  //0x1ffa    2       Update Counter
		u16 Checksum;       //0x1ffc    2       Additive Checksum
		u16 Checksum_Inv;   //0x1ffe    2       Inverse Checksum
	} dir, dir_backup;

	Directory *CurrentDir, *PreviousDir;
	struct BlockAlloc
	{
		u16 Checksum;       //0x0000    2       Additive Checksum
		u16 Checksum_Inv;   //0x0002    2       Inverse Checksum
		u16 UpdateCounter;  //0x0004    2       Update Counter
		u16 FreeBlocks;     //0x0006    2       Free Blocks
		u16 LastAllocated;  //0x0008    2       Last allocated Block
		u16 Map[BAT_SIZE];  //0x000a    0x1ff8  Map of allocated Blocks
		u16 GetNextBlock(u16 Block) const;
		u16 NextFreeBlock(u16 MaxBlock, u16 StartingBlock = MC_FST_BLOCKS) const;
		bool ClearBlocks(u16 StartingBlock, u16 Length);
	} bat,bat_backup;

	BlockAlloc *CurrentBat, *PreviousBat;
	struct GCMC_Header
	{
		Header *hdr;
		Directory *dir, *dir_backup;
		BlockAlloc *bat, *bat_backup;
	};
#pragma pack(pop)

	u32 ImportGciInternal(FILE* gcih, const std::string& inputFile, const std::string &outputFile);
	static void FormatInternal(GCMC_Header &GCP);
	void initDirBatPointers() ;
public:

	GCMemcard(const std::string& fileName, bool forceCreation=false, bool sjis=false);
	bool IsValid() const { return m_valid; }
	bool IsAsciiEncoding() const;
	bool Save();
	bool Format(bool sjis = false, u16 SizeMb = MemCard2043Mb);
	static bool Format(u8 * card_data, bool sjis = false, u16 SizeMb = MemCard2043Mb);

	static void calc_checksumsBE(u16 *buf, u32 length, u16 *csum, u16 *inv_csum);
	u32 TestChecksums() const;
	bool FixChecksums();

	// get number of file entries in the directory
	u8 GetNumFiles() const;
	u8 GetFileIndex(u8 fileNumber) const;

	// get the free blocks from bat
	u16 GetFreeBlocks() const;

	// If title already on memcard returns index, otherwise returns -1
	u8 TitlePresent(DEntry d) const;

	bool GCI_FileName(u8 index, std::string &filename) const;
	// DEntry functions, all take u8 index < DIRLEN (127)
	std::string DEntry_GameCode(u8 index) const;
	std::string DEntry_Makercode(u8 index) const;
	std::string DEntry_BIFlags(u8 index) const;
	std::string DEntry_FileName(u8 index) const;
	u32 DEntry_ModTime(u8 index) const;
	u32 DEntry_ImageOffset(u8 index) const;
	std::string DEntry_IconFmt(u8 index) const;
	std::string DEntry_AnimSpeed(u8 index) const;
	std::string DEntry_Permissions(u8 index) const;
	u8 DEntry_CopyCounter(u8 index) const;
	// get first block for file
	u16 DEntry_FirstBlock(u8 index) const;
	// get file length in blocks
	u16 DEntry_BlockCount(u8 index) const;
	u32 DEntry_CommentsAddress(u8 index) const;
	std::string GetSaveComment1(u8 index) const;
	std::string GetSaveComment2(u8 index) const;
	// Copies a DEntry from u8 index to DEntry& data
	bool GetDEntry(u8 index, DEntry &dest) const;

	// assumes there's enough space in buffer
	// old determines if function uses old or new method of copying data
	// some functions only work with old way, some only work with new way
	// TODO: find a function that works for all calls or split into 2 functions
	u32 GetSaveData(u8 index, std::vector<GCMBlock> &saveBlocks) const;

	// adds the file to the directory and copies its contents
	u32 ImportFile(DEntry& direntry, std::vector<GCMBlock> &saveBlocks);

	// delete a file from the directory
	u32 RemoveFile(u8 index);

	// reads a save from another memcard, and imports the data into this memcard
	u32 CopyFrom(const GCMemcard& source, u8 index);

	// reads a .gci/.gcs/.sav file and calls ImportFile or saves out a gci file
	u32 ImportGci(const std::string& inputFile,const std::string &outputFile);

	// writes a .gci file to disk containing index
	u32 ExportGci(u8 index, const std::string& fileName, const std::string &directory) const;

	// GCI files are untouched, SAV files are byteswapped
	// GCS files have the block count set, default is 1 (For export as GCS)
	static void Gcs_SavConvert(DEntry &tempDEntry, int saveType, int length = BLOCK_SIZE);

	// reads the banner image
	bool ReadBannerRGBA8(u8 index, u32* buffer) const;

	// reads the animation frames
	u32 ReadAnimRGBA8(u8 index, u32* buffer, u8 *delays) const;

	void CARD_GetSerialNo(u32 *serial1,u32 *serial2);
	s32 FZEROGX_MakeSaveGameValid(DEntry& direntry, std::vector<GCMBlock> &FileBuffer);
	s32 PSO_MakeSaveGameValid(DEntry& direntry, std::vector<GCMBlock> &FileBuffer);
};
