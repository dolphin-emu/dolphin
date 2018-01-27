// Copyright 2003 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <EGL/egl.h>
#include <android/log.h>
#include <android/native_window_jni.h>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <jni.h>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "ButtonManager.h"

#include "Common/CPUDetect.h"
#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/Event.h"
#include "Common/FileUtil.h"
#include "Common/GL/GLInterfaceBase.h"
#include "Common/Logging/LogManager.h"
#include "Common/MsgHandler.h"
#include "Common/Version.h"

#include "Core/Boot/Boot.h"
#include "Core/BootManager.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteReal/WiimoteReal.h"
#include "Core/Host.h"
#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/Profiler.h"
#include "Core/State.h"

#include "DiscIO/Enums.h"
#include "DiscIO/Volume.h"

#include "UICommon/UICommon.h"

#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/VideoBackendBase.h"

#define DOLPHIN_TAG "DolphinEmuNative"

JavaVM* g_java_vm;

namespace
{
ANativeWindow* s_surf;
std::string s_set_userpath;

jclass s_jni_class;
jmethodID s_jni_method_alert;

// The Core only supports using a single Host thread.
// If multiple threads want to call host functions then they need to queue
// sequentially for access.
std::mutex s_host_identity_lock;
Common::Event s_update_main_frame_event;
bool s_have_wm_user_stop = false;
}  // Anonymous namespace

/*
 * Cache the JavaVM so that we can call into it later.
 */
jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
  g_java_vm = vm;

  return JNI_VERSION_1_6;
}

void Host_NotifyMapLoaded()
{
}
void Host_RefreshDSPDebuggerWindow()
{
}

void Host_Message(int Id)
{
  if (Id == WM_USER_JOB_DISPATCH)
  {
    s_update_main_frame_event.Set();
  }
  else if (Id == WM_USER_STOP)
  {
    s_have_wm_user_stop = true;
    if (Core::IsRunning())
      Core::QueueHostJob(&Core::Stop);
  }
}

void* Host_GetRenderHandle()
{
  return s_surf;
}

void Host_UpdateTitle(const std::string& title)
{
  __android_log_write(ANDROID_LOG_INFO, DOLPHIN_TAG, title.c_str());
}

void Host_UpdateDisasmDialog()
{
}

void Host_UpdateMainFrame()
{
}

void Host_RequestRenderWindowSize(int width, int height)
{
}

void Host_SetStartupDebuggingParameters()
{
}

bool Host_UINeedsControllerState()
{
  return true;
}

bool Host_RendererHasFocus()
{
  return true;
}

bool Host_RendererIsFullscreen()
{
  return false;
}

void Host_SetWiiMoteConnectionState(int _State)
{
}

void Host_ShowVideoConfig(void*, const std::string&)
{
}

void Host_YieldToUI()
{
}

void Host_UpdateProgressDialog(const char* caption, int position, int total)
{
}

static bool MsgAlert(const char* caption, const char* text, bool yes_no, MsgType /*style*/)
{
  __android_log_print(ANDROID_LOG_ERROR, DOLPHIN_TAG, "%s:%s", caption, text);

  // Associate the current Thread with the Java VM.
  JNIEnv* env;
  g_java_vm->AttachCurrentThread(&env, NULL);

  // Execute the Java method.
  env->CallStaticVoidMethod(s_jni_class, s_jni_method_alert, env->NewStringUTF(text));

  // Must be called before the current thread exits; might as well do it here.
  g_java_vm->DetachCurrentThread();

  return false;
}

#define DVD_BANNER_WIDTH 96
#define DVD_BANNER_HEIGHT 32

static inline u32 Average32(u32 a, u32 b)
{
  return ((a >> 1) & 0x7f7f7f7f) + ((b >> 1) & 0x7f7f7f7f);
}

static inline u32 GetPixel(u32* buffer, unsigned int x, unsigned int y)
{
  // thanks to unsignedness, these also check for <0 automatically.
  if (x > 191)
    return 0;
  if (y > 63)
    return 0;
  return buffer[y * 192 + x];
}

static bool LoadBanner(std::string filename, u32* Banner)
{
  std::unique_ptr<DiscIO::Volume> pVolume(DiscIO::CreateVolumeFromFilename(filename));

  if (pVolume != nullptr)
  {
    int Width, Height;
    std::vector<u32> BannerVec = pVolume->GetBanner(&Width, &Height);
    // This code (along with above inlines) is moved from
    // elsewhere.  Someone who knows anything about Android
    // please get rid of it and use proper high-resolution
    // images.
    if (Height == 64 && Width == 192)
    {
      u32* Buffer = &BannerVec[0];
      for (int y = 0; y < 32; y++)
      {
        for (int x = 0; x < 96; x++)
        {
          // simplified plus-shaped "gaussian"
          u32 surround = Average32(
              Average32(GetPixel(Buffer, x * 2 - 1, y * 2), GetPixel(Buffer, x * 2 + 1, y * 2)),
              Average32(GetPixel(Buffer, x * 2, y * 2 - 1), GetPixel(Buffer, x * 2, y * 2 + 1)));
          Banner[y * 96 + x] = Average32(GetPixel(Buffer, x * 2, y * 2), surround);
        }
      }
      return true;
    }
    else if (Height == 32 && Width == 96)
    {
      memcpy(Banner, &BannerVec[0], 96 * 32 * 4);
      return true;
    }
  }

  return false;
}

static int GetCountry(std::string filename)
{
  std::unique_ptr<DiscIO::Volume> pVolume(DiscIO::CreateVolumeFromFilename(filename));

  if (pVolume != nullptr)
  {
    int country = static_cast<int>(pVolume->GetCountry());

    __android_log_print(ANDROID_LOG_INFO, DOLPHIN_TAG, "Country Code: %i", country);

    return country;
  }

  return static_cast<int>(DiscIO::Country::COUNTRY_UNKNOWN);
}

static int GetPlatform(std::string filename)
{
  std::unique_ptr<DiscIO::Volume> pVolume(DiscIO::CreateVolumeFromFilename(filename));

  if (pVolume != nullptr)
  {
    switch (pVolume->GetVolumeType())
    {
    case DiscIO::Platform::GAMECUBE_DISC:
      __android_log_print(ANDROID_LOG_INFO, DOLPHIN_TAG, "Volume is a GameCube disc.");
      return 0;
    case DiscIO::Platform::WII_DISC:
      __android_log_print(ANDROID_LOG_INFO, DOLPHIN_TAG, "Volume is a Wii disc.");
      return 1;
    case DiscIO::Platform::WII_WAD:
      __android_log_print(ANDROID_LOG_INFO, DOLPHIN_TAG, "Volume is a Wii WAD.");
      return 2;
    }
  }

  return -1;
}

static std::string GetTitle(std::string filename)
{
  __android_log_print(ANDROID_LOG_WARN, DOLPHIN_TAG, "Getting Title for file: %s",
                      filename.c_str());

  std::unique_ptr<DiscIO::Volume> pVolume(DiscIO::CreateVolumeFromFilename(filename));

  if (pVolume != nullptr)
  {
    std::map<DiscIO::Language, std::string> titles = pVolume->GetLongNames();
    if (titles.empty())
      titles = pVolume->GetShortNames();

    /*
    bool is_wii_title = pVolume->GetVolumeType() != DiscIO::Platform::GAMECUBE_DISC;
    DiscIO::Language language = SConfig::GetInstance().GetCurrentLanguage(is_wii_title);

    auto it = titles.find(language);
    if (it != end)
      return it->second;*/

    auto end = titles.end();

    // English tends to be a good fallback when the requested language isn't available
    // if (language != DiscIO::Language::LANGUAGE_ENGLISH) {
    auto it = titles.find(DiscIO::Language::LANGUAGE_ENGLISH);
    if (it != end)
      return it->second;
    //}

    // If English isn't available either, just pick something
    if (!titles.empty())
      return titles.cbegin()->second;

    // No usable name, return filename (better than nothing)
    std::string name;
    SplitPath(filename, nullptr, &name, nullptr);
    return name;
  }

  return std::string("");
}

static std::string GetDescription(std::string filename)
{
  __android_log_print(ANDROID_LOG_WARN, DOLPHIN_TAG, "Getting Description for file: %s",
                      filename.c_str());

  std::unique_ptr<DiscIO::Volume> volume(DiscIO::CreateVolumeFromFilename(filename));

  if (volume != nullptr)
  {
    std::map<DiscIO::Language, std::string> descriptions = volume->GetDescriptions();

    /*
    bool is_wii_title = pVolume->GetVolumeType() != DiscIO::Platform::GAMECUBE_DISC;
    DiscIO::Language language = SConfig::GetInstance().GetCurrentLanguage(is_wii_title);

    auto it = descriptions.find(language);
    if (it != end)
      return it->second;*/

    auto end = descriptions.end();

    // English tends to be a good fallback when the requested language isn't available
    // if (language != DiscIO::Language::LANGUAGE_ENGLISH) {
    auto it = descriptions.find(DiscIO::Language::LANGUAGE_ENGLISH);
    if (it != end)
      return it->second;
    //}

    // If English isn't available either, just pick something
    if (!descriptions.empty())
      return descriptions.cbegin()->second;
  }

  return std::string();
}

static std::string GetGameId(std::string filename)
{
  __android_log_print(ANDROID_LOG_WARN, DOLPHIN_TAG, "Getting ID for file: %s", filename.c_str());

  std::unique_ptr<DiscIO::Volume> volume(DiscIO::CreateVolumeFromFilename(filename));
  if (volume == nullptr)
    return std::string();

  std::string id = volume->GetGameID();
  __android_log_print(ANDROID_LOG_INFO, DOLPHIN_TAG, "Game ID: %s", id.c_str());
  return id;
}

static std::string GetCompany(std::string filename)
{
  __android_log_print(ANDROID_LOG_WARN, DOLPHIN_TAG, "Getting Company for file: %s",
                      filename.c_str());

  std::unique_ptr<DiscIO::Volume> volume(DiscIO::CreateVolumeFromFilename(filename));
  if (volume == nullptr)
    return std::string();

  std::string company = DiscIO::GetCompanyFromID(volume->GetMakerID());
  __android_log_print(ANDROID_LOG_INFO, DOLPHIN_TAG, "Company: %s", company.c_str());
  return company;
}

static u64 GetFileSize(std::string filename)
{
  __android_log_print(ANDROID_LOG_WARN, DOLPHIN_TAG, "Getting size of file: %s", filename.c_str());

  std::unique_ptr<DiscIO::Volume> volume(DiscIO::CreateVolumeFromFilename(filename));
  if (volume == nullptr)
    return -1;

  u64 size = volume->GetSize();
  __android_log_print(ANDROID_LOG_INFO, DOLPHIN_TAG, "Size: %" PRIu64, size);
  return size;
}

static std::string GetJString(JNIEnv* env, jstring jstr)
{
  std::string result = "";
  if (!jstr)
    return result;

  const char* s = env->GetStringUTFChars(jstr, nullptr);
  result = s;
  env->ReleaseStringUTFChars(jstr, s);
  return result;
}

#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_UnPauseEmulation(JNIEnv* env,
                                                                                     jobject obj);
JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_PauseEmulation(JNIEnv* env,
                                                                                   jobject obj);
JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_StopEmulation(JNIEnv* env,
                                                                                  jobject obj);
JNIEXPORT jboolean JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_onGamePadEvent(
    JNIEnv* env, jobject obj, jstring jDevice, jint Button, jint Action);
JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_onGamePadMoveEvent(
    JNIEnv* env, jobject obj, jstring jDevice, jint Axis, jfloat Value);
JNIEXPORT jintArray JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_GetBanner(JNIEnv* env,
                                                                                   jobject obj,
                                                                                   jstring jFile);
JNIEXPORT jstring JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_GetTitle(JNIEnv* env,
                                                                                jobject obj,
                                                                                jstring jFilename);
JNIEXPORT jstring JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_GetDescription(
    JNIEnv* env, jobject obj, jstring jFilename);
JNIEXPORT jstring JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_GetGameId(JNIEnv* env,
                                                                                 jobject obj,
                                                                                 jstring jFilename);
JNIEXPORT jint JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_GetCountry(JNIEnv* env,
                                                                               jobject obj,
                                                                               jstring jFilename);
JNIEXPORT jstring JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_GetCompany(
    JNIEnv* env, jobject obj, jstring jFilename);
JNIEXPORT jlong JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_GetFilesize(JNIEnv* env,
                                                                                 jobject obj,
                                                                                 jstring jFilename);
JNIEXPORT jint JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_GetPlatform(JNIEnv* env,
                                                                                jobject obj,
                                                                                jstring jFilename);
JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_NativeLibrary_GetVersionString(JNIEnv* env, jobject obj);
JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_SaveScreenShot(JNIEnv* env,
                                                                                   jobject obj);
JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_eglBindAPI(JNIEnv* env,
                                                                               jobject obj,
                                                                               jint api);
JNIEXPORT jstring JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_GetConfig(
    JNIEnv* env, jobject obj, jstring jFile, jstring jSection, jstring jKey, jstring jDefault);
JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_SetConfig(
    JNIEnv* env, jobject obj, jstring jFile, jstring jSection, jstring jKey, jstring jValue);
JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_SetFilename(JNIEnv* env,
                                                                                jobject obj,
                                                                                jstring jFile);
JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_SaveState(JNIEnv* env,
                                                                              jobject obj,
                                                                              jint slot);
JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_LoadState(JNIEnv* env,
                                                                              jobject obj,
                                                                              jint slot);
JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_CreateUserFolders(JNIEnv* env,
                                                                                      jobject obj);
JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_SetUserDirectory(
    JNIEnv* env, jobject obj, jstring jDirectory);
JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_NativeLibrary_GetUserDirectory(JNIEnv* env, jobject obj);
JNIEXPORT jint JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_DefaultCPUCore(JNIEnv* env,
                                                                                   jobject obj);
JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_SetProfiling(JNIEnv* env,
                                                                                 jobject obj,
                                                                                 jboolean enable);
JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_NativeLibrary_WriteProfileResults(JNIEnv* env, jobject obj);
JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_NativeLibrary_CacheClassesAndMethods(JNIEnv* env, jobject obj);
JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_Run(JNIEnv* env, jobject obj,
                                                                        jstring jFile);
JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_SurfaceChanged(JNIEnv* env,
                                                                                   jobject obj,
                                                                                   jobject surf);
JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_SurfaceDestroyed(JNIEnv* env,
                                                                                     jobject obj);

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_UnPauseEmulation(JNIEnv* env,
                                                                                     jobject obj)
{
  std::lock_guard<std::mutex> guard(s_host_identity_lock);
  Core::SetState(Core::State::Running);
}
JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_PauseEmulation(JNIEnv* env,
                                                                                   jobject obj)
{
  std::lock_guard<std::mutex> guard(s_host_identity_lock);
  Core::SetState(Core::State::Paused);
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_StopEmulation(JNIEnv* env,
                                                                                  jobject obj)
{
  std::lock_guard<std::mutex> guard(s_host_identity_lock);
  Core::SaveScreenShot("thumb", true);
  Core::Stop();
  s_update_main_frame_event.Set();  // Kick the waiting event
}
JNIEXPORT jboolean JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_onGamePadEvent(
    JNIEnv* env, jobject obj, jstring jDevice, jint Button, jint Action)
{
  return ButtonManager::GamepadEvent(GetJString(env, jDevice), Button, Action);
}
JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_onGamePadMoveEvent(
    JNIEnv* env, jobject obj, jstring jDevice, jint Axis, jfloat Value)
{
  ButtonManager::GamepadAxisEvent(GetJString(env, jDevice), Axis, Value);
}

JNIEXPORT jintArray JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_GetBanner(JNIEnv* env,
                                                                                   jobject obj,
                                                                                   jstring jFile)
{
  std::string file = GetJString(env, jFile);
  u32 uBanner[DVD_BANNER_WIDTH * DVD_BANNER_HEIGHT];
  jintArray Banner = env->NewIntArray(DVD_BANNER_WIDTH * DVD_BANNER_HEIGHT);

  if (LoadBanner(file, uBanner))
  {
    env->SetIntArrayRegion(Banner, 0, DVD_BANNER_WIDTH * DVD_BANNER_HEIGHT, (jint*)uBanner);
  }
  return Banner;
}

JNIEXPORT jstring JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_GetTitle(JNIEnv* env,
                                                                                jobject obj,
                                                                                jstring jFilename)
{
  std::string filename = GetJString(env, jFilename);
  std::string name = GetTitle(filename);
  return env->NewStringUTF(name.c_str());
}

JNIEXPORT jstring JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_GetDescription(
    JNIEnv* env, jobject obj, jstring jFilename)
{
  std::string filename = GetJString(env, jFilename);
  std::string description = GetDescription(filename);
  return env->NewStringUTF(description.c_str());
}

JNIEXPORT jstring JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_GetGameId(JNIEnv* env,
                                                                                 jobject obj,
                                                                                 jstring jFilename)
{
  std::string filename = GetJString(env, jFilename);
  std::string id = GetGameId(filename);
  return env->NewStringUTF(id.c_str());
}

JNIEXPORT jstring JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_GetCompany(JNIEnv* env,
                                                                                  jobject obj,
                                                                                  jstring jFilename)
{
  std::string filename = GetJString(env, jFilename);
  std::string company = GetCompany(filename);
  return env->NewStringUTF(company.c_str());
}

JNIEXPORT jint JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_GetCountry(JNIEnv* env,
                                                                               jobject obj,
                                                                               jstring jFilename)
{
  std::string filename = GetJString(env, jFilename);
  int country = GetCountry(filename);
  return country;
}

JNIEXPORT jlong JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_GetFilesize(JNIEnv* env,
                                                                                 jobject obj,
                                                                                 jstring jFilename)
{
  std::string filename = GetJString(env, jFilename);
  u64 size = GetFileSize(filename);
  return size;
}

JNIEXPORT jint JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_GetPlatform(JNIEnv* env,
                                                                                jobject obj,
                                                                                jstring jFilename)
{
  std::string filename = GetJString(env, jFilename);
  int platform = GetPlatform(filename);
  return platform;
}

JNIEXPORT jstring JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_GetVersionString(JNIEnv* env,
                                                                                        jobject obj)
{
  return env->NewStringUTF(Common::scm_rev_str.c_str());
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_SaveScreenShot(JNIEnv* env,
                                                                                   jobject obj)
{
  std::lock_guard<std::mutex> guard(s_host_identity_lock);
  Core::SaveScreenShot();
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_eglBindAPI(JNIEnv* env,
                                                                               jobject obj,
                                                                               jint api)
{
  eglBindAPI(api);
}

JNIEXPORT jstring JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_GetConfig(
    JNIEnv* env, jobject obj, jstring jFile, jstring jSection, jstring jKey, jstring jDefault)
{
  IniFile ini;
  std::string file = GetJString(env, jFile);
  std::string section = GetJString(env, jSection);
  std::string key = GetJString(env, jKey);
  std::string defaultValue = GetJString(env, jDefault);

  ini.Load(File::GetUserPath(D_CONFIG_IDX) + std::string(file));
  std::string value;

  ini.GetOrCreateSection(section)->Get(key, &value, defaultValue);

  return env->NewStringUTF(value.c_str());
}
JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_SetConfig(
    JNIEnv* env, jobject obj, jstring jFile, jstring jSection, jstring jKey, jstring jValue)
{
  IniFile ini;
  std::string file = GetJString(env, jFile);
  std::string section = GetJString(env, jSection);
  std::string key = GetJString(env, jKey);
  std::string value = GetJString(env, jValue);

  ini.Load(File::GetUserPath(D_CONFIG_IDX) + std::string(file));

  ini.GetOrCreateSection(section)->Set(key, value);
  ini.Save(File::GetUserPath(D_CONFIG_IDX) + std::string(file));
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_SaveState(JNIEnv* env,
                                                                              jobject obj,
                                                                              jint slot)
{
  std::lock_guard<std::mutex> guard(s_host_identity_lock);
  State::Save(slot);
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_LoadState(JNIEnv* env,
                                                                              jobject obj,
                                                                              jint slot)
{
  std::lock_guard<std::mutex> guard(s_host_identity_lock);
  State::Load(slot);
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_CreateUserFolders(JNIEnv* env,
                                                                                      jobject obj)
{
  File::CreateFullPath(File::GetUserPath(D_CONFIG_IDX));
  File::CreateFullPath(File::GetUserPath(D_GCUSER_IDX));
  File::CreateFullPath(File::GetUserPath(D_WIIROOT_IDX) + DIR_SEP WII_WC24CONF_DIR DIR_SEP
                       "mbox" DIR_SEP);
  File::CreateFullPath(File::GetUserPath(D_WIIROOT_IDX) + DIR_SEP "shared2" DIR_SEP
                                                                  "succession" DIR_SEP);
  File::CreateFullPath(File::GetUserPath(D_WIIROOT_IDX) + DIR_SEP "shared2" DIR_SEP "ec" DIR_SEP);
  File::CreateFullPath(File::GetUserPath(D_WIIROOT_IDX) + DIR_SEP WII_SYSCONF_DIR DIR_SEP);
  File::CreateFullPath(File::GetUserPath(D_CACHE_IDX));
  File::CreateFullPath(File::GetUserPath(D_DUMPDSP_IDX));
  File::CreateFullPath(File::GetUserPath(D_DUMPTEXTURES_IDX));
  File::CreateFullPath(File::GetUserPath(D_HIRESTEXTURES_IDX));
  File::CreateFullPath(File::GetUserPath(D_SCREENSHOTS_IDX));
  File::CreateFullPath(File::GetUserPath(D_STATESAVES_IDX));
  File::CreateFullPath(File::GetUserPath(D_MAILLOGS_IDX));
  File::CreateFullPath(File::GetUserPath(D_SHADERS_IDX) + "Anaglyph" DIR_SEP);
  File::CreateFullPath(File::GetUserPath(D_GCUSER_IDX) + USA_DIR DIR_SEP);
  File::CreateFullPath(File::GetUserPath(D_GCUSER_IDX) + EUR_DIR DIR_SEP);
  File::CreateFullPath(File::GetUserPath(D_GCUSER_IDX) + JAP_DIR DIR_SEP);
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_SetUserDirectory(
    JNIEnv* env, jobject obj, jstring jDirectory)
{
  std::lock_guard<std::mutex> guard(s_host_identity_lock);
  std::string directory = GetJString(env, jDirectory);
  s_set_userpath = directory;
  UICommon::SetUserDirectory(directory);
}

JNIEXPORT jstring JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_GetUserDirectory(JNIEnv* env,
                                                                                        jobject obj)
{
  return env->NewStringUTF(File::GetUserPath(D_USER_IDX).c_str());
}

JNIEXPORT jint JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_DefaultCPUCore(JNIEnv* env,
                                                                                   jobject obj)
{
  return PowerPC::DefaultCPUCore();
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_SetProfiling(JNIEnv* env,
                                                                                 jobject obj,
                                                                                 jboolean enable)
{
  std::lock_guard<std::mutex> guard(s_host_identity_lock);
  Core::SetState(Core::State::Paused);
  JitInterface::ClearCache();
  Profiler::g_ProfileBlocks = enable;
  Core::SetState(Core::State::Running);
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_WriteProfileResults(JNIEnv* env,
                                                                                        jobject obj)
{
  std::lock_guard<std::mutex> guard(s_host_identity_lock);
  std::string filename = File::GetUserPath(D_DUMP_IDX) + "Debug/profiler.txt";
  File::CreateFullPath(filename);
  JitInterface::WriteProfileResults(filename);
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_NativeLibrary_CacheClassesAndMethods(JNIEnv* env, jobject obj)
{
  // This class reference is only valid for the lifetime of this method.
  jclass localClass = env->FindClass("org/dolphinemu/dolphinemu/NativeLibrary");

  // This reference, however, is valid until we delete it.
  s_jni_class = reinterpret_cast<jclass>(env->NewGlobalRef(localClass));

  // TODO Find a place for this.
  // So we don't leak a reference to NativeLibrary.class.
  // env->DeleteGlobalRef(s_jni_class);

  // Method signature taken from javap -s
  // Source/Android/app/build/intermediates/classes/arm/debug/org/dolphinemu/dolphinemu/NativeLibrary.class
  s_jni_method_alert =
      env->GetStaticMethodID(s_jni_class, "displayAlertMsg", "(Ljava/lang/String;)V");
}

// Surface Handling
JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_SurfaceChanged(JNIEnv* env,
                                                                                   jobject obj,
                                                                                   jobject surf)
{
  s_surf = ANativeWindow_fromSurface(env, surf);
  if (s_surf == nullptr)
    __android_log_print(ANDROID_LOG_ERROR, DOLPHIN_TAG, "Error: Surface is null.");

  if (g_renderer)
    g_renderer->ChangeSurface(s_surf);
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_SurfaceDestroyed(JNIEnv* env,
                                                                                     jobject obj)
{
  if (g_renderer)
    g_renderer->ChangeSurface(nullptr);

  if (s_surf)
  {
    ANativeWindow_release(s_surf);
    s_surf = nullptr;
  }
}
JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_RefreshWiimotes(JNIEnv* env,
                                                                                    jobject obj)
{
  std::lock_guard<std::mutex> guard(s_host_identity_lock);
  WiimoteReal::Refresh();
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_Run(JNIEnv* env, jobject obj,
                                                                        jstring jFile)
{
  const std::string path = GetJString(env, jFile);
  __android_log_print(ANDROID_LOG_INFO, DOLPHIN_TAG, "Running : %s", path.c_str());

  // Install our callbacks
  OSD::AddCallback(OSD::CallbackType::Initialization, ButtonManager::Init);
  OSD::AddCallback(OSD::CallbackType::Shutdown, ButtonManager::Shutdown);

  RegisterMsgAlertHandler(&MsgAlert);

  std::unique_lock<std::mutex> guard(s_host_identity_lock);
  UICommon::SetUserDirectory(s_set_userpath);
  UICommon::Init();

  WiimoteReal::InitAdapterClass();

  // No use running the loop when booting fails
  s_have_wm_user_stop = false;
  if (BootManager::BootCore(BootParameters::GenerateFromFile(path)))
  {
    static constexpr int TIMEOUT = 10000;
    static constexpr int WAIT_STEP = 25;
    int time_waited = 0;
    // A Core::CORE_ERROR state would be helpful here.
    while (!Core::IsRunning() && time_waited < TIMEOUT && !s_have_wm_user_stop)
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(WAIT_STEP));
      time_waited += WAIT_STEP;
    }
    while (Core::IsRunning())
    {
      guard.unlock();
      s_update_main_frame_event.Wait();
      guard.lock();
      Core::HostDispatchJobs();
    }
  }

  Core::Shutdown();
  UICommon::Shutdown();
  guard.unlock();

  if (s_surf)
  {
    ANativeWindow_release(s_surf);
    s_surf = nullptr;
  }
}

#ifdef __cplusplus
}
#endif
