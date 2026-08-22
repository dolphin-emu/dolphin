// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "CameraCommon/CameraBackend/NDKCameraBackend.h"

#include <camera/NdkCameraManager.h>

#include "CameraCommon/CameraInterface/NDKCameraInterface.h"
#include "Common/Logging/Log.h"
#include "Common/ScopeGuard.h"

std::vector<std::unique_ptr<CameraInterface>> NDKCameraBackend::Enumerate()
{
  std::vector<std::unique_ptr<CameraInterface>> cameras;

  ACameraManager* manager = ACameraManager_create();
  if (!manager)
  {
    ERROR_LOG_FMT(CAMERA, "NDKCamera: Failed to create camera manager");
    return cameras;
  }
  Common::ScopeGuard camera_manager_guard([manager] { ACameraManager_delete(manager); });

  ACameraIdList* id_list = nullptr;
  camera_status_t status = ACameraManager_getCameraIdList(manager, &id_list);
  if (status != ACAMERA_OK || !id_list)
  {
    ERROR_LOG_FMT(CAMERA, "NDKCamera: Failed to get camera id list");
    return cameras;
  }
  Common::ScopeGuard camera_id_list_guard(
      [id_list] { ACameraManager_deleteCameraIdList(id_list); });

  for (int i = 0; i < id_list->numCameras; ++i)
    cameras.push_back(std::make_unique<NDKCameraInterface>(std::string(id_list->cameraIds[i]),
                                                           static_cast<u32>(i)));

  return cameras;
}
