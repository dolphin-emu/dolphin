// Copyright 2003 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <EGL/egl.h>
#include <UICommon/GameFile.h>
#include <android/log.h>
#include <android/native_window_jni.h>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <jni.h>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>

#include "Common/AndroidAnalytics.h"
#include "Common/CPUDetect.h"
#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/Event.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/Logging/LogManager.h"
#include "Common/MsgHandler.h"
#include "Common/Version.h"
#include "Common/WindowSystemInfo.h"

#include "Core/Analytics.h"
#include "Core/Boot/Boot.h"
#include "Core/BootManager.h"
#include "Core/ConfigLoaders/GameConfigLoader.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/DVD/DVDInterface.h"
#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteReal/WiimoteReal.h"
#include "Core/Host.h"
#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/Profiler.h"
#include "Core/State.h"
#include "Core/WiiUtils.h"

#include "DiscIO/Enums.h"
#include "DiscIO/Volume.h"

#include "InputCommon/ControllerInterface/Android/Android.h"
#include "InputCommon/ControllerInterface/Touch/ButtonManager.h"

#include "UICommon/UICommon.h"

#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/VideoBackendBase.h"

#include "../../Core/Common/WindowSystemInfo.h"
#include "jni/AndroidCommon/AndroidCommon.h"
#include "jni/AndroidCommon/IDCache.h"

namespace
{
constexpr char DOLPHIN_TAG[] = "DolphinEmuNative";

ANativeWindow* s_surf;
IniFile s_ini;

// The Core only supports using a single Host thread.
// If multiple threads want to call host functions then they need to queue
// sequentially for access.
std::mutex s_host_identity_lock;
Common::Event s_update_main_frame_event;
bool s_have_wm_user_stop = false;
}  // Anonymous namespace

void UpdatePointer()
{
  // Update touch pointer
  JNIEnv* env = IDCache::GetEnvForThread();
  env->CallStaticVoidMethod(IDCache::GetNativeLibraryClass(), IDCache::GetUpdateTouchPointer());
}

void Host_NotifyMapLoaded()
{
}
void Host_RefreshDSPDebuggerWindow()
{
}
bool Host_UIBlocksControllerState()
{
  return false;
}

void Host_Message(HostMessageID id)
{
  if (id == HostMessageID::WMUserJobDispatch)
  {
    s_update_main_frame_event.Set();
  }
  else if (id == HostMessageID::WMUserStop)
  {
    s_have_wm_user_stop = true;
    if (Core::IsRunning())
      Core::QueueHostJob(&Core::Stop);
  }
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
  std::thread jnicall(UpdatePointer);
  jnicall.join();
}

bool Host_RendererHasFocus()
{
  return true;
}

bool Host_RendererIsFullscreen()
{
  return false;
}

void Host_YieldToUI()
{
}

void Host_TitleChanged()
{
}

static bool MsgAlert(const char* caption, const char* text, bool yes_no, Common::MsgType /*style*/)
{
  JNIEnv* env = IDCache::GetEnvForThread();

  // Execute the Java method.
  jboolean result = env->CallStaticBooleanMethod(
      IDCache::GetNativeLibraryClass(), IDCache::GetDisplayAlertMsg(), ToJString(env, caption),
      ToJString(env, text), yes_no ? JNI_TRUE : JNI_FALSE);

  return result != JNI_FALSE;
}

static void ReportSend(const std::string& endpoint, const std::string& report)
{
  JNIEnv* env = IDCache::GetEnvForThread();

  jbyteArray output_array = env->NewByteArray(report.size());
  jbyte* output = env->GetByteArrayElements(output_array, nullptr);
  memcpy(output, report.data(), report.size());
  env->ReleaseByteArrayElements(output_array, output, 0);
  env->CallStaticVoidMethod(IDCache::GetAnalyticsClass(), IDCache::GetSendAnalyticsReport(),
                            ToJString(env, endpoint), output_array);
}

static std::string GetAnalyticValue(const std::string& key)
{
  JNIEnv* env = IDCache::GetEnvForThread();

  auto value = reinterpret_cast<jstring>(env->CallStaticObjectMethod(
      IDCache::GetAnalyticsClass(), IDCache::GetAnalyticsValue(), ToJString(env, key)));

  std::string stdvalue = GetJString(env, value);

  return stdvalue;
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
JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_NativeLibrary_WaitUntilDoneBooting(JNIEnv* env, jobject obj);
JNIEXPORT jboolean JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_IsRunning(JNIEnv* env,
                                                                                  jobject obj);
JNIEXPORT jboolean JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_onGamePadEvent(
    JNIEnv* env, jobject obj, jstring jDevice, jint Button, jint Action);
JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_onGamePadMoveEvent(
    JNIEnv* env, jobject obj, jstring jDevice, jint Axis, jfloat Value);
JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_SetMotionSensorsEnabled(
    JNIEnv* env, jobject obj, jboolean accelerometer_enabled, jboolean gyroscope_enabled);
JNIEXPORT double JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_GetInputRadiusAtAngle(
    JNIEnv* env, jobject obj, int emu_pad_id, int stick, double angle);
JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_NativeLibrary_GetVersionString(JNIEnv* env, jobject obj);
JNIEXPORT jstring JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_GetGitRevision(JNIEnv* env,
                                                                                      jobject obj);
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
                                                                              jint slot,
                                                                              jboolean wait);
JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_SaveStateAs(JNIEnv* env,
                                                                                jobject obj,
                                                                                jstring path,
                                                                                jboolean wait);
JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_LoadState(JNIEnv* env,
                                                                              jobject obj,
                                                                              jint slot);
JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_LoadStateAs(JNIEnv* env,
                                                                                jobject obj,
                                                                                jstring path);
JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_utils_DirectoryInitialization_CreateUserDirectories(JNIEnv* env,
                                                                                   jobject obj);
JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_SetUserDirectory(
    JNIEnv* env, jobject obj, jstring jDirectory);
JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_NativeLibrary_GetUserDirectory(JNIEnv* env, jobject obj);
JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_SetCacheDirectory(
    JNIEnv* env, jobject obj, jstring jDirectory);
JNIEXPORT jint JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_DefaultCPUCore(JNIEnv* env,
                                                                                   jobject obj);
JNIEXPORT jint JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_GetMaxLogLevel(JNIEnv* env,
                                                                                   jobject obj);
JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_SetProfiling(JNIEnv* env,
                                                                                 jobject obj,
                                                                                 jboolean enable);
JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_Initialize(JNIEnv* env,
                                                                               jobject obj);
JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_NativeLibrary_ReportStartToAnalytics(JNIEnv* env, jobject obj);
JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_NativeLibrary_WriteProfileResults(JNIEnv* env, jobject obj);
JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_Run__Ljava_lang_String_2(
    JNIEnv* env, jobject obj, jstring jFile);
JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_NativeLibrary_Run__Ljava_lang_String_2Ljava_lang_String_2Z(
    JNIEnv* env, jobject obj, jstring jFile, jstring jSavestate, jboolean jDeleteSavestate);
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
  Core::Stop();
  s_update_main_frame_event.Set();  // Kick the waiting event
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_NativeLibrary_WaitUntilDoneBooting(JNIEnv* env, jobject obj)
{
  Core::WaitUntilDoneBooting();
}

JNIEXPORT jboolean JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_IsRunning(JNIEnv* env,
                                                                                  jobject obj)
{
  return Core::IsRunning();
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_ChangeDisc(JNIEnv* env,
                                                                               jobject obj,
                                                                               jstring jFile);

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

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_SetMotionSensorsEnabled(
    JNIEnv* env, jobject obj, jboolean accelerometer_enabled, jboolean gyroscope_enabled)
{
  ciface::Android::SetMotionSensorsEnabled(accelerometer_enabled, gyroscope_enabled);
}

JNIEXPORT double JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_GetInputRadiusAtAngle(
    JNIEnv* env, jobject obj, int emu_pad_id, int stick, double angle)
{
  const auto casted_stick = static_cast<ButtonManager::ButtonType>(stick);
  return ButtonManager::GetInputRadiusAtAngle(emu_pad_id, casted_stick, angle);
}

JNIEXPORT jstring JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_GetVersionString(JNIEnv* env,
                                                                                        jobject obj)
{
  return ToJString(env, Common::scm_rev_str);
}

JNIEXPORT jstring JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_GetGitRevision(JNIEnv* env,
                                                                                      jobject obj)
{
  return ToJString(env, Common::scm_rev_git_str);
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

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_InitGameIni(JNIEnv* env,
                                                                                jobject obj,
                                                                                jstring jGameID)
{
  // Initialize an empty INI file
  IniFile ini;
  std::string gameid = GetJString(env, jGameID);

  __android_log_print(ANDROID_LOG_DEBUG, "InitGameIni", "Initializing base game config file");
  ini.Save(File::GetUserPath(D_GAMESETTINGS_IDX) + gameid + ".ini");
}

JNIEXPORT jstring JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_GetUserSetting(
    JNIEnv* env, jobject obj, jstring jGameID, jstring jSection, jstring jKey)
{
  IniFile ini;
  std::string gameid = GetJString(env, jGameID);
  std::string section = GetJString(env, jSection);
  std::string key = GetJString(env, jKey);

  ini = SConfig::LoadGameIni(gameid, 0);
  std::string value;

  ini.GetOrCreateSection(section)->Get(key, &value, "-1");

  return ToJString(env, value);
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_NewGameIniFile(JNIEnv* env,
                                                                                   jobject obj)
{
  s_ini = IniFile();
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_LoadGameIniFile(JNIEnv* env,
                                                                                    jobject obj,
                                                                                    jstring jGameID)
{
  std::string gameid = GetJString(env, jGameID);
  s_ini.Load(File::GetUserPath(D_GAMESETTINGS_IDX) + gameid + ".ini");
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_SaveGameIniFile(JNIEnv* env,
                                                                                    jobject obj,
                                                                                    jstring jGameID)
{
  std::string gameid = GetJString(env, jGameID);
  s_ini.Save(File::GetUserPath(D_GAMESETTINGS_IDX) + gameid + ".ini");
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_SetUserSetting(
    JNIEnv* env, jobject obj, jstring jGameID, jstring jSection, jstring jKey, jstring jValue)
{
  std::string gameid = GetJString(env, jGameID);
  std::string section = GetJString(env, jSection);
  std::string key = GetJString(env, jKey);
  std::string val = GetJString(env, jValue);

  if (val != "-1")
  {
    s_ini.GetOrCreateSection(section)->Set(key, val);
  }
  else
  {
    s_ini.GetOrCreateSection(section)->Delete(key);
  }
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_SetProfileSetting(
    JNIEnv* env, jobject obj, jstring jProfile, jstring jSection, jstring jKey, jstring jValue)
{
  IniFile ini;
  std::string profile = GetJString(env, jProfile);
  std::string section = GetJString(env, jSection);
  std::string key = GetJString(env, jKey);
  std::string val = GetJString(env, jValue);

  ini.Load(File::GetUserPath(D_CONFIG_IDX) + "Profiles/Wiimote/" + profile + ".ini");

  if (val != "-1")
  {
    ini.GetOrCreateSection(section)->Set(key, val);
  }
  else
  {
    ini.GetOrCreateSection(section)->Delete(key);
  }

  ini.Save(File::GetUserPath(D_CONFIG_IDX) + "Profiles/Wiimote/" + profile + ".ini");
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

  return ToJString(env, value);
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
                                                                              jint slot,
                                                                              jboolean wait)
{
  std::lock_guard<std::mutex> guard(s_host_identity_lock);
  State::Save(slot, wait);
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_SaveStateAs(JNIEnv* env,
                                                                                jobject obj,
                                                                                jstring path,
                                                                                jboolean wait)
{
  std::lock_guard<std::mutex> guard(s_host_identity_lock);
  State::SaveAs(GetJString(env, path), wait);
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_LoadState(JNIEnv* env,
                                                                              jobject obj,
                                                                              jint slot)
{
  std::lock_guard<std::mutex> guard(s_host_identity_lock);
  State::Load(slot);
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_LoadStateAs(JNIEnv* env,
                                                                                jobject obj,
                                                                                jstring path)
{
  std::lock_guard<std::mutex> guard(s_host_identity_lock);
  State::LoadAs(GetJString(env, path));
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_utils_DirectoryInitialization_SetSysDirectory(
    JNIEnv* env, jobject obj, jstring jPath)
{
  const std::string path = GetJString(env, jPath);
  File::SetSysDirectory(path);
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_utils_DirectoryInitialization_CreateUserDirectories(JNIEnv* env,
                                                                                   jobject obj)
{
  UICommon::CreateDirectories();
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_SetUserDirectory(
    JNIEnv* env, jobject obj, jstring jDirectory)
{
  std::lock_guard<std::mutex> guard(s_host_identity_lock);
  UICommon::SetUserDirectory(GetJString(env, jDirectory));
}

JNIEXPORT jstring JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_GetUserDirectory(JNIEnv* env,
                                                                                        jobject obj)
{
  return ToJString(env, File::GetUserPath(D_USER_IDX));
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_SetCacheDirectory(
    JNIEnv* env, jobject obj, jstring jDirectory)
{
  std::lock_guard<std::mutex> guard(s_host_identity_lock);
  File::SetUserPath(D_CACHE_IDX, GetJString(env, jDirectory) + DIR_SEP);
}

JNIEXPORT jint JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_DefaultCPUCore(JNIEnv* env,
                                                                                   jobject obj)
{
  return static_cast<jint>(PowerPC::DefaultCPUCore());
}

JNIEXPORT jint JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_GetMaxLogLevel(JNIEnv* env,
                                                                                   jobject obj)
{
  return static_cast<jint>(MAX_LOGLEVEL);
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_SetProfiling(JNIEnv* env,
                                                                                 jobject obj,
                                                                                 jboolean enable)
{
  std::lock_guard<std::mutex> guard(s_host_identity_lock);
  Core::SetState(Core::State::Paused);
  JitInterface::ClearCache();
  JitInterface::SetProfilingState(enable ? JitInterface::ProfilingState::Enabled :
                                           JitInterface::ProfilingState::Disabled);
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

JNIEXPORT jfloat JNICALL
Java_org_dolphinemu_dolphinemu_NativeLibrary_GetGameAspectRatio(JNIEnv* env, jobject obj)
{
  return g_renderer->CalculateDrawAspectRatio();
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_RefreshWiimotes(JNIEnv* env,
                                                                                    jobject obj)
{
  std::lock_guard<std::mutex> guard(s_host_identity_lock);
  WiimoteReal::Refresh();
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_ReloadWiimoteConfig(JNIEnv* env,
                                                                                        jobject obj)
{
  WiimoteReal::LoadSettings();
  Wiimote::LoadConfig();
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_ReloadConfig(JNIEnv* env,
                                                                                 jobject obj)
{
  SConfig::GetInstance().LoadSettings();
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_Initialize(JNIEnv* env,
                                                                               jobject obj)
{
  Common::RegisterMsgAlertHandler(&MsgAlert);
  Common::AndroidSetReportHandler(&ReportSend);
  DolphinAnalytics::AndroidSetGetValFunc(&GetAnalyticValue);
  UICommon::Init();
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_NativeLibrary_ReportStartToAnalytics(JNIEnv* env, jobject obj)
{
  DolphinAnalytics::Instance().ReportDolphinStart(GetAnalyticValue("DEVICE_TYPE"));
}

// Returns the scale factor for imgui rendering.
// Based on the scaledDensity of the device's display metrics.
static float GetRenderSurfaceScale(JNIEnv* env)
{
  jclass native_library_class = env->FindClass("org/dolphinemu/dolphinemu/NativeLibrary");
  jmethodID get_render_surface_scale_method =
      env->GetStaticMethodID(native_library_class, "getRenderSurfaceScale", "()F");
  return env->CallStaticFloatMethod(native_library_class, get_render_surface_scale_method);
}

static void Run(JNIEnv* env, const std::vector<std::string>& paths,
                const std::optional<std::string>& savestate_path = {},
                bool delete_savestate = false)
{
  ASSERT(!paths.empty());
  __android_log_print(ANDROID_LOG_INFO, DOLPHIN_TAG, "Running : %s", paths[0].c_str());

  std::unique_lock<std::mutex> guard(s_host_identity_lock);

  WiimoteReal::InitAdapterClass();

  // No use running the loop when booting fails
  s_have_wm_user_stop = false;
  std::unique_ptr<BootParameters> boot = BootParameters::GenerateFromFile(paths, savestate_path);
  boot->delete_savestate = delete_savestate;
  WindowSystemInfo wsi(WindowSystemType::Android, nullptr, s_surf, s_surf);
  wsi.render_surface_scale = GetRenderSurfaceScale(env);
  if (BootManager::BootCore(std::move(boot), wsi))
  {
    ButtonManager::Init(SConfig::GetInstance().GetGameID());
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
  ButtonManager::Shutdown();
  guard.unlock();

  if (s_surf)
  {
    ANativeWindow_release(s_surf);
    s_surf = nullptr;
  }
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_Run___3Ljava_lang_String_2(
    JNIEnv* env, jobject obj, jobjectArray jPaths)
{
  Run(env, JStringArrayToVector(env, jPaths));
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_NativeLibrary_Run___3Ljava_lang_String_2Ljava_lang_String_2Z(
    JNIEnv* env, jobject obj, jobjectArray jPaths, jstring jSavestate, jboolean jDeleteSavestate)
{
  Run(env, JStringArrayToVector(env, jPaths), GetJString(env, jSavestate), jDeleteSavestate);
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_ChangeDisc(JNIEnv* env,
                                                                               jobject obj,
                                                                               jstring jFile)
{
  const std::string path = GetJString(env, jFile);
  __android_log_print(ANDROID_LOG_INFO, DOLPHIN_TAG, "Change Disc: %s", path.c_str());
  Core::RunAsCPUThread([&path] { DVDInterface::ChangeDisc(path); });
}

JNIEXPORT jobject JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_GetLogTypeNames(JNIEnv* env,
                                                                                       jobject obj)
{
  std::map<std::string, std::string> map = Common::Log::LogManager::GetInstance()->GetLogTypes();

  auto map_size = static_cast<jsize>(map.size());
  jobject linked_hash_map =
      env->NewObject(IDCache::GetLinkedHashMapClass(), IDCache::GetLinkedHashMapInit(), map_size);
  for (const auto& entry : map)
  {
    env->CallObjectMethod(linked_hash_map, IDCache::GetLinkedHashMapPut(),
                          ToJString(env, entry.first), ToJString(env, entry.second));
  }
  return linked_hash_map;
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_ReloadLoggerConfig(JNIEnv* env,
                                                                                       jobject obj)
{
  Common::Log::LogManager::Init();
}

JNIEXPORT jboolean JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_InstallWAD(JNIEnv* env,
                                                                                   jobject obj,
                                                                                   jstring jFile)
{
  const std::string path = GetJString(env, jFile);
  return static_cast<jboolean>(WiiUtils::InstallWAD(path));
}

JNIEXPORT jstring JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_FormatSize(JNIEnv* env,
                                                                                  jobject obj,
                                                                                  jlong bytes,
                                                                                  jint decimals)
{
  return ToJString(env, UICommon::FormatSize(bytes, decimals));
}

#ifdef __cplusplus
}
#endif
