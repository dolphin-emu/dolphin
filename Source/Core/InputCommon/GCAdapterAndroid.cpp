// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/GCAdapterBackend.h"

#include <atomic>
#include <mutex>
#include <shared_mutex>

#include <jni.h>

#include "Common/Assert.h"
#include "Common/Logging/Log.h"

#include "jni/AndroidCommon/IDCache.h"

namespace GCAdapter
{
namespace
{
static class AndroidBackend* s_backend = nullptr;

class AndroidBackend final : public Backend
{
public:
  AndroidBackend(Callbacks callbacks) : Backend{std::move(callbacks)}
  {
    // Note: Multiple AndroidBackend are currently not allowed and will do bad things.
    // This could probably be rectified if Kotlin's GCAdapter had non-static members
    //  and the hotplug callbacks had a userdata parameter.
    ASSERT(s_backend == nullptr);
    s_backend = this;

    JNIEnv* env = IDCache::GetEnvForThread();
    const jclass local_class = env->FindClass("org/dolphinemu/dolphinemu/utils/GCAdapter");
    if (!local_class)
      return;

    m_adapter_class = reinterpret_cast<jclass>(env->NewGlobalRef(local_class));
    env->DeleteLocalRef(local_class);

    if (!m_adapter_class)
      return;

    m_payload_field = env->GetStaticFieldID(m_adapter_class, "controllerPayload", "[B");

    m_open_adapter = env->GetStaticMethodID(m_adapter_class, "openAdapter", "()Z");
    m_close_adapter = env->GetStaticMethodID(m_adapter_class, "closeAdapter", "()V");

    m_input = env->GetStaticMethodID(m_adapter_class, "input", "()I");
    m_output = env->GetStaticMethodID(m_adapter_class, "output", "([B)I");

    m_enable_hotplug = env->GetStaticMethodID(m_adapter_class, "enableHotplugCallback", "()V");
    m_disable_hotplug = env->GetStaticMethodID(m_adapter_class, "disableHotplugCallback", "()V");

    // Enable hotplug
    env->CallStaticVoidMethod(m_adapter_class, m_enable_hotplug);
  }

  ~AndroidBackend()
  {
    if (m_adapter_class)
    {
      JNIEnv* env = IDCache::GetEnvForThread();
      env->CallStaticVoidMethod(m_adapter_class, m_disable_hotplug);
      env->CallStaticVoidMethod(m_adapter_class, m_close_adapter);
      env->DeleteGlobalRef(m_adapter_class);
    }

    s_backend = nullptr;
  }

  bool IsConnected(std::string* error_message = nullptr) const override
  {
    return m_connected.load(std::memory_order_relaxed);
  }

  int Read(std::span<u8> buffer) override
  {
    std::shared_lock lk{m_connection_mutex};

    if (!IsConnected())
      return -1;

    JNIEnv* env = IDCache::GetEnvForThread();
    const jint size = env->CallStaticIntMethod(m_adapter_class, m_input);

    if (size < 0 || static_cast<std::size_t>(size) > buffer.size())
      return -1;

    auto payload =
        static_cast<jbyteArray>(env->GetStaticObjectField(m_adapter_class, m_payload_field));

    if (!payload)
      return -1;

    env->GetByteArrayRegion(payload, 0, size, reinterpret_cast<jbyte*>(buffer.data()));
    env->DeleteLocalRef(payload);

    return size;
  }

  int Write(std::span<const u8> buffer) override
  {
    std::shared_lock lk{m_connection_mutex};

    if (!IsConnected())
      return -1;

    JNIEnv* env = IDCache::GetEnvForThread();
    jbyteArray payload = env->NewByteArray(static_cast<jsize>(buffer.size()));
    if (!payload)
      return -1;

    env->SetByteArrayRegion(payload, 0, static_cast<jsize>(buffer.size()),
                            reinterpret_cast<const jbyte*>(buffer.data()));

    const jint result = env->CallStaticIntMethod(m_adapter_class, m_output, payload);
    env->DeleteLocalRef(payload);

    return result;
  }

  void OnJavaConnected()
  {
    {
      std::lock_guard lk{m_connection_mutex};

      JNIEnv* env = IDCache::GetEnvForThread();
      if (!env->CallStaticBooleanMethod(m_adapter_class, m_open_adapter))
      {
        ERROR_LOG_FMT(CONTROLLERINTERFACE, "GCAdapter failed to open");
        return;
      }

      m_connected.store(true, std::memory_order_relaxed);
    }

    InvokeCallback(&Callbacks::on_connected);
  }

  void OnJavaDisconnected()
  {
    {
      std::lock_guard lk{m_connection_mutex};

      JNIEnv* env = IDCache::GetEnvForThread();
      env->CallStaticBooleanMethod(m_adapter_class, m_close_adapter);

      m_connected.store(false, std::memory_order_relaxed);
    }

    InvokeCallback(&Callbacks::on_disconnected);
  }

private:
  jclass m_adapter_class{};

  jfieldID m_payload_field{};

  jmethodID m_open_adapter{};
  jmethodID m_close_adapter{};

  jmethodID m_input{};
  jmethodID m_output{};

  jmethodID m_enable_hotplug{};
  jmethodID m_disable_hotplug{};

  std::atomic_bool m_connected{};

  // This also protects concurrent access to `usbConnection` in Kotlin code.
  std::shared_mutex m_connection_mutex;
};
}  // namespace

std::unique_ptr<Backend> CreateBackend(Backend::Callbacks callbacks)
{
  return std::make_unique<AndroidBackend>(std::move(callbacks));
}
}  // namespace GCAdapter

extern "C" {
JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_utils_GCAdapter_onAdapterConnected(JNIEnv*,
                                                                                         jclass)
{
  GCAdapter::s_backend->OnJavaConnected();
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_utils_GCAdapter_onAdapterDisconnected(JNIEnv*,
                                                                                            jclass)
{
  GCAdapter::s_backend->OnJavaDisconnected();
}
}
