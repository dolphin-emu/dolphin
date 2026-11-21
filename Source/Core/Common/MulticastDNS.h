// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <concepts>
#include <span>
#include <vector>

#include "Common/BitUtils.h"
#include "Common/Buffer.h"
#include "Common/Network.h"
#include "Common/Swap.h"

namespace MulticastDNS
{

struct DNSHeader
{
  Common::BigEndianValue<u16> transaction_id;
  Common::BigEndianValue<u16> flags;
  Common::BigEndianValue<u16> question_count;
  Common::BigEndianValue<u16> answer_rr_count;
  Common::BigEndianValue<u16> authority_rr_count;
  Common::BigEndianValue<u16> additional_rr_count;
};
static_assert(sizeof(DNSHeader) == 12);

enum class RecordType : u16
{
  PTR = 12,  // Pointer
  TXT = 16,  // Text
  ANY = 255,
};

enum class ClassCode : u16
{
  IN = 1,  // Internet
};

struct QueryData
{
  std::span<const u8> resource_name;
  RecordType record_type = RecordType::ANY;
  ClassCode class_code = ClassCode::IN;  // FYI: High bit is "unicast-response is desired".
};

struct GenericRecordData
{
  std::span<const u8> resource_name;
  RecordType record_type = RecordType::ANY;
  ClassCode class_code = ClassCode::IN;  // FYI: High bit is "purged outdated cached records".
  u32 ttl{};

  std::span<const u8> data{};
};

class PacketBuilder
{
public:
  PacketBuilder()
  {
    m_data.reserve(1024);
    m_data.resize(sizeof(DNSHeader));
  }

  // Callers *must* Add in order: Question, Answer, Authority, Additional.

  void AddQuestion(QueryData question);

  void AddAnswerRecord(const auto& record_data)
  {
    AddRecordImpl(record_data);
    ++m_answer_rr_count;
  }

  void AddAuthorityRecord(const auto& record_data)
  {
    AddRecordImpl(record_data);
    ++m_authority_rr_count;
  }

  void AddAdditionalRecord(const auto& record_data)
  {
    AddRecordImpl(record_data);
    ++m_additional_rr_count;
  }

  const std::vector<u8>& GetData(u16 transaction_id, u16 flags);

private:
  void AddRecordImpl(const auto& record_data)
  {
    m_data.append_range(record_data.resource_name);
    m_data.append_range(
        Common::AsU8Span(Common::BigEndianValue(std::to_underlying(record_data.record_type))));
    m_data.append_range(
        Common::AsU8Span(Common::BigEndianValue(std::to_underlying(record_data.class_code))));
    m_data.append_range(Common::AsU8Span(Common::BigEndianValue(record_data.ttl)));

    const auto pos = m_data.size();
    m_data.resize(m_data.size() + sizeof(u16));

    record_data.EncodeData(&m_data);
    const auto data_size = m_data.size() - pos - sizeof(u16);

    std::ranges::copy(Common::AsU8Span(Common::BigEndianValue(u16(data_size))),
                      m_data.data() + pos);
  }

  u16 m_question_count{};
  u16 m_answer_rr_count{};
  u16 m_authority_rr_count{};
  u16 m_additional_rr_count{};

  std::vector<u8> m_data;
};

// Produces the prefix-length wire-encoded data for a "foo.bar.local" looking string.
[[nodiscard]] std::vector<u8> EncodeResourceName(std::string_view resource_name);

// Append a prefix-length wire-encoded string to an existing buffer.
void AppendEncodedString(std::vector<u8>* buffer, std::string_view str);

// Splits the input on the first '=' character.
std::pair<std::string_view, std::string_view> SplitKeyValuePair(std::string_view input);

// Decodes the prefix-length wire-encoded data into a "foo.bar.local" looking string.
std::optional<std::string> DecodeResourceName(std::span<const u8> payload);

bool ParseVariableLengthString(std::span<const u8>* payload, std::string_view* result);
bool ParseResourceName(std::span<const u8>* payload, std::span<const u8>* result);
bool ParseQuery(std::span<const u8>* payload, QueryData* result);
bool ParseRecord(std::span<const u8>* payload, GenericRecordData* result);

enum class RecordCategory
{
  Answer,
  Authority,
  Additional,
};

bool DecodePacket(std::span<const u8> packet, std::invocable<QueryData> auto&& query_handler,
                  std::invocable<RecordCategory, GenericRecordData> auto&& record_handler)
{
  if (packet.size() < sizeof(DNSHeader))
    return false;

  const DNSHeader header = Common::BitCastPtr<DNSHeader>(packet.data());

  packet = packet.subspan(sizeof(DNSHeader));

  for (u16 i = header.question_count; i != 0; --i)
  {
    QueryData query;
    if (!ParseQuery(&packet, &query))
      return false;
    query_handler(query);
  }
  for (u16 i = header.answer_rr_count; i != 0; --i)
  {
    GenericRecordData record;
    if (!ParseRecord(&packet, &record))
      return false;
    record_handler(RecordCategory::Answer, record);
  }
  for (u16 i = header.authority_rr_count; i != 0; --i)
  {
    GenericRecordData record;
    if (!ParseRecord(&packet, &record))
      return false;
    record_handler(RecordCategory::Authority, record);
  }
  for (u16 i = header.additional_rr_count; i != 0; --i)
  {
    GenericRecordData record;
    if (!ParseRecord(&packet, &record))
      return false;
    record_handler(RecordCategory::Additional, record);
  }

  return true;
}

struct PTRRecordData
{
  static constexpr RecordType record_type = RecordType::PTR;

  std::span<const u8> resource_name{};
  ClassCode class_code = ClassCode::IN;
  u32 ttl{};

  std::span<const u8> target{};

  explicit operator bool() const { return resource_name.data() != nullptr; }

  void EncodeData(std::vector<u8>* buffer) const { buffer->append_range(target); }

  static PTRRecordData TryParse(GenericRecordData generic_record)
  {
    if (generic_record.record_type != record_type)
      return {};

    return {
        .resource_name = generic_record.resource_name,
        .class_code = generic_record.class_code,
        .ttl = generic_record.ttl,
        .target = generic_record.data,
    };
  }
};

struct TXTRecordData
{
  static constexpr RecordType record_type = RecordType::TXT;

  std::span<const u8> resource_name{};
  ClassCode class_code = ClassCode::IN;
  u32 ttl{};

  std::span<const u8> data{};

  explicit operator bool() const { return resource_name.data() != nullptr; }

  void EncodeData(std::vector<u8>* buffer) const { buffer->append_range(data); }

  static TXTRecordData TryParse(GenericRecordData generic_record)
  {
    if (generic_record.record_type != record_type)
      return {};

    return {
        .resource_name = generic_record.resource_name,
        .class_code = generic_record.class_code,
        .ttl = generic_record.ttl,
        .data = generic_record.data,
    };
  }
};

class QuerySocket
{
public:
  QuerySocket();
  ~QuerySocket();

  bool IsOpen() const;

  QuerySocket& operator=(const QuerySocket&) = delete;
  QuerySocket& operator=(QuerySocket&&) = delete;
  QuerySocket(const QuerySocket&) = delete;
  QuerySocket(QuerySocket&&) = delete;

  void Send(std::span<const u8> data);

  int ReceiveFrom(DT timeout, Common::UniqueBuffer<u8>* buffer, Common::IPAddress* ip_addr);

private:
  int m_sock = -1;
};

}  // namespace MulticastDNS
