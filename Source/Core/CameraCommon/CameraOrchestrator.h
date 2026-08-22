// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <expected>
#include <memory>
#include <unordered_map>
#include <vector>

#include "CameraCommon/CameraBackend/CameraBackend.h"
#include "CameraCommon/CameraInterface/CameraInterface.h"
#include "CameraCommon/CameraQualifier.h"

class CameraOrchestrator final
{
public:
  static CameraOrchestrator& GetInstance()
  {
    static CameraOrchestrator instance;
    return instance;
  }

  CameraOrchestrator();
  ~CameraOrchestrator();

  CameraOrchestrator(const CameraOrchestrator&) = delete;
  CameraOrchestrator& operator=(const CameraOrchestrator&) = delete;

  std::vector<std::shared_ptr<CameraInterface>> EnumerateCameras();
  std::shared_ptr<CameraInterface> GetCameraByQualifier(const CameraQualifier& qualifier);

  std::expected<std::shared_ptr<CameraInterface>, CameraError>
  OpenCamera(const std::shared_ptr<CameraInterface>& camera);

  void CloseCamera(const std::shared_ptr<CameraInterface>& camera);

private:
  std::vector<std::unique_ptr<CameraBackend>> m_backends;
  std::unordered_map<CameraQualifier, std::shared_ptr<CameraInterface>> m_known_cameras;
  std::unordered_map<std::shared_ptr<CameraInterface>, int> m_open_cameras;
};
