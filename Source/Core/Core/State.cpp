// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/State.h"

#include <algorithm>
#include <filesystem>
#include <locale>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <utility>
#include <vector>

#include <fmt/chrono.h>
#include <fmt/format.h>

#include <lz4.h>
#include <lzo/lzo1x.h>

#include "Common/Buffer.h"
#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Contains.h"
#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/Thread.h"
#include "Common/TimeUtil.h"
#include "Common/TransferableSharedMutex.h"
#include "Common/Version.h"
#include "Common/WorkQueueThread.h"

#include "Core/AchievementManager.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/GeckoCode.h"
#include "Core/HW/HW.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/Wiimote.h"
#include "Core/Movie.h"
#include "Core/NetPlayProto.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

#include "UICommon/UICommon.h"

#include "VideoCommon/FrameDumpFFMpeg.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/VideoBackendBase.h"

namespace State
{
#if defined(__LZO_STRICT_16BIT)
static const u32 IN_LEN = 8 * 1024u;
#elif defined(LZO_ARCH_I086) && !defined(LZO_HAVE_MM_HUGE_ARRAY)
static const u32 IN_LEN = 60 * 1024u;
#else
static const u32 IN_LEN = 128 * 1024u;
#endif

static const u32 OUT_LEN = IN_LEN + (IN_LEN / 16) + 64 + 3;

static unsigned char __LZO_MMODEL out[OUT_LEN];

static AfterLoadCallbackFunc s_on_after_load_callback;

static Common::EventHook s_flush_unsaved_data_hook;

// Temporary undo state buffer
static Common::UniqueBuffer<u8> s_undo_load_buffer;

// Used to estimate buffer size for the next save.
static u32 s_last_state_size = 0;

// Shared locks are acquired for each state save task.
// Tasks generally transition from: Calling thread -> CPU thread -> Compress/Write thread.
// Holding an "exclusive" lock will:
// 1. Ensure all previous save tasks have been completely written to the file systen.
// 2. Prevent new tasks from starting.
static Common::TransferableSharedMutex s_state_saves_in_progress;

struct CompressAndDumpStateArgs
{
  Common::UniqueBuffer<u8> buffer;
  std::string filename;
  std::shared_lock<decltype(s_state_saves_in_progress)> task_lock;
};

// Queue for compressing and writing savestates to disk.
// Only the CPU thread manipulates this worker.
static Common::WorkQueueThreadSP<CompressAndDumpStateArgs> s_compress_and_dump_thread;

// Don't forget to increase this after doing changes on the savestate system
constexpr u32 STATE_VERSION = 177;  // Last changed in PR 13844

// Increase this if the StateExtendedHeader definition changes
constexpr u32 EXTENDED_HEADER_VERSION = 1;  // Last changed in PR 12217

// Change this if we ever need to store more data in the extended header
constexpr u32 COMPRESSED_DATA_OFFSET = 0;

constexpr u32 COOKIE_BASE = 0xBAADBABE;

// Maps savestate versions to Dolphin versions.
// Versions after 42 don't need to be added to this list,
// because they save the exact Dolphin version to savestates.
static const std::map<u32, std::pair<std::string, std::string>> s_old_versions = {
    // The 16 -> 17 change modified the size of StateHeader,
    // so versions older than that can't even be decompressed anymore
    {17, {"3.5-1311", "3.5-1364"}}, {18, {"3.5-1366", "3.5-1371"}}, {19, {"3.5-1372", "3.5-1408"}},
    {20, {"3.5-1409", "4.0-704"}},  {21, {"4.0-705", "4.0-889"}},   {22, {"4.0-905", "4.0-1871"}},
    {23, {"4.0-1873", "4.0-1900"}}, {24, {"4.0-1902", "4.0-1919"}}, {25, {"4.0-1921", "4.0-1936"}},
    {26, {"4.0-1939", "4.0-1959"}}, {27, {"4.0-1961", "4.0-2018"}}, {28, {"4.0-2020", "4.0-2291"}},
    {29, {"4.0-2293", "4.0-2360"}}, {30, {"4.0-2362", "4.0-2628"}}, {31, {"4.0-2632", "4.0-3331"}},
    {32, {"4.0-3334", "4.0-3340"}}, {33, {"4.0-3342", "4.0-3373"}}, {34, {"4.0-3376", "4.0-3402"}},
    {35, {"4.0-3409", "4.0-3603"}}, {36, {"4.0-3610", "4.0-4480"}}, {37, {"4.0-4484", "4.0-4943"}},
    {38, {"4.0-4963", "4.0-5267"}}, {39, {"4.0-5279", "4.0-5525"}}, {40, {"4.0-5531", "4.0-5809"}},
    {41, {"4.0-5811", "4.0-5923"}}, {42, {"4.0-5925", "4.0-5946"}}};

static constexpr bool s_use_compression = true;

// Acquired for tasks that will write state save data to the filesystem.
// This allows for later waiting on completion of said tasks when necessary.
// We want to maintain a proper order of async operations, e.g. Save, Save, GetInfoString.
[[nodiscard]] static auto GetStateSaveTaskLock()
{
  return std::shared_lock{s_state_saves_in_progress};
}

static bool ReadHeader(const std::string& filename, StateHeader& header);

static void DoState(Core::System& system, PointerWrap& p)
{
  bool is_wii = system.IsWii() || system.IsMIOS();
  const bool is_wii_currently = is_wii;
  p.Do(is_wii);
  if (is_wii != is_wii_currently)
  {
    OSD::AddMessage(fmt::format("Cannot load a savestate created under {} mode in {} mode",
                                is_wii ? "Wii" : "GC", is_wii_currently ? "Wii" : "GC"),
                    OSD::Duration::NORMAL, OSD::Color::RED);
    p.SetMeasureMode();
    return;
  }

  // Check to make sure the emulated memory sizes are the same as the savestate
  auto& memory = system.GetMemory();
  u32 state_mem1_size = memory.GetRamSizeReal();
  u32 state_mem2_size = memory.GetExRamSizeReal();
  p.Do(state_mem1_size);
  p.Do(state_mem2_size);
  if (state_mem1_size != memory.GetRamSizeReal() || state_mem2_size != memory.GetExRamSizeReal())
  {
    OSD::AddMessage(fmt::format("Memory size mismatch!\n"
                                "Current | MEM1 {:08X} ({:3}MB)    MEM2 {:08X} ({:3}MB)\n"
                                "State   | MEM1 {:08X} ({:3}MB)    MEM2 {:08X} ({:3}MB)",
                                memory.GetRamSizeReal(), memory.GetRamSizeReal() / 0x100000U,
                                memory.GetExRamSizeReal(), memory.GetExRamSizeReal() / 0x100000U,
                                state_mem1_size, state_mem1_size / 0x100000U, state_mem2_size,
                                state_mem2_size / 0x100000U));
    p.SetMeasureMode();
    return;
  }

  // Movie must be done before the video backend, because the window is redrawn in the video backend
  // state load, and the frame number must be up-to-date.
  system.GetMovie().DoState(p);
  p.DoMarker("Movie");

  // Begin with video backend, so that it gets a chance to clear its caches and writeback modified
  // things to RAM
  g_video_backend->DoState(p);
  p.DoMarker("video_backend");

  // CoreTiming needs to be restored before restoring Hardware because
  // the controller code might need to schedule an event if the controller has changed.
  system.GetCoreTiming().DoState(p);
  p.DoMarker("CoreTiming");

  // HW needs to be restored before PowerPC because the data cache might need to be flushed.
  HW::DoState(system, p);
  p.DoMarker("HW");

  system.GetPowerPC().DoState(p);
  p.DoMarker("PowerPC");

  if (system.IsWii())
    Wiimote::DoState(p);
  p.DoMarker("Wiimote");
  Gecko::DoState(p);
  p.DoMarker("Gecko");

#ifdef USE_RETRO_ACHIEVEMENTS
  AchievementManager::GetInstance().DoState(p);
#endif  // USE_RETRO_ACHIEVEMENTS
}

static bool CheckIfStateLoadIsAllowed(Core::System& system)
{
  if (!Core::IsRunningOrStarting(system))
    return false;

  if (NetPlay::IsNetPlayRunning())
  {
    OSD::AddMessage("Loading savestates is disabled in Netplay to prevent desyncs");
    return false;
  }

  if (AchievementManager::GetInstance().IsHardcoreModeActive())
  {
    OSD::AddMessage("Loading savestates is disabled in RetroAchievements hardcore mode");
    return false;
  }

  return true;
}

static bool LoadFromBuffer(Core::System& system, std::span<u8> buffer)
{
  u8* ptr = buffer.data();
  PointerWrap p(&ptr, buffer.size(), PointerWrap::Mode::Read);
  DoState(system, p);
  return p.IsReadMode();
}

// Returns the required size, or 0 on failure.
static std::size_t SaveToBuffer(Core::System& system, Common::UniqueBuffer<u8>& buffer)
{
  // Attempt to save to our provided buffer as-is.
  // If buffer isn't large enough, PointerWrap transitions to MeasureMode,
  //  and then we have our measurement for a second attempt.
  u8* ptr = buffer.data();
  PointerWrap pointer_wrap(&ptr, buffer.size(), PointerWrap::Mode::Write);
  DoState(system, pointer_wrap);
  const auto measured_size = pointer_wrap.GetOffsetFromPreviousPosition(buffer.data());

  if (pointer_wrap.IsWriteMode())
  {
    s_last_state_size = measured_size;
    return measured_size;
  }

  if (measured_size > buffer.size())
  {
    DEBUG_LOG_FMT(CORE, "SaveToBuffer: Growing buffer from size {} to measured size {}",
                  buffer.size(), measured_size);
    buffer.reset(measured_size);
    return SaveToBuffer(system, buffer);
  }

  // Buffer was large enough but we still failed for some other reason.
  return 0;
}

namespace
{
struct SlotWithTimestamp
{
  // 1-based indexing.
  int slot;
  double timestamp;
};
}  // namespace

// Returns first slot number (1-based indexing) not in the vector.
static std::optional<int> GetEmptySlot(const std::vector<SlotWithTimestamp>& used_slots)
{
  for (int i = 1; i <= int(NUM_STATES); ++i)
  {
    if (!Common::Contains(used_slots, i, &SlotWithTimestamp::slot))
      return i;
  }
  return std::nullopt;
}

// Arbitrarily chosen value (38 years) that is subtracted in GetSystemTimeAsDouble()
// to increase sub-second precision of the resulting double timestamp
static constexpr int DOUBLE_TIME_OFFSET = (38 * 365 * 24 * 60 * 60);

static double GetSystemTimeAsDouble()
{
  const auto since_epoch = std::chrono::system_clock::now().time_since_epoch();

  const auto since_double_time_epoch = since_epoch - std::chrono::seconds(DOUBLE_TIME_OFFSET);
  return std::chrono::duration_cast<std::chrono::duration<double>>(since_double_time_epoch).count();
}

static std::string SystemTimeAsDoubleToString(double time)
{
  // revert adjustments from GetSystemTimeAsDouble() to get a normal Unix timestamp again
  const time_t seconds = static_cast<time_t>(time) + DOUBLE_TIME_OFFSET;
  const auto local_time = Common::LocalTime(seconds);
  if (!local_time)
    return "";

  // fmt is locale agnostic by default, so explicitly use current locale.
  return fmt::format(std::locale{""}, "{:%x %X}", *local_time);
}

static std::string MakeStateFilename(int number)
{
  return fmt::format("{}{}.s{:02d}", File::GetUserPath(D_STATESAVES_IDX),
                     SConfig::GetInstance().GetGameID(), number);
}

static std::vector<SlotWithTimestamp> GetUsedSlotsWithTimestamp()
{
  std::vector<SlotWithTimestamp> result;
  StateHeader header;
  for (int i = 1; i <= int(NUM_STATES); ++i)
  {
    std::string filename = MakeStateFilename(i);
    if (!File::Exists(filename) || !ReadHeader(filename, header))
      continue;

    result.emplace_back(SlotWithTimestamp{.slot = i, .timestamp = header.legacy_header.time});
  }
  return result;
}

static void CompressBufferToFile(std::span<const u8> raw_buffer, File::IOFile& f)
{
  u64 total_bytes_compressed = 0;

  while (true)
  {
    const u64 bytes_left_to_compress = raw_buffer.size() - total_bytes_compressed;

    const int bytes_to_compress =
        static_cast<int>(std::min(static_cast<u64>(LZ4_MAX_INPUT_SIZE), bytes_left_to_compress));
    Common::UniqueBuffer<char> compressed_buffer(LZ4_compressBound(bytes_to_compress));
    const int compressed_len = LZ4_compress_default(
        reinterpret_cast<const char*>(raw_buffer.data()) + total_bytes_compressed,
        compressed_buffer.get(), bytes_to_compress, int(compressed_buffer.size()));

    if (compressed_len == 0)
    {
      PanicAlertFmtT("Internal LZ4 Error - compression failed");
      break;
    }

    // The size of the data to write is 'compressed_len'
    f.WriteArray(&compressed_len, 1);
    f.WriteBytes(compressed_buffer.get(), compressed_len);

    total_bytes_compressed += bytes_to_compress;
    if (total_bytes_compressed == raw_buffer.size())
      break;
  }
}

static void CreateExtendedHeader(StateExtendedHeader& extended_header, size_t uncompressed_size)
{
  StateExtendedBaseHeader& base_header = extended_header.base_header;
  base_header.header_version = EXTENDED_HEADER_VERSION;
  base_header.compression_type =
      s_use_compression ? CompressionType::LZ4 : CompressionType::Uncompressed;
  base_header.payload_offset = COMPRESSED_DATA_OFFSET;
  base_header.uncompressed_size = uncompressed_size;

  // If more fields are added to StateExtendedHeader, set them here.
}

static void WriteHeadersToFile(size_t uncompressed_size, File::IOFile& f)
{
  StateHeader header{};
  SConfig::GetInstance().GetGameID().copy(header.legacy_header.game_id,
                                          std::size(header.legacy_header.game_id));
  header.legacy_header.time = GetSystemTimeAsDouble();

  header.version_header.version_cookie = COOKIE_BASE + STATE_VERSION;
  header.version_string = Common::GetScmRevStr();
  header.version_header.version_string_length = static_cast<u32>(header.version_string.length());

  StateExtendedHeader extended_header{};
  CreateExtendedHeader(extended_header, uncompressed_size);

  f.WriteArray(&header.legacy_header, 1);
  f.WriteArray(&header.version_header, 1);
  f.WriteString(header.version_string);

  f.WriteArray(&extended_header.base_header, 1);
  // If StateExtendedHeader is amended to include more than the base, add WriteBytes() calls here.
}

static void CompressAndDumpState(Core::System& system, const CompressAndDumpStateArgs& save_args)
{
  const auto& buffer = save_args.buffer;
  const std::string& filename = save_args.filename;

  // Find free temporary filename.

  // TODO: The file exists check and the actual opening of the file should be atomic.
  // This is only an issue for multiple instances of dolphin operating on the same user folder.
  std::string temp_filename;
  auto temp_counter = static_cast<size_t>(Common::CurrentThreadId());
  do
  {
    temp_filename = fmt::format("{}{}.tmp", filename, temp_counter);
    ++temp_counter;
  } while (File::Exists(temp_filename));

  File::IOFile f(temp_filename, "wb");
  if (!f)
  {
    Core::DisplayMessage("Failed to create state file", 2000);
    return;
  }

  WriteHeadersToFile(buffer.size(), f);

  if (s_use_compression)
    CompressBufferToFile(buffer, f);
  else
    f.WriteBytes(buffer.data(), buffer.size());

  if (!f.IsGood())
    Core::DisplayMessage("Failed to write state file", 2000);

  const std::string last_state_filename = File::GetUserPath(D_STATESAVES_IDX) + "lastState.sav";
  const std::string last_state_dtmname = last_state_filename + ".dtm";
  const std::string dtmname = filename + ".dtm";

  // Backup existing state (overwriting an existing backup, if any).
  if (File::Exists(filename))
  {
    if (File::Exists(last_state_filename))
      File::Delete((last_state_filename));
    if (File::Exists(last_state_dtmname))
      File::Delete((last_state_dtmname));

    if (!File::Rename(filename, last_state_filename))
    {
      Core::DisplayMessage("Failed to move previous state to state undo backup", 1000);
    }
    else if (File::Exists(dtmname))
    {
      if (!File::Rename(dtmname, last_state_dtmname))
        Core::DisplayMessage("Failed to move previous state's dtm to state undo backup", 1000);
    }
  }

  auto& movie = system.GetMovie();
  if ((movie.IsMovieActive()) && !movie.IsJustStartingRecordingInputFromSaveState())
    movie.SaveRecording(dtmname);
  else if (!movie.IsMovieActive())
    File::Delete(dtmname);

  // Move written state to final location.
  // TODO: This should also be atomic. This is possible on all systems, but needs a special
  // implementation of IOFile on Windows.
  if (!f.Close())
    Core::DisplayMessage("Failed to close state file", 2000);

  if (!File::Rename(temp_filename, filename))
  {
    Core::DisplayMessage("Failed to rename state file", 2000);
  }
  else
  {
    const std::filesystem::path temp_path(filename);
    Core::DisplayMessage(fmt::format("Saved State to {}", temp_path.filename().string()), 2000);
  }
}

static void SaveAsFromCore(Core::System& system, std::string filename)
{
  // Try with a buffer a bit larger than the previous state.
  // This will often avoid the "Measure" step.
  const auto buffer_size_estimate = std::size_t(s_last_state_size) * 110 / 100;
  Common::UniqueBuffer<u8> buffer{buffer_size_estimate};

  if (const auto actual_size = SaveToBuffer(system, buffer))
  {
    // Adjust the oversized buffer down to the actual size.
    buffer.assign(buffer.extract().first, actual_size);

    CompressAndDumpStateArgs dump_args{
        .buffer = std::move(buffer),
        .filename = std::move(filename),
        .task_lock = GetStateSaveTaskLock(),
    };
    Core::DisplayMessage("Saving State...", 1000);
    s_compress_and_dump_thread.EmplaceItem(std::move(dump_args));
  }
  else
  {
    Core::DisplayMessage("Unable to save: Internal DoState Error", 4000);
  }
}

void SaveAs(Core::System& system, std::string filename)
{
  Core::RunOnCPUThread(
      system, [&system, filename = std::move(filename), lock = GetStateSaveTaskLock()]() mutable {
        SaveAsFromCore(system, std::move(filename));
      });
}

static bool GetVersionFromLZO(StateHeader& header, File::IOFile& f)
{
  // Just read the first block, since it will contain the full revision string
  lzo_uint32 cur_len = 0;  // size of compressed bytes
  lzo_uint new_len = 0;    // size of uncompressed bytes
  Common::UniqueBuffer<u8> buffer(header.legacy_header.lzo_size);

  if (!f.ReadArray(&cur_len, 1) || !f.ReadBytes(out, cur_len))
    return false;

  const int res = lzo1x_decompress(out, cur_len, buffer.data(), &new_len, nullptr);
  if (res != LZO_E_OK)
  {
    // This doesn't seem to happen anymore.
    PanicAlertFmtT("Internal LZO Error - decompression failed ({0}) ({1}) \n"
                   "Unable to retrieve outdated savestate version info.",
                   res, new_len);
    return false;
  }

  // Read in cookie and string length
  if (buffer.size() >= sizeof(StateHeaderVersion))
  {
    memcpy(&header.version_header, buffer.data(), sizeof(StateHeaderVersion));
  }
  else
  {
    PanicAlertFmtT("Internal LZO Error - failed to parse decompressed version cookie and version "
                   "string length ({0})",
                   buffer.size());
    return false;
  }

  // Read in the string
  if (buffer.size() >= sizeof(StateHeaderVersion) + header.version_header.version_string_length)
  {
    header.version_string.assign(
        reinterpret_cast<char*>(buffer.data() + sizeof(StateHeaderVersion)),
        header.version_header.version_string_length);
  }
  else
  {
    PanicAlertFmtT("Internal LZO Error - failed to parse decompressed version string ({0} / {1})",
                   header.version_header.version_string_length, buffer.size());
    return false;
  }

  return true;
}

static bool ReadStateHeaderFromFile(StateHeader& header, File::IOFile& f,
                                    bool get_version_header = true)
{
  if (!f.IsOpen())
  {
    Core::DisplayMessage("State not found", 2000);
    return false;
  }

  if (!f.ReadArray(&header.legacy_header, 1))
  {
    Core::DisplayMessage("Failed to read state legacy header", 2000);
    return false;
  }

  // Bail out if we only care for retrieving the legacy header.
  // This is the case with ReadHeader() calls.
  if (!get_version_header)
    return true;

  if (header.legacy_header.lzo_size != 0)
  {
    // Parse out version from legacy LZO compressed states
    if (!GetVersionFromLZO(header, f))
      return false;
  }
  else
  {
    if (!f.ReadArray(&header.version_header, 1))
    {
      Core::DisplayMessage("Failed to read state version header", 2000);
      return false;
    }

    std::string version_buffer(header.version_header.version_string_length, '\0');
    if (!f.ReadBytes(version_buffer.data(), version_buffer.size()))
    {
      Core::DisplayMessage("Failed to read state version string", 2000);
      return false;
    }

    header.version_string = std::move(version_buffer);
  }

  return true;
}

static bool ReadHeader(const std::string& filename, StateHeader& header)
{
  File::IOFile f(filename, "rb");
  constexpr bool get_version_header = false;
  return ReadStateHeaderFromFile(header, f, get_version_header);
}

std::string GetInfoStringOfSlot(int slot, bool translate)
{
  std::lock_guard lk{s_state_saves_in_progress};

  std::string filename = MakeStateFilename(slot);
  if (!File::Exists(filename))
    return translate ? Common::GetStringT("Empty") : "Empty";

  State::StateHeader header;
  if (!ReadHeader(filename, header))
    return translate ? Common::GetStringT("Unknown") : "Unknown";

  return SystemTimeAsDoubleToString(header.legacy_header.time);
}

u64 GetUnixTimeOfSlot(int slot)
{
  std::lock_guard lk{s_state_saves_in_progress};

  State::StateHeader header;
  if (!ReadHeader(MakeStateFilename(slot), header))
    return 0;

  constexpr u64 MS_PER_SEC = 1000;
  return static_cast<u64>(header.legacy_header.time * MS_PER_SEC) +
         (DOUBLE_TIME_OFFSET * MS_PER_SEC);
}

static bool DecompressLZ4(Common::UniqueBuffer<u8>& raw_buffer, u64 size, File::IOFile& f)
{
  raw_buffer.reset(size);

  u64 total_bytes_read = 0;
  while (true)
  {
    s32 compressed_data_len = 0;
    if (!f.ReadArray(&compressed_data_len, 1))
    {
      PanicAlertFmt("Could not read state data length");
      return false;
    }

    if (compressed_data_len <= 0)
    {
      PanicAlertFmtT("Internal LZ4 Error - Tried decompressing {0} bytes", compressed_data_len);
      return false;
    }

    Common::UniqueBuffer<char> compressed_data(compressed_data_len);
    if (!f.ReadBytes(compressed_data.get(), compressed_data_len))
    {
      PanicAlertFmt("Could not read state data");
      return false;
    }

    const auto max_decompress_size =
        static_cast<int>(std::min((u64)LZ4_MAX_INPUT_SIZE, size - total_bytes_read));

    int bytes_read = LZ4_decompress_safe(
        compressed_data.get(), reinterpret_cast<char*>(raw_buffer.data()) + total_bytes_read,
        compressed_data_len, max_decompress_size);

    if (bytes_read < 0)
    {
      PanicAlertFmtT("Internal LZ4 Error - decompression failed ({0}, {1}, {2})", bytes_read,
                     compressed_data_len, max_decompress_size);
      return false;
    }

    total_bytes_read += static_cast<u64>(bytes_read);

    if (total_bytes_read == size)
    {
      return true;
    }

    if (total_bytes_read > size)
    {
      PanicAlertFmtT("Internal LZ4 Error - payload size mismatch ({0} / {1}))", total_bytes_read,
                     size);
      return false;
    }
  }
}

static bool ValidateHeaders(const StateHeader& header)
{
  bool success = true;

  // Game ID
  if (strncmp(SConfig::GetInstance().GetGameID().c_str(), header.legacy_header.game_id, 6) != 0)
  {
    Core::DisplayMessage(fmt::format("State belongs to a different game (ID {})",
                                     std::string_view{header.legacy_header.game_id,
                                                      std::size(header.legacy_header.game_id)}),
                         2000);
    return false;
  }

  // Check the state version.
  // FYI: We don't require an exact revision string match.
  std::string loaded_str = header.version_string;
  const u32 loaded_version = header.version_header.version_cookie - COOKIE_BASE;

  if (const auto it = s_old_versions.find(loaded_version); it != s_old_versions.end())
  {
    // This is a REALLY old version, before we started writing the version string to file
    success = false;

    std::pair<std::string, std::string> version_range = it->second;
    std::string oldest_version = version_range.first;
    std::string newest_version = version_range.second;

    loaded_str = "Dolphin " + oldest_version + " - " + newest_version;
  }
  else if (loaded_version != STATE_VERSION)
  {
    success = false;
  }

  if (!success)
  {
    const std::string message =
        loaded_str.empty() ?
            "This savestate was created using an incompatible version of Dolphin" :
            "This savestate was created using the incompatible version " + loaded_str;
    Core::DisplayMessage(message, OSD::Duration::NORMAL);
  }

  return success;
}

static void LoadFileStateData(const std::string& filename, Common::UniqueBuffer<u8>& ret_data)
{
  File::IOFile f;
  f.Open(filename, "rb");

  StateHeader header;
  if (!ReadStateHeaderFromFile(header, f) || !ValidateHeaders(header))
    return;

  StateExtendedHeader extended_header;
  if (!f.ReadArray(&extended_header.base_header, 1))
  {
    PanicAlertFmt("Unable to read state header");
    return;
  }
  // If StateExtendedHeader is amended to include more than the base, add ReadBytes() calls here.

  if (extended_header.base_header.header_version != EXTENDED_HEADER_VERSION)
  {
    PanicAlertFmt("State header corrupted");
    return;
  }

  Common::UniqueBuffer<u8> buffer;

  switch (extended_header.base_header.compression_type)
  {
  case CompressionType::LZ4:
  {
    Core::DisplayMessage("Decompressing State...", OSD::Duration::SHORT);
    if (!DecompressLZ4(buffer, extended_header.base_header.uncompressed_size, f))
      return;

    break;
  }
  case CompressionType::Uncompressed:
  {
    u64 header_len = sizeof(StateHeaderLegacy) + sizeof(StateHeaderVersion) +
                     header.version_header.version_string_length + sizeof(StateExtendedBaseHeader) +
                     extended_header.base_header.payload_offset;

    u64 file_size = f.GetSize();
    if (file_size < header_len)
    {
      PanicAlertFmt("State header length corrupted");
      return;
    }

    const auto size = static_cast<size_t>(file_size - header_len);
    buffer.reset(size);

    if (!f.ReadBytes(buffer.data(), size))
    {
      PanicAlertFmt("Error reading bytes: {0}", size);
      return;
    }
    break;
  }
  default:
    PanicAlertFmt("Unknown compression type {0}", extended_header.base_header.compression_type);
    return;
  }

  // all good
  ret_data.swap(buffer);
}

static void LoadAsFromCore(Core::System& system, std::string filename)
{
  // Ensure all data has reached the filesystem before trying to use it.
  s_compress_and_dump_thread.WaitForCompletion();

  // Save temp buffer for undo load state
  auto& movie = system.GetMovie();
  if (!movie.IsJustStartingRecordingInputFromSaveState())
  {
    SaveToBuffer(system, s_undo_load_buffer);
    const std::string dtmpath = File::GetUserPath(D_STATESAVES_IDX) + "undo.dtm";
    if (movie.IsMovieActive())
      movie.SaveRecording(dtmpath);
    else if (File::Exists(dtmpath))
      File::Delete(dtmpath);
  }

  bool was_file_read = false;
  bool loaded_successfully = false;

  // brackets here are so buffer gets freed ASAP
  {
    Common::UniqueBuffer<u8> buffer;
    LoadFileStateData(filename, buffer);

    if (!buffer.empty())
    {
      was_file_read = true;
      loaded_successfully = LoadFromBuffer(system, buffer);
    }
  }

  if (was_file_read)
  {
    if (loaded_successfully)
    {
      std::filesystem::path temp_filename(std::move(filename));
      Core::DisplayMessage(fmt::format("Loaded State from {}", temp_filename.filename().string()),
                           2000);
      if (File::Exists(filename + ".dtm"))
      {
        movie.LoadInput(filename + ".dtm");
      }
      else if (!movie.IsJustStartingRecordingInputFromSaveState() &&
               !movie.IsJustStartingPlayingInputFromSaveState())
      {
        movie.EndPlayInput(false);
      }
    }
    else
    {
      Core::DisplayMessage("The savestate could not be loaded", OSD::Duration::NORMAL);

      // since we could be in an inconsistent state now (and might crash or whatever), undo.
      UndoLoadState(system);
    }
  }

  if (s_on_after_load_callback)
    s_on_after_load_callback();
}

void LoadAs(Core::System& system, std::string filename)
{
  if (!CheckIfStateLoadIsAllowed(system))
    return;

  Core::RunOnCPUThread(system, [&system, filename = std::move(filename)]() mutable {
    LoadAsFromCore(system, std::move(filename));
  });
}

void SetOnAfterLoadCallback(AfterLoadCallbackFunc callback)
{
  s_on_after_load_callback = std::move(callback);
}

void Init(Core::System& system)
{
  s_compress_and_dump_thread.Reset("Savestate Worker",
                                   std::bind_front(&CompressAndDumpState, std::ref(system)));

  s_flush_unsaved_data_hook = UICommon::AddFlushUnsavedDataCallback([] {
    // Holding the lock for any amount of time means there are no pending state save tasks.
    std::lock_guard lk{s_state_saves_in_progress};
  });
}

void Shutdown()
{
  s_compress_and_dump_thread.Shutdown();
  s_undo_load_buffer.reset();
  s_flush_unsaved_data_hook.reset();
}

void Save(Core::System& system, int slot)
{
  SaveAs(system, MakeStateFilename(slot));
}

void Load(Core::System& system, int slot)
{
  LoadAs(system, MakeStateFilename(slot));
}

void LoadLastSaved(Core::System& system, int i)
{
  if (!CheckIfStateLoadIsAllowed(system))
    return;

  Core::RunOnCPUThread(system, [&system, i] {
    // Data must reach the filesystem for up to date "UsedSlots".
    s_compress_and_dump_thread.WaitForCompletion();

    std::vector<SlotWithTimestamp> used_slots = GetUsedSlotsWithTimestamp();
    if (std::size_t(i) > used_slots.size())
    {
      Core::DisplayMessage("State doesn't exist", 2000);
      return;
    }

    std::ranges::stable_sort(used_slots, std::ranges::greater{}, &SlotWithTimestamp::timestamp);
    LoadAsFromCore(system, MakeStateFilename(used_slots[i].slot));
  });
}

void SaveFirstSaved(Core::System& system)
{
  Core::RunOnCPUThread(system, [&system, lock = GetStateSaveTaskLock()] {
    // Data must reach the filesystem for up to date "UsedSlots".
    s_compress_and_dump_thread.WaitForCompletion();

    std::vector<SlotWithTimestamp> used_slots = GetUsedSlotsWithTimestamp();
    auto slot = GetEmptySlot(used_slots);
    if (!slot.has_value())
    {
      // overwrite the oldest state
      std::ranges::stable_sort(used_slots, {}, &SlotWithTimestamp::timestamp);
      slot = used_slots.front().slot;
    }

    SaveAsFromCore(system, MakeStateFilename(*slot));
  });
}

// Load the last state before loading the state
void UndoLoadState(Core::System& system)
{
  if (!CheckIfStateLoadIsAllowed(system))
    return;

  Core::RunOnCPUThread(system, [&system] {
    if (s_undo_load_buffer.empty())
    {
      PanicAlertFmtT("There is nothing to undo!");
      return;
    }

    auto& movie = system.GetMovie();
    if (movie.IsMovieActive())
    {
      // Note: Only the CPU thread writes to "undo.dtm".
      const std::string dtmpath = File::GetUserPath(D_STATESAVES_IDX) + "undo.dtm";
      if (File::Exists(dtmpath))
      {
        LoadFromBuffer(system, s_undo_load_buffer);
        movie.LoadInput(dtmpath);
      }
      else
      {
        PanicAlertFmtT("No undo.dtm found, aborting undo load state to prevent movie desyncs");
      }
    }
    else
    {
      LoadFromBuffer(system, s_undo_load_buffer);
    }
  });
}

// Load the state that the last save state overwritten on
void UndoSaveState(Core::System& system)
{
  LoadAs(system, File::GetUserPath(D_STATESAVES_IDX) + "lastState.sav");
}

}  // namespace State
