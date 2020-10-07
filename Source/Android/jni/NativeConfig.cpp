// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <memory>
#include <string>

#include <jni.h>

#include "Common/Assert.h"
#include "Common/Config/Config.h"
#include "Core/ConfigLoaders/GameConfigLoader.h"
#include "Core/ConfigLoaders/IsSettingSaveable.h"
#include "jni/AndroidCommon/AndroidCommon.h"

constexpr jint LAYER_BASE_OR_CURRENT = 0;
constexpr jint LAYER_LOCAL_GAME = 1;

static Config::Location GetLocation(JNIEnv* env, jstring file, jstring section, jstring key)
{
  const std::string decoded_file = GetJString(env, file);

  Config::System system;
  if (decoded_file == "Dolphin")
  {
    system = Config::System::Main;
  }
  else if (decoded_file == "SYSCONF")
  {
    system = Config::System::SYSCONF;
  }
  else if (decoded_file == "GFX")
  {
    system = Config::System::GFX;
  }
  else if (decoded_file == "Logger")
  {
    system = Config::System::Logger;
  }
  else
  {
    ASSERT(false);
    return {};
  }

  return Config::Location{system, GetJString(env, section), GetJString(env, key)};
}

static std::shared_ptr<Config::Layer> GetLayer(jint layer, const Config::Location& location)
{
  switch (layer)
  {
  case LAYER_BASE_OR_CURRENT:
    if (GetActiveLayerForConfig(location) == Config::LayerType::Base)
      return Config::GetLayer(Config::LayerType::Base);
    else
      return Config::GetLayer(Config::LayerType::CurrentRun);

  case LAYER_LOCAL_GAME:
    return Config::GetLayer(Config::LayerType::LocalGame);

  default:
    ASSERT(false);
    return nullptr;
  }
}

template <typename T>
static T Get(jint layer, const Config::Location& location, T default_value)
{
  return GetLayer(layer, location)->Get<T>(location).value_or(default_value);
}

template <typename T>
static void Set(jint layer, const Config::Location& location, T value)
{
  GetLayer(layer, location)->Set(location, value);
  Config::InvokeConfigChangedCallbacks();
}

#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_NativeConfig_isSettingSaveable(
    JNIEnv* env, jclass obj, jstring file, jstring section, jstring key)
{
  const Config::Location location = GetLocation(env, file, section, key);
  return static_cast<jboolean>(ConfigLoaders::IsSettingSaveable(location));
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_NativeConfig_loadGameInis(JNIEnv* env,
                                                                                 jclass obj,
                                                                                 jstring jGameId,
                                                                                 jint jRevision)
{
  const std::string game_id = GetJString(env, jGameId);
  const u16 revision = static_cast<u16>(jRevision);
  Config::AddLayer(ConfigLoaders::GenerateGlobalGameConfigLoader(game_id, revision));
  Config::AddLayer(ConfigLoaders::GenerateLocalGameConfigLoader(game_id, revision));
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_NativeConfig_unloadGameInis(JNIEnv* env,
                                                                                   jclass obj)
{
  Config::RemoveLayer(Config::LayerType::GlobalGame);
  Config::RemoveLayer(Config::LayerType::LocalGame);
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_features_settings_model_NativeConfig_save(
    JNIEnv* env, jclass obj, jint layer)
{
  return GetLayer(layer, {})->Save();
}

JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_NativeConfig_isOverridden(
    JNIEnv* env, jclass obj, jstring file, jstring section, jstring key)
{
  const Config::Location location = GetLocation(env, file, section, key);
  const bool result = Config::GetActiveLayerForConfig(location) != Config::LayerType::Base;
  return static_cast<jboolean>(result);
}

JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_NativeConfig_deleteKey(
    JNIEnv* env, jclass obj, jint layer, jstring file, jstring section, jstring key)
{
  const Config::Location location = GetLocation(env, file, section, key);
  return static_cast<jboolean>(GetLayer(layer, location)->DeleteKey(location));
}

JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_NativeConfig_getString(
    JNIEnv* env, jclass obj, jint layer, jstring file, jstring section, jstring key,
    jstring default_value)
{
  const Config::Location location = GetLocation(env, file, section, key);
  return ToJString(env, Get(layer, location, GetJString(env, default_value)));
}

JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_NativeConfig_getBoolean(
    JNIEnv* env, jclass obj, jint layer, jstring file, jstring section, jstring key,
    jboolean default_value)
{
  const Config::Location location = GetLocation(env, file, section, key);
  return static_cast<jboolean>(Get(layer, location, static_cast<bool>(default_value)));
}

JNIEXPORT jint JNICALL Java_org_dolphinemu_dolphinemu_features_settings_model_NativeConfig_getInt(
    JNIEnv* env, jclass obj, jint layer, jstring file, jstring section, jstring key,
    jint default_value)
{
  return Get(layer, GetLocation(env, file, section, key), default_value);
}

JNIEXPORT jfloat JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_NativeConfig_getFloat(
    JNIEnv* env, jclass obj, jint layer, jstring file, jstring section, jstring key,
    jfloat default_value)
{
  return Get(layer, GetLocation(env, file, section, key), default_value);
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_NativeConfig_setString(
    JNIEnv* env, jclass obj, jint layer, jstring file, jstring section, jstring key, jstring value)
{
  return Set(layer, GetLocation(env, file, section, key), GetJString(env, value));
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_NativeConfig_setBoolean(
    JNIEnv* env, jclass obj, jint layer, jstring file, jstring section, jstring key, jboolean value)
{
  return Set(layer, GetLocation(env, file, section, key), static_cast<bool>(value));
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_features_settings_model_NativeConfig_setInt(
    JNIEnv* env, jclass obj, jint layer, jstring file, jstring section, jstring key, jint value)
{
  return Set(layer, GetLocation(env, file, section, key), value);
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_features_settings_model_NativeConfig_setFloat(
    JNIEnv* env, jclass obj, jint layer, jstring file, jstring section, jstring key, jfloat value)
{
  return Set(layer, GetLocation(env, file, section, key), value);
}

#ifdef __cplusplus
}
#endif
