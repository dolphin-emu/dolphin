// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <expected>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/StbImage.h"

enum class CameraError
{
  None,
  DeviceNotFound,
  PermissionDenied,
  Unknown
};

class CameraInterface
{
public:
  explicit CameraInterface(std::string name, const u32 id) : m_name(std::move(name)), m_id(id) {}
  virtual ~CameraInterface() = default;

  CameraInterface(const CameraInterface&) = delete;
  CameraInterface& operator=(const CameraInterface&) = delete;
  CameraInterface(CameraInterface&&) = delete;
  CameraInterface& operator=(CameraInterface&&) = delete;

  virtual std::string_view Backend() const = 0;
  u32 Id() const { return m_id; }
  const std::string& Name() const { return m_name; }
  int Width() const { return m_width; }
  int Height() const { return m_height; }
  bool IsOpen() const { return m_is_open; }

  virtual Common::StbImage CaptureFrame() = 0;

protected:
  const std::string m_name;
  const u32 m_id;
  bool m_is_open = false;

  int m_width = 0;
  int m_height = 0;

  virtual std::expected<void, CameraError> Open() = 0;
  virtual void Close() = 0;

  friend class CameraOrchestrator;
};
