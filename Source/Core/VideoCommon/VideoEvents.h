// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string_view>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/HookableEvent.h"

namespace Core
{
class System;
}

struct PresentInfo
{
  enum class PresentReason
  {
    Immediate,                // FIFO is Presenting the XFB immediately, straight after the XFB copy
    VideoInterface,           // VideoInterface has triggered a present with a new frame
    VideoInterfaceDuplicate,  // VideoInterface has triggered a present with a duplicate frame
  };

  // The number of (unique) frames since the emulated console booted
  u64 frame_count = 0;

  // The number of presents since the video backend was initialized.
  // never goes backwards.
  u64 present_count = 0;

  // The frame is identical to the previous frame
  PresentReason reason = PresentReason::Immediate;

  // The exact emulated time of the when real hardware would have presented this frame
  u64 emulated_timestamp = 0;

  TimePoint intended_present_time{};

  // AfterPresent only: The actual time the frame was presented
  TimePoint actual_present_time{};

  enum class PresentTimeAccuracy
  {
    // The Driver/OS has given us an exact timestamp of when the first line of the frame started
    // scanning out to the monitor
    PresentOnScreenExact,

    // An approximate timestamp of scanout.
    PresentOnScreen,

    // Dolphin doesn't have visibility of the present time. But the present operation has
    // been queued with the GPU driver and will happen in the near future.
    PresentInProgress,

    // Not implemented
    Unimplemented,
  };

  // Accuracy of actual_present_time
  PresentTimeAccuracy present_time_accuracy = PresentTimeAccuracy::Unimplemented;

  std::vector<std::string_view> xfb_copy_hashes;
};

struct VideoEvents
{
  // Called when certain video config setting are changed
  Common::HookableEvent<u32> config_changed_event;

  // An event called just before the first draw call of a frame
  Common::HookableEvent<> before_frame_event;

  // An event called after the frame XFB copy begins processing on the host GPU.
  // Useful for "once per frame" usecases.
  // Note: In a few rare cases, games do multiple XFB copies per frame and join them while
  // presenting.
  //       If this matters to your usecase, you should use BeforePresent instead.
  Common::HookableEvent<Core::System&> after_frame_event;

  // An event called just as a frame is queued for presentation.
  // The exact timing of this event depends on the "Immediately Present XFB" option.
  //
  // If enabled, this event will trigger immediately after AfterFrame
  // If disabled, this event won't trigger until the emulated interface starts drawing out a new
  // frame.
  //
  // frame_count: The number of frames
  Common::HookableEvent<PresentInfo&> before_present_event;

  // An event that is triggered after a frame is presented.
  // The exact timing of this event depends on backend/driver support.
  Common::HookableEvent<PresentInfo&> after_present_event;

  // An end of frame event that runs on the CPU thread
  Common::HookableEvent<> vi_end_field_event;
};

VideoEvents& GetVideoEvents();
