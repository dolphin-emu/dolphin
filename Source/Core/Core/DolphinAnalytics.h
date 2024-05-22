// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "Common/Analytics.h"
#include "Common/CommonTypes.h"

#if defined(ANDROID)
#include <functional>
#endif
// Non generic part of the Dolphin Analytics framework. See Common/Analytics.h
// for the main documentation.

enum class GameQuirk
{
  // The Wii remote hardware makes it possible to bypass normal data reporting and directly
  // "read" extension or IR data. This would break our current TAS/NetPlay implementation.
  DIRECTLY_READS_WIIMOTE_INPUT = 0,

  // Several Wii DI commands that are rarely/never used and not implemented by Dolphin
  USES_DVD_LOW_STOP_LASER,
  USES_DVD_LOW_OFFSET,
  USES_DVD_LOW_READ_DISK_BCA,  // NSMBW known to use this
  USES_DVD_LOW_REQUEST_DISC_STATUS,
  USES_DVD_LOW_REQUEST_RETRY_NUMBER,
  USES_DVD_LOW_SER_MEAS_CONTROL,

  // Dolphin only implements the simple DVDLowOpenPartition, not any of the variants where some
  // already-read data is provided
  USES_DIFFERENT_PARTITION_COMMAND,

  // IOS has implementations for ioctls 0x85 and 0x89 and a stub for 0x87, but
  // DVDLowMaskCoverInterrupt/DVDLowUnmaskCoverInterrupt/DVDLowUnmaskStatusInterrupts
  // are all stubbed on the PPC side so they presumably will never be used.
  // (DVDLowClearCoverInterrupt is used, though)
  USES_DI_INTERRUPT_MASK_COMMAND,

  // Some games configure a mismatched number of texture coordinates or colors between the transform
  // and TEV/BP stages of the rendering pipeline. Currently, Dolphin just skips over these objects
  // as the hardware renderers are not equipped to handle the case where the registers between
  // stages are mismatched.
  MISMATCHED_GPU_TEXGENS_BETWEEN_XF_AND_BP,
  MISMATCHED_GPU_COLORS_BETWEEN_XF_AND_BP,

  // The WD module can be configured to operate in six different modes.
  // In practice, only mode 1 (DS communications) and mode 3 (AOSS access point scanning)
  // are used by games and the system menu respectively.
  USES_UNCOMMON_WD_MODE,

  USES_WD_UNIMPLEMENTED_IOCTL,

  // Some games use invalid/unknown graphics commands (see e.g. bug 10931).
  // These are different from unknown opcodes: it is known that a BP/CP/XF command is being used,
  // but the command itself is not understood.
  USES_UNKNOWN_BP_COMMAND,
  USES_UNKNOWN_CP_COMMAND,
  USES_UNKNOWN_XF_COMMAND,
  // YAGCD and Dolphin's implementation disagree about what is valid in some cases
  USES_MAYBE_INVALID_CP_COMMAND,
  // These commands are used by a few games (e.g. bug 12461), and seem to relate to perf queries.
  // Track them separately.
  USES_CP_PERF_COMMAND,

  // We don't implement all AX features yet.
  USES_UNIMPLEMENTED_AX_COMMAND,
  USES_AX_INITIAL_TIME_DELAY,

  // We don't implement XFMEM_CLIPDISABLE yet.
  SETS_XF_CLIPDISABLE_BIT_0,
  SETS_XF_CLIPDISABLE_BIT_1,
  SETS_XF_CLIPDISABLE_BIT_2,

  // Similar to the XF-BP mismatch, CP and XF might be configured with different vertex formats.
  // Real hardware seems to hang in this case, so games probably don't do this, but it would
  // be good to know if anything does it.
  MISMATCHED_GPU_COLORS_BETWEEN_CP_AND_XF,
  MISMATCHED_GPU_NORMALS_BETWEEN_CP_AND_XF,
  MISMATCHED_GPU_TEX_COORDS_BETWEEN_CP_AND_XF,
  // Both CP and XF have normally-identical matrix index information.  We currently always
  // use the CP one in the hardware renderers and the XF one in the software renderer,
  // but testing is needed to find out which of these is actually used for what.
  MISMATCHED_GPU_MATRIX_INDICES_BETWEEN_CP_AND_XF,

  // Only a few games use the Bounding Box feature. Note that every game initializes the bounding
  // box registers (using BPMEM_CLEARBBOX1/BPMEM_CLEARBBOX2) on startup, as part of the SDK, but
  // only a few read them (from PE_BBOX_LEFT etc.)
  READS_BOUNDING_BOX,

  // A few games use invalid vertex component formats, but the two known cases (Fifa Street and
  // Def Jam: Fight for New York, see https://bugs.dolphin-emu.org/issues/12719) only use invalid
  // normal formats and lighting is disabled in those cases, so it doesn't end up mattering.
  // It's possible other games use invalid formats, possibly on other vertex components.
  INVALID_POSITION_COMPONENT_FORMAT,
  INVALID_NORMAL_COMPONENT_FORMAT,
  INVALID_TEXTURE_COORDINATE_COMPONENT_FORMAT,
  INVALID_COLOR_COMPONENT_FORMAT,

  COUNT,
};

class DolphinAnalytics
{
public:
  // Performs lazy-initialization of a singleton and returns the instance.
  static DolphinAnalytics& Instance();

#if defined(ANDROID)
  // Get value from java.
  static void AndroidSetGetValFunc(std::function<std::string(std::string)> function);
#endif
  // Resets and recreates the analytics system in order to reload
  // configuration.
  void ReloadConfig();
  // Rotates the unique identifier used for this instance of Dolphin and saves
  // it into the configuration.
  void GenerateNewIdentity();

  // Reports a Dolphin start event.
  void ReportDolphinStart(std::string_view ui_type);

  // Generates a base report for a "Game start" event. Also preseeds the
  // per-game base data.
  void ReportGameStart();

  // Generates a report for a special condition being hit by a game. This is automatically throttled
  // to once per game run.
  void ReportGameQuirk(GameQuirk quirk);

  // Get the base builder for building a report
  const Common::AnalyticsReportBuilder& BaseBuilder() const { return m_base_builder; }

  struct PerformanceSample
  {
    double speed_ratio;  // See SystemTimers::GetEstimatedEmulationPerformance().
    int num_prims;
    int num_draw_calls;
  };
  // Reports performance information. This method performs its own throttling / aggregation --
  // calling it does not guarantee when a report will actually be sent.
  //
  // This method is NOT thread-safe.
  void ReportPerformanceInfo(PerformanceSample&& sample);

  // Forward Send method calls to the reporter.
  template <typename T>
  void Send(T report)
  {
    std::lock_guard lk{m_reporter_mutex};
    m_reporter.Send(report);
  }

private:
  DolphinAnalytics();

  void MakeBaseBuilder();
  void MakePerGameBuilder();

  // Returns a unique ID derived on the global unique ID, hashed with some
  // report-specific data. This avoid correlation between different types of
  // events.
  std::string MakeUniqueId(std::string_view data) const;

  // Unique ID. This should never leave the application. Only used derived
  // values created by MakeUniqueId.
  std::string m_unique_id;

  // Performance sampling configuration constants.
  //
  // 5min after startup + rand(0, 3min) jitter time, collect performance for 100 frames in a row.
  // Repeat collection after 30min + rand(0, 3min).
  static constexpr int NUM_PERFORMANCE_SAMPLES_PER_REPORT = 100;
  static constexpr int PERFORMANCE_SAMPLING_INITIAL_WAIT_TIME_SECS = 300;
  static constexpr int PERFORMANCE_SAMPLING_WAIT_TIME_JITTER_SECS = 180;
  static constexpr int PERFORMANCE_SAMPLING_INTERVAL_SECS = 1800;

  // Performance sampling state & internal helpers.
  void InitializePerformanceSampling();  // Called on game start / title switch.
  bool ShouldStartPerformanceSampling();
  u64 m_sampling_next_start_us;              // Next timestamp (in us) at which to trigger sampling.
  bool m_sampling_performance_info = false;  // Whether we are currently collecting samples.
  std::vector<PerformanceSample> m_performance_samples;

  // What quirks have already been reported about the current game.
  std::array<bool, static_cast<size_t>(GameQuirk::COUNT)> m_reported_quirks;

  // Builder that contains all non variable data that should be sent with all
  // reports.
  Common::AnalyticsReportBuilder m_base_builder;

  // Builder that contains per game data and is initialized when a game start
  // report is sent.
  Common::AnalyticsReportBuilder m_per_game_builder;

  std::mutex m_reporter_mutex;
  Common::AnalyticsReporter m_reporter;
};
