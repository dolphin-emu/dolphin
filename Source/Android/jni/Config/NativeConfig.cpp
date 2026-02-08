// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <memory>
#include <string>

#include <jni.h>

#include "Common/Assert.h"
#include "Common/Config/Config.h"
#include "Core/ConfigLoaders/GameConfigLoader.h"
#include "Core/ConfigLoaders/IsSettingSaveable.h"
#include "jni/AndroidCommon/AndroidCommon.h"
#include "jni/Host.h"

constexpr jint LAYER_BASE_OR_CURRENT = 0;
constexpr jint LAYER_BASE = 1;
constexpr jint LAYER_LOCAL_GAME = 2;
constexpr jint LAYER_ACTIVE = 3;
constexpr jint LAYER_CURRENT = 4;

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
  else if (decoded_file == "WiimoteNew")
  {
    system = Config::System::WiiPad;
  }
  else if (decoded_file == "GameSettingsOnly")
  {
    system = Config::System::GameSettingsOnly;
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
  Config::LayerType layer_type;

  switch (layer)
  {
  case LAYER_BASE_OR_CURRENT:
    if (GetActiveLayerForConfig(location) == Config::LayerType::Base)
      layer_type = Config::LayerType::Base;
    else
      layer_type = Config::LayerType::CurrentRun;
    break;

  case LAYER_BASE:
    layer_type = Config::LayerType::Base;
    break;

  case LAYER_LOCAL_GAME:
    layer_type = Config::LayerType::LocalGame;
    break;

  case LAYER_ACTIVE:
    layer_type = Config::GetActiveLayerForConfig(location);
    break;

  case LAYER_CURRENT:
    layer_type = Config::LayerType::CurrentRun;
    break;

  default:
    ASSERT(false);
    return nullptr;
  }

  return Config::GetLayer(layer_type);
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
  Config::OnConfigChanged();
}

extern "C" {

JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_NativeConfig_isSettingSaveable(
    JNIEnv* env, jclass, jstring file, jstring section, jstring key)
{
  const Config::Location location = GetLocation(env, file, section, key);
  return static_cast<jboolean>(ConfigLoaders::IsSettingSaveable(location));
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_NativeConfig_loadGameInis(JNIEnv* env,
                                                                                 jclass,
                                                                                 jstring jGameId,
                                                                                 jint jRevision)
{
  const std::string game_id = GetJString(env, jGameId);
  const u16 revision = static_cast<u16>(jRevision);
  Config::AddLayer(ConfigLoaders::GenerateGlobalGameConfigLoader(game_id, revision));
  Config::AddLayer(ConfigLoaders::GenerateLocalGameConfigLoader(game_id, revision));
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_NativeConfig_unloadGameInis(JNIEnv*, jclass)
{
  Config::RemoveLayer(Config::LayerType::GlobalGame);
  Config::RemoveLayer(Config::LayerType::LocalGame);
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_features_settings_model_NativeConfig_save(
    JNIEnv*, jclass, jint layer)
{
  // HostThreadLock is used to ensure we don't try to save to SYSCONF at the same time as
  // emulation shutdown does
  HostThreadLock guard;

  return GetLayer(layer, {})->Save();
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_NativeConfig_deleteAllKeys(JNIEnv*, jclass,
                                                                                  jint layer)
{
  return GetLayer(layer, {})->DeleteAllKeys();
}

JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_NativeConfig_isOverridden(
    JNIEnv* env, jclass, jstring file, jstring section, jstring key)
{
  const Config::Location location = GetLocation(env, file, section, key);
  const bool result = Config::GetActiveLayerForConfig(location) != Config::LayerType::Base;
  return static_cast<jboolean>(result);
}

JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_NativeConfig_deleteKey(
    JNIEnv* env, jclass, jint layer, jstring file, jstring section, jstring key)
{
  const Config::Location location = GetLocation(env, file, section, key);
  const bool had_value = GetLayer(layer, location)->DeleteKey(location);
  if (had_value)
    Config::OnConfigChanged();
  return static_cast<jboolean>(had_value);
}

JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_NativeConfig_exists(JNIEnv* env, jclass,
                                                                           jint layer, jstring file,
                                                                           jstring section,
                                                                           jstring key)
{
  const Config::Location location = GetLocation(env, file, section, key);
  return static_cast<jboolean>(GetLayer(layer, location)->Exists(location));
}

JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_NativeConfig_getString(
    JNIEnv* env, jclass, jint layer, jstring file, jstring section, jstring key,
    jstring default_value)
{
  const Config::Location location = GetLocation(env, file, section, key);
  return ToJString(env, Get(layer, location, GetJString(env, default_value)));
}

JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_NativeConfig_getBoolean(
    JNIEnv* env, jclass, jint layer, jstring file, jstring section, jstring key,
    jboolean default_value)
{
  const Config::Location location = GetLocation(env, file, section, key);
  return static_cast<jboolean>(Get(layer, location, static_cast<bool>(default_value)));
}

JNIEXPORT jint JNICALL Java_org_dolphinemu_dolphinemu_features_settings_model_NativeConfig_getInt(
    JNIEnv* env, jclass, jint layer, jstring file, jstring section, jstring key, jint default_value)
{
  return Get(layer, GetLocation(env, file, section, key), default_value);
}

JNIEXPORT jfloat JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_NativeConfig_getFloat(
    JNIEnv* env, jclass, jint layer, jstring file, jstring section, jstring key,
    jfloat default_value)
{
  return Get(layer, GetLocation(env, file, section, key), default_value);
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_NativeConfig_setString(
    JNIEnv* env, jclass, jint layer, jstring file, jstring section, jstring key, jstring value)
{
  return Set(layer, GetLocation(env, file, section, key), GetJString(env, value));
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_NativeConfig_setBoolean(
    JNIEnv* env, jclass, jint layer, jstring file, jstring section, jstring key, jboolean value)
{
  return Set(layer, GetLocation(env, file, section, key), static_cast<bool>(value));
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_features_settings_model_NativeConfig_setInt(
    JNIEnv* env, jclass, jint layer, jstring file, jstring section, jstring key, jint value)
{
  return Set(layer, GetLocation(env, file, section, key), value);
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_features_settings_model_NativeConfig_setFloat(
    JNIEnv* env, jclass, jint layer, jstring file, jstring section, jstring key, jfloat value)
{
  return Set(layer, GetLocation(env, file, section, key), value);
}
}
