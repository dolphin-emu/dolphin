#include "TriforceCamera.h"

#include <optional>

#include <fmt/format.h>
#include <fmt/ranges.h>
#include <stb_image.h>
#include <stb_image_resize2.h>
#include <stb_image_write.h>

#include "Common/Assert.h"
#include "Common/Config/Config.h"
#include "Common/IOFile.h"
#include "Common/Network.h"
#include "Common/ScopeGuard.h"
#include "Core/Config/MainSettings.h"
#include "Core/HW/DVD/AMMediaboard.h"

namespace
{
std::optional<TriforceCameraInstance> s_instance;
}

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

static std::optional<std::vector<u8>> MakeJPEGCompliant(std::vector<u8> input)
{
  int width = 0;
  int height = 0;
  int channels = 0;
  u8* image = stbi_load_from_memory(input.data(), static_cast<int>(input.size()), &width, &height,
                                    &channels, 3);
  if (!image)
    return std::nullopt;
  Common::ScopeGuard guard([image] { stbi_image_free(image); });

  constexpr int target_width = 320;
  constexpr int target_height = 240;
  std::vector<u8> image_resized;
  u8* pixels;
  if (width != target_width || height != target_height)
  {
    image_resized.resize(target_width * target_height * 3);
    if (!stbir_resize_uint8_srgb(image, width, height, 0, image_resized.data(), target_width,
                                 target_height, 0, STBIR_RGB))
    {
      return std::nullopt;
    }
    pixels = image_resized.data();
  }
  else
  {
    pixels = image;
  }

  std::vector<u8> output;
  stbi_write_func* write_func = [](void* context, void* data, int size) {
    auto* vec = reinterpret_cast<std::vector<u8>*>(context);
    vec->insert(vec->end(), reinterpret_cast<u8*>(data), reinterpret_cast<u8*>(data) + size);
  };

  if (!stbi_write_jpg_to_func(write_func, &output, target_width, target_height, 3, pixels, 100))
  {
    return std::nullopt;
  }

  return output;
}

TriforceCameraInstance::TriforceCameraInstance()
{
  Recreate();
}

void TriforceCameraInstance::Recreate()
{
  m_redirection_address.reset();
  m_http_server.reset();

  if (!Config::Get(Config::MAIN_TRIFORCE_INTEGRATED_CAMERA))
  {
    Config::SetBaseOrCurrent(
        Config::MAIN_TRIFORCE_IP_REDIRECTIONS,
        fmt::format("{}", fmt::join(RedirectionsWithoutIntegratedCamera(), ",")));
    // TODO: We assume 192.168.29.104 is always this machine's camera. Is this true?
    const Common::IPv4Port local_camera_address = {.ip_address = {192, 168, 29, 104}, .port = 0};
    for (auto&& redirection : AMMediaboard::GetIPRedirections())
    {
      if (redirection.original.IsMatch(local_camera_address))
        m_redirection_address = redirection.Apply(local_camera_address);
    }
    return;
  }

  const std::string ip =
      fmt::format("{}:{}", Config::Get(Config::MAIN_TRIFORCE_INTEGRATED_CAMERA_SERVER_IP),
                  Config::Get(Config::MAIN_TRIFORCE_INTEGRATED_CAMERA_SERVER_PORT));

  const std::optional<Common::IPv4Port> address = Common::StringToIPv4(ip);

  if (!address)
  {
    ERROR_LOG_FMT(CORE, "Failed to determine IP address for Triforce Camera server");
    return;
  }

  m_http_server.emplace(*address);

  m_http_server->ServePath("/img.jpg", []() {
    std::vector<u8> response;

    // TODO: Support camera devices. For now, only static images are supported.
    {
      File::IOFile file(Config::Get(Config::MAIN_TRIFORCE_INTEGRATED_CAMERA_STATIC_IMAGE), "rb",
                        File::SharedAccess::Read);

      if (file)
      {
        std::vector<u8> contents(static_cast<size_t>(file.GetSize()));
        file.ReadBytes(contents.data(), contents.size());
        const auto compliant = MakeJPEGCompliant(std::move(contents));
        if (compliant)
          response = std::move(*compliant);
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

std::optional<Common::IPv4Port> TriforceCameraInstance::GetAddress() const
{
  if (m_http_server)
    return m_http_server->GetAddress();
  return m_redirection_address;
}

namespace TriforceCamera
{
void Init()
{
  if (!s_instance)
    s_instance.emplace();
}

void Shutdown()
{
  s_instance.reset();
}

TriforceCameraInstance& GetInstance()
{
  ASSERT(IsInitialized());
  return *s_instance;
}

bool IsInitialized()
{
  return s_instance.has_value();
}
}  // namespace TriforceCamera
