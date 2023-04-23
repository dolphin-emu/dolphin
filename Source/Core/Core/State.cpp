// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/State.h"

#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <fmt/format.h>

#include <lzo/lzo1x.h>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Event.h"
#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Common/MsgHandler.h"
#include "Common/Thread.h"
#include "Common/Timer.h"
#include "Common/Version.h"
#include "Common/WorkQueueThread.h"

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/GeckoCode.h"
#include "Core/HW/HW.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/Wiimote.h"
#include "Core/Host.h"
#include "Core/Movie.h"
#include "Core/NetPlayClient.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

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

#define HEAP_ALLOC(var, size)                                                                      \
  lzo_align_t __LZO_MMODEL var[((size) + (sizeof(lzo_align_t) - 1)) / sizeof(lzo_align_t)]

static HEAP_ALLOC(wrkmem, LZO1X_1_MEM_COMPRESS);

static AfterLoadCallbackFunc s_on_after_load_callback;

// Temporary undo state buffer
static std::vector<u8> s_undo_load_buffer;
static std::mutex s_undo_load_buffer_mutex;

static std::mutex s_load_or_save_in_progress_mutex;

struct CompressAndDumpState_args
{
  std::vector<u8> buffer_vector;
  std::string filename;
  std::shared_ptr<Common::Event> state_write_done_event;
};

// Protects against simultaneous reads and writes to the final savestate location from multiple
// threads.
static std::mutex s_save_thread_mutex;

// Queue for compressing and writing savestates to disk.
static Common::WorkQueueThread<CompressAndDumpState_args> s_save_thread;

// Keeps track of savestate writes that are currently happening, so we don't load a state while
// another one is still saving. This is particularly important so if you save to a slot and then
// immediately load from the same one, you don't accidentally load the state that's still at that
// file path before the write is done.
static std::mutex s_state_writes_in_queue_mutex;
static size_t s_state_writes_in_queue;
static std::condition_variable s_state_write_queue_is_empty;

// Don't forget to increase this after doing changes on the savestate system
constexpr u32 STATE_VERSION = 162;  // Last changed in PR 11767

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

enum
{
  STATE_NONE = 0,
  STATE_SAVE = 1,
  STATE_LOAD = 2,
};

static bool s_use_compression = true;

void EnableCompression(bool compression)
{
  s_use_compression = compression;
}

// Returns true if state version matches current Dolphin state version, false otherwise.
static bool DoStateVersion(PointerWrap& p, std::string* version_created_by)
{
  u32 version = STATE_VERSION;
  {
    static const u32 COOKIE_BASE = 0xBAADBABE;
    u32 cookie = version + COOKIE_BASE;
    p.Do(cookie);
    version = cookie - COOKIE_BASE;
  }

  *version_created_by = Common::GetScmRevStr();
  if (version > 42)
    p.Do(*version_created_by);
  else
    version_created_by->clear();

  if (version != STATE_VERSION)
  {
    if (version_created_by->empty() && s_old_versions.count(version))
    {
      // The savestate is from an old version that doesn't
      // save the Dolphin version number to savestates, but
      // by looking up the savestate version number, it is possible
      // to know approximately which Dolphin version was used.

      std::pair<std::string, std::string> version_range = s_old_versions.find(version)->second;
      std::string oldest_version = version_range.first;
      std::string newest_version = version_range.second;

      *version_created_by = "Dolphin " + oldest_version + " - " + newest_version;
    }

    return false;
  }

  p.DoMarker("Version");
  return true;
}

static void DoState(PointerWrap& p)
{
  std::string version_created_by;
  if (!DoStateVersion(p, &version_created_by))
  {
    const std::string message =
        version_created_by.empty() ?
            "This savestate was created using an incompatible version of Dolphin" :
            "This savestate was created using the incompatible version " + version_created_by;
    Core::DisplayMessage(message, OSD::Duration::NORMAL);
    p.SetMeasureMode();
    return;
  }

  bool is_wii = SConfig::GetInstance().bWii || SConfig::GetInstance().m_is_mios;
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
  auto& system = Core::System::GetInstance();
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
  Movie::DoState(p);
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

  if (SConfig::GetInstance().bWii)
    Wiimote::DoState(p);
  p.DoMarker("Wiimote");
  Gecko::DoState(p);
  p.DoMarker("Gecko");
}

void LoadFromBuffer(std::vector<u8>& buffer)
{
  if (NetPlay::IsNetPlayRunning())
  {
    OSD::AddMessage("Loading savestates is disabled in Netplay to prevent desyncs");
    return;
  }

  Core::RunOnCPUThread(
      [&] {
        u8* ptr = buffer.data();
        PointerWrap p(&ptr, buffer.size(), PointerWrap::Mode::Read);
        DoState(p);
      },
      true);
}

void SaveToBuffer(std::vector<u8>& buffer)
{
  Core::RunOnCPUThread(
      [&] {
        u8* ptr = nullptr;
        PointerWrap p_measure(&ptr, 0, PointerWrap::Mode::Measure);

        DoState(p_measure);
        const size_t buffer_size = reinterpret_cast<size_t>(ptr);
        buffer.resize(buffer_size);

        ptr = buffer.data();
        PointerWrap p(&ptr, buffer_size, PointerWrap::Mode::Write);
        DoState(p);
      },
      true);
}

// return state number not in map
static int GetEmptySlot(std::map<double, int> m)
{
  for (int i = 1; i <= (int)NUM_STATES; i++)
  {
    bool found = false;
    for (auto& p : m)
    {
      if (p.second == i)
      {
        found = true;
        break;
      }
    }
    if (!found)
      return i;
  }
  return -1;
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
  time_t seconds = (time_t)time + DOUBLE_TIME_OFFSET;
  tm* localTime = localtime(&seconds);

#ifdef _WIN32
  wchar_t tmp[32] = {};
  wcsftime(tmp, std::size(tmp), L"%x %X", localTime);
  return WStringToUTF8(tmp);
#else
  char tmp[32] = {};
  strftime(tmp, sizeof(tmp), "%x %X", localTime);
  return tmp;
#endif
}

static std::string MakeStateFilename(int number);

// read state timestamps
static std::map<double, int> GetSavedStates()
{
  StateHeader header;
  std::map<double, int> m;
  for (int i = 1; i <= (int)NUM_STATES; i++)
  {
    std::string filename = MakeStateFilename(i);
    if (File::Exists(filename))
    {
      if (ReadHeader(filename, header))
      {
        double d = GetSystemTimeAsDouble() - header.time;

        // increase time until unique value is obtained
        while (m.find(d) != m.end())
          d += .001;

        m.emplace(d, i);
      }
    }
  }
  return m;
}

static void CompressAndDumpState(CompressAndDumpState_args& save_args)
{
  const u8* const buffer_data = save_args.buffer_vector.data();
  const size_t buffer_size = save_args.buffer_vector.size();
  const std::string& filename = save_args.filename;

  // Find free temporary filename.
  // TODO: The file exists check and the actual opening of the file should be atomic, we don't have
  // functions for that.
  std::string temp_filename;
  size_t temp_counter = static_cast<size_t>(Common::CurrentThreadId());
  do
  {
    temp_filename = fmt::format("{}{}.tmp", filename, temp_counter);
    ++temp_counter;
  } while (File::Exists(temp_filename));

  File::IOFile f(temp_filename, "wb");
  if (!f)
  {
    Core::DisplayMessage("Could not save state", 2000);
    return;
  }

  // Setting up the header
  StateHeader header{};
  SConfig::GetInstance().GetGameID().copy(header.gameID, std::size(header.gameID));
  header.size = s_use_compression ? (u32)buffer_size : 0;
  header.time = GetSystemTimeAsDouble();

  f.WriteArray(&header, 1);

  if (header.size != 0)  // non-zero header size means the state is compressed
  {
    lzo_uint i = 0;
    while (true)
    {
      lzo_uint32 cur_len = 0;
      lzo_uint out_len = 0;

      if ((i + IN_LEN) >= buffer_size)
      {
        cur_len = (lzo_uint32)(buffer_size - i);
      }
      else
      {
        cur_len = IN_LEN;
      }

      if (lzo1x_1_compress(buffer_data + i, cur_len, out, &out_len, wrkmem) != LZO_E_OK)
        PanicAlertFmtT("Internal LZO Error - compression failed");

      // The size of the data to write is 'out_len'
      f.WriteArray((lzo_uint32*)&out_len, 1);
      f.WriteBytes(out, out_len);

      if (cur_len != IN_LEN)
        break;

      i += cur_len;
    }
  }
  else  // uncompressed
  {
    f.WriteBytes(buffer_data, buffer_size);
  }

  const std::string last_state_filename = File::GetUserPath(D_STATESAVES_IDX) + "lastState.sav";
  const std::string last_state_dtmname = last_state_filename + ".dtm";
  const std::string dtmname = filename + ".dtm";

  {
    std::lock_guard lk(s_save_thread_mutex);

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

    if ((Movie::IsMovieActive()) && !Movie::IsJustStartingRecordingInputFromSaveState())
      Movie::SaveRecording(dtmname);
    else if (!Movie::IsMovieActive())
      File::Delete(dtmname);

    // Move written state to final location.
    // TODO: This should also be atomic. This is possible on all systems, but needs a special
    // implementation of IOFile on Windows.
    f.Close();
    File::Rename(temp_filename, filename);
  }

  std::filesystem::path tempfilename(filename);
  Core::DisplayMessage(fmt::format("Saved State to {}", tempfilename.filename().string()), 2000);
  Host_UpdateMainFrame();
}

void SaveAs(const std::string& filename, bool wait)
{
  std::unique_lock lk(s_load_or_save_in_progress_mutex, std::try_to_lock);
  if (!lk)
    return;

  Core::RunOnCPUThread(
      [&] {
        {
          std::lock_guard lk_(s_state_writes_in_queue_mutex);
          ++s_state_writes_in_queue;
        }

        // Measure the size of the buffer.
        u8* ptr = nullptr;
        PointerWrap p_measure(&ptr, 0, PointerWrap::Mode::Measure);
        DoState(p_measure);
        const size_t buffer_size = reinterpret_cast<size_t>(ptr);

        // Then actually do the write.
        std::vector<u8> current_buffer;
        current_buffer.resize(buffer_size);
        ptr = current_buffer.data();
        PointerWrap p(&ptr, buffer_size, PointerWrap::Mode::Write);
        DoState(p);

        if (p.IsWriteMode())
        {
          Core::DisplayMessage("Saving State...", 1000);

          std::shared_ptr<Common::Event> sync_event;

          CompressAndDumpState_args save_args;
          save_args.buffer_vector = std::move(current_buffer);
          save_args.filename = filename;
          if (wait)
          {
            sync_event = std::make_shared<Common::Event>();
            save_args.state_write_done_event = sync_event;
          }

          s_save_thread.EmplaceItem(std::move(save_args));

          if (sync_event)
            sync_event->Wait();
        }
        else
        {
          // someone aborted the save by changing the mode?
          {
            // Note: The worker thread takes care of this in the other branch.
            std::lock_guard lk_(s_state_writes_in_queue_mutex);
            if (--s_state_writes_in_queue == 0)
              s_state_write_queue_is_empty.notify_all();
          }
          Core::DisplayMessage("Unable to save: Internal DoState Error", 4000);
        }
      },
      true);
}

bool ReadHeader(const std::string& filename, StateHeader& header)
{
  // ensure that the savestate write thread isn't moving around states while we do this
  std::lock_guard lk(s_save_thread_mutex);

  File::IOFile f(filename, "rb");
  return f.ReadArray(&header, 1);
}

std::string GetInfoStringOfSlot(int slot, bool translate)
{
  std::string filename = MakeStateFilename(slot);
  if (!File::Exists(filename))
    return translate ? Common::GetStringT("Empty") : "Empty";

  State::StateHeader header;
  if (!ReadHeader(filename, header))
    return translate ? Common::GetStringT("Unknown") : "Unknown";

  return SystemTimeAsDoubleToString(header.time);
}

u64 GetUnixTimeOfSlot(int slot)
{
  State::StateHeader header;
  if (!ReadHeader(MakeStateFilename(slot), header))
    return 0;

  constexpr u64 MS_PER_SEC = 1000;
  return static_cast<u64>(header.time * MS_PER_SEC) + (DOUBLE_TIME_OFFSET * MS_PER_SEC);
}

static void LoadFileStateData(const std::string& filename, std::vector<u8>& ret_data)
{
  File::IOFile f;

  {
    // If a state is currently saving, wait for that to end or time out.
    std::unique_lock lk(s_state_writes_in_queue_mutex);
    if (s_state_writes_in_queue != 0)
    {
      if (!s_state_write_queue_is_empty.wait_for(lk, std::chrono::seconds(3),
                                                 []() { return s_state_writes_in_queue == 0; }))
      {
        Core::DisplayMessage(
            "A previous state saving operation is still in progress, cancelling load.", 2000);
        return;
      }
    }
    f.Open(filename, "rb");
  }

  StateHeader header;
  if (!f.ReadArray(&header, 1))
  {
    Core::DisplayMessage("State not found", 2000);
    return;
  }

  if (strncmp(SConfig::GetInstance().GetGameID().c_str(), header.gameID, 6))
  {
    Core::DisplayMessage(fmt::format("State belongs to a different game (ID {})",
                                     std::string_view{header.gameID, std::size(header.gameID)}),
                         2000);
    return;
  }

  std::vector<u8> buffer;

  if (header.size != 0)  // non-zero size means the state is compressed
  {
    Core::DisplayMessage("Decompressing State...", 500);

    buffer.resize(header.size);

    lzo_uint i = 0;
    while (true)
    {
      lzo_uint32 cur_len = 0;  // number of bytes to read
      lzo_uint new_len = 0;    // number of bytes to write

      if (!f.ReadArray(&cur_len, 1))
        break;

      f.ReadBytes(out, cur_len);
      const int res = lzo1x_decompress(out, cur_len, &buffer[i], &new_len, nullptr);
      if (res != LZO_E_OK)
      {
        // This doesn't seem to happen anymore.
        PanicAlertFmtT("Internal LZO Error - decompression failed ({0}) ({1}, {2}) \n"
                       "Try loading the state again",
                       res, i, new_len);
        return;
      }

      i += new_len;
    }
  }
  else  // uncompressed
  {
    const auto size = static_cast<size_t>(f.GetSize() - sizeof(StateHeader));
    buffer.resize(size);

    if (!f.ReadBytes(&buffer[0], size))
    {
      PanicAlertFmt("Error reading bytes: {0}", size);
      return;
    }
  }

  // all good
  ret_data.swap(buffer);
}

void LoadAs(const std::string& filename)
{
  if (!Core::IsRunning())
    return;

  if (NetPlay::IsNetPlayRunning())
  {
    OSD::AddMessage("Loading savestates is disabled in Netplay to prevent desyncs");
    return;
  }

  std::unique_lock lk(s_load_or_save_in_progress_mutex, std::try_to_lock);
  if (!lk)
    return;

  Core::RunOnCPUThread(
      [&] {
        // Save temp buffer for undo load state
        if (!Movie::IsJustStartingRecordingInputFromSaveState())
        {
          std::lock_guard lk2(s_undo_load_buffer_mutex);
          SaveToBuffer(s_undo_load_buffer);
          const std::string dtmpath = File::GetUserPath(D_STATESAVES_IDX) + "undo.dtm";
          if (Movie::IsMovieActive())
            Movie::SaveRecording(dtmpath);
          else if (File::Exists(dtmpath))
            File::Delete(dtmpath);
        }

        bool loaded = false;
        bool loadedSuccessfully = false;

        // brackets here are so buffer gets freed ASAP
        {
          std::vector<u8> buffer;
          LoadFileStateData(filename, buffer);

          if (!buffer.empty())
          {
            u8* ptr = buffer.data();
            PointerWrap p(&ptr, buffer.size(), PointerWrap::Mode::Read);
            DoState(p);
            loaded = true;
            loadedSuccessfully = p.IsReadMode();
          }
        }

        if (loaded)
        {
          if (loadedSuccessfully)
          {
            std::filesystem::path tempfilename(filename);
            Core::DisplayMessage(
                fmt::format("Loaded State from {}", tempfilename.filename().string()), 2000);
            if (File::Exists(filename + ".dtm"))
              Movie::LoadInput(filename + ".dtm");
            else if (!Movie::IsJustStartingRecordingInputFromSaveState() &&
                     !Movie::IsJustStartingPlayingInputFromSaveState())
              Movie::EndPlayInput(false);
          }
          else
          {
            Core::DisplayMessage("The savestate could not be loaded", OSD::Duration::NORMAL);

            // since we could be in an inconsistent state now (and might crash or whatever), undo.
            UndoLoadState();
          }
        }

        if (s_on_after_load_callback)
          s_on_after_load_callback();
      },
      true);
}

void SetOnAfterLoadCallback(AfterLoadCallbackFunc callback)
{
  s_on_after_load_callback = std::move(callback);
}

void Init()
{
  if (lzo_init() != LZO_E_OK)
    PanicAlertFmtT("Internal LZO Error - lzo_init() failed");

  s_save_thread.Reset("Savestate Worker", [](CompressAndDumpState_args args) {
    CompressAndDumpState(args);

    {
      std::lock_guard lk(s_state_writes_in_queue_mutex);
      if (--s_state_writes_in_queue == 0)
        s_state_write_queue_is_empty.notify_all();
    }

    if (args.state_write_done_event)
      args.state_write_done_event->Set();
  });
}

void Shutdown()
{
  s_save_thread.Shutdown();

  // swapping with an empty vector, rather than clear()ing
  // this gives a better guarantee to free the allocated memory right NOW (as opposed to, actually,
  // never)
  {
    std::lock_guard lk(s_undo_load_buffer_mutex);
    std::vector<u8>().swap(s_undo_load_buffer);
  }
}

static std::string MakeStateFilename(int number)
{
  return fmt::format("{}{}.s{:02d}", File::GetUserPath(D_STATESAVES_IDX),
                     SConfig::GetInstance().GetGameID(), number);
}

void Save(int slot, bool wait)
{
  SaveAs(MakeStateFilename(slot), wait);
}

void Load(int slot)
{
  LoadAs(MakeStateFilename(slot));
}

void LoadLastSaved(int i)
{
  std::map<double, int> savedStates = GetSavedStates();

  if (i > (int)savedStates.size())
    Core::DisplayMessage("State doesn't exist", 2000);
  else
  {
    std::map<double, int>::iterator it = savedStates.begin();
    std::advance(it, i - 1);
    Load(it->second);
  }
}

// must wait for state to be written because it must know if all slots are taken
void SaveFirstSaved()
{
  std::map<double, int> savedStates = GetSavedStates();

  // save to an empty slot
  if (savedStates.size() < NUM_STATES)
    Save(GetEmptySlot(savedStates), true);
  // overwrite the oldest state
  else
  {
    std::map<double, int>::iterator it = savedStates.begin();
    std::advance(it, savedStates.size() - 1);
    Save(it->second, true);
  }
}

// Load the last state before loading the state
void UndoLoadState()
{
  std::lock_guard lk(s_undo_load_buffer_mutex);
  if (!s_undo_load_buffer.empty())
  {
    if (Movie::IsMovieActive())
    {
      const std::string dtmpath = File::GetUserPath(D_STATESAVES_IDX) + "undo.dtm";
      if (File::Exists(dtmpath))
      {
        LoadFromBuffer(s_undo_load_buffer);
        Movie::LoadInput(dtmpath);
      }
      else
      {
        PanicAlertFmtT("No undo.dtm found, aborting undo load state to prevent movie desyncs");
      }
    }
    else
    {
      LoadFromBuffer(s_undo_load_buffer);
    }
  }
  else
  {
    PanicAlertFmtT("There is nothing to undo!");
  }
}

// Load the state that the last save state overwritten on
void UndoSaveState()
{
  LoadAs(File::GetUserPath(D_STATESAVES_IDX) + "lastState.sav");
}

}  // namespace State
