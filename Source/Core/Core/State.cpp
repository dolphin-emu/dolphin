// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <atomic>
#include <lzo/lzo1x.h>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Event.h"
#include "Common/FileUtil.h"
#include "Common/MsgHandler.h"
#include "Common/ScopeGuard.h"
#include "Common/StringUtil.h"
#include "Common/Thread.h"
#include "Common/Timer.h"

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/HW/HW.h"
#include "Core/HW/Wiimote.h"
#include "Core/Host.h"
#include "Core/Movie.h"
#include "Core/NetPlayClient.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/State.h"

#include "VideoCommon/AVIDump.h"
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

#define HEAP_ALLOC(var, size)                                                                      \
  lzo_align_t __LZO_MMODEL var[((size) + (sizeof(lzo_align_t) - 1)) / sizeof(lzo_align_t)]

static HEAP_ALLOC(wrkmem, LZO1X_1_MEM_COMPRESS);

static std::string g_last_filename;

static CallbackFunc g_on_after_load_save_cb = nullptr;

// Temporary undo state buffer
static MallocBuffer g_undo_load_buffer;
static MallocBuffer g_current_buffer;

static std::mutex g_cs_undo_load_buffer;
static std::mutex g_cs_current_buffer;

// covers both the files themselves and the following array
static std::mutex g_cs_save_slot_files;
static std::array<double, NUM_STATES - 1> g_save_in_progress_times;

// Don't forget to increase this after doing changes on the savestate system
static const u32 STATE_VERSION = 55;

// Maps savestate versions to Dolphin versions.
// Versions after 42 don't need to be added to this list,
// beacuse they save the exact Dolphin version to savestates.
static const std::map<u32, std::pair<std::string, std::string>> s_old_versions = {
    // The 16 -> 17 change modified the size of StateHeader,
    // so version older than that can't even be decompressed anymore
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

static std::atomic<bool> g_use_compression(true);

void EnableCompression(bool compression)
{
  g_use_compression = compression;
}

// Returns true if state version matches current Dolphin state version, false otherwise.
static bool DoStateVersion(PointerWrap& p)
{
  u32 version = STATE_VERSION;
  {
    static const u32 COOKIE_BASE = 0xBAADBABE;
    u32 cookie = version + COOKIE_BASE;
    p.Do(cookie);
    version = cookie - COOKIE_BASE;
  }

  std::string version_created_by = scm_rev_str;
  if (version > 42)
    p.Do(version_created_by);
  else
    version_created_by = "an unknown Dolphin version";

  if (version != STATE_VERSION)
  {
    if (version <= 42 && s_old_versions.count(version))
    {
      // The savestate is from an old version that doesn't
      // save the Dolphin version number to savestates, but
      // by looking up the savestate version number, it is possible
      // to know approximately which Dolphin version was used.

      std::pair<std::string, std::string> version_range = s_old_versions.find(version)->second;
      std::string oldest_version = version_range.first;
      std::string newest_version = version_range.second;

      version_created_by = "Dolphin " + oldest_version + " - " + newest_version;
    }
    p.SetError("Can't load state from other revisions.  This state was saved by " +
               version_created_by);

    return false;
  }

  p.DoMarker("Version");
  return true;
}

static void DoState(StateLoadStore& p)
{
  if (!DoStateVersion(p))
    return;

  // Begin with video backend, so that it gets a chance to clear its caches and writeback modified
  // things to RAM
  if (!g_video_backend->DoState(p))
    return;
  p.DoMarker("video_backend");

  if (SConfig::GetInstance().bWii)
    Wiimote::DoState(p);
  p.DoMarker("Wiimote");

  PowerPC::DoState(p);
  p.DoMarker("PowerPC");
  // CoreTiming needs to be restored before restoring Hardware because
  // the controller code might need to schedule an event if the controller has changed.
  CoreTiming::DoState(p);
  p.DoMarker("CoreTiming");
  if (!HW::DoState(p))
    return;
  p.DoMarker("HW");
  Movie::DoState(p);
  p.DoMarker("Movie");

#if defined(HAVE_LIBAV) || defined(_WIN32)
  AVIDump::DoState();
#endif
}

bool LoadFromBuffer(const u8* buf, size_t size, std::string* p_error)
{
  bool wasUnpaused = Core::PauseAndLock(true);
  auto p = StateLoadStore::CreateForLoad(buf, size);
  DoState(p);
  if (!p.IsGood())
    *p_error = p.GetError();

  Core::PauseAndLock(false, wasUnpaused);
  return p.IsGood();
}

bool SaveToBuffer(MallocBuffer* p_buffer, std::string* p_error)
{
  bool wasUnpaused = Core::PauseAndLock(true);

  size_t initial_size = 128 * 1024 * 1024;  // enough for Wii RAM and some other stuff
  if (!p_buffer->ptr)
  {
    *p_buffer = MallocBuffer(initial_size);
    if (!p_buffer->ptr)
    {
      PanicAlertT("Out of memory saving state");
      Crash();
    }
  }
  auto p = StateLoadStore::CreateForStore(std::move(*p_buffer));
  DoState(p);
  if (!p.IsGood())
    *p_error = p.GetError();
  *p_buffer = p.TakeBuffer();

  Core::PauseAndLock(false, wasUnpaused);
  return p.IsGood();
}

static std::string MakeStateFilename(int number);

// returns array of (timestamp, num); g_cs_save_slot_files should be locked
static std::array<std::pair<double, int>, NUM_STATES - 1>
GetSaveTimestamps(bool include_in_progress)
{
  std::lock_guard<std::mutex> lk(g_cs_save_slot_files);
  std::array<std::pair<double, int>, NUM_STATES - 1> ret;
  for (int i = 1; i <= (int)NUM_STATES; i++)
  {
    ret[i - 1] = std::make_pair(0.0, i);
    std::string filename = MakeStateFilename(i);
    if (File::Exists(filename))
    {
      StateHeader header;
      if (ReadHeader(filename, header))
        ret[i - 1].first = header.time;
    }
  }
  if (include_in_progress)
  {
    for (u32 i = 0; i < NUM_STATES; i++)
      ret[i].first = std::max(ret[i].first, g_save_in_progress_times[i]);
  }
  return ret;
}

static void CompressAndDumpState(MallocBuffer buffer, const std::string& filename, int for_slot)
{
  // For easy debugging
  Common::SetCurrentThreadName("SaveState thread");

  std::string temp_filename = File::GetTempFilenameForAtomicWrite(filename);
  std::string temp_filename_dtm = temp_filename + ".dtm";
  std::string filename_dtm = filename + ".dtm";

  File::IOFile f(temp_filename, "wb");
  if (!f)
  {
    Core::DisplayMessage("Could not save state", 2000);
    return;
  }

  bool use_compression = g_use_compression;
  u8* buffer_data = buffer.ptr;
  size_t buffer_size = buffer.length;

  // Setting up the header
  StateHeader header;
  strncpy(header.gameID, SConfig::GetInstance().GetUniqueID().c_str(), 6);
  header.size = use_compression ? (u32)buffer_size : 0;
  header.time = Common::Timer::GetDoubleTime();
  f.WriteArray(&header, 1);

  if (g_use_compression)
  {
    auto out = std::make_unique<u8[]>(OUT_LEN);

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

      if (lzo1x_1_compress(buffer_data + i, cur_len, out.get(), &out_len, wrkmem) != LZO_E_OK)
        PanicAlertT("Internal LZO Error - compression failed");

      // The size of the data to write is 'out_len'
      f.WriteArray((lzo_uint32*)&out_len, 1);
      f.WriteBytes(out.get(), out_len);

      if (cur_len != IN_LEN)
        break;

      i += cur_len;
    }
  }
  else  // uncompressed
  {
    f.WriteBytes(buffer_data, buffer_size);
  }

  bool save_dtm = Movie::IsMovieActive() && !Movie::IsJustStartingRecordingInputFromSaveState();

  if (!f.IsGood() || (save_dtm && !Movie::SaveRecording(temp_filename_dtm)))
  {
    Core::DisplayMessage("Failed to write save data", 1000);
    goto bad;
  }

  {
    // Moving to last overwritten save-state
    // No easy cross-platform way of moving A->B and B->C atomically, so just
    // be atomic within this process.
    std::lock_guard<std::mutex> lk(g_cs_save_slot_files);

    if (for_slot != -1)
      g_save_in_progress_times[for_slot - 1] = 0.0;

    if (File::Exists(filename))
    {
      std::string base = File::GetUserPath(D_STATESAVES_IDX);
      std::string laststate_sav = base + "lastState.sav";
      std::string laststate_sav_dtm = base + "lastState.sav.dtm";

      bool src_existed;
      if ((!File::Rename(filename, laststate_sav, &src_existed) && src_existed) ||
          (!File::Rename(filename_dtm, laststate_sav_dtm, &src_existed) && src_existed))
      {
        Core::DisplayMessage("Failed to move previous state to state undo backup", 1000);
        goto bad;
      }
    }
    if (!File::Rename(temp_filename, filename) ||
        (save_dtm && !File::Rename(temp_filename_dtm, filename_dtm)))

    {
      Core::DisplayMessage("Failed to move previous state to state undo backup", 1000);
      goto bad;
    }
    if (!save_dtm)
    {
      bool src_existed;
      File::Delete(filename_dtm, &src_existed);
    }
  }

  Core::DisplayMessage(StringFromFormat("Saved state to %s", filename.c_str()), 2000);

bad:
  g_on_after_load_save_cb();
  {
    // Return the buffer, unless another one was returned
    std::lock_guard<std::mutex> lk(g_cs_current_buffer);
    if (!g_current_buffer.ptr)
      g_current_buffer = std::move(buffer);
  }
}

void SaveAs(std::string&& filename, int for_slot)
{
  if (for_slot != -1)
    g_save_in_progress_times[for_slot - 1] = Common::Timer::GetDoubleTime();
  std::lock_guard<std::mutex> lk(g_cs_current_buffer);
  std::string error;
  bool ok = SaveToBuffer(&g_current_buffer, &error);
  if (!ok)
  {
    g_current_buffer = MallocBuffer();
    Core::DisplayMessage("Unable to save: " + error, 4000);
    return;
  }
  Core::DisplayMessage("Saving State...", 1000);

  g_last_filename = filename;

  std::thread([
    for_slot, the_buffer = std::move(g_current_buffer), filename = std::move(filename)
  ]() mutable {
    CompressAndDumpState(std::move(the_buffer), filename, for_slot);
  }).detach();
}

bool ReadHeader(const std::string& filename, StateHeader& header)
{
  File::IOFile f(filename, "rb");
  if (!f)
  {
    Core::DisplayMessage("State not found", 2000);
    return false;
  }

  f.ReadArray(&header, 1);
  return true;
}

std::string GetInfoStringOfSlot(int slot)
{
  std::string filename = MakeStateFilename(slot);
  if (!File::Exists(filename))
    return GetStringT("Empty");

  State::StateHeader header;
  if (!ReadHeader(filename, header))
    return GetStringT("Unknown");

  return Common::Timer::GetDateTimeFormatted(header.time);
}

static void LoadFileStateData(const std::string& filename, std::vector<u8>& ret_data)
{
  File::IOFile f(filename, "rb");
  if (!f)
  {
    Core::DisplayMessage("State not found", 2000);
    return;
  }

  StateHeader header;
  f.ReadArray(&header, 1);

  if (strncmp(SConfig::GetInstance().GetUniqueID().c_str(), header.gameID, 6))
  {
    Core::DisplayMessage(
        StringFromFormat("State belongs to a different game (ID %.*s)", 6, header.gameID), 2000);
    return;
  }

  std::vector<u8> buffer;

  if (header.size != 0)  // non-zero size means the state is compressed
  {
    Core::DisplayMessage("Decompressing State...", 500);

    buffer.resize(header.size);
    auto out = std::make_unique<u8[]>(OUT_LEN);

    lzo_uint i = 0;
    while (true)
    {
      lzo_uint32 cur_len = 0;  // number of bytes to read
      lzo_uint new_len;        // number of bytes to write

      if (!f.ReadArray(&cur_len, 1))
        break;
      if (cur_len > OUT_LEN)
      {
        PanicAlertT("Internal LZO Error - buffer overflow");
        return;
      }

      f.ReadBytes(out.get(), cur_len);
      new_len = buffer.size() - i;
      const int res =
          lzo1x_decompress_safe(out.get(), cur_len, buffer.data() + i, &new_len, nullptr);
      if (res != LZO_E_OK)
      {
        // This doesn't seem to happen anymore.
        PanicAlertT("Internal LZO Error - decompression failed (%d) (%li, %li) \n"
                    "Try loading the state again",
                    res, i, new_len);
        return;
      }

      i += new_len;
    }
  }
  else  // uncompressed
  {
    const size_t size = (size_t)(f.GetSize() - sizeof(StateHeader));
    buffer.resize(size);

    if (!f.ReadBytes(&buffer[0], size))
    {
      PanicAlert("wtf? reading bytes: %zu", size);
      return;
    }
  }

  // all good
  ret_data.swap(buffer);
}

void LoadAs(const std::string& filename)
{
  if (!Core::IsRunning())
  {
    return;
  }
  else if (NetPlay::IsNetPlayRunning())
  {
    OSD::AddMessage("Loading savestates is disabled in Netplay to prevent desyncs");
    return;
  }

  // Save temp buffer for undo load state
  if (!Movie::IsJustStartingRecordingInputFromSaveState())
  {
    std::lock_guard<std::mutex> lk(g_cs_undo_load_buffer);
    std::string error;
    if (!SaveToBuffer(&g_undo_load_buffer, &error))
    {
      OSD::AddMessage("Error saving undo buffer: " + error);
      return;
    }
    std::string undo_dtm = File::GetUserPath(D_STATESAVES_IDX) + "undo.dtm";
    if (Movie::IsMovieActive())
      Movie::SaveRecording(undo_dtm);
    else
    {
      bool existed;
      File::Delete(undo_dtm, &existed);
    }
  }

  bool load_ok;
  std::string load_error;

  // brackets here are so buffer gets freed ASAP
  {
    std::vector<u8> buffer;
    LoadFileStateData(filename, buffer);
    if (buffer.empty())
      return;

    load_ok = LoadFromBuffer(buffer.data(), buffer.size(), &load_error);
  }

  if (!load_ok)
  {
    Core::DisplayMessage("Unable to load: " + load_error, 4000);
    // since we could be in an inconsistent state now (and might crash or whatever), undo.
    if (!LoadFromBuffer(g_undo_load_buffer.ptr, g_undo_load_buffer.length, &load_error))
    {
      // we're stuck in an inconsistent state adn are quite likely to just crash
      PanicAlertT("Unable to load undo state: %s", load_error.c_str());
      Crash();
    }
    return;
  }

  Core::DisplayMessage(StringFromFormat("Loaded state from %s", filename.c_str()), 2000);

  if (g_on_after_load_save_cb)
    g_on_after_load_save_cb();
}

void SetOnAfterLoadSaveCallback(CallbackFunc callback)
{
  g_on_after_load_save_cb = callback;
}

void Init()
{
  if (lzo_init() != LZO_E_OK)
    PanicAlertT("Internal LZO Error - lzo_init() failed");
}

void Shutdown()
{
  {
    std::lock_guard<std::mutex> lk(g_cs_current_buffer);
    g_current_buffer = MallocBuffer();
  }

  {
    std::lock_guard<std::mutex> lk(g_cs_undo_load_buffer);
    g_undo_load_buffer = MallocBuffer();
  }
}

static std::string MakeStateFilename(int number)
{
  return StringFromFormat("%s%s.s%02i", File::GetUserPath(D_STATESAVES_IDX).c_str(),
                          SConfig::GetInstance().GetUniqueID().c_str(), number);
}

void Save(int slot)
{
  SaveAs(MakeStateFilename(slot));
}

void Load(int slot)
{
  LoadAs(MakeStateFilename(slot));
}

void LoadLastSaved(int i)
{
  std::lock_guard<std::mutex> lk(g_cs_save_slot_files);
  int idx = -1;
  if (1 <= i && i <= (int)NUM_STATES)
  {
    auto timestamps = GetSaveTimestamps(/* include_in_progress */ false);
    size_t nth_biggest_idx = NUM_STATES - i;
    std::nth_element(&timestamps[0], &timestamps[nth_biggest_idx], &timestamps[NUM_STATES - 1]);
    if (timestamps[nth_biggest_idx].first > 0.0)
      idx = timestamps[nth_biggest_idx].second;
  }
  if (idx == -1)
    Core::DisplayMessage("State doesn't exist", 2000);
  else
    Load(idx);
}

void SaveFirstSaved()
{
  int idx;
  {
    std::lock_guard<std::mutex> lk(g_cs_save_slot_files);
    auto timestamps = GetSaveTimestamps(/* include_in_progress */ true);
    idx = std::min_element(&timestamps[0], &timestamps[NUM_STATES - 1])->second;
  }
  Save(idx);
}

// Load the last state before loading the state
void UndoLoadState()
{
  std::lock_guard<std::mutex> lk(g_cs_undo_load_buffer);
  if (!g_undo_load_buffer.ptr)
  {
    Core::DisplayMessage("There is no load to undo", 2000);
    return;
  }
  std::string undo_dtm = File::GetUserPath(D_STATESAVES_IDX) + "undo.dtm";
  if (Movie::IsMovieActive() && !File::Exists(undo_dtm))
  {
    Core::DisplayMessage("No undo.dtm found; aborting undo load state to prevent movie desyncs",
                         2000);
    return;
  }
  std::string error;
  if (!LoadFromBuffer(g_undo_load_buffer.ptr, g_undo_load_buffer.length, &error))
  {
    Core::DisplayMessage("Unable to load undo state: " + error, 4000);
    return;
  }

  if (Movie::IsMovieActive())
    Movie::LoadInput(File::GetUserPath(D_STATESAVES_IDX) + "undo.dtm");
}

// Load the state that the last save state overwritten on
void UndoSaveState()
{
  LoadAs(File::GetUserPath(D_STATESAVES_IDX) + "lastState.sav");
}

}  // namespace State
