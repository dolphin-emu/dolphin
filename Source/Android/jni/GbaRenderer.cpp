// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <jni.h>

#ifdef HAS_LIBMGBA
#include <algorithm>
#include "AudioCommon/AudioCommon.h"
#include "AudioCommon/Mixer.h"
#include "Core/HW/GBAPad.h"
#include "Core/HW/SI/SI.h"
#include "Core/System.h"
#endif

#include "VideoCommon/Present.h"

extern "C" {
JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_gba_GbaRenderer_resetGbaCore(JNIEnv*, jclass, jint slot)
{
#ifdef HAS_LIBMGBA
  Pad::SetGBAReset(slot, true);
#endif
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_gba_GbaRenderer_resetToMultiboot(JNIEnv*, jclass, jint slot)
{
#ifdef HAS_LIBMGBA
  Pad::SetGBAMultibootReset(slot, true);
#endif
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_features_gba_GbaRenderer_setGbaVolume(
    JNIEnv*, jclass, jint slot, jint volume)
{
#ifdef HAS_LIBMGBA
  if (slot < 0 || slot >= 4)
    return;

  if (auto* stream = Core::System::GetInstance().GetSoundStream())
  {
    const u32 vol = std::clamp(volume, 0, 0xff);
    stream->GetMixer()->SetGBAVolume(slot, vol, vol);
  }
#endif
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_features_gba_GbaRenderer_setTvLeftOffset(
    JNIEnv*, jclass, jint offset)
{
  if (g_presenter)
    g_presenter->SetTVLeftOffset(offset);
}

JNIEXPORT jint JNICALL
Java_org_dolphinemu_dolphinemu_features_gba_GbaRenderer_getFrameCount(JNIEnv*, jclass)
{
  return g_presenter ? (jint)g_presenter->FrameCount() : 0;
}

JNIEXPORT jint JNICALL
Java_org_dolphinemu_dolphinemu_features_gba_GbaRenderer_getTvDrawWidth(JNIEnv*, jclass)
{
  return g_presenter ? g_presenter->GetTargetRectangle().GetWidth() : 0;
}

JNIEXPORT jint JNICALL
Java_org_dolphinemu_dolphinemu_features_gba_GbaRenderer_getTvDrawHeight(JNIEnv*, jclass)
{
  return g_presenter ? g_presenter->GetTargetRectangle().GetHeight() : 0;
}

JNIEXPORT jint JNICALL Java_org_dolphinemu_dolphinemu_features_gba_GbaRenderer_getTvDrawTop(JNIEnv*,
                                                                                            jclass)
{
  return g_presenter ? g_presenter->GetTargetRectangle().top : 0;
}
}
