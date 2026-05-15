#include <jni.h>

#ifdef HAS_LIBMGBA
#include <algorithm>

#include "AudioCommon/AudioCommon.h"
#include "AudioCommon/Mixer.h"
#include "Core/HW/GBACore.h"
#include "Core/HW/SI/SI.h"
#include "Core/System.h"
#endif

#include "VideoCommon/Present.h"

extern "C" {
JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_gba_GbaLibrary_resetGbaCore(JNIEnv*, jclass, jint slot)
{
#ifdef HAS_LIBMGBA
  Core::System::GetInstance().GetSerialInterface().ResetGBACore(slot);
#endif
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_features_gba_GbaLibrary_setGbaVolume(
    JNIEnv*, jclass, jint slot, jint volume)
{
#ifdef HAS_LIBMGBA
  if (slot < 0 || slot >= 4)
    return;

  const int clamped_volume = std::clamp(static_cast<int>(volume), 0, 0xff);
  if (SoundStream* sound_stream = Core::System::GetInstance().GetSoundStream())
  {
    sound_stream->GetMixer()->SetGBAVolume(static_cast<std::size_t>(slot),
                                           static_cast<u32>(clamped_volume),
                                           static_cast<u32>(clamped_volume));
  }
#endif
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_gba_GbaLibrary_setGCLeftOffset(JNIEnv*, jclass, jint offset)
{
  if (g_presenter)
    g_presenter->SetGCLeftOffset(static_cast<int>(offset));
}

JNIEXPORT jint JNICALL
Java_org_dolphinemu_dolphinemu_features_gba_GbaLibrary_getGCDrawWidth(JNIEnv*, jclass)
{
  if (g_presenter)
    return static_cast<jint>(g_presenter->GetGCDrawWidth());
  return 0;
}
}
