// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef HAS_LIBMGBA

#include "jni/AndroidGBAHost.h"

#include <jni.h>
#include <vector>
#include "Common/CommonTypes.h"
#include "jni/AndroidCommon/IDCache.h"

AndroidGBAHost::AndroidGBAHost(std::weak_ptr<HW::GBA::Core> core, int device_number)
    : m_core(std::move(core)), m_device_number(device_number)
{
}

AndroidGBAHost::~AndroidGBAHost() = default;

void AndroidGBAHost::GameChanged()
{
  // Nothing needed on Android for game changes
}

void AndroidGBAHost::FrameEnded(std::span<const u32> video_buffer)
{
  JNIEnv* env = IDCache::GetEnvForThread();
  if (!env || video_buffer.empty())
    return;

  const jlong byte_size = static_cast<jlong>(video_buffer.size() * sizeof(u32));
  jobject byte_buffer = env->NewDirectByteBuffer(const_cast<u32*>(video_buffer.data()), byte_size);
  if (!byte_buffer)
    return;

  env->CallStaticVoidMethod(IDCache::GetGbaLibraryClass(), IDCache::GetOnGbaFrameWithBuffer(),
                            static_cast<jint>(m_device_number), byte_buffer);

  env->DeleteLocalRef(byte_buffer);
}

#endif  // HAS_LIBMGBA
