// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "CameraOrchestrator.h"

#include "CameraCommon/CameraBackend/NullCameraBackend.h"
#ifdef HAVE_SDL3
#include "CameraCommon/CameraBackend/SDLCameraBackend.h"
#endif

CameraOrchestrator::CameraOrchestrator()
{
#ifdef HAVE_SDL3
  m_backends.push_back(std::make_unique<SDLCameraBackend>());
#endif
  m_backends.push_back(std::make_unique<NullCameraBackend>());
}

CameraOrchestrator::~CameraOrchestrator()
{
  for (auto& [camera, _] : m_open_cameras)
    camera->Close();
}

std::vector<std::shared_ptr<CameraInterface>> CameraOrchestrator::EnumerateCameras()
{
  std::vector<std::shared_ptr<CameraInterface>> cameras;

  for (auto& backend : m_backends)
  {
    for (auto& cam : backend->Enumerate())
    {
      const CameraQualifier qualifier(cam.get());

      auto it = m_known_cameras.find(qualifier);
      if (it != m_known_cameras.end())
      {
        cameras.push_back(it->second);
        continue;
      }

      std::shared_ptr<CameraInterface> shared_cam(std::move(cam));
      m_known_cameras.emplace(qualifier, shared_cam);
      cameras.push_back(std::move(shared_cam));
    }
  }

  return cameras;
}

std::shared_ptr<CameraInterface>
CameraOrchestrator::GetCameraByQualifier(const CameraQualifier& qualifier)
{
  for (auto& camera : EnumerateCameras())
  {
    if (qualifier == camera.get())
      return camera;
  }

  return nullptr;
}

std::expected<std::shared_ptr<CameraInterface>, CameraError>
CameraOrchestrator::OpenCamera(const std::shared_ptr<CameraInterface>& camera)
{
  if (!camera)
    return std::unexpected(CameraError::DeviceNotFound);

  auto it = m_open_cameras.find(camera);
  if (it != m_open_cameras.end())
  {
    it->second++;
    return camera;
  }

  const auto result = camera->Open();
  if (!result)
    return std::unexpected(result.error());

  m_open_cameras[camera] = 1;
  return camera;
}

void CameraOrchestrator::CloseCamera(const std::shared_ptr<CameraInterface>& camera)
{
  if (!camera)
    return;

  auto it = m_open_cameras.find(camera);
  if (it == m_open_cameras.end())
    return;

  it->second--;
  if (it->second <= 0)
  {
    camera->Close();
    m_open_cameras.erase(it);
  }
}
