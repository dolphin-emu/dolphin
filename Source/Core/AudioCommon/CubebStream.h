// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include "AudioCommon/SoundStream.h"

#include <cubeb/cubeb.h>

class CubebStream final : public SoundStream
{
public:
  bool Start() override;
  void Stop() override;
  void SetVolume(int) override;

private:
  bool m_stereo = false;
  std::shared_ptr<cubeb> m_ctx;
  cubeb_stream* m_stream = nullptr;

  std::vector<short> m_short_buffer;
  std::vector<float> m_floatstereo_buffer;

  static long DataCallback(cubeb_stream* stream, void* user_data, const void* /*input_buffer*/,
                           void* output_buffer, long num_frames);
  static void StateCallback(cubeb_stream* stream, void* user_data, cubeb_state state);
};
