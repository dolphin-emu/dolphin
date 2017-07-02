// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cmath>
#include <cstdio>
#include <string>

#include "Common/Analytics.h"
#include "Common/CommonTypes.h"
#include "Common/StringUtil.h"
#include "Common/Thread.h"

namespace Common
{
namespace
{
// Format version number, used as the first byte of every report sent.
// Increment for any change to the wire format.
constexpr u8 WIRE_FORMAT_VERSION = 0;

// Identifiers for the value types supported by the analytics reporting wire
// format.
enum class TypeId : u8
{
  STRING = 0,
  BOOL = 1,
  UINT = 2,
  SINT = 3,
  FLOAT = 4,
};

void AppendBool(std::string* out, bool v)
{
  out->push_back(v ? '\xFF' : '\x00');
}

void AppendVarInt(std::string* out, u64 v)
{
  do
  {
    u8 current_byte = v & 0x7F;
    v >>= 7;
    current_byte |= (!!v) << 7;
    out->push_back(current_byte);
  } while (v);
}

void AppendBytes(std::string* out, const u8* bytes, u32 length, bool encode_length = true)
{
  if (encode_length)
  {
    AppendVarInt(out, length);
  }
  out->append(reinterpret_cast<const char*>(bytes), length);
}

void AppendType(std::string* out, TypeId type)
{
  out->push_back(static_cast<u8>(type));
}
}  // namespace

AnalyticsReportBuilder::AnalyticsReportBuilder()
{
  m_report.push_back(WIRE_FORMAT_VERSION);
}

void AnalyticsReportBuilder::AppendSerializedValue(std::string* report, const std::string& v)
{
  AppendType(report, TypeId::STRING);
  AppendBytes(report, reinterpret_cast<const u8*>(v.data()), static_cast<u32>(v.size()));
}

void AnalyticsReportBuilder::AppendSerializedValue(std::string* report, const char* v)
{
  AppendSerializedValue(report, std::string(v));
}

void AnalyticsReportBuilder::AppendSerializedValue(std::string* report, bool v)
{
  AppendType(report, TypeId::BOOL);
  AppendBool(report, v);
}

void AnalyticsReportBuilder::AppendSerializedValue(std::string* report, u64 v)
{
  AppendType(report, TypeId::UINT);
  AppendVarInt(report, v);
}

void AnalyticsReportBuilder::AppendSerializedValue(std::string* report, s64 v)
{
  AppendType(report, TypeId::SINT);
  AppendBool(report, v >= 0);
  AppendVarInt(report, static_cast<u64>(std::abs(v)));
}

void AnalyticsReportBuilder::AppendSerializedValue(std::string* report, u32 v)
{
  AppendSerializedValue(report, static_cast<u64>(v));
}

void AnalyticsReportBuilder::AppendSerializedValue(std::string* report, s32 v)
{
  AppendSerializedValue(report, static_cast<s64>(v));
}

void AnalyticsReportBuilder::AppendSerializedValue(std::string* report, float v)
{
  AppendType(report, TypeId::FLOAT);
  AppendBytes(report, reinterpret_cast<u8*>(&v), sizeof(v), false);
}

AnalyticsReporter::AnalyticsReporter()
{
  m_reporter_thread = std::thread(&AnalyticsReporter::ThreadProc, this);
}

AnalyticsReporter::~AnalyticsReporter()
{
  // Set the exit request flag and wait for the thread to honor it.
  m_reporter_stop_request.Set();
  m_reporter_event.Set();
  m_reporter_thread.join();
}

void AnalyticsReporter::Send(AnalyticsReportBuilder&& report)
{
#if defined(USE_ANALYTICS) && USE_ANALYTICS
  // Put a bound on the size of the queue to avoid uncontrolled memory growth.
  constexpr u32 QUEUE_SIZE_LIMIT = 25;
  if (m_reports_queue.Size() < QUEUE_SIZE_LIMIT)
  {
    m_reports_queue.Push(report.Consume());
    m_reporter_event.Set();
  }
#endif
}

void AnalyticsReporter::ThreadProc()
{
  Common::SetCurrentThreadName("Analytics");
  while (true)
  {
    m_reporter_event.Wait();
    if (m_reporter_stop_request.IsSet())
    {
      return;
    }

    while (!m_reports_queue.Empty())
    {
      std::shared_ptr<AnalyticsReportingBackend> backend(m_backend);

      if (backend)
      {
        std::string report;
        m_reports_queue.Pop(report);
        backend->Send(std::move(report));
      }
      else
      {
        break;
      }

      // Recheck after each report sent.
      if (m_reporter_stop_request.IsSet())
      {
        return;
      }
    }
  }
}

void StdoutAnalyticsBackend::Send(std::string report)
{
  printf("Analytics report sent:\n%s",
         HexDump(reinterpret_cast<const u8*>(report.data()), report.size()).c_str());
}

HttpAnalyticsBackend::HttpAnalyticsBackend(const std::string& endpoint) : m_endpoint(endpoint)
{
}

HttpAnalyticsBackend::~HttpAnalyticsBackend() = default;

void HttpAnalyticsBackend::Send(std::string report)
{
  if (m_http.IsValid())
    m_http.Post(m_endpoint, report);
}

}  // namespace Common
