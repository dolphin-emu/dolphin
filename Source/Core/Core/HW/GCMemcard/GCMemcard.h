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
  SAV = 0x80,
  GCS = 0x110,

  CI8SHARED = 1,
  RGB5A3,
  CI8,
};

enum class GCMemcardGetSaveDataRetVal
{
  SUCCESS,
  FAIL,
  NOMEMCARD,
};

enum class GCMemcardImportFileRetVal
{
  SUCCESS,
  FAIL,
  NOMEMCARD,
  OUTOFDIRENTRIES,
  OUTOFBLOCKS,
  TITLEPRESENT,
  INVALIDFILESIZE,
  GCSFAIL,
  SAVFAIL,
  OPENFAIL,
  LENGTHFAIL,
};

enum class GCMemcardExportFileRetVal
{
  SUCCESS,
  FAIL,
  NOMEMCARD,
  OPENFAIL,
  WRITEFAIL,
  UNUSED,
};

enum class GCMemcardRemoveFileRetVal
{
  SUCCESS,
  NOMEMCARD,
  DELETE_FAIL,
};

// size of a single memory card block in bytes
constexpr u32 BLOCK_SIZE = 0x2000;

// the amount of memory card blocks in a megabit of data
constexpr u32 MBIT_TO_BLOCKS = (1024 * 1024) / (BLOCK_SIZE * 8);

// number of metadata and filesystem blocks before the actual user data blocks
constexpr u32 MC_FST_BLOCKS = 0x05;

// maximum number of saves that can be stored on a single memory card
constexpr u8 DIRLEN = 0x7F;

// maximum size of memory card file comment in bytes
constexpr u32 DENTRY_STRLEN = 0x20;

// size of a single entry in the Directory in bytes
constexpr u32 DENTRY_SIZE = 0x40;

// number of block entries in the BAT; one entry uses 2 bytes
constexpr u16 BAT_SIZE = 0xFFB;

// possible sizes of memory cards in megabits
// TODO: Do memory card sizes have to be power of two?
// TODO: Are these all of them? A 4091 block card should work in theory at least.
constexpr u16 MemCard59Mb = 0x04;
constexpr u16 MemCard123Mb = 0x08;
constexpr u16 MemCard251Mb = 0x10;
constexpr u16 Memcard507Mb = 0x20;
constexpr u16 MemCard1019Mb = 0x40;
constexpr u16 MemCard2043Mb = 0x80;

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
  GCMBlock();
  void Erase();
  std::array<u8, BLOCK_SIZE> m_block;
};

#pragma pack(push, 1)
struct Header
{
  // NOTE: libogc refers to 'Serial' as the first 0x20 bytes of the header,
  // so the data from m_serial until m_unknown_2 (inclusive)

  // 12 bytes at 0x0000
  std::array<u8, 12> m_serial;

  // 8 bytes at 0x000c: Time of format (OSTime value)
  Common::BigEndianValue<u64> m_format_time;

  // 4 bytes at 0x0014; SRAM bias at time of format
  u32 m_sram_bias;

  // 4 bytes at 0x0018: SRAM language
  Common::BigEndianValue<u32> m_sram_language;

  // 4 bytes at 0x001c: ? almost always 0
  std::array<u8, 4> m_unknown_2;

  // 2 bytes at 0x0020: 0 if formated in slot A, 1 if formated in slot B
  Common::BigEndianValue<u16> m_device_id;

  // 2 bytes at 0x0022: Size of memcard in Mbits
  Common::BigEndianValue<u16> m_size_mb;

  // 2 bytes at 0x0024: Encoding (Windows-1252 or Shift JIS)
  Common::BigEndianValue<u16> m_encoding;

  // 468 bytes at 0x0026: Unused (0xff)
  std::array<u8, 468> m_unused_1;

  // 2 bytes at 0x01fa: Update Counter (?, probably unused)
  // TODO: This seems to be 0xFFFF in all my memory cards, might still be part of m_unused_1.
  u16 m_update_counter;

  // 2 bytes at 0x01fc: Additive Checksum
  u16 m_checksum;

  // 2 bytes at 0x01fe: Inverse Checksum
  u16 m_checksum_inv;

  // 0x1e00 bytes at 0x0200: Unused (0xff)
  std::array<u8, 7680> m_unused_2;

  explicit Header(int slot = 0, u16 size_mbits = MemCard2043Mb, bool shift_jis = false);

  // Calculates the card serial numbers used for encrypting some save files.
  std::pair<u32, u32> CalculateSerial() const;

  void FixChecksums();
  std::pair<u16, u16> CalculateChecksums() const;
};
static_assert(sizeof(Header) == BLOCK_SIZE);

struct DEntry
{
  DEntry();

  // TODO: This probably shouldn't be here at all?
  std::string GCI_FileName() const;

  static constexpr std::array<u8, 4> UNINITIALIZED_GAMECODE{{0xFF, 0xFF, 0xFF, 0xFF}};

  // 4 bytes at 0x00: Gamecode
  std::array<u8, 4> m_gamecode;

  // 2 bytes at 0x04: Makercode
  std::array<u8, 2> m_makercode;

  // 1 byte at 0x06: reserved/unused (always 0xff, has no effect)
  u8 m_unused_1;

  // 1 byte at 0x07: banner gfx format and icon animation (Image Key)
  //      Bit(s)  Description
  //      2       Icon Animation 0: forward 1: ping-pong
  //      1       [--0: No Banner 1: Banner present--] WRONG! YAGCD LIES!
  //      0       [--Banner Color 0: RGB5A3 1: CI8--]  WRONG! YAGCD LIES!
  //      bits 0 and 1: image format
  //      00 no banner
  //      01 CI8 banner
  //      10 RGB5A3 banner
  //      11 ? maybe ==00? Time Splitters 2 and 3 have it and don't have banner
  u8 m_banner_and_icon_flags;

  // 0x20 bytes at 0x08: Filename
  std::array<u8, DENTRY_STRLEN> m_filename;

  // 4 bytes at 0x28: Time of file's last modification in seconds since 12am, January 1st, 2000
  Common::BigEndianValue<u32> m_modification_time;

  // 4 bytes at 0x2c: image data offset
  Common::BigEndianValue<u32> m_image_offset;

  // 2 bytes at 0x30: icon gfx format (2bits per icon)
  //      Bits    Description
  //      00      No icon
  //      01      CI8 with a shared color palette after the last frame
  //      10      RGB5A3
  //      11      CI8 with a unique color palette after itself
  Common::BigEndianValue<u16> m_icon_format;

  // 2 bytes at 0x32: Animation speed (2bits per icon)
  //      Bits    Description
  //      00      No icon
  //      01      Icon lasts for 4 frames
  //      10      Icon lasts for 8 frames
  //      11      Icon lasts for 12 frames
  Common::BigEndianValue<u16> m_animation_speed;

  // 1 byte at 0x34: File-permissions
  //      Bit Permission  Description
  //      4   no move     File cannot be moved by the IPL
  //      3   no copy     File cannot be copied by the IPL
  //      2   public      Can be read by any game
  u8 m_file_permissions;

  // 1 byte at 0x35: Copy counter
  u8 m_copy_counter;

  // 2 bytes at 0x36: Block number of first block of file (0 == offset 0)
  Common::BigEndianValue<u16> m_first_block;

  // 2 bytes at 0x38: File-length (number of blocks in file)
  Common::BigEndianValue<u16> m_block_count;

  // 2 bytes at 0x3a: Reserved/unused (always 0xffff, has no effect)
  std::array<u8, 2> m_unused_2;

  // 4 bytes at 0x3c: Address of the two comments within the file data
  Common::BigEndianValue<u32> m_comments_address;
};
static_assert(sizeof(DEntry) == DENTRY_SIZE);

struct Directory
{
  // 127 files of 0x40 bytes each
  std::array<DEntry, DIRLEN> m_dir_entries;

  // 0x3a bytes at 0x1fc0: Unused, always 0xFF
  std::array<u8, 0x3a> m_padding;

  // 2 bytes at 0x1ffa: Update Counter
  // TODO: What happens if this overflows? Is there a special case for preferring 0 over max value?
  Common::BigEndianValue<u16> m_update_counter;

  // 2 bytes at 0x1ffc: Additive Checksum
  u16 m_checksum;

  // 2 bytes at 0x1ffe: Inverse Checksum
  u16 m_checksum_inv;

  // Constructs an empty Directory block.
  Directory();

  // Replaces the file metadata at the given index (range 0-126)
  // with the given DEntry data.
  bool Replace(const DEntry& entry, size_t index);

  void FixChecksums();
  std::pair<u16, u16> CalculateChecksums() const;
};
static_assert(sizeof(Directory) == BLOCK_SIZE);

struct BlockAlloc
{
  // 2 bytes at 0x0000: Additive Checksum
  u16 m_checksum;

  // 2 bytes at 0x0002: Inverse Checksum
  u16 m_checksum_inv;

  // 2 bytes at 0x0004: Update Counter
  Common::BigEndianValue<u16> m_update_counter;

  // 2 bytes at 0x0006: Free Blocks
  Common::BigEndianValue<u16> m_free_blocks;

  // 2 bytes at 0x0008: Last allocated Block
  Common::BigEndianValue<u16> m_last_allocated_block;

  // 0x1ff8 bytes at 0x000a: Map of allocated Blocks
  std::array<Common::BigEndianValue<u16>, BAT_SIZE> m_map;

  explicit BlockAlloc(u16 size_mbits = MemCard2043Mb);

  u16 GetNextBlock(u16 block) const;
  u16 NextFreeBlock(u16 max_block, u16 starting_block = MC_FST_BLOCKS) const;
  bool ClearBlocks(u16 starting_block, u16 block_count);
  u16 AssignBlocksContiguous(u16 length);

  void FixChecksums();
  std::pair<u16, u16> CalculateChecksums() const;
};
static_assert(sizeof(BlockAlloc) == BLOCK_SIZE);
#pragma pack(pop)

class GCMemcard
{
private:
  bool m_valid;
  std::string m_filename;

  u32 m_size_blocks;
  u16 m_size_mb;

  Header m_header_block;
  std::array<Directory, 2> m_directory_blocks;
  std::array<BlockAlloc, 2> m_bat_blocks;
  std::vector<GCMBlock> m_data_blocks;

  int m_active_directory;
  int m_active_bat;

  GCMemcardImportFileRetVal ImportGciInternal(File::IOFile&& gci, const std::string& inputFile);
  void InitActiveDirBat();

  const Directory& GetActiveDirectory() const;
  const BlockAlloc& GetActiveBat() const;

  void UpdateDirectory(const Directory& directory);
  void UpdateBat(const BlockAlloc& bat);

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

  // Fetches a DEntry from the given file index.
  std::optional<DEntry> GetDEntry(u8 index) const;

  GCMemcardGetSaveDataRetVal GetSaveData(u8 index, std::vector<GCMBlock>& saveBlocks) const;

  // adds the file to the directory and copies its contents
  GCMemcardImportFileRetVal ImportFile(const DEntry& direntry, std::vector<GCMBlock>& saveBlocks);

  // delete a file from the directory
  GCMemcardRemoveFileRetVal RemoveFile(u8 index);

  // reads a save from another memcard, and imports the data into this memcard
  GCMemcardImportFileRetVal CopyFrom(const GCMemcard& source, u8 index);

  // reads a .gci/.gcs/.sav file and calls ImportFile
  GCMemcardImportFileRetVal ImportGci(const std::string& inputFile);

  // writes a .gci file to disk containing index
  GCMemcardExportFileRetVal ExportGci(u8 index, const std::string& fileName,
                                      const std::string& directory) const;

  // GCI files are untouched, SAV files are byteswapped
  // GCS files have the block count set, default is 1 (For export as GCS)
  static void Gcs_SavConvert(DEntry& tempDEntry, int saveType, u64 length = BLOCK_SIZE);

  // reads the banner image
  bool ReadBannerRGBA8(u8 index, u32* buffer) const;

  // reads the animation frames
  u32 ReadAnimRGBA8(u8 index, u32* buffer, u8* delays) const;
};
