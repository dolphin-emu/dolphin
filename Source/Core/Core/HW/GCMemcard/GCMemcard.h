// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <array>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/NandPaths.h"
#include "Common/Swap.h"
#include "Common/Timer.h"

#include "Core/HW/EXI/EXI_DeviceIPL.h"
#include "Core/HW/Sram.h"

namespace File
{
class IOFile;
}

#define BE64(x) (Common::swap64(x))
#define BE32(x) (Common::swap32(x))
#define BE16(x) (Common::swap16(x))
#define ArrayByteSwap(a) (ByteSwap(a, a + sizeof(u8)));

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

  MC_FST_BLOCKS = 0x05,
  MBIT_TO_BLOCKS = 0x10,
  DENTRY_STRLEN = 0x20,
  DENTRY_SIZE = 0x40,
  BLOCK_SIZE = 0x2000,
  BAT_SIZE = 0xFFB,

  MemCard59Mb = 0x04,
  MemCard123Mb = 0x08,
  MemCard251Mb = 0x10,
  Memcard507Mb = 0x20,
  MemCard1019Mb = 0x40,
  MemCard2043Mb = 0x80,

  CI8SHARED = 1,
  RGB5A3,
  CI8,
};

class MemoryCardBase
{
public:
  explicit MemoryCardBase(int card_index = 0, int size_mbits = MemCard2043Mb)
      : m_card_index(card_index), m_nintendo_card_id(size_mbits)
  {
  }
  virtual ~MemoryCardBase() {}
  virtual s32 Read(u32 src_address, s32 length, u8* dest_address) = 0;
  virtual s32 Write(u32 dest_address, s32 length, const u8* src_address) = 0;
  virtual void ClearBlock(u32 address) = 0;
  virtual void ClearAll() = 0;
  virtual void DoState(PointerWrap& p) = 0;
  u32 GetCardId() const { return m_nintendo_card_id; }
  bool IsAddressInBounds(u32 address) const { return address <= (m_memory_card_size - 1); }

protected:
  int m_card_index;
  u16 m_nintendo_card_id;
  u32 m_memory_card_size;
};

struct GCMBlock
{
  GCMBlock() { Erase(); }
  void Erase() { memset(block, 0xFF, BLOCK_SIZE); }
  u8 block[BLOCK_SIZE];
};

void calc_checksumsBE(const u16* buf, u32 length, u16* csum, u16* inv_csum);

#pragma pack(push, 1)
struct Header  // Offset    Size    Description
{
  // Serial in libogc
  std::array<u8, 12> m_serial;                  // 0x0000    12      ?
  Common::BigEndianValue<u64> m_format_time;    // 0x000c    8       Time of format (OSTime value)
  u32 m_sram_bias;                              // 0x0014    4       SRAM bias at time of format
  Common::BigEndianValue<u32> m_sram_language;  // 0x0018    4       SRAM language
  std::array<u8, 4> m_unknown_2;                // 0x001c    4       ? almost always 0
  // end Serial in libogc
  Common::BigEndianValue<u16>
      m_device_id;  // 0x0020    2       0 if formated in slot A 1 if formated in slot B
  Common::BigEndianValue<u16> m_size_mb;   // 0x0022    2       Size of memcard in Mbits
  Common::BigEndianValue<u16> m_encoding;  // 0x0024    2       Encoding (Windows-1252 or Shift JIS)
  std::array<u8, 468> m_unused_1;          // 0x0026    468     Unused (0xff)
  u16 m_update_counter;                    // 0x01fa    2       Update Counter (?, probably unused)
  u16 m_checksum;                          // 0x01fc    2       Additive Checksum
  u16 m_checksum_inv;                      // 0x01fe    2       Inverse Checksum
  std::array<u8, 7680> m_unused_2;         // 0x0200    0x1e00  Unused (0xff)

  void CARD_GetSerialNo(u32* serial1, u32* serial2) const
  {
    u32 serial[8];

    for (int i = 0; i < 8; i++)
    {
      memcpy(&serial[i], (u8*)this + (i * 4), 4);
    }

    *serial1 = serial[0] ^ serial[2] ^ serial[4] ^ serial[6];
    *serial2 = serial[1] ^ serial[3] ^ serial[5] ^ serial[7];
  }

  // Nintendo format algorithm.
  // Constants are fixed by the GC SDK
  // Changing the constants will break memory card support
  explicit Header(int slot = 0, u16 sizeMb = MemCard2043Mb, bool shift_jis = false)
  {
    memset(this, 0xFF, BLOCK_SIZE);
    m_size_mb = sizeMb;
    m_encoding = shift_jis ? 1 : 0;
    u64 rand = Common::Timer::GetLocalTimeSinceJan1970() - ExpansionInterface::CEXIIPL::GC_EPOCH;
    m_format_time = rand;
    for (int i = 0; i < 12; i++)
    {
      rand = (((rand * (u64)0x0000000041c64e6dULL) + (u64)0x0000000000003039ULL) >> 16);
      m_serial[i] = (u8)(g_SRAM.settings_ex.flash_id[slot][i] + (u32)rand);
      rand = (((rand * (u64)0x0000000041c64e6dULL) + (u64)0x0000000000003039ULL) >> 16);
      rand &= (u64)0x0000000000007fffULL;
    }
    m_sram_bias = g_SRAM.settings.rtc_bias;
    m_sram_language = static_cast<u32>(g_SRAM.settings.language);
    // TODO: determine the purpose of m_unknown_2
    // 1 works for slot A, 0 works for both slot A and slot B
    memset(m_unknown_2.data(), 0,
           m_unknown_2.size());  // = _viReg[55];  static vu16* const _viReg = (u16*)0xCC002000;
    m_device_id = 0;
    calc_checksumsBE((u16*)this, 0xFE, &m_checksum, &m_checksum_inv);
  }
};
static_assert(sizeof(Header) == BLOCK_SIZE);

struct DEntry
{
  static const u8 DENTRY_SIZE = 0x40;
  DEntry() { memset(this, 0xFF, DENTRY_SIZE); }
  std::string GCI_FileName() const
  {
    std::string filename = std::string((char*)m_makercode, 2) + '-' +
                           std::string((char*)m_gamecode, 4) + '-' + (char*)m_filename + ".gci";
    return Common::EscapeFileName(filename);
  }

  u8 m_gamecode[4];            // 0x00      0x04    Gamecode
  u8 m_makercode[2];           // 0x04      0x02    Makercode
  u8 m_unused_1;               // 0x06      0x01    reserved/unused (always 0xff, has no effect)
  u8 m_banner_and_icon_flags;  // 0x07      0x01    banner gfx format and icon animation (Image Key)
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
  u8 m_filename[DENTRY_STRLEN];  // 0x08      0x20     Filename
  Common::BigEndianValue<u32>
      m_modification_time;  // 0x28      0x04    Time of file's last modification in seconds since
                            // 12am, January 1st, 2000
  Common::BigEndianValue<u32> m_image_offset;  // 0x2c      0x04    image data offset
  Common::BigEndianValue<u16> m_icon_format;   // 0x30      0x02    icon gfx format (2bits per icon)
  //      Bits    Description
  //      00      No icon
  //      01      CI8 with a shared color palette after the last frame
  //      10      RGB5A3
  //      11      CI8 with a unique color palette after itself
  //
  Common::BigEndianValue<u16>
      m_animation_speed;  // 0x32      0x02    Animation speed (2bits per icon) (*1)
  //      Bits    Description
  //      00      No icon
  //      01      Icon lasts for 4 frames
  //      10      Icon lasts for 8 frames
  //      11      Icon lasts for 12 frames
  //
  u8 m_file_permissions;  // 0x34      0x01    File-permissions
  //      Bit Permission  Description
  //      4   no move     File cannot be moved by the IPL
  //      3   no copy     File cannot be copied by the IPL
  //      2   public      Can be read by any game
  //
  u8 m_copy_counter;         // 0x35      0x01    Copy counter (*2)
  u8 m_first_block[2];       // 0x36      0x02    Block no of first block of file (0 == offset 0)
  u8 m_block_count[2];       // 0x38      0x02    File-length (number of blocks in file)
  u8 m_unused_2[2];          // 0x3a      0x02    Reserved/unused (always 0xffff, has no effect)
  u8 m_comments_address[4];  // 0x3c      0x04    Address of the two comments within the file data
                             // (*3)
};
static_assert(sizeof(DEntry) == DENTRY_SIZE);

struct Directory
{
  DEntry m_dir_entries[DIRLEN];  // 0x0000            Directory Entries (max 127)
  u8 m_padding[0x3a];
  u16 m_update_counter;  // 0x1ffa    2       Update Counter
  u16 m_checksum;        // 0x1ffc    2       Additive Checksum
  u16 m_checksum_inv;    // 0x1ffe    2       Inverse Checksum
  Directory()
  {
    memset(this, 0xFF, BLOCK_SIZE);
    m_update_counter = 0;
    m_checksum = BE16(0xF003);
    m_checksum_inv = 0;
  }
  void Replace(DEntry d, int idx)
  {
    m_dir_entries[idx] = d;
    fixChecksums();
  }
  void fixChecksums() { calc_checksumsBE((u16*)this, 0xFFE, &m_checksum, &m_checksum_inv); }
};
static_assert(sizeof(Directory) == BLOCK_SIZE);

struct BlockAlloc
{
  u16 m_checksum;              // 0x0000    2       Additive Checksum
  u16 m_checksum_inv;          // 0x0002    2       Inverse Checksum
  u16 m_update_counter;        // 0x0004    2       Update Counter
  u16 m_free_blocks;           // 0x0006    2       Free Blocks
  u16 m_last_allocated_block;  // 0x0008    2       Last allocated Block
  u16 m_map[BAT_SIZE];         // 0x000a    0x1ff8  Map of allocated Blocks
  u16 GetNextBlock(u16 Block) const;
  u16 NextFreeBlock(u16 MaxBlock, u16 StartingBlock = MC_FST_BLOCKS) const;
  bool ClearBlocks(u16 StartingBlock, u16 Length);
  void fixChecksums()
  {
    calc_checksumsBE((u16*)&m_update_counter, 0xFFE, &m_checksum, &m_checksum_inv);
  }
  explicit BlockAlloc(u16 sizeMb = MemCard2043Mb)
  {
    memset(this, 0, BLOCK_SIZE);
    m_free_blocks = BE16((sizeMb * MBIT_TO_BLOCKS) - MC_FST_BLOCKS);
    m_last_allocated_block = BE16(4);
    fixChecksums();
  }
  u16 AssignBlocksContiguous(u16 length)
  {
    u16 starting = BE16(m_last_allocated_block) + 1;
    if (length > BE16(m_free_blocks))
      return 0xFFFF;
    u16 current = starting;
    while ((current - starting + 1) < length)
    {
      m_map[current - 5] = BE16(current + 1);
      current++;
    }
    m_map[current - 5] = 0xFFFF;
    m_last_allocated_block = BE16(current);
    m_free_blocks = BE16(BE16(m_free_blocks) - length);
    fixChecksums();
    return BE16(starting);
  }
};
static_assert(sizeof(BlockAlloc) == BLOCK_SIZE);
#pragma pack(pop)

class GCIFile
{
public:
  bool LoadSaveBlocks();
  bool HasCopyProtection() const
  {
    if ((strcmp((char*)m_gci_header.m_filename, "PSO_SYSTEM") == 0) ||
        (strcmp((char*)m_gci_header.m_filename, "PSO3_SYSTEM") == 0) ||
        (strcmp((char*)m_gci_header.m_filename, "f_zero.dat") == 0))
      return true;
    return false;
  }

  void DoState(PointerWrap& p);
  DEntry m_gci_header;
  std::vector<GCMBlock> m_save_data;
  std::vector<u16> m_used_blocks;
  int UsesBlock(u16 blocknum);
  bool m_dirty;
  std::string m_filename;
};

class GCMemcard
{
private:
  bool m_valid;
  std::string m_fileName;

  u32 maxBlock;
  u16 m_sizeMb;

  Header hdr;
  Directory dir, dir_backup, *CurrentDir, *PreviousDir;
  BlockAlloc bat, bat_backup, *CurrentBat, *PreviousBat;

  std::vector<GCMBlock> mc_data_blocks;

  u32 ImportGciInternal(File::IOFile&& gci, const std::string& inputFile,
                        const std::string& outputFile);
  void InitDirBatPointers();

public:
  explicit GCMemcard(const std::string& fileName, bool forceCreation = false,
                     bool shift_jis = false);

  GCMemcard(const GCMemcard&) = delete;
  GCMemcard& operator=(const GCMemcard&) = delete;
  GCMemcard(GCMemcard&&) = default;
  GCMemcard& operator=(GCMemcard&&) = default;

  bool IsValid() const { return m_valid; }
  bool IsShiftJIS() const;
  bool Save();
  bool Format(bool shift_jis = false, u16 SizeMb = MemCard2043Mb);
  static bool Format(u8* card_data, bool shift_jis = false, u16 SizeMb = MemCard2043Mb);
  static s32 FZEROGX_MakeSaveGameValid(const Header& cardheader, const DEntry& direntry,
                                       std::vector<GCMBlock>& FileBuffer);
  static s32 PSO_MakeSaveGameValid(const Header& cardheader, const DEntry& direntry,
                                   std::vector<GCMBlock>& FileBuffer);

  u32 TestChecksums() const;
  bool FixChecksums();

  // get number of file entries in the directory
  u8 GetNumFiles() const;
  u8 GetFileIndex(u8 fileNumber) const;

  // get the free blocks from bat
  u16 GetFreeBlocks() const;

  // If title already on memcard returns index, otherwise returns -1
  u8 TitlePresent(const DEntry& d) const;

  bool GCI_FileName(u8 index, std::string& filename) const;
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
  bool GetDEntry(u8 index, DEntry& dest) const;

  u32 GetSaveData(u8 index, std::vector<GCMBlock>& saveBlocks) const;

  // adds the file to the directory and copies its contents
  u32 ImportFile(const DEntry& direntry, std::vector<GCMBlock>& saveBlocks);

  // delete a file from the directory
  u32 RemoveFile(u8 index);

  // reads a save from another memcard, and imports the data into this memcard
  u32 CopyFrom(const GCMemcard& source, u8 index);

  // reads a .gci/.gcs/.sav file and calls ImportFile or saves out a gci file
  u32 ImportGci(const std::string& inputFile, const std::string& outputFile);

  // writes a .gci file to disk containing index
  u32 ExportGci(u8 index, const std::string& fileName, const std::string& directory) const;

  // GCI files are untouched, SAV files are byteswapped
  // GCS files have the block count set, default is 1 (For export as GCS)
  static void Gcs_SavConvert(DEntry& tempDEntry, int saveType, int length = BLOCK_SIZE);

  // reads the banner image
  bool ReadBannerRGBA8(u8 index, u32* buffer) const;

  // reads the animation frames
  u32 ReadAnimRGBA8(u8 index, u32* buffer, u8* delays) const;
};
