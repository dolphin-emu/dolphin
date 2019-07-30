// Copyright 2003 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

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

#include "Common/CPUDetect.h"
#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/Event.h"
#include "Common/FileUtil.h"
#include "Common/Logging/LogManager.h"
#include "Common/MsgHandler.h"
#include "Common/Version.h"
#include "Common/WindowSystemInfo.h"

#include "Common/Config/Config.h"
#include "Common/Config/Enums.h"
#include "Core/ARDecrypt.h"
#include "Core/Boot/Boot.h"
#include "Core/BootManager.h"
#include "Core/Config/GraphicsSettings.h"
#include "Core/Config/MainSettings.h"
#include "Core/Config/SYSCONFSettings.h"
#include "Core/ConfigLoaders/BaseConfigLoader.h"
#include "Core/ConfigLoaders/GameConfigLoader.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/DVD/DVDInterface.h"
#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"
#include "Core/Host.h"
#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/Profiler.h"
#include "Core/State.h"
#include "Core/TitleDatabase.h"
#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"
#include "VideoCommon/VideoConfig.h"

#include "DiscIO/Enums.h"
#include "DiscIO/Volume.h"

#include "UICommon/UICommon.h"

#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/VideoBackendBase.h"

#include "AudioCommon/AudioCommon.h"
#include "jni/AndroidCommon/AndroidCommon.h"
#include "jni/AndroidCommon/IDCache.h"
#include "jni/ButtonManager.h"

static constexpr char DOLPHIN_TAG[] = "DolphinEmuNative";
static ANativeWindow* s_surf;
static IniFile s_game_ini;
static std::unique_ptr<Core::TitleDatabase> s_title_database;
static float s_scaled_density = 1.0f;

// The Core only supports using a single Host thread.
// If multiple threads want to call host functions then they need to queue
// sequentially for access.
static std::mutex s_host_identity_lock;
static Common::Event s_update_main_frame_event;
static bool s_have_wm_user_stop = false;

static bool MsgAlert(const char* caption, const char* text, bool yes_no, Common::MsgType /*style*/)
{
  // Associate the current Thread with the Java VM.
  JNIEnv* env = IDCache::GetEnvForThread();

  // Execute the Java method.
  jboolean result = env->CallStaticBooleanMethod(
      IDCache::sNativeLibrary.Clazz, IDCache::sNativeLibrary.DisplayAlertMsg,
      ToJString(env, caption), ToJString(env, text), yes_no ? JNI_TRUE : JNI_FALSE);

  return result != JNI_FALSE;
}

const Core::TitleDatabase* Host_GetTitleDatabase()
{
  return s_title_database.get();
}

void Host_NotifyMapLoaded()
{
}

void Host_RefreshDSPDebuggerWindow()
{
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
  auto UpdateWindowSize = [&width, &height] {
    JNIEnv* env = IDCache::GetEnvForThread();
    env->CallStaticVoidMethod(IDCache::sNativeLibrary.Clazz,
                              IDCache::sNativeLibrary.UpdateWindowSize, width, height);
  };

  if (SConfig::GetInstance().bCPUThread)
  {
    UpdateWindowSize();
  }
  else
  {
    std::thread(UpdateWindowSize).join();
  }
}

bool Host_UINeedsControllerState()
{
  return true;
}

bool Host_RendererHasFocus()
{
  return true;
}

void Host_YieldToUI()
{
}

void Host_UpdateProgressDialog(const char* caption, int position, int total)
{
}

void Host_TitleChanged()
{
}

void BootGame(const std::vector<std::string>& paths, const std::optional<std::string>& savestate,
              bool delete_savestate)
{
  std::unique_lock<std::mutex> guard(s_host_identity_lock);

  // No use running the loop when booting fails
  s_have_wm_user_stop = false;
  std::unique_ptr<BootParameters> boot = BootParameters::GenerateFromFile(paths, savestate);
  boot->delete_savestate = delete_savestate;
  WindowSystemInfo wsi(WindowSystemType::Android, nullptr, s_surf);
  wsi.render_surface_scale = s_scaled_density;
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
  SConfig::GetInstance().SaveSettings();
  ButtonManager::Shutdown();
  guard.unlock();

  if (s_surf)
  {
    ANativeWindow_release(s_surf);
    s_surf = nullptr;
  }
}

#ifdef __cplusplus
extern "C" {
#endif

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
  // Kick the waiting event
  s_update_main_frame_event.Set();
}

JNIEXPORT jboolean JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_IsRunning(JNIEnv* env,
                                                                                  jobject obj)
{
  return Core::IsRunning();
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

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_SaveScreenShot(JNIEnv* env,
                                                                                   jobject obj)
{
  std::lock_guard<std::mutex> guard(s_host_identity_lock);
  Core::SaveScreenShot();
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_LoadGameIniFile(JNIEnv* env,
                                                                                    jobject obj,
                                                                                    jstring jGameID)
{
  std::string gameid = GetJString(env, jGameID);
  s_game_ini.Load(File::GetUserPath(D_GAMESETTINGS_IDX) + gameid + ".ini");
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_SaveGameIniFile(JNIEnv* env,
                                                                                    jobject obj,
                                                                                    jstring jGameID)
{
  std::string gameid = GetJString(env, jGameID);
  s_game_ini.Save(File::GetUserPath(D_GAMESETTINGS_IDX) + gameid + ".ini");
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_SetUserSetting(
    JNIEnv* env, jobject obj, jstring jGameID, jstring jSection, jstring jKey, jstring jValue)
{
  std::string gameid = GetJString(env, jGameID);
  std::string section = GetJString(env, jSection);
  std::string key = GetJString(env, jKey);
  std::string val = GetJString(env, jValue);

  if (val != "-1" && !val.empty())
  {
    s_game_ini.GetOrCreateSection(section)->Set(key, val);
  }
  else
  {
    s_game_ini.GetOrCreateSection(section)->Delete(key);
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

  if (val != "-1" && !val.empty())
  {
    ini.GetOrCreateSection(section)->Set(key, val);
  }
  else
  {
    ini.GetOrCreateSection(section)->Delete(key);
  }

  ini.Save(File::GetUserPath(D_CONFIG_IDX) + "Profiles/Wiimote/" + profile + ".ini");
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

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_SetSysDirectory(JNIEnv* env,
                                                                                    jobject obj,
                                                                                    jstring jPath)
{
  const std::string path = GetJString(env, jPath);
  File::SetSysDirectory(path);
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_NativeLibrary_CreateUserDirectories(JNIEnv* env, jobject obj)
{
  UICommon::CreateDirectories();
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_SetUserDirectory(
    JNIEnv* env, jobject obj, jstring jDirectory)
{
  std::lock_guard<std::mutex> guard(s_host_identity_lock);
  UICommon::SetUserDirectory(GetJString(env, jDirectory));

  UICommon::Init();
}

JNIEXPORT jstring JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_GetUserDirectory(JNIEnv* env,
                                                                                        jobject obj)
{
  return ToJString(env, File::GetUserPath(D_USER_IDX));
}

JNIEXPORT jint JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_DefaultCPUCore(JNIEnv* env,
                                                                                   jobject obj)
{
  return static_cast<jint>(PowerPC::DefaultCPUCore());
}

JNIEXPORT jobjectArray JNICALL
Java_org_dolphinemu_dolphinemu_NativeLibrary_GetAudioBackendList(JNIEnv* env, jobject obj)
{
  const std::vector<std::string> backends = AudioCommon::GetSoundBackends();
  jsize size = (jsize)backends.size();
  jobjectArray list = (jobjectArray)env->NewObjectArray(size, env->FindClass("java/lang/String"),
                                                        ToJString(env, ""));

  for (int i = 0; i < size; ++i)
  {
    env->SetObjectArrayElement(list, i, ToJString(env, backends[i]));
  }
  return list;
}

JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_NativeLibrary_DefaultAudioBackend(JNIEnv* env, jobject obj)
{
  return ToJString(env, AudioCommon::GetDefaultSoundBackend());
}

JNIEXPORT jintArray JNICALL
Java_org_dolphinemu_dolphinemu_NativeLibrary_getRunningSettings(JNIEnv* env, jobject obj)
{
  int i = 0;
  int settings[14];

  // gfx
  settings[i++] = Config::Get(Config::GFX_SHOW_FPS);
  settings[i++] = Config::Get(Config::GFX_HACK_EFB_ACCESS_ENABLE) == false;
  settings[i++] = Config::Get(Config::GFX_HACK_SKIP_EFB_COPY_TO_RAM);
  settings[i++] = Config::Get(Config::GFX_HACK_EFB_EMULATE_FORMAT_CHANGES) == false;
  settings[i++] = Config::Get(Config::GFX_ENHANCE_ARBITRARY_MIPMAP_DETECTION);
  settings[i++] = Config::Get(Config::GFX_HACK_IMMEDIATE_XFB);
  settings[i++] = Config::Get(Config::GFX_DISPLAY_SCALE) * 100;

  // core
  settings[i++] = Config::Get(Config::MAIN_SYNC_ON_SKIP_IDLE);
  settings[i++] = Config::Get(Config::MAIN_OVERCLOCK_ENABLE);
  settings[i++] = Config::Get(Config::MAIN_OVERCLOCK) * 100;
  settings[i++] = Config::Get(Config::MAIN_JIT_FOLLOW_BRANCH);

  // wii
  if (SConfig::GetInstance().bWii)
  {
    settings[i++] = Config::Get(Config::MAIN_IR_PITCH);
    settings[i++] = Config::Get(Config::MAIN_IR_YAW);
    settings[i++] = Config::Get(Config::MAIN_IR_VERTICAL_OFFSET);
  }

  jintArray array = env->NewIntArray(i);
  env->SetIntArrayRegion(array, 0, i, settings);
  return array;
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_setRunningSettings(
    JNIEnv* env, jobject obj, jintArray array)
{
  int i = 0;
  jint* settings = env->GetIntArrayElements(array, nullptr);

  // gfx settings need refresh to take effect
  // and will save changes to ini file
  Config::Set(Config::LayerType::LocalGame, Config::GFX_SHOW_FPS, settings[i++]);
  Config::Set(Config::LayerType::LocalGame, Config::GFX_HACK_EFB_ACCESS_ENABLE, settings[i++] == 0);
  Config::Set(Config::LayerType::LocalGame, Config::GFX_HACK_SKIP_EFB_COPY_TO_RAM, settings[i++]);
  Config::Set(Config::LayerType::LocalGame, Config::GFX_HACK_EFB_EMULATE_FORMAT_CHANGES,
              settings[i++] == 0);
  Config::Set(Config::LayerType::LocalGame, Config::GFX_ENHANCE_ARBITRARY_MIPMAP_DETECTION,
              settings[i++]);
  Config::Set(Config::LayerType::LocalGame, Config::GFX_HACK_IMMEDIATE_XFB, settings[i++]);
  Config::Set(Config::LayerType::LocalGame, Config::GFX_DISPLAY_SCALE,
              settings[i++] / 100.0f + FLT_EPSILON);

  g_Config.Refresh();
  UpdateActiveConfig();

  // Main.Core
  Config::Set(Config::LayerType::LocalGame, Config::MAIN_SYNC_ON_SKIP_IDLE, settings[i++]);
  Config::Set(Config::LayerType::LocalGame, Config::MAIN_OVERCLOCK_ENABLE, settings[i++]);
  Config::Set(Config::LayerType::LocalGame, Config::MAIN_OVERCLOCK,
              settings[i++] / 100.0f + FLT_EPSILON);
  Config::Set(Config::LayerType::LocalGame, Config::MAIN_JIT_FOLLOW_BRANCH, settings[i++]);

  SConfig::GetInstance().bSyncGPUOnSkipIdleHack = Config::Get(Config::MAIN_SYNC_ON_SKIP_IDLE);
  SConfig::GetInstance().m_OCEnable = Config::Get(Config::MAIN_OVERCLOCK_ENABLE);
  SConfig::GetInstance().m_OCFactor = Config::Get(Config::MAIN_OVERCLOCK);
  SConfig::GetInstance().bJITFollowBranch = Config::Get(Config::MAIN_JIT_FOLLOW_BRANCH);

  // wii
  if (SConfig::GetInstance().bWii)
  {
    Config::Set(Config::LayerType::LocalGame, Config::MAIN_IR_PITCH, settings[i++]);
    Config::Set(Config::LayerType::LocalGame, Config::MAIN_IR_YAW, settings[i++]);
    Config::Set(Config::LayerType::LocalGame, Config::MAIN_IR_VERTICAL_OFFSET, settings[i++]);

    ControllerEmu::ControlGroup* cg = Wiimote::GetWiimoteGroup(0, WiimoteEmu::WiimoteGroup::Point);
    for (auto& s : cg->numeric_settings)
    {
      if (strcmp(s->GetIniName(), "Total Pitch") == 0)
      {
        static_cast<ControllerEmu::NumericSetting<double>*>(s.get())->SetValue(
            Config::Get(Config::MAIN_IR_PITCH));
      }
      else if (strcmp(s->GetIniName(), "Total Yaw") == 0)
      {
        static_cast<ControllerEmu::NumericSetting<double>*>(s.get())->SetValue(
            Config::Get(Config::MAIN_IR_YAW));
      }
      else if (strcmp(s->GetIniName(), "Vertical Offset") == 0)
      {
        static_cast<ControllerEmu::NumericSetting<double>*>(s.get())->SetValue(
            Config::Get(Config::MAIN_IR_VERTICAL_OFFSET));
      }
    }
  }

  env->ReleaseIntArrayElements(array, settings, 0);
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_NativeLibrary_setSystemLanguage__Ljava_lang_String_2(JNIEnv* env,
                                                                                    jobject obj,
                                                                                    jstring jFile)
{
  if (SConfig::GetInstance().m_use_builtin_title_database)
  {
    std::string language = GetJString(env, jFile);
    s_title_database = std::make_unique<Core::TitleDatabase>(language);
  }
  else
  {
    s_title_database.reset();
  }
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

JNIEXPORT jstring JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_DecryptARCode(JNIEnv* env,
                                                                                     jobject obj,
                                                                                     jstring jCodes)
{
  std::vector<ActionReplay::AREntry> entries;
  std::vector<std::string> encrypted_lines;
  std::string codes = GetJString(env, jCodes);
  std::string result;
  size_t offset = 0;
  size_t position = 0;
  while ((position = codes.find('\n', offset)) != std::string::npos)
  {
    encrypted_lines.push_back(codes.substr(offset, position - offset));
    offset = position + 1;
  }

  if (offset + 1 < codes.size())
  {
    encrypted_lines.push_back(codes.substr(offset));
  }

  ActionReplay::DecryptARCode(encrypted_lines, &entries);
  for (const auto& op : entries)
  {
    result += StringFromFormat("%08X %08X\n", op.cmd_addr, op.value);
  }
  return ToJString(env, result);
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_SetScaledDensity(
    JNIEnv* env, jobject obj, jfloat scaledDensity)
{
  s_scaled_density = scaledDensity;
}

JNIEXPORT jfloat JNICALL
Java_org_dolphinemu_dolphinemu_NativeLibrary_GetGameAspectRatio(JNIEnv* env, jobject obj)
{
  return g_renderer->CalculateDrawAspectRatio();
}

JNIEXPORT jfloat JNICALL
Java_org_dolphinemu_dolphinemu_NativeLibrary_GetGameDisplayScale(JNIEnv* env, jobject obj)
{
  return Config::Get(Config::GFX_DISPLAY_SCALE);
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
  Wiimote::LoadConfig();
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_Run(JNIEnv* env, jclass obj,
                                                                        jobjectArray jPaths,
                                                                        jstring jSavestate)
{
  const std::vector<std::string> paths = JStringArrayToVector(env, jPaths);
  const std::string state_path = GetJString(env, jSavestate);
  std::optional<std::string> savestate;
  bool delete_savestate = false;

  ASSERT(!paths.empty());

  if (!state_path.empty())
  {
    savestate = state_path;
    if (state_path.find("temp.sav") != std::string::npos)
    {
      delete_savestate = true;
    }
  }

  // alert handler
  RegisterMsgAlertHandler(&MsgAlert);

  // reload settings
  Config::SetBaseOrCurrent(Config::GFX_ENHANCE_POST_SHADER, "");
  SConfig::GetInstance().LoadSettings();
  VideoBackendBase::ActivateBackend(SConfig::GetInstance().m_strVideoBackend);

  const std::string systemBack = Config::Get(Config::MAIN_SYSTEM_BACK_BIND);
  env->CallStaticVoidMethod(IDCache::sNativeLibrary.Clazz, IDCache::sNativeLibrary.BindSystemBack,
                            ToJString(env, systemBack));

  BootGame(paths, savestate, delete_savestate);
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_ChangeDisc(JNIEnv* env,
                                                                               jobject obj,
                                                                               jstring jFile)
{
  const std::string path = GetJString(env, jFile);
  Core::RunAsCPUThread([&path] { DVDInterface::ChangeDisc(path); });
}

#ifdef __cplusplus
}
#endif
