// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "jni/GBA/AndroidGBAHost.h"

#include <atomic>
#include <memory>
#include <span>
#include <vector>

#include <android/log.h>
#include <jni.h>

#include "Common/CommonTypes.h"
#include "Core/HW/GBACore.h"
#include "jni/AndroidCommon/AndroidCommon.h"
#include "jni/AndroidCommon/IDCache.h"

namespace
{
constexpr const char* DOLPHIN_TAG = "DolphinEmuNative";

std::atomic<int> s_visible_device{-1};

void ClearPendingException(JNIEnv* env, const char* callback)
{
  if (!env->ExceptionCheck())
    return;

  __android_log_print(ANDROID_LOG_ERROR, DOLPHIN_TAG, "[AndroidGBAHost] Java exception in %s",
                      callback);
  env->ExceptionDescribe();
  env->ExceptionClear();
}

jint ToAndroidArgb(u32 pixel)
{
  return static_cast<jint>(0xff000000 | ((pixel & 0x000000ff) << 16) | (pixel & 0x0000ff00) |
                           ((pixel & 0x00ff0000) >> 16));
}

class AndroidGBAHost final : public GBAHostInterface
{
public:
  explicit AndroidGBAHost(std::weak_ptr<HW::GBA::Core> core) : m_core(std::move(core))
  {
    if (const std::shared_ptr<HW::GBA::Core> core_ptr = m_core.lock())
      m_device_number = core_ptr->GetCoreInfo().device_number;
    GameChanged();
  }

  ~AndroidGBAHost() override
  {
    if (m_device_number < 0)
      return;

    JNIEnv* env = IDCache::GetEnvForThread();
    env->CallStaticVoidMethod(IDCache::GetGbaHostBridgeClass(),
                              IDCache::GetGbaHostBridgeOnCoreStopped(),
                              static_cast<jint>(m_device_number));
    ClearPendingException(env, "GbaHostBridge.onCoreStopped");
  }

  void GameChanged() override
  {
    const std::shared_ptr<HW::GBA::Core> core = m_core.lock();
    if (!core)
      return;

    const HW::GBA::CoreInfo info = core->GetCoreInfo();
    JNIEnv* env = IDCache::GetEnvForThread();
    jstring title = ToJString(env, info.game_title);
    if (env->ExceptionCheck())
    {
      ClearPendingException(env, "ToJString");
      return;
    }

    env->CallStaticVoidMethod(IDCache::GetGbaHostBridgeClass(),
                              IDCache::GetGbaHostBridgeOnGameChanged(),
                              static_cast<jint>(info.device_number), static_cast<jint>(info.width),
                              static_cast<jint>(info.height), static_cast<jboolean>(info.is_gba),
                              static_cast<jboolean>(info.has_rom), title);
    env->DeleteLocalRef(title);
    ClearPendingException(env, "GbaHostBridge.onGameChanged");
  }

  void FrameEnded(std::span<const u32> video_buffer) override
  {
    const std::shared_ptr<HW::GBA::Core> core = m_core.lock();
    if (!core)
      return;

    const HW::GBA::CoreInfo info = core->GetCoreInfo();
    if (s_visible_device.load(std::memory_order_relaxed) != static_cast<int>(info.device_number))
      return;

    const size_t pixel_count = static_cast<size_t>(info.width) * static_cast<size_t>(info.height);
    if (pixel_count == 0 || video_buffer.size() < pixel_count)
      return;

    JNIEnv* env = IDCache::GetEnvForThread();
    jintArray pixels = env->NewIntArray(static_cast<jsize>(pixel_count));
    if (!pixels)
    {
      ClearPendingException(env, "NewIntArray");
      return;
    }

    m_pixels.resize(pixel_count);
    for (size_t i = 0; i < pixel_count; ++i)
      m_pixels[i] = ToAndroidArgb(video_buffer[i]);

    env->SetIntArrayRegion(pixels, 0, static_cast<jsize>(pixel_count), m_pixels.data());
    if (env->ExceptionCheck())
    {
      env->DeleteLocalRef(pixels);
      ClearPendingException(env, "SetIntArrayRegion");
      return;
    }

    env->CallStaticVoidMethod(IDCache::GetGbaHostBridgeClass(), IDCache::GetGbaHostBridgeOnFrame(),
                              static_cast<jint>(info.device_number), static_cast<jint>(info.width),
                              static_cast<jint>(info.height), pixels);
    env->DeleteLocalRef(pixels);
    ClearPendingException(env, "GbaHostBridge.onFrame");
  }

private:
  std::weak_ptr<HW::GBA::Core> m_core;
  std::vector<jint> m_pixels;
  int m_device_number = -1;
};
}  // namespace

std::unique_ptr<GBAHostInterface> CreateAndroidGBAHost(std::weak_ptr<HW::GBA::Core> core)
{
#ifdef HAS_LIBMGBA
  return std::make_unique<AndroidGBAHost>(std::move(core));
#else
  return nullptr;
#endif
}

extern "C" {

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_dualscreen_GbaHostBridge_setVisibleDevice(
    JNIEnv*, jclass, jint device_number)
{
  s_visible_device.store(static_cast<int>(device_number), std::memory_order_relaxed);
}
}
