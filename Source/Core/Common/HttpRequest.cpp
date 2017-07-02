// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/HttpRequest.h"

#include <chrono>
#include <cstddef>
#include <curl/curl.h>

#include "Common/Logging/Log.h"
#include "Common/ScopeGuard.h"
#include "Common/StringUtil.h"

namespace Common
{
class HttpRequest::Impl final
{
public:
  enum class Method
  {
    GET,
    POST,
  };

  explicit Impl(std::chrono::milliseconds timeout_ms);

  bool IsValid() const;
  Response Fetch(const std::string& url, Method method, const Headers& headers, const u8* payload,
                 size_t size);

private:
  std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> m_curl{curl_easy_init(), curl_easy_cleanup};
};

HttpRequest::HttpRequest(std::chrono::milliseconds timeout_ms)
    : m_impl(std::make_unique<Impl>(timeout_ms))
{
}

HttpRequest::~HttpRequest() = default;

bool HttpRequest::IsValid() const
{
  return m_impl->IsValid();
}

HttpRequest::Response HttpRequest::Get(const std::string& url, const Headers& headers)
{
  return m_impl->Fetch(url, Impl::Method::GET, headers, nullptr, 0);
}

HttpRequest::Response HttpRequest::Post(const std::string& url, const std::vector<u8>& payload,
                                        const Headers& headers)
{
  return m_impl->Fetch(url, Impl::Method::POST, headers, payload.data(), payload.size());
}

HttpRequest::Response HttpRequest::Post(const std::string& url, const std::string& payload,
                                        const Headers& headers)
{
  return m_impl->Fetch(url, Impl::Method::POST, headers,
                       reinterpret_cast<const u8*>(payload.data()), payload.size());
}

HttpRequest::Impl::Impl(std::chrono::milliseconds timeout_ms)
{
  if (!m_curl)
    return;

  // libcurl may not have been built with async DNS support, so we disable
  // signal handlers to avoid a possible and likely crash if a resolve times out.
  curl_easy_setopt(m_curl.get(), CURLOPT_NOSIGNAL, true);
  curl_easy_setopt(m_curl.get(), CURLOPT_TIMEOUT_MS, static_cast<long>(timeout_ms.count()));
#ifdef _WIN32
  // ALPN support is enabled by default but requires Windows >= 8.1.
  curl_easy_setopt(m_curl.get(), CURLOPT_SSL_ENABLE_ALPN, false);
#endif
}

bool HttpRequest::Impl::IsValid() const
{
  return m_curl != nullptr;
}

static size_t CurlCallback(char* data, size_t size, size_t nmemb, void* userdata)
{
  auto* buffer = static_cast<std::vector<u8>*>(userdata);
  const size_t actual_size = size * nmemb;
  buffer->insert(buffer->end(), data, data + actual_size);
  return actual_size;
}

HttpRequest::Response HttpRequest::Impl::Fetch(const std::string& url, Method method,
                                               const Headers& headers, const u8* payload,
                                               size_t size)
{
  curl_easy_setopt(m_curl.get(), CURLOPT_POST, method == Method::POST);
  curl_easy_setopt(m_curl.get(), CURLOPT_URL, url.c_str());
  if (method == Method::POST)
  {
    curl_easy_setopt(m_curl.get(), CURLOPT_POSTFIELDS, payload);
    curl_easy_setopt(m_curl.get(), CURLOPT_POSTFIELDSIZE, size);
  }

  curl_slist* list = nullptr;
  Common::ScopeGuard list_guard{[&list] { curl_slist_free_all(list); }};
  for (const std::pair<std::string, std::optional<std::string>>& header : headers)
  {
    if (!header.second)
      list = curl_slist_append(list, (header.first + ":").c_str());
    else if (header.second->empty())
      list = curl_slist_append(list, (header.first + ";").c_str());
    else
      list = curl_slist_append(list, (header.first + ": " + *header.second).c_str());
  }
  curl_easy_setopt(m_curl.get(), CURLOPT_HTTPHEADER, list);

  std::vector<u8> buffer;
  curl_easy_setopt(m_curl.get(), CURLOPT_WRITEFUNCTION, CurlCallback);
  curl_easy_setopt(m_curl.get(), CURLOPT_WRITEDATA, &buffer);

  const char* type = method == Method::POST ? "POST" : "GET";
  const CURLcode res = curl_easy_perform(m_curl.get());
  if (res != CURLE_OK)
  {
    ERROR_LOG(COMMON, "Failed to %s %s: %s", type, url.c_str(), curl_easy_strerror(res));
    return {};
  }

  long response_code = 0;
  curl_easy_getinfo(m_curl.get(), CURLINFO_RESPONSE_CODE, &response_code);
  if (response_code != 200)
  {
    ERROR_LOG(COMMON, "Failed to %s %s: server replied with code %li and body\n\x1b[0m%s", type,
              url.c_str(), response_code, buffer.data());
    return {};
  }

  return buffer;
}
}  // namespace Common
