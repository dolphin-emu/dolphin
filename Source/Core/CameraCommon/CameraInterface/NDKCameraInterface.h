// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "CameraCommon/CameraInterface/CameraInterface.h"

#include <atomic>
#include <mutex>

#include <camera/NdkCameraCaptureSession.h>
#include <camera/NdkCameraDevice.h>
#include <media/NdkImageReader.h>

class ACameraCaptureSession;
class ACameraDevice;
class ACameraManager;
class ACameraOutputTarget;
class ACaptureRequest;
class ACaptureSessionOutputContainer;
class ACaptureSessionOutput;
class AImageReader;
class ANativeWindow;

class NDKCameraInterface final : public CameraInterface
{
public:
  NDKCameraInterface(std::string name, const u32 id);
  ~NDKCameraInterface() override;

  static void OnError(void* ctx, ACameraDevice* dev, int err);
  static void OnDisconnected(void* ctx, ACameraDevice* dev);

  static void OnSessionActive(void* ctx, ACameraCaptureSession* session);
  static void OnSessionReady(void* ctx, ACameraCaptureSession* session);
  static void OnSessionClosed(void* ctx, ACameraCaptureSession* session);

  static void OnImageAvailable(void* ctx, AImageReader* reader);

  std::string_view Backend() const override { return "NDK"; }
  Common::StbImage CaptureFrame() override;

protected:
  std::expected<void, CameraError> Open() override;
  void Close() override;

private:
  ACameraManager* m_manager = nullptr;
  ACameraDevice* m_device = nullptr;

  ACameraDevice_stateCallbacks m_camera_open_callbacks{};
  AImageReader_ImageListener m_image_listener{};
  AImageReader* m_image_reader = nullptr;
  ANativeWindow* m_image_reader_surface = nullptr;
  ACameraOutputTarget* m_output_target = nullptr;

  ACameraCaptureSession_stateCallbacks m_session_state_callbacks{};
  ACameraCaptureSession_captureCallbacks m_capture_callbacks{};
  ACaptureRequest* m_capture_request = nullptr;
  ACaptureSessionOutputContainer* m_output_container = nullptr;
  ACaptureSessionOutput* m_session_output = nullptr;
  ACameraCaptureSession* m_capture_session = nullptr;

  int32_t m_sensor_orientation = 0;

  mutable Common::StbImage m_current_frame;

  mutable std::mutex m_frame_mutex;
  std::mutex m_close_mutex;
};
