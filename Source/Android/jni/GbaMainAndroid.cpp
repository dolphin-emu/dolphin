#include <jni.h>

#ifdef HAS_LIBMGBA
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
  auto core = Core::System::GetInstance().GetSerialInterface().GetGBACore(slot);
  if (core)
    core->Reset();
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
