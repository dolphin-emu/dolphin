// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <jni/AndroidCommon/IDCache.h>

#include <sstream>
#include <thread>
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/ControllerInterface/Android/Motor.h"

namespace ciface::Android
{
Motor::~Motor()
{
}

std::string Motor::GetName() const
{
  std::ostringstream ss;
  ss << "Rumble " << static_cast<int>(m_index);
  return ss.str();
}

void Motor::SetState(ControlState state)
{
  if (state > 0)
  {
    std::thread(Rumble, m_pad_id, state).detach();
  }
}

void Motor::Rumble(int pad_id, double state)
{
  JNIEnv* env = IDCache::GetEnvForThread();
  env->CallStaticVoidMethod(IDCache::GetNativeLibraryClass(), IDCache::GetDoRumble(), pad_id,
                            state);
}
} // namespace ciface::Android