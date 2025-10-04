// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/WiimoteReal/IOAndroid.h"

#include <jni.h>

#include <fmt/format.h>

#include "Common/CommonTypes.h"
#include "Common/Event.h"
#include "Common/Flag.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Common/Thread.h"
#include "Common/Timer.h"

#include "jni/AndroidCommon/IDCache.h"

namespace WiimoteReal
{
// Java classes
static jclass s_adapter_class;

auto WiimoteScannerAndroid::FindAttachedWiimotes() -> FindResults
{
  FindResults results;

  NOTICE_LOG_FMT(WIIMOTE, "Finding Wiimotes");

  JNIEnv* env = IDCache::GetEnvForThread();

  jmethodID openadapter_func = env->GetStaticMethodID(s_adapter_class, "openAdapter", "()Z");
  jmethodID queryadapter_func = env->GetStaticMethodID(s_adapter_class, "queryAdapter", "()Z");

  if (env->CallStaticBooleanMethod(s_adapter_class, queryadapter_func) &&
      env->CallStaticBooleanMethod(s_adapter_class, openadapter_func))
  {
    for (int i = 0; i < MAX_WIIMOTES; ++i)
    {
      if (!IsNewWiimote(WiimoteAndroid::GetIdFromDolphinBarIndex(i)))
        continue;

      auto wiimote = std::make_unique<WiimoteAndroid>(i);

      if (!wiimote->ConnectInternal())
        continue;

      // TODO: We make no attempt to differentiate balance boards here.
      // wiimote->IsBalanceBoard() would probably be enough to do that.

      results.wii_remotes.emplace_back(std::move(wiimote));
    }
  }

  return results;
}

WiimoteAndroid::WiimoteAndroid(int index) : Wiimote(), m_mayflash_index(index)
{
}

WiimoteAndroid::~WiimoteAndroid()
{
  Shutdown();
}

std::string WiimoteAndroid::GetId() const
{
  return GetIdFromDolphinBarIndex(m_mayflash_index);
}

std::string WiimoteAndroid::GetIdFromDolphinBarIndex(int index)
{
  return fmt::format("Android {}", index);
}

// Connect to a Wiimote with a known address.
bool WiimoteAndroid::ConnectInternal()
{
  if (IsConnected())
    return true;

  auto* const env = IDCache::GetEnvForThread();

  jfieldID payload_field = env->GetStaticFieldID(s_adapter_class, "wiimotePayload", "[[B");
  jobjectArray payload_object =
      reinterpret_cast<jobjectArray>(env->GetStaticObjectField(s_adapter_class, payload_field));
  m_java_wiimote_payload = (jbyteArray)env->GetObjectArrayElement(payload_object, m_mayflash_index);

  // Get function pointers
  m_input_func = env->GetStaticMethodID(s_adapter_class, "input", "(I)I");
  m_output_func = env->GetStaticMethodID(s_adapter_class, "output", "(I[BI)I");

  // Test a write to see if a remote is actually connected to the DolphinBar.
  constexpr u8 report[] = {WR_SET_REPORT | BT_OUTPUT,
                           u8(WiimoteCommon::OutputReportID::RequestStatus), 0};
  if (IOWrite(report, sizeof(report)) <= 0)
    return false;

  is_connected = true;

  return true;
}

void WiimoteAndroid::DisconnectInternal()
{
}

bool WiimoteAndroid::IsConnected() const
{
  return is_connected;
}

// positive = read packet
// negative = didn't read packet
// zero = error
int WiimoteAndroid::IORead(u8* buf)
{
  auto* const env = IDCache::GetEnvForThread();

  int read_size = env->CallStaticIntMethod(s_adapter_class, m_input_func, m_mayflash_index);
  if (read_size > 0)
  {
    jbyte* java_data = env->GetByteArrayElements(m_java_wiimote_payload, nullptr);
    memcpy(buf + 1, java_data, std::min(MAX_PAYLOAD - 1, read_size));
    buf[0] = 0xA1;
    env->ReleaseByteArrayElements(m_java_wiimote_payload, java_data, 0);
  }
  return read_size <= 0 ? read_size : read_size + 1;
}

int WiimoteAndroid::IOWrite(u8 const* buf, size_t len)
{
  auto* const env = IDCache::GetEnvForThread();

  jbyteArray output_array = env->NewByteArray(len);
  jbyte* output = env->GetByteArrayElements(output_array, nullptr);
  memcpy(output, buf, len);
  env->ReleaseByteArrayElements(output_array, output, 0);
  int written =
      env->CallStaticIntMethod(s_adapter_class, m_output_func, m_mayflash_index, output_array, len);
  env->DeleteLocalRef(output_array);
  return written;
}

void InitAdapterClass()
{
  JNIEnv* env = IDCache::GetEnvForThread();
  jclass adapter_class = env->FindClass("org/dolphinemu/dolphinemu/utils/WiimoteAdapter");
  s_adapter_class = reinterpret_cast<jclass>(env->NewGlobalRef(adapter_class));
}
}  // namespace WiimoteReal
