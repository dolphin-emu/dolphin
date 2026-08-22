// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "NDKCameraInterface.h"

#include <camera/NdkCameraManager.h>
#include <media/NdkImage.h>
#include <media/NdkImageReader.h>

#include "Common/Assert.h"
#include "Common/Logging/Log.h"
#include "Common/ScopeGuard.h"

NDKCameraInterface::NDKCameraInterface(std::string name, const u32 id)
    : CameraInterface(std::move(name), id)
{
}

NDKCameraInterface::~NDKCameraInterface()
{
  if (m_is_open)
    Close();
}

void NDKCameraInterface::OnError(void* ctx, ACameraDevice* dev, int err)
{
  auto* self = static_cast<NDKCameraInterface*>(ctx);
  ERROR_LOG_FMT(CAMERA, "NDKCamera: {}/{} Errored: {}", self->m_id, self->m_name, err);
  self->Close();
}

void NDKCameraInterface::OnDisconnected(void* ctx, ACameraDevice* dev)
{
  auto* self = static_cast<NDKCameraInterface*>(ctx);
  INFO_LOG_FMT(CAMERA, "NDKCamera: {}/{} Disconnected", self->m_id, self->m_name);
  self->Close();
}

void NDKCameraInterface::OnSessionActive(void* ctx, ACameraCaptureSession* session)
{
  auto* self = static_cast<NDKCameraInterface*>(ctx);
  INFO_LOG_FMT(CAMERA, "NDKCamera: {}/{} Session Active", self->m_id, self->m_name);
}

void NDKCameraInterface::OnSessionReady(void* ctx, ACameraCaptureSession* session)
{
  auto* self = static_cast<NDKCameraInterface*>(ctx);
  INFO_LOG_FMT(CAMERA, "NDKCamera: {}/{} Session Ready", self->m_id, self->m_name);
}

void NDKCameraInterface::OnSessionClosed(void* ctx, ACameraCaptureSession* session)
{
  auto* self = static_cast<NDKCameraInterface*>(ctx);
  INFO_LOG_FMT(CAMERA, "NDKCamera: {}/{} Session Closed", self->m_id, self->m_name);
}

void NDKCameraInterface::OnImageAvailable(void* ctx, AImageReader* reader)
{
  auto* self = static_cast<NDKCameraInterface*>(ctx);

  AImage* image = nullptr;
  if (AImageReader_acquireLatestImage(reader, &image) != AMEDIA_OK)
    return;

  Common::ScopeGuard image_guard([image] { AImage_delete(image); });

  int32_t width = 0;
  int32_t height = 0;
  AImage_getWidth(image, &width);
  AImage_getHeight(image, &height);

  uint8_t* y_data = nullptr;
  uint8_t* u_data = nullptr;
  uint8_t* v_data = nullptr;
  int y_len = 0;
  int u_len = 0;
  int v_len = 0;
  if (AImage_getPlaneData(image, 0, &y_data, &y_len) != AMEDIA_OK ||
      AImage_getPlaneData(image, 1, &u_data, &u_len) != AMEDIA_OK ||
      AImage_getPlaneData(image, 2, &v_data, &v_len) != AMEDIA_OK)
    return;

  int y_row_stride = 0;
  int uv_row_stride = 0;
  int u_pixel_stride = 0;
  int v_pixel_stride = 0;
  AImage_getPlaneRowStride(image, 0, &y_row_stride);
  AImage_getPlaneRowStride(image, 1, &uv_row_stride);
  AImage_getPlanePixelStride(image, 1, &u_pixel_stride);
  AImage_getPlanePixelStride(image, 2, &v_pixel_stride);

  std::vector<uint8_t> rgb(width * height * 3);
  for (int32_t row = 0; row < height; ++row)
  {
    for (int32_t col = 0; col < width; ++col)
    {
      const int y_val = y_data[row * y_row_stride + col];
      const int u_val = u_data[(row / 2) * uv_row_stride + (col / 2) * u_pixel_stride];
      const int v_val = v_data[(row / 2) * uv_row_stride + (col / 2) * v_pixel_stride];

      const int c = y_val - 16;
      const int d = u_val - 128;
      const int e = v_val - 128;

      const int r = std::clamp((298 * c + 409 * e + 128) >> 8, 0, 255);
      const int g = std::clamp((298 * c - 100 * d - 208 * e + 128) >> 8, 0, 255);
      const int b = std::clamp((298 * c + 516 * d + 128) >> 8, 0, 255);

      const int base = (row * width + col) * 3;
      rgb[base + 0] = static_cast<uint8_t>(r);
      rgb[base + 1] = static_cast<uint8_t>(g);
      rgb[base + 2] = static_cast<uint8_t>(b);
    }
  }

  Common::StbImage frame(std::move(rgb), width, height, 3);

  switch (self->m_sensor_orientation)
  {
  case 90:
    frame.Rotate90(true);
    break;
  case 180:
    frame.Rotate180();
    break;
  case 270:
    frame.Rotate90(false);
    break;
  }

  {
    std::lock_guard lock(self->m_frame_mutex);
    self->m_current_frame = std::move(frame);
  }
}

Common::StbImage NDKCameraInterface::CaptureFrame()
{
  ASSERT(m_is_open);
  std::lock_guard lock(m_frame_mutex);
  return m_current_frame;
}

std::expected<void, CameraError> NDKCameraInterface::Open()
{
  Common::ScopeGuard cleanup_guard([this] { Close(); });
  m_manager = ACameraManager_create();

  // Open the camera
  m_camera_open_callbacks = {
      .context = this,
      .onDisconnected = NDKCameraInterface::OnDisconnected,
      .onError = NDKCameraInterface::OnError,
  };

  camera_status_t camera_status =
      ACameraManager_openCamera(m_manager, m_name.c_str(), &m_camera_open_callbacks, &m_device);
  switch (camera_status)
  {
  case ACAMERA_OK:
    break;
  case ACAMERA_ERROR_CAMERA_DISABLED:
  case ACAMERA_ERROR_PERMISSION_DENIED:
    return std::unexpected(CameraError::PermissionDenied);
  case ACAMERA_ERROR_INVALID_PARAMETER:
  case ACAMERA_ERROR_CAMERA_DISCONNECTED:
    return std::unexpected(CameraError::DeviceNotFound);
  default:
    return std::unexpected(CameraError::Unknown);
  }

  // Get the camera resolution
  ACameraMetadata* chars = nullptr;
  ACameraManager_getCameraCharacteristics(m_manager, m_name.c_str(), &chars);
  Common::ScopeGuard chars_guard([chars] { ACameraMetadata_free(chars); });

  m_width = 0;
  m_height = 0;
  ACameraMetadata_const_entry available_streams;
  if (ACameraMetadata_getConstEntry(chars, ACAMERA_SCALER_AVAILABLE_STREAM_CONFIGURATIONS,
                                    &available_streams) == ACAMERA_OK)
  {
    // Maximum preview resolution on Android
    static constexpr int TARGET_WIDTH = 1920;
    static constexpr int TARGET_HEIGHT = 1080;
    int64_t best_diff = std::numeric_limits<int64_t>::max();
    for (size_t i = 0; i < available_streams.count; i += 4)
    {
      int32_t format = available_streams.data.i32[i + 0];
      int32_t width = available_streams.data.i32[i + 1];
      int32_t height = available_streams.data.i32[i + 2];
      int32_t dir = available_streams.data.i32[i + 3];

      if (format != AIMAGE_FORMAT_YUV_420_888 ||
          dir != ACAMERA_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT)
        continue;

      if (width > TARGET_WIDTH || height > TARGET_HEIGHT)
        continue;

      int64_t diff = static_cast<int64_t>(TARGET_WIDTH - width) * (TARGET_WIDTH - width) +
                     static_cast<int64_t>(TARGET_HEIGHT - height) * (TARGET_HEIGHT - height);
      if (diff < best_diff)
      {
        best_diff = diff;
        m_width = width;
        m_height = height;
      }
    }
  }

  if (m_width == 0 || m_height == 0)
  {
    ERROR_LOG_FMT(CAMERA, "NDKCamera: {}/{} Could not find supported YUV_420_888 format", m_id,
                  m_name);
    return std::unexpected(CameraError::Unknown);
  }

  INFO_LOG_FMT(CAMERA, "NDKCamera: {}/{} Selected resolution {}x{}", m_id, m_name, m_width,
               m_height);

  // Create the image reader
  media_status_t media_status =
      AImageReader_new(m_width, m_height, AIMAGE_FORMAT_YUV_420_888, 4, &m_image_reader);
  if (media_status != AMEDIA_OK)
    return std::unexpected(CameraError::Unknown);

  m_image_listener = {
      .context = this,
      .onImageAvailable = NDKCameraInterface::OnImageAvailable,
  };
  AImageReader_setImageListener(m_image_reader, &m_image_listener);

  // Create the output surface
  if (AImageReader_getWindow(m_image_reader, &m_image_reader_surface) != AMEDIA_OK)
    return std::unexpected(CameraError::Unknown);
  ANativeWindow_acquire(m_image_reader_surface);

  // Create a capture request from the camera to the output surface
  if (ACameraDevice_createCaptureRequest(m_device, TEMPLATE_PREVIEW, &m_capture_request) !=
      ACAMERA_OK)
    return std::unexpected(CameraError::Unknown);

  if (ACameraOutputTarget_create(m_image_reader_surface, &m_output_target) != ACAMERA_OK)
    return std::unexpected(CameraError::Unknown);

  if (ACaptureRequest_addTarget(m_capture_request, m_output_target) != ACAMERA_OK)
    return std::unexpected(CameraError::Unknown);

  // Set up the capture session's output surface
  if (ACaptureSessionOutputContainer_create(&m_output_container) != ACAMERA_OK)
    return std::unexpected(CameraError::Unknown);

  if (ACaptureSessionOutput_create(m_image_reader_surface, &m_session_output) != ACAMERA_OK)
    return std::unexpected(CameraError::Unknown);

  if (ACaptureSessionOutputContainer_add(m_output_container, m_session_output) != ACAMERA_OK)
    return std::unexpected(CameraError::Unknown);

  // Finally create the capture session
  m_session_state_callbacks = {
      .context = this,
      .onClosed = NDKCameraInterface::OnSessionClosed,
      .onReady = NDKCameraInterface::OnSessionReady,
      .onActive = NDKCameraInterface::OnSessionActive,
  };

  if (ACameraDevice_createCaptureSession(m_device, m_output_container, &m_session_state_callbacks,
                                         &m_capture_session) != ACAMERA_OK)
    return std::unexpected(CameraError::Unknown);

  m_capture_callbacks = {
      .context = this,
      .onCaptureStarted = nullptr,
      .onCaptureProgressed = nullptr,
      .onCaptureCompleted = nullptr,
      .onCaptureFailed = nullptr,
      .onCaptureSequenceCompleted = nullptr,
      .onCaptureSequenceAborted = nullptr,
      .onCaptureBufferLost = nullptr,
  };

  if (ACameraCaptureSession_setRepeatingRequest(m_capture_session, &m_capture_callbacks, 1,
                                                &m_capture_request, nullptr) != ACAMERA_OK)
    return std::unexpected(CameraError::Unknown);

  // Store the sensor orientation
  ACameraMetadata_const_entry sensor_orientation;
  if (ACameraMetadata_getConstEntry(chars, ACAMERA_SENSOR_ORIENTATION, &sensor_orientation) ==
      ACAMERA_OK)
  {
    m_sensor_orientation = sensor_orientation.data.i32[0];
  }

  m_is_open = true;
  cleanup_guard.Dismiss();
  return {};
}

void NDKCameraInterface::Close()
{
  std::lock_guard lock(m_close_mutex);

  m_is_open = false;

  if (m_capture_session)
  {
    ACameraCaptureSession_close(m_capture_session);
    m_capture_session = nullptr;
  }
  if (m_output_container)
  {
    ACaptureSessionOutputContainer_free(m_output_container);
    m_output_container = nullptr;
  }
  if (m_session_output)
  {
    ACaptureSessionOutput_free(m_session_output);
    m_session_output = nullptr;
  }
  if (m_capture_request)
  {
    ACaptureRequest_free(m_capture_request);
    m_capture_request = nullptr;
  }
  if (m_output_target)
  {
    ACameraOutputTarget_free(m_output_target);
    m_output_target = nullptr;
  }
  if (m_device)
  {
    ACameraDevice_close(m_device);
    m_device = nullptr;
  }
  if (m_image_reader)
  {
    AImageReader_delete(m_image_reader);
    m_image_reader = nullptr;
  }
  if (m_image_reader_surface)
  {
    ANativeWindow_release(m_image_reader_surface);
    m_image_reader_surface = nullptr;
  }
  if (m_manager)
  {
    ACameraManager_delete(m_manager);
    m_manager = nullptr;
  }
}
