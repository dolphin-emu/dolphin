// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/MulticastDNS.h"

#ifndef _WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif
#define closesocket close
#else
#include <winsock2.h>
#include <ws2ipdef.h>
#include <ws2tcpip.h>
using socklen_t = int;
#endif

#include "Common/Logging/Log.h"
#include "Common/Network.h"

namespace
{
constexpr Common::IPAddress MULTICAST_IP = {224, 0, 0, 251};
constexpr u16 MULTICAST_PORT = 5353;

bool ParseFixedData(std::span<const u8>* payload, std::size_t length, std::span<const u8>* result)
{
  if (payload->size() < length)
    return false;

  *result = Common::AsU8Span(payload->subspan(0, length));
  *payload = payload->subspan(length);
  return true;
}

template <typename T>
bool ParseBigEndianValue(std::span<const u8>* payload, T* result)
{
  std::span<const u8> big_endian_value;
  if (ParseFixedData(payload, sizeof(T), &big_endian_value))
  {
    using BE = Common::BigEndianValue<T>;
    *result = BE(Common::BitCastPtr<BE>(big_endian_value.data()));
    return true;
  }
  return false;
}

template <typename T>
bool ParseBigEndianEnum(std::span<const u8>* payload, T* result)
{
  std::underlying_type_t<T> value{};
  return ParseBigEndianValue(payload, &value) && (*result = T(value), true);
}

template <std::integral LengthType = u8>
bool ParseVariableLengthData(std::span<const u8>* payload, std::span<const u8>* result)
{
  LengthType len{};
  return ParseBigEndianValue(payload, &len) && ParseFixedData(payload, len, result);
}

}  // namespace

namespace MulticastDNS
{

void PacketBuilder::AddQuestion(QueryData question)
{
  m_data.append_range(question.resource_name);
  m_data.append_range(
      Common::AsU8Span(Common::BigEndianValue(std::to_underlying(question.record_type))));
  m_data.append_range(
      Common::AsU8Span(Common::BigEndianValue(std::to_underlying(question.class_code))));
  ++m_question_count;
}

const std::vector<u8>& PacketBuilder::GetData(u16 transaction_id, u16 flags)
{
  DNSHeader header{};
  header.transaction_id = transaction_id;
  header.flags = flags;
  header.question_count = m_question_count;
  header.answer_rr_count = m_answer_rr_count;
  header.authority_rr_count = m_authority_rr_count;
  header.additional_rr_count = m_additional_rr_count;

  std::ranges::copy(Common::AsU8Span(header), m_data.data());
  return m_data;
}

void AppendEncodedString(std::vector<u8>* buffer, std::string_view str)
{
  buffer->emplace_back(u8(str.size()));
  buffer->append_range(Common::AsU8Span(str));
}

std::vector<u8> EncodeResourceName(std::string_view resource_name)
{
  std::vector<u8> buffer;
  buffer.reserve(resource_name.size() + 2);
  while (true)
  {
    const auto label_end = resource_name.find('.');
    const auto label = resource_name.substr(0, label_end);
    AppendEncodedString(&buffer, label);
    if (label_end == std::string_view::npos)
      break;
    resource_name = resource_name.substr(label_end + 1);
  }

  buffer.emplace_back(u8{});
  return buffer;
}

std::pair<std::string_view, std::string_view> SplitKeyValuePair(std::string_view input)
{
  std::pair<std::string_view, std::string_view> result;
  const auto eq_pos = input.find_first_of('=');
  result.first = input.substr(0, eq_pos);
  if (eq_pos != std::string_view::npos)
    result.second = input.substr(eq_pos + 1);
  return result;
}

std::optional<std::string> DecodeResourceName(std::span<const u8> payload)
{
  std::optional<std::string> result;
  result.emplace();
  result->reserve(payload.size());

  if (std::string_view first_part; ParseVariableLengthString(&payload, &first_part))
    *result = first_part;
  else
    return std::nullopt;

  while (true)
  {
    std::string_view part;
    if (!ParseVariableLengthString(&payload, &part))
      return std::nullopt;

    if (part.empty())
      return result;

    *result += '.';
    *result += part;
  }
}

bool ParseVariableLengthString(std::span<const u8>* payload, std::string_view* result)
{
  u8 len{};
  std::span<const u8> sresult;
  return ParseBigEndianValue(payload, &len) && ParseFixedData(payload, len, &sresult) &&
         (*result = std::string_view{reinterpret_cast<const char*>(sresult.data()), sresult.size()},
          true);
}

bool ParseResourceName(std::span<const u8>* payload, std::span<const u8>* result)
{
  const auto payload_copy = *payload;
  u8 len{};
  while (ParseBigEndianValue(payload, &len) && (payload->size() >= len))
  {
    *payload = payload->subspan(len);
    if (len == 0)
    {
      *result = Common::AsU8Span(payload_copy.subspan(0, payload_copy.size() - payload->size()));
      return true;
    }
  }

  return false;
}

bool ParseQuery(std::span<const u8>* payload, QueryData* result)
{
  return ParseResourceName(payload, &result->resource_name) &&
         ParseBigEndianEnum(payload, &result->record_type) &&
         ParseBigEndianEnum(payload, &result->class_code);
}

bool ParseRecord(std::span<const u8>* payload, GenericRecordData* result)
{
  return ParseResourceName(payload, &result->resource_name) &&
         ParseBigEndianEnum(payload, &result->record_type) &&
         ParseBigEndianEnum(payload, &result->class_code) &&
         ParseBigEndianValue(payload, &result->ttl) &&
         ParseVariableLengthData<u16>(payload, &result->data);
}

QuerySocket::QuerySocket()
{
  m_sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (m_sock < 0)
  {
    ERROR_LOG_FMT(COMMON, "Failed to create socket.");
    return;
  }

  constexpr int reuse = 1;
  if (setsockopt(m_sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse),
                 sizeof(reuse)) < 0)
  {
    ERROR_LOG_FMT(COMMON, "Failed to set SO_REUSEADDR on socket: {}", Common::StrNetworkError());
  };

  sockaddr_in addr{
      .sin_family = AF_INET,
      .sin_port = htons(MULTICAST_PORT),
  };
  addr.sin_addr.s_addr = INADDR_ANY;

  if (bind(m_sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
  {
    ERROR_LOG_FMT(COMMON, "Failed to bind to port {}.", MULTICAST_PORT);
    closesocket(m_sock);
    m_sock = -1;
    return;
  }

  ip_mreq membership{
      .imr_multiaddr = std::bit_cast<in_addr>(MULTICAST_IP),
  };
  membership.imr_interface.s_addr = INADDR_ANY;

  if (setsockopt(m_sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &membership, sizeof(membership)) < 0)
  {
    ERROR_LOG_FMT(COMMON, "Failed join multicast group.");
    closesocket(m_sock);
    m_sock = -1;
    return;
  }
}

QuerySocket::~QuerySocket()
{
  closesocket(m_sock);
}

bool QuerySocket::IsOpen() const
{
  return m_sock != -1;
}

void QuerySocket::Send(std::span<const u8> data)
{
  sockaddr_in dest{
      .sin_family = AF_INET,
      .sin_port = htons(MULTICAST_PORT),
      .sin_addr = std::bit_cast<in_addr>(MULTICAST_IP),
  };
  sendto(m_sock, reinterpret_cast<const char*>(data.data()), static_cast<int>(data.size()), 0,
         reinterpret_cast<sockaddr*>(&dest), sizeof(dest));
}

int QuerySocket::ReceiveFrom(DT timeout, Common::UniqueBuffer<u8>* buffer,
                             Common::IPAddress* ip_addr)
{
#if defined(_WIN32)
  DWORD tv = duration_cast<std::chrono::milliseconds>(timeout).count();
  if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv)) < 0)
#else
  timeval tv{.tv_usec = duration_cast<std::chrono::microseconds>(timeout).count()};
  if (setsockopt(m_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
#endif
  {
    ERROR_LOG_FMT(COMMON, "Failed to set SO_RCVTIMEO on socket: {}", Common::StrNetworkError());
  };

  sockaddr_in from{};
  socklen_t from_len = sizeof(from);

  constexpr std::size_t max_size = 1472;
  if (buffer->size() < max_size)
    buffer->reset(max_size);

  const int result =
      recvfrom(m_sock, reinterpret_cast<char*>(buffer->data()), static_cast<int>(buffer->size()), 0,
               reinterpret_cast<sockaddr*>(&from), &from_len);
  if (result >= 0)
    *ip_addr = std::bit_cast<Common::IPAddress>(from.sin_addr);

  return result;
}

}  // namespace MulticastDNS
