
#include <jni.h>

#include "Common/IniFile.h"
#include "jni/AndroidCommon/AndroidCommon.h"
#include "jni/AndroidCommon/IDCache.h"

static IniFile* GetPointer(JNIEnv* env, jobject obj)
{
  return reinterpret_cast<IniFile*>(env->GetLongField(obj, IDCache::sIniFile.Pointer));
}

#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT jlong JNICALL Java_org_dolphinemu_dolphinemu_model_IniFile_newIniFile(JNIEnv* env,
                                                                                jobject obj)
{
  return reinterpret_cast<jlong>(new IniFile());
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_model_IniFile_finalize(JNIEnv* env,
                                                                             jobject obj)
{
  delete GetPointer(env, obj);
}

JNIEXPORT jboolean JNICALL Java_org_dolphinemu_dolphinemu_model_IniFile_loadFile(
    JNIEnv* env, jobject obj, jstring jPath, jboolean jKeepCurrentData)
{
  std::string path = GetJString(env, jPath);
  return GetPointer(env, obj)->Load(path, jKeepCurrentData);
}

JNIEXPORT jboolean JNICALL Java_org_dolphinemu_dolphinemu_model_IniFile_saveFile(JNIEnv* env,
                                                                                 jobject obj,
                                                                                 jstring jPath)
{
  std::string path = GetJString(env, jPath);
  return GetPointer(env, obj)->Save(path);
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_model_IniFile_setString(
    JNIEnv* env, jobject obj, jstring jSection, jstring jKey, jstring jValue)
{
  std::string section = GetJString(env, jSection);
  std::string key = GetJString(env, jKey);
  std::string value = GetJString(env, jValue);
  GetPointer(env, obj)->GetOrCreateSection(section)->Set(key, value);
}

JNIEXPORT jstring JNICALL Java_org_dolphinemu_dolphinemu_model_IniFile_getString(
    JNIEnv* env, jobject obj, jstring jSection, jstring jKey, jstring jDefaultValue)
{
  std::string section = GetJString(env, jSection);
  std::string key = GetJString(env, jKey);
  std::string value;
  std::string default_value = GetJString(env, jDefaultValue);
  GetPointer(env, obj)->GetOrCreateSection(section)->Get(key, &value, default_value);
  return ToJString(env, value);
}

JNIEXPORT jboolean JNICALL Java_org_dolphinemu_dolphinemu_model_IniFile_delete(
  JNIEnv* env, jobject obj, jstring jSection, jstring jKey)
{
  std::string section = GetJString(env, jSection);
  std::string key = GetJString(env, jKey);
  return GetPointer(env, obj)->GetOrCreateSection(section)->Delete(key);
}

#ifdef __cplusplus
}
#endif
