// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "NullCameraInterface.h"

#include "Common/Assert.h"

Common::StbImage NullCameraInterface::CaptureFrame()
{
  ASSERT(m_is_open);
  static Common::StbImage s_frame{std::vector<u8>(m_width * m_height * m_channels, 0), m_width,
                                  m_height, m_channels};
  return s_frame;
}

std::expected<void, CameraError> NullCameraInterface::Open()
{
  m_is_open = true;
  return {};
}

void NullCameraInterface::Close()
{
  m_is_open = false;
}
