// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/WiimoteReal/IOAndroid.h"

#include <jni.h>

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

void WiimoteScannerAndroid::FindWiimotes(std::vector<Wiimote*>& found_wiimotes,
                                         Wiimote*& found_board)
{
  found_wiimotes.clear();
  found_board = nullptr;

  NOTICE_LOG_FMT(WIIMOTE, "Finding Wiimotes");

  JNIEnv* env = IDCache::GetEnvForThread();

  jmethodID openadapter_func = env->GetStaticMethodID(s_adapter_class, "OpenAdapter", "()Z");
  jmethodID queryadapter_func = env->GetStaticMethodID(s_adapter_class, "QueryAdapter", "()Z");

  if (env->CallStaticBooleanMethod(s_adapter_class, queryadapter_func) &&
      env->CallStaticBooleanMethod(s_adapter_class, openadapter_func))
  {
    for (int i = 0; i < MAX_WIIMOTES; ++i)
      found_wiimotes.emplace_back(new WiimoteAndroid(i));
  }
}

WiimoteAndroid::WiimoteAndroid(int index) : Wiimote(), m_mayflash_index(index)
{
}

WiimoteAndroid::~WiimoteAndroid()
{
  Shutdown();
}

// Connect to a Wiimote with a known address.
bool WiimoteAndroid::ConnectInternal()
{
  m_env = IDCache::GetEnvForThread();

  jfieldID payload_field = m_env->GetStaticFieldID(s_adapter_class, "wiimote_payload", "[[B");
  jobjectArray payload_object =
      reinterpret_cast<jobjectArray>(m_env->GetStaticObjectField(s_adapter_class, payload_field));
  m_java_wiimote_payload =
      (jbyteArray)m_env->GetObjectArrayElement(payload_object, m_mayflash_index);

  // Get function pointers
  m_input_func = m_env->GetStaticMethodID(s_adapter_class, "Input", "(I)I");
  m_output_func = m_env->GetStaticMethodID(s_adapter_class, "Output", "(I[BI)I");

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
  int read_size = m_env->CallStaticIntMethod(s_adapter_class, m_input_func, m_mayflash_index);
  if (read_size > 0)
  {
    jbyte* java_data = m_env->GetByteArrayElements(m_java_wiimote_payload, nullptr);
    memcpy(buf + 1, java_data, std::min(MAX_PAYLOAD - 1, read_size));
    buf[0] = 0xA1;
    m_env->ReleaseByteArrayElements(m_java_wiimote_payload, java_data, 0);
  }
  return read_size <= 0 ? read_size : read_size + 1;
}

int WiimoteAndroid::IOWrite(u8 const* buf, size_t len)
{
  jbyteArray output_array = m_env->NewByteArray(len);
  jbyte* output = m_env->GetByteArrayElements(output_array, nullptr);
  memcpy(output, buf, len);
  m_env->ReleaseByteArrayElements(output_array, output, 0);
  int written = m_env->CallStaticIntMethod(s_adapter_class, m_output_func, m_mayflash_index,
                                           output_array, len);
  m_env->DeleteLocalRef(output_array);
  return written;
}

void InitAdapterClass()
{
  JNIEnv* env = IDCache::GetEnvForThread();
  jclass adapter_class = env->FindClass("org/dolphinemu/dolphinemu/utils/Java_WiimoteAdapter");
  s_adapter_class = reinterpret_cast<jclass>(env->NewGlobalRef(adapter_class));
}
}  // namespace WiimoteReal
