// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Camera.h"

#include <optional>

#include <fmt/format.h>
#include <fmt/ranges.h>

#include "CameraCommon/CameraInterface/CameraInterface.h"
#include "CameraCommon/CameraOrchestrator.h"
#include "CameraCommon/CameraQualifier.h"
#include "Common/Config/Config.h"
#include "Common/IOFile.h"
#include "Common/Logging/Log.h"
#include "Common/Network.h"
#include "Common/StbImage.h"
#include "Core/Config/MainSettings.h"
#include "Core/HW/DVD/AMMediaboard.h"

static std::vector<std::string> RedirectionsWithoutIntegratedCamera()
{
  std::vector<std::string> result;

  const auto ip_redirections_str = Config::Get(Config::MAIN_TRIFORCE_IP_REDIRECTIONS);
  for (auto&& ip_pair : ip_redirections_str | std::views::split(','))
  {
    const std::string_view ip_pair_str{ip_pair};
    const auto parsed = AMMediaboard::ParseIPRedirection(ip_pair_str);
    if (parsed && parsed->description != "Integrated Camera")
    {
      result.emplace_back(ip_pair_str);
    }
  }

  return result;
}

namespace Triforce
{
void Camera::Init()
{
  if (IsInitialized())
    return;
  m_camera_initialized.store(false);
  if (m_open_camera)
    CameraOrchestrator::GetInstance().CloseCamera(m_open_camera);
  m_open_camera.reset();

  if (!Config::Get(Config::MAIN_TRIFORCE_INTEGRATED_CAMERA))
  {
    Config::SetBaseOrCurrent(
        Config::MAIN_TRIFORCE_IP_REDIRECTIONS,
        fmt::format("{}", fmt::join(RedirectionsWithoutIntegratedCamera(), ",")));
    // TODO: We currently grab the first match in the 104-107 range. It would be better if we could
    // grab only the IP of the current cabinet.
    Common::IPv4Port local_camera_address = {.ip_address = {192, 168, 29, 0}, .port = 0};
    for (u8 last_octet = 104; last_octet <= 107; ++last_octet)
    {
      local_camera_address.ip_address[3] = last_octet;
      for (auto&& redirection : AMMediaboard::GetIPRedirections())
      {
        if (redirection.original.IsMatch(local_camera_address))
        {
          m_redirection_address = redirection.Apply(local_camera_address);
          return;
        }
      }
    }
    ERROR_LOG_FMT(AMMEDIABOARD, "Camera: Missing camera IP redirection 192.168.29.104-107");
    return;
  }

  const std::optional<Common::IPv4Port> address =
      Common::StringToIPv4Port(Config::Get(Config::MAIN_TRIFORCE_INTEGRATED_CAMERA_SERVER_IP));

  if (!address)
  {
    ERROR_LOG_FMT(AMMEDIABOARD, "Camera: Failed to determine IP address for the server");
    return;
  }

  const std::string configured_camera = Config::Get(Config::MAIN_TRIFORCE_INTEGRATED_CAMERA_DEVICE);
  static constexpr int target_width = 320;
  static constexpr int target_height = 240;

  std::optional<CameraQualifier> qualifier;
  if (configured_camera != "static")
  {
    if (configured_camera.empty())
    {
      auto& orchestrator = CameraOrchestrator::GetInstance();
      if (const auto cameras = orchestrator.EnumerateCameras(); !cameras.empty())
      {
        const auto auto_qualifier = CameraQualifier(cameras.front().get());
        if (auto_qualifier.source != "NULL")
          qualifier = auto_qualifier;
      }
    }
    else
    {
      qualifier = CameraQualifier::FromString(configured_camera);
      if (!qualifier)
      {
        ERROR_LOG_FMT(CORE,
                      "Configured camera '{}' is not a valid camera qualifier. Falling back to a "
                      "static image.",
                      configured_camera);
      }
    }
  }

  m_http_server.emplace(*address);

  m_http_server->ServePath("/img.jpg", [this, configured_camera, qualifier]() mutable {
    bool expected = false;
    if (m_camera_initialized.compare_exchange_strong(expected, true) && qualifier)
    {
      auto& orchestrator = CameraOrchestrator::GetInstance();
      if (const auto camera = orchestrator.GetCameraByQualifier(*qualifier); !camera)
      {
        ERROR_LOG_FMT(CORE,
                      "Configured camera '{}' could not be found. Falling back to a static image.",
                      configured_camera);
      }
      else if (const auto opened = orchestrator.OpenCamera(camera); !opened)
      {
        ERROR_LOG_FMT(CORE,
                      "Configured camera '{}' could not be opened. Falling back to a static image.",
                      configured_camera);
      }
      else
      {
        m_open_camera = *opened;
      }
    }

    std::vector<u8> response;
    if (m_open_camera && m_open_camera->IsOpen())
    {
      auto frame = m_open_camera->CaptureFrame();

      frame.Resize(target_width, target_height);

      if (auto jpeg = frame.EncodeToBaselineJPEG(75))
        response.swap(*jpeg);
    }
    else
    {
      File::IOFile file(Config::Get(Config::MAIN_TRIFORCE_INTEGRATED_CAMERA_STATIC_IMAGE), "rb",
                        File::SharedAccess::Read);

      auto image = Common::StbImage::LoadFromFile(std::move(file), 3);
      if (image)
      {
        image->Resize(target_width, target_height);
        if (auto jpeg = image->EncodeToBaselineJPEG(75))
          response.swap(*jpeg);
      }
    }

    static const Common::HttpHeaders headers = {{"Content-Type", "image/jpeg"}};

    return std::make_pair(headers, std::move(response));
  });

  m_http_server->Start();

  const std::string integrated_redirection =
      fmt::format("192.168.29.104-107={} Integrated Camera", GetAddress()->ToString());

  std::vector<std::string> new_redirections{integrated_redirection};
  const auto redirections_without_integrated_camera = RedirectionsWithoutIntegratedCamera();
  new_redirections.insert(new_redirections.end(), redirections_without_integrated_camera.begin(),
                          redirections_without_integrated_camera.end());

  Config::SetBaseOrCurrent(Config::MAIN_TRIFORCE_IP_REDIRECTIONS,
                           fmt::format("{}", fmt::join(new_redirections, ",")));
}

void Camera::Shutdown()
{
  m_redirection_address.reset();
  m_http_server.reset();
  if (m_open_camera)
    CameraOrchestrator::GetInstance().CloseCamera(m_open_camera);
}

void Camera::Recreate()
{
  Shutdown();
  Init();
}

bool Camera::IsInitialized()
{
  return (m_redirection_address.has_value() || m_http_server.has_value());
}

std::optional<Common::IPv4Port> Camera::GetAddress() const
{
  if (m_http_server)
    return m_http_server->GetAddress();
  return m_redirection_address;
}

}  // namespace Triforce
