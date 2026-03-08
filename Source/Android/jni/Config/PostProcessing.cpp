// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <string>
#include <vector>

#include <jni.h>

#include "VideoCommon/PostProcessing.h"
#include "jni/AndroidCommon/AndroidCommon.h"

static VideoCommon::PostProcessingConfiguration s_post_processing_configuration;

static jobject
CreateShaderOptionBool(JNIEnv* env,
                       const VideoCommon::PostProcessingConfiguration::ConfigurationOption& option)
{
  jclass cls = env->FindClass(
      "org/dolphinemu/dolphinemu/features/settings/model/PostProcessing$ShaderOption$Bool");
  jmethodID ctor =
      env->GetMethodID(cls, "<init>", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Z)V");
  return env->NewObject(cls, ctor, ToJString(env, option.m_gui_name),
                        ToJString(env, option.m_option_name),
                        ToJString(env, option.m_dependent_option), option.m_bool_value);
}

static jobject
CreateShaderOptionFloat(JNIEnv* env,
                        const VideoCommon::PostProcessingConfiguration::ConfigurationOption& option)
{
  jclass cls = env->FindClass(
      "org/dolphinemu/dolphinemu/features/settings/model/PostProcessing$ShaderOption$Float");
  jmethodID ctor = env->GetMethodID(
      cls, "<init>", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;[F[F[F[F)V");
  auto to_jfloat = [&](const std::vector<float>& v) {
    jfloatArray a = env->NewFloatArray((jsize)v.size());
    env->SetFloatArrayRegion(a, 0, (jsize)v.size(), v.data());
    return a;
  };
  return env->NewObject(cls, ctor, ToJString(env, option.m_gui_name),
                        ToJString(env, option.m_option_name),
                        ToJString(env, option.m_dependent_option), to_jfloat(option.m_float_values),
                        to_jfloat(option.m_float_min_values), to_jfloat(option.m_float_max_values),
                        to_jfloat(option.m_float_step_values));
}

static jobject CreateShaderOptionInteger(
    JNIEnv* env, const VideoCommon::PostProcessingConfiguration::ConfigurationOption& option)
{
  jclass cls = env->FindClass(
      "org/dolphinemu/dolphinemu/features/settings/model/PostProcessing$ShaderOption$Integer");
  jmethodID ctor = env->GetMethodID(
      cls, "<init>", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;[I[I[I[I)V");
  auto to_jint = [&](const std::vector<s32>& v) {
    jintArray a = env->NewIntArray((jsize)v.size());
    env->SetIntArrayRegion(a, 0, (jsize)v.size(), reinterpret_cast<const jint*>(v.data()));
    return a;
  };
  return env->NewObject(cls, ctor, ToJString(env, option.m_gui_name),
                        ToJString(env, option.m_option_name),
                        ToJString(env, option.m_dependent_option), to_jint(option.m_integer_values),
                        to_jint(option.m_integer_min_values), to_jint(option.m_integer_max_values),
                        to_jint(option.m_integer_step_values));
}

extern "C" {

JNIEXPORT jobjectArray JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_PostProcessing_getShaderList(JNIEnv* env,
                                                                                    jclass)
{
  return SpanToJStringArray(env, VideoCommon::PostProcessing::GetShaderList());
}

JNIEXPORT jobjectArray JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_PostProcessing_getAnaglyphShaderList(
    JNIEnv* env, jclass)
{
  return SpanToJStringArray(env, VideoCommon::PostProcessing::GetAnaglyphShaderList());
}

JNIEXPORT jobjectArray JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_PostProcessing_getPassiveShaderList(
    JNIEnv* env, jclass)
{
  return SpanToJStringArray(env, VideoCommon::PostProcessing::GetPassiveShaderList());
}

JNIEXPORT jobject JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_PostProcessing_getShaderOptions(
    JNIEnv* env, jclass, jstring j_shader_name)
{
  const std::string shader_name = GetJString(env, j_shader_name);
  s_post_processing_configuration.LoadShader(shader_name);
  if (!s_post_processing_configuration.HasOptions())
    return nullptr;

  const auto& options = s_post_processing_configuration.GetOptions();
  jclass list_cls = env->FindClass("java/util/ArrayList");
  jmethodID list_ctor = env->GetMethodID(list_cls, "<init>", "()V");
  jmethodID list_add = env->GetMethodID(list_cls, "add", "(Ljava/lang/Object;)Z");
  jobject list = env->NewObject(list_cls, list_ctor);

  for (const auto& it : options)
  {
    const auto& opt = it.second;
    jobject j_opt = nullptr;
    switch (opt.m_type)
    {
    case VideoCommon::PostProcessingConfiguration::ConfigurationOption::OptionType::Bool:
      j_opt = CreateShaderOptionBool(env, opt);
      break;
    case VideoCommon::PostProcessingConfiguration::ConfigurationOption::OptionType::Float:
      j_opt = CreateShaderOptionFloat(env, opt);
      break;
    case VideoCommon::PostProcessingConfiguration::ConfigurationOption::OptionType::Integer:
      j_opt = CreateShaderOptionInteger(env, opt);
      break;
    }
    if (j_opt)
      env->CallBooleanMethod(list, list_add, j_opt);
  }

  jclass so_cls = env->FindClass(
      "org/dolphinemu/dolphinemu/features/settings/model/PostProcessing$ShaderOptions");
  jmethodID so_ctor = env->GetMethodID(so_cls, "<init>", "(Ljava/util/List;)V");
  return env->NewObject(so_cls, so_ctor, list);
}

static void EnsureShaderLoaded(const std::string& shader)
{
if (s_post_processing_configuration.GetShader() != shader)
  s_post_processing_configuration.LoadShader(shader);
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_PostProcessing_setShaderOption__Ljava_lang_String_2Ljava_lang_String_2Z(
    JNIEnv* env, jclass, jstring j_shader, jstring j_option, jboolean value)
{
  const std::string shader = GetJString(env, j_shader);
  const std::string option = GetJString(env, j_option);
  EnsureShaderLoaded(shader);
  s_post_processing_configuration.SetOptionb(option, value);
  s_post_processing_configuration.SaveOptionsConfiguration();
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_PostProcessing_setShaderOption__Ljava_lang_String_2Ljava_lang_String_2_3F(
    JNIEnv* env, jclass, jstring j_shader, jstring j_option, jfloatArray values)
{
  const std::string shader = GetJString(env, j_shader);
  const std::string option = GetJString(env, j_option);
  EnsureShaderLoaded(shader);
  jsize len = env->GetArrayLength(values);
  jfloat* elems = env->GetFloatArrayElements(values, nullptr);
  for (jsize i = 0; i < len; i++)
    s_post_processing_configuration.SetOptionf(option, i, elems[i]);
  env->ReleaseFloatArrayElements(values, elems, JNI_ABORT);
  s_post_processing_configuration.SaveOptionsConfiguration();
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_PostProcessing_setShaderOption__Ljava_lang_String_2Ljava_lang_String_2_3I(
    JNIEnv* env, jclass, jstring j_shader, jstring j_option, jintArray values)
{
  const std::string shader = GetJString(env, j_shader);
  const std::string option = GetJString(env, j_option);
  EnsureShaderLoaded(shader);
  jsize len = env->GetArrayLength(values);
  jint* elems = env->GetIntArrayElements(values, nullptr);
  for (jsize i = 0; i < len; i++)
    s_post_processing_configuration.SetOptioni(option, i, (s32)elems[i]);
  env->ReleaseIntArrayElements(values, elems, JNI_ABORT);
  s_post_processing_configuration.SaveOptionsConfiguration();
}
}
