// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <algorithm>
#include <array>
#include <bitset>
#include <limits>
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

namespace Memcard
{
enum
{
  SLOT_A = 0,
  SLOT_B = 1,
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
  NOMEMCARD,
  OUTOFDIRENTRIES,
  OUTOFBLOCKS,
  TITLEPRESENT,
};

enum class GCMemcardRemoveFileRetVal
{
  SUCCESS,
  NOMEMCARD,
  DELETE_FAIL,
};

enum class GCMemcardValidityIssues
{
  FAILED_TO_OPEN,
  IO_ERROR,
  INVALID_CARD_SIZE,
  INVALID_CHECKSUM,
  MISMATCHED_CARD_SIZE,
  FREE_BLOCK_MISMATCH,
  DIR_BAT_INCONSISTENT,
  DATA_IN_UNUSED_AREA,
  COUNT
};

class GCMemcardErrorCode
{
public:
  bool HasCriticalErrors() const;
  bool Test(GCMemcardValidityIssues code) const;
  void Set(GCMemcardValidityIssues code);
  GCMemcardErrorCode& operator|=(const GCMemcardErrorCode& other);

private:
  std::bitset<static_cast<size_t>(GCMemcardValidityIssues::COUNT)> m_errors;
};

struct GCMemcardAnimationFrameRGBA8
{
  std::vector<u32> image_data;
  u8 delay = 0;
};

// size of a single memory card block in bytes
constexpr u32 BLOCK_SIZE = 0x2000;

// the amount of memory card blocks in a megabit of data
constexpr u32 MBIT_TO_BLOCKS = (1024 * 1024) / (BLOCK_SIZE * 8);

// number of metadata and filesystem blocks before the actual user data blocks
constexpr u32 MC_FST_BLOCKS = 0x05;

// maximum number of saves that can be stored on a single memory card
constexpr u8 DIRLEN = 0x7F;

// maximum size of a single memory card file comment in bytes
constexpr u32 DENTRY_STRLEN = 0x20;

// size of a single entry in the Directory in bytes
constexpr u32 DENTRY_SIZE = 0x40;

// number of block entries in the BAT; one entry uses 2 bytes
constexpr u16 BAT_SIZE = 0xFFB;

// possible sizes of memory cards in megabits
// TODO: Do memory card sizes have to be power of two?
// TODO: Are these all of them? A 4091 block card should work in theory at least.
constexpr u16 MBIT_SIZE_MEMORY_CARD_59 = 0x04;
constexpr u16 MBIT_SIZE_MEMORY_CARD_123 = 0x08;
constexpr u16 MBIT_SIZE_MEMORY_CARD_251 = 0x10;
constexpr u16 MBIT_SIZE_MEMORY_CARD_507 = 0x20;
constexpr u16 MBIT_SIZE_MEMORY_CARD_1019 = 0x40;
constexpr u16 MBIT_SIZE_MEMORY_CARD_2043 = 0x80;

// width and height of a save file's banner in pixels
constexpr u32 MEMORY_CARD_BANNER_WIDTH = 96;
constexpr u32 MEMORY_CARD_BANNER_HEIGHT = 32;

// color format of banner as stored in the lowest two bits of m_banner_and_icon_flags
constexpr u8 MEMORY_CARD_BANNER_FORMAT_CI8 = 1;
constexpr u8 MEMORY_CARD_BANNER_FORMAT_RGB5A3 = 2;

// width and height of a save file's icon in pixels
constexpr u32 MEMORY_CARD_ICON_WIDTH = 32;
constexpr u32 MEMORY_CARD_ICON_HEIGHT = 32;

// maximum number of frames a save file's icon animation can have
constexpr u32 MEMORY_CARD_ICON_ANIMATION_MAX_FRAMES = 8;

// color format of icon frame as stored in m_icon_format (two bits per frame)
constexpr u8 MEMORY_CARD_ICON_FORMAT_CI8_SHARED_PALETTE = 1;
constexpr u8 MEMORY_CARD_ICON_FORMAT_RGB5A3 = 2;
constexpr u8 MEMORY_CARD_ICON_FORMAT_CI8_UNIQUE_PALETTE = 3;

// number of palette entries in a CI8 palette of a banner or icon
// each palette entry is 16 bits in RGB5A3 format
constexpr u32 MEMORY_CARD_CI8_PALETTE_ENTRIES = 256;

constexpr u32 MbitToFreeBlocks(u16 size_mb)
{
  return size_mb * MBIT_TO_BLOCKS - MC_FST_BLOCKS;
}

struct GCMBlock
{
  GCMBlock();
  void Erase();
  std::array<u8, BLOCK_SIZE> m_block;
};
static_assert(sizeof(GCMBlock) == BLOCK_SIZE);
static_assert(std::is_trivially_copyable_v<GCMBlock>);

#pragma pack(push, 1)
// split off from Header to have a small struct with all the data needed to regenerate the header
// for GCI folders
struct HeaderData
{
  // NOTE: libogc refers to 'Serial' as the first 0x20 bytes of the header,
  // so the data from m_serial until m_dtv_status (inclusive)

  // 12 bytes at 0x0000
  std::array<u8, 12> m_serial;

  // 8 bytes at 0x000c: Time of format (OSTime value)
  Common::BigEndianValue<u64> m_format_time;

  // 4 bytes at 0x0014; SRAM bias at time of format
  u32 m_sram_bias;

  // 4 bytes at 0x0018: SRAM language
  Common::BigEndianValue<u32> m_sram_language;

  // 4 bytes at 0x001c: VI DTV status register value (u16 from 0xCC00206E)
  u32 m_dtv_status;

  // 2 bytes at 0x0020: 0 if formated in slot A, 1 if formated in slot B
  Common::BigEndianValue<u16> m_device_id;

  // 2 bytes at 0x0022: Size of memcard in Mbits
  Common::BigEndianValue<u16> m_size_mb;

  // 2 bytes at 0x0024: Encoding (Windows-1252 or Shift JIS)
  Common::BigEndianValue<u16> m_encoding;
};
static_assert(std::is_trivially_copyable_v<HeaderData>);

void InitializeHeaderData(HeaderData* data, const CardFlashId& flash_id, u16 size_mbits,
                          bool shift_jis, u32 rtc_bias, u32 sram_language, u64 format_time);

bool operator==(const HeaderData& lhs, const HeaderData& rhs);
bool operator!=(const HeaderData& lhs, const HeaderData& rhs);

struct Header
{
  HeaderData m_data;

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

  // initialize an unformatted header block
  explicit Header();

  // initialize a formatted header block
  explicit Header(const CardFlashId& flash_id, u16 size_mbits, bool shift_jis, u32 rtc_bias,
                  u32 sram_language, u64 format_time);

  // initialize a header block from existing HeaderData
  explicit Header(const HeaderData& data);

  // Calculates the card serial numbers used for encrypting some save files.
  std::pair<u32, u32> CalculateSerial() const;

  void FixChecksums();
  std::pair<u16, u16> CalculateChecksums() const;

  GCMemcardErrorCode CheckForErrors(u16 card_size_mbits) const;

  bool IsShiftJIS() const;
};
static_assert(sizeof(Header) == BLOCK_SIZE);
static_assert(std::is_trivially_copyable_v<Header>);

struct DEntry
{
  DEntry();

  static constexpr std::array<u8, 4> UNINITIALIZED_GAMECODE{{0xFF, 0xFF, 0xFF, 0xFF}};

  // 4 bytes at 0x00: Gamecode
  std::array<u8, 4> m_gamecode;

  // 2 bytes at 0x04: Makercode
  std::array<u8, 2> m_makercode;

  // 1 byte at 0x06: reserved/unused (always 0xff, has no effect)
  u8 m_unused_1;

  // 1 byte at 0x07: banner gfx format and icon animation (Image Key)
  // First two bits are used for the banner format.
  // YAGCD is wrong about the meaning of these.
  // '0' and '3' both mean no banner.
  // '1' means paletted (8 bits per pixel palette entry + 16 bit color palette in RGB5A3)
  // '2' means direct color (16 bits per pixel in RGB5A3)
  // Third bit is icon animation frame order, 0 for loop (abcabcabc), 1 for ping-pong (abcbabcba).
  // Remaining bits seem unused.
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
static_assert(std::is_trivially_copyable_v<DEntry>);

struct BlockAlloc;

struct Directory
{
  // 127 files of 0x40 bytes each
  std::array<DEntry, DIRLEN> m_dir_entries;

  // 0x3a bytes at 0x1fc0: Unused, always 0xFF
  std::array<u8, 0x3a> m_padding;

  // 2 bytes at 0x1ffa: Update Counter
  Common::BigEndianValue<s16> m_update_counter;

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

  GCMemcardErrorCode CheckForErrors() const;

  GCMemcardErrorCode CheckForErrorsWithBat(const BlockAlloc& bat) const;
};
static_assert(sizeof(Directory) == BLOCK_SIZE);
static_assert(std::is_trivially_copyable_v<Directory>);

struct BlockAlloc
{
  // 2 bytes at 0x0000: Additive Checksum
  u16 m_checksum;

  // 2 bytes at 0x0002: Inverse Checksum
  u16 m_checksum_inv;

  // 2 bytes at 0x0004: Update Counter
  Common::BigEndianValue<s16> m_update_counter;

  // 2 bytes at 0x0006: Free Blocks
  Common::BigEndianValue<u16> m_free_blocks;

  // 2 bytes at 0x0008: Last allocated Block
  Common::BigEndianValue<u16> m_last_allocated_block;

  // 0x1ff8 bytes at 0x000a: Map of allocated Blocks
  std::array<Common::BigEndianValue<u16>, BAT_SIZE> m_map;

  explicit BlockAlloc(u16 size_mbits = MBIT_SIZE_MEMORY_CARD_2043);

  u16 GetNextBlock(u16 block) const;
  u16 NextFreeBlock(u16 max_block, u16 starting_block = MC_FST_BLOCKS) const;
  bool ClearBlocks(u16 starting_block, u16 block_count);
  u16 AssignBlocksContiguous(u16 length);

  void FixChecksums();
  std::pair<u16, u16> CalculateChecksums() const;

  GCMemcardErrorCode CheckForErrors(u16 size_mbits) const;
};
static_assert(sizeof(BlockAlloc) == BLOCK_SIZE);
static_assert(std::is_trivially_copyable_v<BlockAlloc>);
#pragma pack(pop)

struct Savefile
{
  DEntry dir_entry;
  std::vector<GCMBlock> blocks;
};

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

  GCMemcard();

  const Directory& GetActiveDirectory() const;
  const BlockAlloc& GetActiveBat() const;

  void UpdateDirectory(const Directory& directory);
  void UpdateBat(const BlockAlloc& bat);

public:
  static std::optional<GCMemcard> Create(std::string filename, const CardFlashId& flash_id,
                                         u16 size_mbits, bool shift_jis, u32 rtc_bias,
                                         u32 sram_language, u64 format_time);

  static std::pair<GCMemcardErrorCode, std::optional<GCMemcard>> Open(std::string filename);

  GCMemcard(const GCMemcard&) = delete;
  GCMemcard& operator=(const GCMemcard&) = delete;
  GCMemcard(GCMemcard&&) = default;
  GCMemcard& operator=(GCMemcard&&) = default;

  bool IsValid() const { return m_valid; }
  bool IsShiftJIS() const;
  bool Save();
  bool Format(const CardFlashId& flash_id, u16 size_mbits, bool shift_jis, u32 rtc_bias,
              u32 sram_language, u64 format_time);
  static bool Format(u8* card_data, const CardFlashId& flash_id, u16 size_mbits, bool shift_jis,
                     u32 rtc_bias, u32 sram_language, u64 format_time);
  static s32 FZEROGX_MakeSaveGameValid(const Header& cardheader, const DEntry& direntry,
                                       std::vector<GCMBlock>& FileBuffer);
  static s32 PSO_MakeSaveGameValid(const Header& cardheader, const DEntry& direntry,
                                   std::vector<GCMBlock>& FileBuffer);

  bool FixChecksums();

  // get number of file entries in the directory
  u8 GetNumFiles() const;
  u8 GetFileIndex(u8 fileNumber) const;

  // get the free blocks from bat
  u16 GetFreeBlocks() const;

  // Returns index of the save with the same identity as the given DEntry, or nullopt if no save
  // with that identity exists in this card.
  std::optional<u8> TitlePresent(const DEntry& d) const;

  // DEntry functions, all take u8 index < DIRLEN (127)
  bool DEntry_IsPingPong(u8 index) const;
  // get first block for file
  u16 DEntry_FirstBlock(u8 index) const;
  // get file length in blocks
  u16 DEntry_BlockCount(u8 index) const;

  std::optional<std::vector<u8>>
  GetSaveDataBytes(u8 save_index, size_t offset = 0,
                   size_t length = std::numeric_limits<size_t>::max()) const;

  // Returns, if available, the two strings shown on the save file in the GC BIOS, in UTF8.
  // The first is the big line on top, usually the game title, and the second is the smaller line
  // next to the block size, often a progress indicator or subtitle.
  std::optional<std::pair<std::string, std::string>> GetSaveComments(u8 index) const;

  // Fetches a DEntry from the given file index.
  std::optional<DEntry> GetDEntry(u8 index) const;

  GCMemcardGetSaveDataRetVal GetSaveData(u8 index, std::vector<GCMBlock>& saveBlocks) const;

  // Adds the given savefile to the memory card, if possible.
  GCMemcardImportFileRetVal ImportFile(const Savefile& savefile);

  // Fetches the savefile at the given directory index, if any.
  std::optional<Savefile> ExportFile(u8 index) const;

  // delete a file from the directory
  GCMemcardRemoveFileRetVal RemoveFile(u8 index);

  // reads the banner image
  std::optional<std::vector<u32>> ReadBannerRGBA8(u8 index) const;

  // reads the animation frames
  std::optional<std::vector<GCMemcardAnimationFrameRGBA8>> ReadAnimRGBA8(u8 index) const;
};
}  // namespace Memcard
