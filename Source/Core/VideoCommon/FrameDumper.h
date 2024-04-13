// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"
#include "Common/Event.h"
#include "Common/Flag.h"
#include "Common/MathUtil.h"
#include "Common/Thread.h"

#include "VideoCommon/FrameDumpFFMpeg.h"
#include "VideoCommon/VideoEvents.h"

class AbstractStagingTexture;
class AbstractTexture;
class AbstractFramebuffer;

class FrameDumper
{
public:
  FrameDumper();
  ~FrameDumper();

  // Ensures all rendered frames are queued for encoding.
  void FlushFrameDump();

  // Fills the frame dump staging texture with the current XFB texture.
  void DumpCurrentFrame(const AbstractTexture* src_texture,
                        const MathUtil::Rectangle<int>& src_rect,
                        const MathUtil::Rectangle<int>& target_rect, u64 ticks, int frame_number);

  void SaveScreenshot(std::string filename);

  bool IsFrameDumping() const;
  int GetRequiredResolutionLeastCommonMultiple() const;

  void DoState(PointerWrap& p);

private:
  // NOTE: The methods below are called on the framedumping thread.
  void FrameDumpThreadFunc();
  bool StartFrameDumpToFFMPEG(const FrameData&);
  void DumpFrameToFFMPEG(const FrameData&);
  void StopFrameDumpToFFMPEG();
  std::string GetFrameDumpNextImageFileName() const;
  bool StartFrameDumpToImage(const FrameData&);
  void DumpFrameToImage(const FrameData&);

  void ShutdownFrameDumping();

  // Checks that the frame dump render texture exists and is the correct size.
  bool CheckFrameDumpRenderTexture(u32 target_width, u32 target_height);

  // Checks that the frame dump readback texture exists and is the correct size.
  bool CheckFrameDumpReadbackTexture(u32 target_width, u32 target_height);

  // Asynchronously encodes the specified pointer of frame data to the frame dump.
  void DumpFrameData(const u8* data, int w, int h, int stride);

  // Ensures all encoded frames have been written to the output file.
  void FinishFrameData();

  std::thread m_frame_dump_thread;
  Common::Flag m_frame_dump_thread_running;

  // Used to kick frame dump thread.
  Common::Event m_frame_dump_start;

  // Set by frame dump thread on frame completion.
  Common::Event m_frame_dump_done;

  // Holds emulation state during the last swap when dumping.
  FrameState m_last_frame_state;

  // Communication of frame between video and dump threads.
  FrameData m_frame_dump_data;

  // Texture used for screenshot/frame dumping
  std::unique_ptr<AbstractTexture> m_frame_dump_render_texture;
  std::unique_ptr<AbstractFramebuffer> m_frame_dump_render_framebuffer;

  // Double buffer:
  std::unique_ptr<AbstractStagingTexture> m_frame_dump_readback_texture;
  std::unique_ptr<AbstractStagingTexture> m_frame_dump_output_texture;
  // Set when readback texture holds a frame that needs to be dumped.
  bool m_frame_dump_needs_flush = false;
  // Set when thread is processing output texture.
  bool m_frame_dump_frame_running = false;

  // Used to generate screenshot names.
  u32 m_frame_dump_image_counter = 0;

  FFMpegFrameDump m_ffmpeg_dump;

  // Screenshots
  Common::Flag m_screenshot_request;
  Common::Event m_screenshot_completed;
  std::mutex m_screenshot_lock;
  std::string m_screenshot_name;

  Common::EventHook m_frame_end_handle;
};

extern std::unique_ptr<FrameDumper> g_frame_dumper;
