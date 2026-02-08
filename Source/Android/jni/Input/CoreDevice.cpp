// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "jni/Input/CoreDevice.h"

#include <memory>
#include <utility>
#include <vector>

#include <jni.h>

#include "InputCommon/ControllerInterface/CoreDevice.h"
#include "jni/AndroidCommon/AndroidCommon.h"
#include "jni/AndroidCommon/IDCache.h"

static ciface::Core::Device::Control* GetControlPointer(JNIEnv* env, jobject obj)
{
  return reinterpret_cast<ciface::Core::Device::Control*>(
      env->GetLongField(obj, IDCache::GetCoreDeviceControlPointer()));
}

static jobject CoreDeviceControlToJava(JNIEnv* env, jobject device,
                                       ciface::Core::Device::Control* control)
{
  if (!control)
    return nullptr;

  return env->NewObject(IDCache::GetCoreDeviceControlClass(),
                        IDCache::GetCoreDeviceControlConstructor(), device,
                        reinterpret_cast<jlong>(control));
}

template <typename T>
static jobjectArray CoreDeviceControlVectorToJava(JNIEnv* env, jobject device,
                                                  const std::vector<T*>& controls)
{
  return VectorToJObjectArray(
      env, controls, IDCache::GetCoreDeviceControlClass(),
      [device](JNIEnv* env, T* control) { return CoreDeviceControlToJava(env, device, control); });
}

static std::shared_ptr<ciface::Core::Device>* GetDevicePointer(JNIEnv* env, jobject obj)
{
  return reinterpret_cast<std::shared_ptr<ciface::Core::Device>*>(
      env->GetLongField(obj, IDCache::GetCoreDevicePointer()));
}

jobject CoreDeviceToJava(JNIEnv* env, std::shared_ptr<ciface::Core::Device> device)
{
  if (!device)
    return nullptr;

  return env->NewObject(
      IDCache::GetCoreDeviceClass(), IDCache::GetCoreDeviceConstructor(),
      reinterpret_cast<jlong>(new std::shared_ptr<ciface::Core::Device>(std::move(device))));
}

extern "C" {

JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_CoreDevice_00024Control_getName(JNIEnv* env,
                                                                                    jobject obj)
{
  return ToJString(env, GetControlPointer(env, obj)->GetName());
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_CoreDevice_finalize(JNIEnv* env, jobject obj)
{
  delete GetDevicePointer(env, obj);
}

JNIEXPORT jobjectArray JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_CoreDevice_getInputs(JNIEnv* env, jobject obj)
{
  return CoreDeviceControlVectorToJava(env, obj, (*GetDevicePointer(env, obj))->Inputs());
}

JNIEXPORT jobjectArray JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_CoreDevice_getOutputs(JNIEnv* env, jobject obj)
{
  return CoreDeviceControlVectorToJava(env, obj, (*GetDevicePointer(env, obj))->Outputs());
}
}
