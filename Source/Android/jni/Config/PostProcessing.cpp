#include "VideoCommon/PostProcessing.h"
#include <jni.h>
#include <string>
#include <vector>
#include "jni/AndroidCommon/AndroidCommon.h"

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
Java_org_dolphinemu_dolphinemu_features_settings_model_PostProcessing_getShaderOptions(JNIEnv* env,
                                                                                       jclass,
                                                                                       jstring name)
{
  const char* name_chars = env->GetStringUTFChars(name, nullptr);
  const std::string shader_name_str(name_chars);
  env->ReleaseStringUTFChars(name, name_chars);
  VideoCommon::PostProcessingConfiguration config;
  config.LoadShader(shader_name_str);
  if (!config.HasOptions())
  {
    return nullptr;
  }

  jclass shader_options_class = env->FindClass(
      "org/dolphinemu/dolphinemu/features/settings/model/PostProcessing$ShaderOptions");
  jmethodID shader_options_constructor =
      env->GetMethodID(shader_options_class, "<init>", "(Ljava/util/List;)V");

  jclass array_list_class = env->FindClass("java/util/ArrayList");
  jmethodID array_list_constructor = env->GetMethodID(array_list_class, "<init>", "()V");
  jmethodID array_list_add = env->GetMethodID(array_list_class, "add", "(Ljava/lang/Object;)Z");
  jobject options_list = env->NewObject(array_list_class, array_list_constructor);

  jclass bool_class = env->FindClass(
      "org/dolphinemu/dolphinemu/features/settings/model/PostProcessing$ShaderOption$Bool");
  jclass float_class = env->FindClass(
      "org/dolphinemu/dolphinemu/features/settings/model/PostProcessing$ShaderOption$Float");
  jclass integer_class = env->FindClass(
      "org/dolphinemu/dolphinemu/features/settings/model/PostProcessing$ShaderOption$Integer");

  jmethodID bool_constructor = env->GetMethodID(
      bool_class, "<init>", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Z)V");
  jmethodID float_constructor = env->GetMethodID(
      float_class, "<init>", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;[F[F[F[F)V");
  jmethodID integer_constructor = env->GetMethodID(
      integer_class, "<init>", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;[I[I[I[I)V");

  for (const auto& option_pair : config.GetOptions())
  {
    const auto& option = option_pair.second;
    jobject java_option = nullptr;

    jstring gui_name = env->NewStringUTF(option.m_gui_name.c_str());
    jstring option_name = env->NewStringUTF(option.m_option_name.c_str());
    jstring dependent_option = env->NewStringUTF(option.m_dependent_option.c_str());

    switch (option.m_type)
    {
    case VideoCommon::PostProcessingConfiguration::ConfigurationOption::OptionType::Bool:
      java_option = env->NewObject(bool_class, bool_constructor, gui_name, option_name,
                                   dependent_option, option.m_bool_value);
      break;

    case VideoCommon::PostProcessingConfiguration::ConfigurationOption::OptionType::Float:
    {
      jfloatArray values = env->NewFloatArray(static_cast<jsize>(option.m_float_values.size()));
      env->SetFloatArrayRegion(values, 0, static_cast<jsize>(option.m_float_values.size()),
                               option.m_float_values.data());

      jfloatArray min_values =
          env->NewFloatArray(static_cast<jsize>(option.m_float_min_values.size()));
      env->SetFloatArrayRegion(min_values, 0, static_cast<jsize>(option.m_float_min_values.size()),
                               option.m_float_min_values.data());

      jfloatArray max_values =
          env->NewFloatArray(static_cast<jsize>(option.m_float_max_values.size()));
      env->SetFloatArrayRegion(max_values, 0, static_cast<jsize>(option.m_float_max_values.size()),
                               option.m_float_max_values.data());

      jfloatArray step_values =
          env->NewFloatArray(static_cast<jsize>(option.m_float_step_values.size()));
      env->SetFloatArrayRegion(step_values, 0,
                               static_cast<jsize>(option.m_float_step_values.size()),
                               option.m_float_step_values.data());

      java_option = env->NewObject(float_class, float_constructor, gui_name, option_name,
                                   dependent_option, values, min_values, max_values, step_values);
      break;
    }

    case VideoCommon::PostProcessingConfiguration::ConfigurationOption::OptionType::Integer:
    {
      jintArray values = env->NewIntArray(static_cast<jsize>(option.m_integer_values.size()));
      env->SetIntArrayRegion(values, 0, static_cast<jsize>(option.m_integer_values.size()),
                             option.m_integer_values.data());

      jintArray min_values =
          env->NewIntArray(static_cast<jsize>(option.m_integer_min_values.size()));
      env->SetIntArrayRegion(min_values, 0, static_cast<jsize>(option.m_integer_min_values.size()),
                             option.m_integer_min_values.data());

      jintArray max_values =
          env->NewIntArray(static_cast<jsize>(option.m_integer_max_values.size()));
      env->SetIntArrayRegion(max_values, 0, static_cast<jsize>(option.m_integer_max_values.size()),
                             option.m_integer_max_values.data());

      jintArray step_values =
          env->NewIntArray(static_cast<jsize>(option.m_integer_step_values.size()));
      env->SetIntArrayRegion(step_values, 0,
                             static_cast<jsize>(option.m_integer_step_values.size()),
                             option.m_integer_step_values.data());

      java_option = env->NewObject(integer_class, integer_constructor, gui_name, option_name,
                                   dependent_option, values, min_values, max_values, step_values);
      break;
    }
    }

    if (java_option)
    {
      env->CallBooleanMethod(options_list, array_list_add, java_option);
    }

    env->DeleteLocalRef(gui_name);
    env->DeleteLocalRef(option_name);
    env->DeleteLocalRef(dependent_option);
  }

  return env->NewObject(shader_options_class, shader_options_constructor, options_list);
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_PostProcessing_setShaderOption__Ljava_lang_String_2Ljava_lang_String_2Z(
    JNIEnv* env, jclass, jstring shader_name, jstring option_name, jboolean value)
{
  const char* shader_name_chars = env->GetStringUTFChars(shader_name, nullptr);
  const std::string shader_name_str(shader_name_chars);
  env->ReleaseStringUTFChars(shader_name, shader_name_chars);

  const char* option_name_chars = env->GetStringUTFChars(option_name, nullptr);
  const std::string option_name_str(option_name_chars);
  env->ReleaseStringUTFChars(option_name, option_name_chars);
  VideoCommon::PostProcessingConfiguration config;
  config.LoadShader(shader_name_str);
  config.SetOptionb(option_name_str, value);
  config.SaveOptionsConfiguration();
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_PostProcessing_setShaderOption__Ljava_lang_String_2Ljava_lang_String_2_3F(
    JNIEnv* env, jclass, jstring shader_name, jstring option_name, jfloatArray values)
{
  const char* shader_name_chars = env->GetStringUTFChars(shader_name, nullptr);
  const std::string shader_name_str(shader_name_chars);
  env->ReleaseStringUTFChars(shader_name, shader_name_chars);

  const char* option_name_chars = env->GetStringUTFChars(option_name, nullptr);
  const std::string option_name_str(option_name_chars);
  env->ReleaseStringUTFChars(option_name, option_name_chars);

  VideoCommon::PostProcessingConfiguration config;
  config.LoadShader(shader_name_str);
  jfloat* elements = env->GetFloatArrayElements(values, nullptr);
  for (int i = 0; i < env->GetArrayLength(values); i++)
  {
    config.SetOptionf(option_name_str, i, elements[i]);
  }
  env->ReleaseFloatArrayElements(values, elements, JNI_ABORT);
  config.SaveOptionsConfiguration();
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_PostProcessing_setShaderOption__Ljava_lang_String_2Ljava_lang_String_2_3I(
    JNIEnv* env, jclass, jstring shader_name, jstring option_name, jintArray values)
{
  const char* shader_name_chars = env->GetStringUTFChars(shader_name, nullptr);
  const std::string shader_name_str(shader_name_chars);
  env->ReleaseStringUTFChars(shader_name, shader_name_chars);

  const char* option_name_chars = env->GetStringUTFChars(option_name, nullptr);
  const std::string option_name_str(option_name_chars);
  env->ReleaseStringUTFChars(option_name, option_name_chars);

  VideoCommon::PostProcessingConfiguration config;
  config.LoadShader(shader_name_str);
  jint* elements = env->GetIntArrayElements(values, nullptr);
  for (int i = 0; i < env->GetArrayLength(values); i++)
  {
    config.SetOptioni(option_name_str, i, elements[i]);
  }
  env->ReleaseIntArrayElements(values, elements, JNI_ABORT);
  config.SaveOptionsConfiguration();
}
}
