// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DiscIO/VolumeVerifier.h"

#include <algorithm>
#include <cinttypes>
#include <limits>
#include <optional>
#include <string>
#include <unordered_set>

#include <mbedtls/md5.h>
#include <mbedtls/sha1.h>
#include <zlib.h>

#include "Common/Align.h"
#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "Common/Swap.h"
#include "Core/IOS/Device.h"
#include "Core/IOS/ES/ES.h"
#include "Core/IOS/ES/Formats.h"
#include "Core/IOS/IOS.h"
#include "Core/IOS/IOSC.h"
#include "DiscIO/Blob.h"
#include "DiscIO/DiscExtractor.h"
#include "DiscIO/DiscScrubber.h"
#include "DiscIO/Enums.h"
#include "DiscIO/Filesystem.h"
#include "DiscIO/Volume.h"
#include "DiscIO/VolumeWii.h"

namespace DiscIO
{
constexpr u64 MINI_DVD_SIZE = 1459978240;  // GameCube
constexpr u64 SL_DVD_SIZE = 4699979776;    // Wii retail
constexpr u64 SL_DVD_R_SIZE = 4707319808;  // Wii RVT-R
constexpr u64 DL_DVD_SIZE = 8511160320;    // Wii retail
constexpr u64 DL_DVD_R_SIZE = 8543666176;  // Wii RVT-R

constexpr u64 BLOCK_SIZE = 0x20000;

VolumeVerifier::VolumeVerifier(const Volume& volume, Hashes<bool> hashes_to_calculate)
    : m_volume(volume), m_hashes_to_calculate(hashes_to_calculate),
      m_calculating_any_hash(hashes_to_calculate.crc32 || hashes_to_calculate.md5 ||
                             hashes_to_calculate.sha1),
      m_max_progress(volume.GetSize())
{
}

VolumeVerifier::~VolumeVerifier() = default;

void VolumeVerifier::Start()
{
  ASSERT(!m_started);
  m_started = true;

  m_is_tgc = m_volume.GetBlobType() == BlobType::TGC;
  m_is_datel = IsDisc(m_volume.GetVolumeType()) &&
               !GetBootDOLOffset(m_volume, m_volume.GetGamePartition()).has_value();
  m_is_not_retail =
      (m_volume.GetVolumeType() == Platform::WiiDisc && !m_volume.IsEncryptedAndHashed()) ||
      IsDebugSigned();

  CheckPartitions();
  if (m_volume.GetVolumeType() == Platform::WiiWAD)
    CheckCorrectlySigned(PARTITION_NONE, Common::GetStringT("This title is not correctly signed."));
  CheckDiscSize();
  CheckMisc();

  SetUpHashing();
}

void VolumeVerifier::CheckPartitions()
{
  const std::vector<Partition> partitions = m_volume.GetPartitions();
  if (partitions.empty())
  {
    if (m_volume.GetVolumeType() != Platform::WiiWAD &&
        !m_volume.GetFileSystem(m_volume.GetGamePartition()))
    {
      AddProblem(Severity::High,
                 Common::GetStringT("The filesystem is invalid or could not be read."));
    }
    return;
  }

  std::optional<u32> partitions_in_first_table = m_volume.ReadSwapped<u32>(0x40000, PARTITION_NONE);
  if (partitions_in_first_table && *partitions_in_first_table > 8)
  {
    // Not sure if 8 actually is the limit, but there certainly aren't any discs
    // released that have as many partitions as 8 in the first partition table.
    // The only game that has that many partitions in total is Super Smash Bros. Brawl,
    // and that game places all partitions other than UPDATE and DATA in the second table.
    AddProblem(Severity::Low,
               Common::GetStringT("There are too many partitions in the first partition table."));
  }

  std::vector<u32> types;
  for (const Partition& partition : partitions)
  {
    const std::optional<u32> type = m_volume.GetPartitionType(partition);
    if (type)
      types.emplace_back(*type);
  }

  if (std::find(types.cbegin(), types.cend(), PARTITION_UPDATE) == types.cend())
    AddProblem(Severity::Low, Common::GetStringT("The update partition is missing."));

  if (std::find(types.cbegin(), types.cend(), PARTITION_DATA) == types.cend())
    AddProblem(Severity::High, Common::GetStringT("The data partition is missing."));

  const bool has_channel_partition =
      std::find(types.cbegin(), types.cend(), PARTITION_CHANNEL) != types.cend();
  if (ShouldHaveChannelPartition() && !has_channel_partition)
    AddProblem(Severity::Medium, Common::GetStringT("The channel partition is missing."));

  const bool has_install_partition =
      std::find(types.cbegin(), types.cend(), PARTITION_INSTALL) != types.cend();
  if (ShouldHaveInstallPartition() && !has_install_partition)
    AddProblem(Severity::High, Common::GetStringT("The install partition is missing."));

  if (ShouldHaveMasterpiecePartitions() &&
      types.cend() ==
          std::find_if(types.cbegin(), types.cend(), [](u32 type) { return type >= 0xFF; }))
  {
    // i18n: This string is referring to a game mode in Super Smash Bros. Brawl called Masterpieces
    // where you play demos of NES/SNES/N64 games. Official translations:
    // 名作トライアル (Japanese), Masterpieces (English), Meisterstücke (German), Chefs-d'œuvre
    // (French), Clásicos (Spanish), Capolavori (Italian), 클래식 게임 체험판 (Korean).
    // If your language is not one of the languages above, consider leaving the string untranslated
    // so that people will recognize it as the name of the game mode.
    AddProblem(Severity::Medium, Common::GetStringT("The Masterpiece partitions are missing."));
  }

  for (const Partition& partition : partitions)
  {
    if (m_volume.GetPartitionType(partition) == PARTITION_UPDATE && partition.offset != 0x50000)
    {
      AddProblem(Severity::Low,
                 Common::GetStringT("The update partition is not at its normal position."));
    }

    const u64 normal_data_offset = m_volume.IsEncryptedAndHashed() ? 0xF800000 : 0x838000;
    if (m_volume.GetPartitionType(partition) == PARTITION_DATA &&
        partition.offset != normal_data_offset && !has_channel_partition && !has_install_partition)
    {
      AddProblem(Severity::Low,
                 Common::GetStringT(
                     "The data partition is not at its normal position. This will affect the "
                     "emulated loading times. When using NetPlay or sending input recordings to "
                     "other people, you will experience desyncs if anyone is using a good dump."));
    }
  }

  for (const Partition& partition : partitions)
    CheckPartition(partition);
}

bool VolumeVerifier::CheckPartition(const Partition& partition)
{
  std::optional<u32> type = m_volume.GetPartitionType(partition);
  if (!type)
  {
    // Not sure if this can happen in practice
    AddProblem(Severity::Medium, Common::GetStringT("The type of a partition could not be read."));
    return false;
  }

  Severity severity = Severity::Medium;
  if (*type == PARTITION_DATA || *type == PARTITION_INSTALL)
    severity = Severity::High;
  else if (*type == PARTITION_UPDATE)
    severity = Severity::Low;

  const std::string name = GetPartitionName(type);

  if (partition.offset % VolumeWii::BLOCK_TOTAL_SIZE != 0 ||
      m_volume.PartitionOffsetToRawOffset(0, partition) % VolumeWii::BLOCK_TOTAL_SIZE != 0)
  {
    AddProblem(
        Severity::Medium,
        StringFromFormat(Common::GetStringT("The %s partition is not properly aligned.").c_str(),
                         name.c_str()));
  }

  CheckCorrectlySigned(
      partition,
      StringFromFormat(Common::GetStringT("The %s partition is not correctly signed.").c_str(),
                       name.c_str()));

  if (m_volume.SupportsIntegrityCheck() && !m_volume.CheckH3TableIntegrity(partition))
  {
    std::string text = StringFromFormat(
        Common::GetStringT("The H3 hash table for the %s partition is not correct.").c_str(),
        name.c_str());
    AddProblem(Severity::Low, std::move(text));
  }

  bool invalid_disc_header = false;
  std::vector<u8> disc_header(0x80);
  constexpr u32 WII_MAGIC = 0x5D1C9EA3;
  if (!m_volume.Read(0, disc_header.size(), disc_header.data(), partition))
  {
    invalid_disc_header = true;
  }
  else if (Common::swap32(disc_header.data() + 0x18) != WII_MAGIC)
  {
    for (size_t i = 0; i < disc_header.size(); i += 4)
    {
      if (Common::swap32(disc_header.data() + i) != i)
      {
        invalid_disc_header = true;
        break;
      }
    }

    // The loop above ends without breaking for discs that legitimately lack updates.
    // No such discs have been released to end users. Most such discs are debug signed,
    // but there is apparently at least one that is retail signed, the Movie-Ch Install Disc.
    return false;
  }
  if (invalid_disc_header)
  {
    // This can happen when certain programs that create WBFS files scrub the entirety of
    // the Masterpiece partitions in Super Smash Bros. Brawl without removing them from
    // the partition table. https://bugs.dolphin-emu.org/issues/8733
    std::string text = StringFromFormat(
        Common::GetStringT("The %s partition does not seem to contain valid data.").c_str(),
        name.c_str());
    AddProblem(severity, std::move(text));
    return false;
  }

  const DiscIO::FileSystem* filesystem = m_volume.GetFileSystem(partition);
  if (!filesystem)
  {
    std::string text = StringFromFormat(
        Common::GetStringT("The %s partition does not have a valid file system.").c_str(),
        name.c_str());
    AddProblem(severity, std::move(text));
    return false;
  }

  if (type == PARTITION_UPDATE)
  {
    std::unique_ptr<FileInfo> file_info = filesystem->FindFileInfo("_sys");
    bool has_correct_ios = false;
    if (file_info)
    {
      const IOS::ES::TMDReader& tmd = m_volume.GetTMD(m_volume.GetGamePartition());
      if (tmd.IsValid())
      {
        const std::string correct_ios = "IOS" + std::to_string(tmd.GetIOSId() & 0xFF) + "-";
        for (const FileInfo& f : *file_info)
        {
          if (StringBeginsWith(f.GetName(), correct_ios))
          {
            has_correct_ios = true;
            break;
          }
        }
      }
    }

    if (!has_correct_ios)
    {
      // This is reached for hacked dumps where the update partition has been replaced with
      // a very old update partition so that no updates will be installed.
      AddProblem(
          Severity::Low,
          Common::GetStringT("The update partition does not contain the IOS used by this title."));
    }
  }

  // Prepare for hash verification in the Process step
  if (m_volume.SupportsIntegrityCheck())
  {
    u64 offset = m_volume.PartitionOffsetToRawOffset(0, partition);
    const std::optional<u64> size =
        m_volume.ReadSwappedAndShifted(partition.offset + 0x2bc, PARTITION_NONE);
    const u64 end_offset = offset + size.value_or(0);

    for (size_t i = 0; offset < end_offset; ++i, offset += VolumeWii::BLOCK_TOTAL_SIZE)
      m_blocks.emplace_back(BlockToVerify{partition, offset, i});

    m_block_errors.emplace(partition, 0);
  }

  return true;
}

std::string VolumeVerifier::GetPartitionName(std::optional<u32> type) const
{
  if (!type)
    return "???";

  std::string name = NameForPartitionType(*type, false);
  if (ShouldHaveMasterpiecePartitions() && *type > 0xFF)
  {
    // i18n: This string is referring to a game mode in Super Smash Bros. Brawl called Masterpieces
    // where you play demos of NES/SNES/N64 games. This string is referring to a specific such demo
    // rather than the game mode as a whole, so please use the singular form. Official translations:
    // 名作トライアル (Japanese), Masterpieces (English), Meisterstücke (German), Chefs-d'œuvre
    // (French), Clásicos (Spanish), Capolavori (Italian), 클래식 게임 체험판 (Korean).
    // If your language is not one of the languages above, consider leaving the string untranslated
    // so that people will recognize it as the name of the game mode.
    name = StringFromFormat(Common::GetStringT("%s (Masterpiece)").c_str(), name.c_str());
  }
  return name;
}

void VolumeVerifier::CheckCorrectlySigned(const Partition& partition, std::string error_text)
{
  IOS::HLE::Kernel ios;
  const auto es = ios.GetES();
  const std::vector<u8> cert_chain = m_volume.GetCertificateChain(partition);

  if (IOS::HLE::IPC_SUCCESS !=
          es->VerifyContainer(IOS::HLE::Device::ES::VerifyContainerType::Ticket,
                              IOS::HLE::Device::ES::VerifyMode::DoNotUpdateCertStore,
                              m_volume.GetTicket(partition), cert_chain) ||
      IOS::HLE::IPC_SUCCESS !=
          es->VerifyContainer(IOS::HLE::Device::ES::VerifyContainerType::TMD,
                              IOS::HLE::Device::ES::VerifyMode::DoNotUpdateCertStore,
                              m_volume.GetTMD(partition), cert_chain))
  {
    AddProblem(Severity::Low, std::move(error_text));
  }
}

bool VolumeVerifier::IsDebugSigned() const
{
  const IOS::ES::TicketReader& ticket = m_volume.GetTicket(m_volume.GetGamePartition());
  return ticket.IsValid() ? ticket.GetConsoleType() == IOS::HLE::IOSC::ConsoleType::RVT : false;
}

bool VolumeVerifier::ShouldHaveChannelPartition() const
{
  const std::unordered_set<std::string> channel_discs{
      "RFNE01", "RFNJ01", "RFNK01", "RFNP01", "RFNW01", "RFPE01", "RFPJ01", "RFPK01", "RFPP01",
      "RFPW01", "RGWE41", "RGWJ41", "RGWP41", "RGWX41", "RMCE01", "RMCJ01", "RMCK01", "RMCP01",
  };

  return channel_discs.find(m_volume.GetGameID()) != channel_discs.end();
}

bool VolumeVerifier::ShouldHaveInstallPartition() const
{
  const std::unordered_set<std::string> dragon_quest_x{"S4MJGD", "S4SJGD", "S6TJGD", "SDQJGD"};
  return dragon_quest_x.find(m_volume.GetGameID()) != dragon_quest_x.end();
}

bool VolumeVerifier::ShouldHaveMasterpiecePartitions() const
{
  const std::unordered_set<std::string> ssbb{"RSBE01", "RSBJ01", "RSBK01", "RSBP01"};
  return ssbb.find(m_volume.GetGameID()) != ssbb.end();
}

bool VolumeVerifier::ShouldBeDualLayer() const
{
  // The Japanese versions of Xenoblade and The Last Story are single-layer
  // (unlike the other versions) and must not be added to this list.
  const std::unordered_set<std::string> dual_layer_discs{
      "R3ME01", "R3MP01", "R3OE01", "R3OJ01", "R3OP01", "RSBE01", "RSBJ01", "RSBK01", "RSBP01",
      "RXMJ8P", "S59E01", "S59JC8", "S59P01", "S5QJC8", "SK8X52", "SAKENS", "SAKPNS", "SK8V52",
      "SK8X52", "SLSEXJ", "SLSP01", "SQIE4Q", "SQIP4Q", "SQIY4Q", "SR5E41", "SR5P41", "SUOE41",
      "SUOP41", "SVXX52", "SVXY52", "SX4E01", "SX4P01", "SZ3EGT", "SZ3PGT",
  };

  return dual_layer_discs.find(m_volume.GetGameID()) != dual_layer_discs.end();
}

void VolumeVerifier::CheckDiscSize()
{
  if (!IsDisc(m_volume.GetVolumeType()))
    return;

  const u64 biggest_offset = GetBiggestUsedOffset();
  if (biggest_offset > m_volume.GetSize())
  {
    const bool second_layer_missing =
        biggest_offset > SL_DVD_SIZE && m_volume.GetSize() >= SL_DVD_SIZE;
    std::string text =
        second_layer_missing ?
            Common::GetStringT(
                "This disc image is too small and lacks some data. The problem is most likely that "
                "this is a dual-layer disc that has been dumped as a single-layer disc.") :
            Common::GetStringT(
                "This disc image is too small and lacks some data. If your dumping program saved "
                "the disc image as several parts, you need to merge them into one file.");
    AddProblem(Severity::High, std::move(text));
    return;
  }

  if (ShouldBeDualLayer() && biggest_offset <= SL_DVD_R_SIZE)
  {
    AddProblem(Severity::Medium,
               Common::GetStringT(
                   "This game has been hacked to fit on a single-layer DVD. Some content such as "
                   "pre-rendered videos, extra languages or entire game modes will be broken. "
                   "This problem generally only exists in illegal copies of games."));
  }

  if (!m_volume.IsSizeAccurate())
  {
    AddProblem(Severity::Low,
               Common::GetStringT("The format that the disc image is saved in does not "
                                  "store the size of the disc image."));
  }
  else if (!m_is_tgc)
  {
    const Platform platform = m_volume.GetVolumeType();
    const u64 size = m_volume.GetSize();

    const bool valid_gamecube = size == MINI_DVD_SIZE;
    const bool valid_retail_wii = size == SL_DVD_SIZE || size == DL_DVD_SIZE;
    const bool valid_debug_wii = size == SL_DVD_R_SIZE || size == DL_DVD_R_SIZE;

    const bool debug = IsDebugSigned();
    if ((platform == Platform::GameCubeDisc && !valid_gamecube) ||
        (platform == Platform::WiiDisc && (debug ? !valid_debug_wii : !valid_retail_wii)))
    {
      if (debug && valid_retail_wii)
      {
        AddProblem(
            Severity::Low,
            Common::GetStringT("This debug disc image has the size of a retail disc image."));
      }
      else
      {
        const bool small =
            (m_volume.GetVolumeType() == Platform::GameCubeDisc && size < MINI_DVD_SIZE) ||
            (m_volume.GetVolumeType() == Platform::WiiDisc && size < SL_DVD_SIZE);

        if (small)
        {
          AddProblem(
              Severity::Low,
              Common::GetStringT("This disc image has an unusual size. This will likely make the "
                                 "emulated loading times longer. When using NetPlay or sending "
                                 "input recordings to other people, you will likely experience "
                                 "desyncs if anyone is using a good dump."));
        }
        else
        {
          AddProblem(Severity::Low, Common::GetStringT("This disc image has an unusual size."));
        }
      }
    }
  }
}

u64 VolumeVerifier::GetBiggestUsedOffset() const
{
  std::vector<Partition> partitions = m_volume.GetPartitions();
  if (partitions.empty())
    partitions.emplace_back(m_volume.GetGamePartition());

  const u64 disc_header_size = m_volume.GetVolumeType() == Platform::GameCubeDisc ? 0x460 : 0x50000;
  u64 biggest_offset = disc_header_size;
  for (const Partition& partition : partitions)
  {
    if (partition != PARTITION_NONE)
    {
      const u64 offset = m_volume.PartitionOffsetToRawOffset(0x440, partition);
      biggest_offset = std::max(biggest_offset, offset);
    }

    const std::optional<u64> dol_offset = GetBootDOLOffset(m_volume, partition);
    if (dol_offset)
    {
      const std::optional<u64> dol_size = GetBootDOLSize(m_volume, partition, *dol_offset);
      if (dol_size)
      {
        const u64 offset = m_volume.PartitionOffsetToRawOffset(*dol_offset + *dol_size, partition);
        biggest_offset = std::max(biggest_offset, offset);
      }
    }

    const std::optional<u64> fst_offset = GetFSTOffset(m_volume, partition);
    const std::optional<u64> fst_size = GetFSTSize(m_volume, partition);
    if (fst_offset && fst_size)
    {
      const u64 offset = m_volume.PartitionOffsetToRawOffset(*fst_offset + *fst_size, partition);
      biggest_offset = std::max(biggest_offset, offset);
    }

    const FileSystem* fs = m_volume.GetFileSystem(partition);
    if (fs)
    {
      const u64 offset =
          m_volume.PartitionOffsetToRawOffset(GetBiggestUsedOffset(fs->GetRoot()), partition);
      biggest_offset = std::max(biggest_offset, offset);
    }
  }
  return biggest_offset;
}

u64 VolumeVerifier::GetBiggestUsedOffset(const FileInfo& file_info) const
{
  if (file_info.IsDirectory())
  {
    u64 biggest_offset = 0;
    for (const FileInfo& f : file_info)
      biggest_offset = std::max(biggest_offset, GetBiggestUsedOffset(f));
    return biggest_offset;
  }
  else
  {
    return file_info.GetOffset() + file_info.GetSize();
  }
}

void VolumeVerifier::CheckMisc()
{
  const std::string game_id_unencrypted = m_volume.GetGameID(PARTITION_NONE);
  const std::string game_id_encrypted = m_volume.GetGameID(m_volume.GetGamePartition());

  if (game_id_unencrypted != game_id_encrypted)
  {
    bool inconsistent_game_id = true;
    if (game_id_encrypted == "RELSAB")
    {
      if (StringBeginsWith(game_id_unencrypted, "410"))
      {
        // This is the Wii Backup Disc (aka "pinkfish" disc),
        // which legitimately has an inconsistent game ID.
        inconsistent_game_id = false;
      }
      else if (StringBeginsWith(game_id_unencrypted, "010"))
      {
        // Hacked version of the Wii Backup Disc (aka "pinkfish" disc).
        std::string proper_game_id = game_id_unencrypted;
        proper_game_id[0] = '4';
        AddProblem(
            Severity::Low,
            StringFromFormat(Common::GetStringT("The game ID is %s but should be %s.").c_str(),
                             game_id_unencrypted.c_str(), proper_game_id.c_str()));
        inconsistent_game_id = false;
      }
    }

    if (inconsistent_game_id)
    {
      AddProblem(Severity::Low, Common::GetStringT("The game ID is inconsistent."));
    }
  }

  const Region region = m_volume.GetRegion();
  const Platform platform = m_volume.GetVolumeType();

  if (game_id_encrypted.size() < 4)
  {
    AddProblem(Severity::Low, Common::GetStringT("The game ID is unusually short."));
  }
  else
  {
    char country_code;
    if (IsDisc(m_volume.GetVolumeType()))
      country_code = game_id_encrypted[3];
    else
      country_code = static_cast<char>(m_volume.GetTitleID().value_or(0) & 0xff);
    if (CountryCodeToRegion(country_code, platform, region) != region)
    {
      AddProblem(Severity::Medium,
                 Common::GetStringT(
                     "The region code does not match the game ID. If this is because the "
                     "region code has been modified, the game might run at the wrong speed, "
                     "graphical elements might be offset, or the game might not run at all."));
    }
  }

  const IOS::ES::TMDReader& tmd = m_volume.GetTMD(m_volume.GetGamePartition());
  if (tmd.IsValid())
  {
    const u64 ios_id = tmd.GetIOSId() & 0xFF;

    // List of launch day Korean IOSes obtained from https://hackmii.com/2008/09/korean-wii/.
    // More IOSes were released later that were used in Korean games, but they're all over 40.
    // Also, old IOSes like IOS36 did eventually get released for Korean Wiis as part of system
    // updates, but there are likely no Korean games using them since those IOSes were old by then.
    if (region == Region::NTSC_K && ios_id < 40 && ios_id != 4 && ios_id != 9 && ios_id != 21 &&
        ios_id != 37)
    {
      // This is intended to catch pirated Korean games that have had the IOS slot set to 36
      // as a side effect of having to fakesign after changing the common key slot to 0.
      // (IOS36 was the last IOS to have the Trucha bug.) https://bugs.dolphin-emu.org/issues/10319
      AddProblem(
          Severity::High,
          // i18n: You may want to leave the term "ERROR #002" untranslated,
          // since the emulated software always displays it in English.
          Common::GetStringT("This Korean title is set to use an IOS that typically isn't used on "
                             "Korean consoles. This is likely to lead to ERROR #002."));
    }

    if (ios_id >= 0x80)
    {
      // This is also intended to catch fakesigned pirated Korean games,
      // but this time with the IOS slot set to cIOS instead of IOS36.
      AddProblem(Severity::High, Common::GetStringT("This title is set to use an invalid IOS."));
    }
  }

  m_ticket = m_volume.GetTicket(m_volume.GetGamePartition());
  if (m_ticket.IsValid())
  {
    const u8 specified_common_key_index = m_ticket.GetCommonKeyIndex();

    // Wii discs only use common key 0 (regular) and common key 1 (Korean), not common key 2 (vWii).
    if (m_volume.GetVolumeType() == Platform::WiiDisc && specified_common_key_index > 1)
    {
      AddProblem(Severity::High,
                 // i18n: This is "common" as in "shared", not the opposite of "uncommon"
                 Common::GetStringT("This title is set to use an invalid common key."));
    }

    if (m_volume.GetVolumeType() == Platform::WiiWAD)
    {
      m_ticket = m_volume.GetTicketWithFixedCommonKey();
      const u8 fixed_common_key_index = m_ticket.GetCommonKeyIndex();
      if (specified_common_key_index != fixed_common_key_index)
      {
        // Many fakesigned WADs have the common key index set to a (random?) bogus value.
        // For WADs, Dolphin will detect this and use the correct key, making this low severity.
        std::string text = StringFromFormat(
            // i18n: This is "common" as in "shared", not the opposite of "uncommon"
            Common::GetStringT("The specified common key index is %u but should be %u.").c_str(),
            specified_common_key_index, fixed_common_key_index);
        AddProblem(Severity::Low, std::move(text));
      }
    }
  }

  if (IsDisc(m_volume.GetVolumeType()))
  {
    constexpr u32 NKIT_MAGIC = 0x4E4B4954;  // "NKIT"
    if (m_volume.ReadSwapped<u32>(0x200, PARTITION_NONE) == NKIT_MAGIC)
    {
      AddProblem(
          Severity::Low,
          Common::GetStringT("This disc image is in the NKit format. It is not a good dump in its "
                             "current form, but it might become a good dump if converted back. "
                             "The CRC32 of this file might match the CRC32 of a good dump even "
                             "though the files are not identical."));
    }
  }
}

void VolumeVerifier::SetUpHashing()
{
  if (m_volume.GetVolumeType() == Platform::WiiWAD)
  {
    m_content_offsets = m_volume.GetContentOffsets();
  }
  else if (m_volume.GetVolumeType() == Platform::WiiDisc)
  {
    // Set up a DiscScrubber for checking whether blocks with errors are unused
    m_scrubber.SetupScrub(&m_volume, VolumeWii::BLOCK_TOTAL_SIZE);
  }

  std::sort(m_blocks.begin(), m_blocks.end(),
            [](const BlockToVerify& b1, const BlockToVerify& b2) { return b1.offset < b2.offset; });

  if (m_hashes_to_calculate.crc32)
    m_crc32_context = crc32(0, nullptr, 0);

  if (m_hashes_to_calculate.md5)
  {
    mbedtls_md5_init(&m_md5_context);
    mbedtls_md5_starts_ret(&m_md5_context);
  }

  if (m_hashes_to_calculate.sha1)
  {
    mbedtls_sha1_init(&m_sha1_context);
    mbedtls_sha1_starts_ret(&m_sha1_context);
  }
}

void VolumeVerifier::Process()
{
  ASSERT(m_started);
  ASSERT(!m_done);

  if (m_progress == m_max_progress)
    return;

  IOS::ES::Content content;
  bool content_read = false;
  u64 bytes_to_read = BLOCK_SIZE;
  if (m_content_index < m_content_offsets.size() &&
      m_content_offsets[m_content_index] == m_progress)
  {
    m_volume.GetTMD(PARTITION_NONE).GetContent(m_content_index, &content);
    bytes_to_read = Common::AlignUp(content.size, 0x40);
    content_read = true;
  }
  else if (m_content_index < m_content_offsets.size() &&
           m_content_offsets[m_content_index] > m_progress)
  {
    bytes_to_read = std::min(bytes_to_read, m_content_offsets[m_content_index] - m_progress);
  }
  else if (m_block_index < m_blocks.size() && m_blocks[m_block_index].offset == m_progress)
  {
    bytes_to_read = VolumeWii::BLOCK_TOTAL_SIZE;
  }
  else if (m_block_index < m_blocks.size() && m_blocks[m_block_index].offset > m_progress)
  {
    bytes_to_read = std::min(bytes_to_read, m_blocks[m_block_index].offset - m_progress);
  }
  bytes_to_read = std::min(bytes_to_read, m_max_progress - m_progress);

  if (m_calculating_any_hash)
  {
    std::vector<u8> data(bytes_to_read);
    if (!m_volume.Read(m_progress, bytes_to_read, data.data(), PARTITION_NONE))
    {
      m_calculating_any_hash = false;
    }
    else
    {
      if (m_hashes_to_calculate.crc32)
      {
        // It would be nice to use crc32_z here instead of crc32, but it isn't available on Android
        m_crc32_context =
            crc32(m_crc32_context, data.data(), static_cast<unsigned int>(bytes_to_read));
      }

      if (m_hashes_to_calculate.md5)
        mbedtls_md5_update_ret(&m_md5_context, data.data(), bytes_to_read);

      if (m_hashes_to_calculate.sha1)
        mbedtls_sha1_update_ret(&m_sha1_context, data.data(), bytes_to_read);
    }
  }

  m_progress += bytes_to_read;

  if (content_read)
  {
    if (!m_volume.CheckContentIntegrity(content, m_content_offsets[m_content_index], m_ticket))
    {
      AddProblem(
          Severity::High,
          StringFromFormat(Common::GetStringT("Content %08x is corrupt.").c_str(), content.id));
    }

    m_content_index++;
  }

  while (m_block_index < m_blocks.size() && m_blocks[m_block_index].offset < m_progress)
  {
    if (!m_volume.CheckBlockIntegrity(m_blocks[m_block_index].block_index,
                                      m_blocks[m_block_index].partition))
    {
      const u64 offset = m_blocks[m_block_index].offset;
      if (m_scrubber.CanBlockBeScrubbed(offset))
      {
        WARN_LOG(DISCIO, "Integrity check failed for unused block at 0x%" PRIx64, offset);
        m_unused_block_errors[m_blocks[m_block_index].partition]++;
      }
      else
      {
        WARN_LOG(DISCIO, "Integrity check failed for block at 0x%" PRIx64, offset);
        m_block_errors[m_blocks[m_block_index].partition]++;
      }
    }
    m_block_index++;
  }
}

u64 VolumeVerifier::GetBytesProcessed() const
{
  return m_progress;
}

u64 VolumeVerifier::GetTotalBytes() const
{
  return m_max_progress;
}

void VolumeVerifier::Finish()
{
  if (m_done)
    return;
  m_done = true;

  if (m_calculating_any_hash)
  {
    if (m_hashes_to_calculate.crc32)
    {
      m_result.hashes.crc32 = std::vector<u8>(4);
      const u32 crc32_be = Common::swap32(m_crc32_context);
      const u8* crc32_be_ptr = reinterpret_cast<const u8*>(&crc32_be);
      std::copy(crc32_be_ptr, crc32_be_ptr + 4, m_result.hashes.crc32.begin());
    }

    if (m_hashes_to_calculate.md5)
    {
      m_result.hashes.md5 = std::vector<u8>(16);
      mbedtls_md5_finish_ret(&m_md5_context, m_result.hashes.md5.data());
    }

    if (m_hashes_to_calculate.sha1)
    {
      m_result.hashes.sha1 = std::vector<u8>(20);
      mbedtls_sha1_finish_ret(&m_sha1_context, m_result.hashes.sha1.data());
    }
  }

  for (auto [partition, blocks] : m_block_errors)
  {
    if (blocks > 0)
    {
      const std::string name = GetPartitionName(m_volume.GetPartitionType(partition));
      std::string text = StringFromFormat(
          Common::GetStringT("Errors were found in %zu blocks in the %s partition.").c_str(),
          blocks, name.c_str());
      AddProblem(Severity::Medium, std::move(text));
    }
  }

  for (auto [partition, blocks] : m_unused_block_errors)
  {
    if (blocks > 0)
    {
      const std::string name = GetPartitionName(m_volume.GetPartitionType(partition));
      std::string text = StringFromFormat(
          Common::GetStringT("Errors were found in %zu unused blocks in the %s partition.").c_str(),
          blocks, name.c_str());
      AddProblem(Severity::Low, std::move(text));
    }
  }

  // Show the most serious problems at the top
  std::stable_sort(m_result.problems.begin(), m_result.problems.end(),
                   [](const Problem& p1, const Problem& p2) { return p1.severity > p2.severity; });
  const Severity highest_severity =
      m_result.problems.empty() ? Severity::None : m_result.problems[0].severity;

  if (m_is_datel)
  {
    m_result.summary_text = Common::GetStringT("Dolphin is unable to verify unlicensed discs.");
    return;
  }

  if (m_is_tgc)
  {
    m_result.summary_text =
        Common::GetStringT("Dolphin is unable to verify typical TGC files properly, "
                           "since they are not dumps of actual discs.");
    return;
  }

  switch (highest_severity)
  {
  case Severity::None:
    if (IsWii(m_volume.GetVolumeType()) && !m_is_not_retail)
    {
      m_result.summary_text = Common::GetStringT(
          "No problems were found. This does not guarantee that this is a good dump, "
          "but since Wii titles contain a lot of verification data, it does mean that "
          "there most likely are no problems that will affect emulation.");
    }
    else
    {
      m_result.summary_text = Common::GetStringT("No problems were found.");
    }
    break;
  case Severity::Low:
    m_result.summary_text =
        Common::GetStringT("Problems with low severity were found. They will most "
                           "likely not prevent the game from running.");
    break;
  case Severity::Medium:
    m_result.summary_text =
        Common::GetStringT("Problems with medium severity were found. The whole game "
                           "or certain parts of the game might not work correctly.");
    break;
  case Severity::High:
    m_result.summary_text = Common::GetStringT(
        "Problems with high severity were found. The game will most likely not work at all.");
    break;
  }

  if (m_volume.GetVolumeType() == Platform::GameCubeDisc)
  {
    m_result.summary_text +=
        Common::GetStringT("\n\nBecause GameCube disc images contain little verification data, "
                           "there may be problems that Dolphin is unable to detect.");
  }
  else if (m_is_not_retail)
  {
    m_result.summary_text +=
        Common::GetStringT("\n\nBecause this title is not for retail Wii consoles, "
                           "Dolphin cannot verify that it hasn't been tampered with.");
  }
}

const VolumeVerifier::Result& VolumeVerifier::GetResult() const
{
  return m_result;
}

void VolumeVerifier::AddProblem(Severity severity, std::string text)
{
  m_result.problems.emplace_back(Problem{severity, std::move(text)});
}

}  // namespace DiscIO
