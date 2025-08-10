// Copyright 2003 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <EGL/egl.h>
#include <android/log.h>
#include <android/native_window_jni.h>
#include <cstdio>
#include <cstdlib>
#include <fmt/format.h>
#include <jni.h>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>

#include "Common/AndroidAnalytics.h"
#include "Common/Assert.h"
#include "Common/CPUDetect.h"
#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/Event.h"
#include "Common/FileUtil.h"
#include "Common/Flag.h"
#include "Common/IOFile.h"
#include "Common/IniFile.h"
#include "Common/Logging/LogManager.h"
#include "Common/MsgHandler.h"
#include "Common/ScopeGuard.h"
#include "Common/Version.h"
#include "Common/WindowSystemInfo.h"

#include "Core/Boot/Boot.h"
#include "Core/BootManager.h"
#include "Core/CommonTitles.h"
#include "Core/ConfigLoaders/GameConfigLoader.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/DolphinAnalytics.h"
#include "Core/HW/DVD/DVDInterface.h"
#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteReal/WiimoteReal.h"
#include "Core/Host.h"
#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/State.h"
#include "Core/System.h"

#include "DiscIO/Blob.h"
#include "DiscIO/Enums.h"
#include "DiscIO/RiivolutionParser.h"
#include "DiscIO/ScrubbedBlob.h"
#include "DiscIO/Volume.h"

#include "InputCommon/GCAdapter.h"

#include "UICommon/GameFile.h"
#include "UICommon/UICommon.h"

#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/Present.h"
#include "VideoCommon/VideoBackendBase.h"

#include "jni/AndroidCommon/AndroidCommon.h"
#include "jni/AndroidCommon/IDCache.h"
#include "jni/Host.h"

namespace
{
constexpr char DOLPHIN_TAG[] = "DolphinEmuNative";

ANativeWindow* s_surf;

Common::Event s_update_main_frame_event;

// This exists to prevent surfaces from being destroyed during the boot process,
// as that can lead to the boot process dereferencing nullptr.
std::mutex s_surface_lock;
bool s_need_nonblocking_alert_msg;

Common::Flag s_is_booting;
bool s_game_metadata_is_valid = false;
}  // Anonymous namespace

void UpdatePointer()
{
  // Update touch pointer
  JNIEnv* env = IDCache::GetEnvForThread();
  env->CallStaticVoidMethod(IDCache::GetNativeLibraryClass(), IDCache::GetUpdateTouchPointer());
}

std::vector<std::string> Host_GetPreferredLocales()
{
  // We would like to call ConfigurationCompat.getLocales here, but this function gets called
  // during dynamic initialization, and it seems like that makes us unable to obtain a JNIEnv.
  return {};
}

void Host_PPCSymbolsChanged()
{
}

void Host_PPCBreakpointsChanged()
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
    Core::QueueHostJob(&Core::Stop);
  }
}

void Host_UpdateTitle(const std::string& title)
{
  __android_log_write(ANDROID_LOG_INFO, DOLPHIN_TAG, title.c_str());
}

void Host_UpdateDiscordClientID(const std::string& client_id)
{
}

bool Host_UpdateDiscordPresenceRaw(const std::string& details, const std::string& state,
                                   const std::string& large_image_key,
                                   const std::string& large_image_text,
                                   const std::string& small_image_key,
                                   const std::string& small_image_text,
                                   const int64_t start_timestamp, const int64_t end_timestamp,
                                   const int party_size, const int party_max)
{
  return false;
}

void Host_UpdateDisasmDialog()
{
}

void Host_JitCacheInvalidation()
{
}

void Host_JitProfileDataWiped()
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

bool Host_RendererHasFullFocus()
{
  // Mouse cursor locking actually exists in Android but we don't implement (nor need) that
  return true;
}

bool Host_RendererIsFullscreen()
{
  return false;
}

bool Host_TASInputHasFocus()
{
  return false;
}

void Host_YieldToUI()
{
}

void Host_TitleChanged()
{
  s_game_metadata_is_valid = true;

  JNIEnv* env = IDCache::GetEnvForThread();
  env->CallStaticVoidMethod(IDCache::GetNativeLibraryClass(), IDCache::GetOnTitleChanged());
}

void Host_LowerWindow()
{
}

void Host_Exit()
{
}

void Host_PlaybackSeek()
{
}

void Host_Fullscreen()
{
}

std::unique_ptr<GBAHostInterface> Host_CreateGBAHost(std::weak_ptr<HW::GBA::Core> core)
{
  return nullptr;
}

static bool MsgAlert(const char* caption, const char* text, bool yes_no, Common::MsgType style)
{
  JNIEnv* env = IDCache::GetEnvForThread();

  jstring j_caption = ToJString(env, caption);
  jstring j_text = ToJString(env, text);

  // Execute the Java method.
  jboolean result = env->CallStaticBooleanMethod(
      IDCache::GetNativeLibraryClass(), IDCache::GetDisplayAlertMsg(), j_caption, j_text, yes_no,
      style == Common::MsgType::Warning, s_need_nonblocking_alert_msg);

  env->DeleteLocalRef(j_caption);
  env->DeleteLocalRef(j_text);

  return result != JNI_FALSE;
}

static void ReportSend(const std::string& endpoint, const std::string& report)
{
  JNIEnv* env = IDCache::GetEnvForThread();

  jbyteArray output_array = env->NewByteArray(report.size());
  jbyte* output = env->GetByteArrayElements(output_array, nullptr);
  memcpy(output, report.data(), report.size());
  env->ReleaseByteArrayElements(output_array, output, 0);

  jstring j_endpoint = ToJString(env, endpoint);

  env->CallStaticVoidMethod(IDCache::GetAnalyticsClass(), IDCache::GetSendAnalyticsReport(),
                            j_endpoint, output_array);

  env->DeleteLocalRef(output_array);
  env->DeleteLocalRef(j_endpoint);
}

static std::string GetAnalyticValue(const std::string& key)
{
  JNIEnv* env = IDCache::GetEnvForThread();

  jstring j_key = ToJString(env, key);
  auto j_value = reinterpret_cast<jstring>(env->CallStaticObjectMethod(
      IDCache::GetAnalyticsClass(), IDCache::GetAnalyticsValue(), j_key));
  env->DeleteLocalRef(j_key);

  std::string value = GetJString(env, j_value);
  env->DeleteLocalRef(j_value);

  return value;
}

extern "C" {

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_UnPauseEmulation(JNIEnv*,
                                                                                     jclass)
{
  HostThreadLock guard;
  Core::SetState(Core::System::GetInstance(), Core::State::Running);
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_PauseEmulation(JNIEnv*, jclass)
{
  HostThreadLock guard;
  Core::SetState(Core::System::GetInstance(), Core::State::Paused);
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_StopEmulation(JNIEnv*, jclass)
{
  HostThreadLock guard;
  Core::Stop(Core::System::GetInstance());

  // Kick the waiting event
  s_update_main_frame_event.Set();
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_SetIsBooting(JNIEnv*, jclass)
{
  s_is_booting.Set();
}

JNIEXPORT jboolean JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_IsRunning(JNIEnv*, jclass)
{
  return static_cast<jboolean>(Core::IsRunning(Core::System::GetInstance()));
}

JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_NativeLibrary_IsRunningAndUnpaused(JNIEnv*, jclass)
{
  return static_cast<jboolean>(Core::GetState(Core::System::GetInstance()) == Core::State::Running);
}

JNIEXPORT jboolean JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_IsUninitialized(JNIEnv*,
                                                                                        jclass)
{
  return static_cast<jboolean>(Core::IsUninitialized(Core::System::GetInstance()) &&
                               !s_is_booting.IsSet());
}

JNIEXPORT jstring JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_GetVersionString(JNIEnv* env,
                                                                                        jclass)
{
  return ToJString(env, Common::GetScmRevStr());
}

JNIEXPORT jstring JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_GetGitRevision(JNIEnv* env,
                                                                                      jclass)
{
  return ToJString(env, Common::GetScmRevGitStr());
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_SaveScreenShot(JNIEnv*, jclass)
{
  HostThreadLock guard;
  Core::SaveScreenShot();
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_eglBindAPI(JNIEnv*, jclass,
                                                                               jint api)
{
  eglBindAPI(api);
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_SaveState(JNIEnv*, jclass,
                                                                              jint slot,
                                                                              jboolean wait)
{
  HostThreadLock guard;
  State::Save(Core::System::GetInstance(), slot, wait);
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_SaveStateAs(JNIEnv* env, jclass,
                                                                                jstring path,
                                                                                jboolean wait)
{
  HostThreadLock guard;
  State::SaveAs(Core::System::GetInstance(), GetJString(env, path), wait);
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_LoadState(JNIEnv*, jclass,
                                                                              jint slot)
{
  HostThreadLock guard;
  State::Load(Core::System::GetInstance(), slot);
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_LoadStateAs(JNIEnv* env, jclass,
                                                                                jstring path)
{
  HostThreadLock guard;
  State::LoadAs(Core::System::GetInstance(), GetJString(env, path));
}

JNIEXPORT jlong JNICALL
Java_org_dolphinemu_dolphinemu_NativeLibrary_GetUnixTimeOfStateSlot(JNIEnv*, jclass, jint slot)
{
  return static_cast<jlong>(State::GetUnixTimeOfSlot(slot));
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_utils_DirectoryInitialization_SetSysDirectory(
    JNIEnv* env, jclass, jstring jPath)
{
  const std::string path = GetJString(env, jPath);
  File::SetSysDirectory(path);
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_utils_DirectoryInitialization_SetGpuDriverDirectories(
    JNIEnv* env, jclass, jstring jPath, jstring jLibPath)
{
  const std::string path = GetJString(env, jPath);
  const std::string lib_path = GetJString(env, jLibPath);
  File::SetGpuDriverDirectories(path, lib_path);
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_SetUserDirectory(
    JNIEnv* env, jclass, jstring jDirectory)
{
  HostThreadLock guard;
  UICommon::SetUserDirectory(GetJString(env, jDirectory));
}

JNIEXPORT jstring JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_GetUserDirectory(JNIEnv* env,
                                                                                        jclass)
{
  return ToJString(env, File::GetUserPath(D_USER_IDX));
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_SetCacheDirectory(
    JNIEnv* env, jclass, jstring jDirectory)
{
  HostThreadLock guard;
  File::SetUserPath(D_CACHE_IDX, GetJString(env, jDirectory));
}

JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_NativeLibrary_GetCacheDirectory(JNIEnv* env, jclass)
{
  return ToJString(env, File::GetUserPath(D_CACHE_IDX));
}

JNIEXPORT jint JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_DefaultCPUCore(JNIEnv*, jclass)
{
  return static_cast<jint>(PowerPC::DefaultCPUCore());
}

JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_NativeLibrary_GetDefaultGraphicsBackendConfigName(JNIEnv* env,
                                                                                 jclass)
{
  return ToJString(env, VideoBackendBase::GetDefaultBackendConfigName());
}

JNIEXPORT jint JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_GetMaxLogLevel(JNIEnv*, jclass)
{
  return static_cast<jint>(Common::Log::MAX_LOGLEVEL);
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_WipeJitBlockProfilingData(
    JNIEnv* env, jclass native_library_class)
{
  HostThreadLock guard;
  auto& system = Core::System::GetInstance();
  auto& jit_interface = system.GetJitInterface();
  const Core::CPUThreadGuard cpu_guard(system);
  if (jit_interface.GetCore() == nullptr)
  {
    env->CallStaticVoidMethod(native_library_class, IDCache::GetDisplayToastMsg(),
                              ToJString(env, Common::GetStringT("JIT is not active")), JNI_FALSE);
    return;
  }
  jit_interface.WipeBlockProfilingData(cpu_guard);
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_WriteJitBlockLogDump(
    JNIEnv* env, jclass native_library_class)
{
  HostThreadLock guard;
  auto& system = Core::System::GetInstance();
  auto& jit_interface = system.GetJitInterface();
  const Core::CPUThreadGuard cpu_guard(system);
  if (jit_interface.GetCore() == nullptr)
  {
    env->CallStaticVoidMethod(native_library_class, IDCache::GetDisplayToastMsg(),
                              ToJString(env, Common::GetStringT("JIT is not active")), JNI_FALSE);
    return;
  }
  const std::string filename = fmt::format("{}{}.txt", File::GetUserPath(D_DUMPDEBUG_JITBLOCKS_IDX),
                                           SConfig::GetInstance().GetGameID());
  File::IOFile f(filename, "w");
  if (!f)
  {
    env->CallStaticVoidMethod(
        native_library_class, IDCache::GetDisplayToastMsg(),
        ToJString(env, Common::FmtFormatT("Failed to open \"{0}\" for writing.", filename)),
        JNI_FALSE);
    return;
  }
  jit_interface.JitBlockLogDump(cpu_guard, f.GetHandle());
  env->CallStaticVoidMethod(native_library_class, IDCache::GetDisplayToastMsg(),
                            ToJString(env, Common::FmtFormatT("Wrote to \"{0}\".", filename)),
                            JNI_FALSE);
}

// Surface Handling
JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_SurfaceChanged(JNIEnv* env,
                                                                                   jclass,
                                                                                   jobject surf)
{
  std::lock_guard<std::mutex> guard(s_surface_lock);

  s_surf = ANativeWindow_fromSurface(env, surf);
  if (s_surf == nullptr)
    __android_log_print(ANDROID_LOG_ERROR, DOLPHIN_TAG, "Error: Surface is null.");

  if (g_presenter)
    g_presenter->ChangeSurface(s_surf);
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_SurfaceDestroyed(JNIEnv*,
                                                                                     jclass)
{
  {
    // If emulation continues running without a valid surface, we will probably crash,
    // so pause emulation until we get a valid surface again. EmulationFragment handles resuming.

    HostThreadLock host_identity_guard;

    while (s_is_booting.IsSet())
    {
      // Need to wait for boot to finish before we can pause
      host_identity_guard.Unlock();
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      host_identity_guard.Lock();
    }

    if (Core::GetState(Core::System::GetInstance()) == Core::State::Running)
      Core::SetState(Core::System::GetInstance(), Core::State::Paused);
  }

  std::lock_guard surface_guard(s_surface_lock);

  if (g_presenter)
    g_presenter->ChangeSurface(nullptr);

  if (s_surf)
  {
    ANativeWindow_release(s_surf);
    s_surf = nullptr;
  }
}

JNIEXPORT jboolean JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_HasSurface(JNIEnv*, jclass)
{
  std::lock_guard guard(s_surface_lock);

  return s_surf ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jfloat JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_GetGameAspectRatio(JNIEnv*,
                                                                                         jclass)
{
  return g_presenter->CalculateDrawAspectRatio();
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_RefreshWiimotes(JNIEnv*, jclass)
{
  HostThreadLock guard;
  WiimoteReal::Refresh();
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_ReloadConfig(JNIEnv*, jclass)
{
  HostThreadLock guard;
  SConfig::GetInstance().LoadSettings();
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_NativeLibrary_UpdateGCAdapterScanThread(JNIEnv*, jclass)
{
  HostThreadLock guard;
  if (GCAdapter::UseAdapter())
  {
    GCAdapter::StartScanThread();
  }
  else
  {
    GCAdapter::StopScanThread();
  }
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_Initialize(JNIEnv*, jclass)
{
  HostThreadLock guard;

  UICommon::CreateDirectories();
  Common::RegisterMsgAlertHandler(&MsgAlert);
  Common::AndroidSetReportHandler(&ReportSend);
  DolphinAnalytics::AndroidSetGetValFunc(&GetAnalyticValue);

  WiimoteReal::InitAdapterClass();
  UICommon::Init();
  UICommon::InitControllers(WindowSystemInfo(WindowSystemType::Android, nullptr, nullptr, nullptr));
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_ReportStartToAnalytics(JNIEnv*,
                                                                                           jclass)
{
  DolphinAnalytics::Instance().ReportDolphinStart(GetAnalyticValue("DEVICE_TYPE"));
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_GenerateNewStatisticsId(JNIEnv*,
                                                                                            jclass)
{
  DolphinAnalytics::Instance().GenerateNewIdentity();
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

static void Run(JNIEnv* env, std::unique_ptr<BootParameters>&& boot, bool riivolution)
{
  HostThreadLock host_identity_guard;

  if (riivolution && std::holds_alternative<BootParameters::Disc>(boot->parameters))
  {
    const std::string& riivolution_dir = File::GetUserPath(D_RIIVOLUTION_IDX);
    const DiscIO::Volume& volume = *std::get<BootParameters::Disc>(boot->parameters).volume;

    AddRiivolutionPatches(boot.get(), DiscIO::Riivolution::GenerateRiivolutionPatchesFromConfig(
                                          riivolution_dir, volume.GetGameID(), volume.GetRevision(),
                                          volume.GetDiscNumber()));
  }

  WindowSystemInfo wsi(WindowSystemType::Android, nullptr, s_surf, s_surf);
  wsi.render_surface_scale = GetRenderSurfaceScale(env);

  s_need_nonblocking_alert_msg = true;
  std::unique_lock<std::mutex> surface_guard(s_surface_lock);

  if (BootManager::BootCore(Core::System::GetInstance(), std::move(boot), wsi))
  {
    static constexpr int WAIT_STEP = 25;
    while (Core::GetState(Core::System::GetInstance()) == Core::State::Starting)
      std::this_thread::sleep_for(std::chrono::milliseconds(WAIT_STEP));
  }

  s_is_booting.Clear();
  s_need_nonblocking_alert_msg = false;
  surface_guard.unlock();

  while (Core::IsRunning(Core::System::GetInstance()))
  {
    host_identity_guard.Unlock();
    s_update_main_frame_event.Wait();
    host_identity_guard.Lock();
    Core::HostDispatchJobs(Core::System::GetInstance());
  }

  s_game_metadata_is_valid = false;
  Core::Shutdown(Core::System::GetInstance());
  host_identity_guard.Unlock();

  env->CallStaticVoidMethod(IDCache::GetNativeLibraryClass(),
                            IDCache::GetFinishEmulationActivity());
}

static void Run(JNIEnv* env, const std::vector<std::string>& paths, bool riivolution,
                BootSessionData boot_session_data = BootSessionData())
{
  ASSERT(!paths.empty());
  __android_log_print(ANDROID_LOG_INFO, DOLPHIN_TAG, "Running : %s", paths[0].c_str());

  Run(env, BootParameters::GenerateFromFile(paths, std::move(boot_session_data)), riivolution);
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_Run___3Ljava_lang_String_2Z(
    JNIEnv* env, jclass, jobjectArray jPaths, jboolean jRiivolution)
{
  Run(env, JStringArrayToVector(env, jPaths), jRiivolution);
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_NativeLibrary_Run___3Ljava_lang_String_2ZLjava_lang_String_2Z(
    JNIEnv* env, jclass, jobjectArray jPaths, jboolean jRiivolution, jstring jSavestate,
    jboolean jDeleteSavestate)
{
  DeleteSavestateAfterBoot delete_state =
      jDeleteSavestate ? DeleteSavestateAfterBoot::Yes : DeleteSavestateAfterBoot::No;
  Run(env, JStringArrayToVector(env, jPaths), jRiivolution,
      BootSessionData(GetJString(env, jSavestate), delete_state));
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_RunSystemMenu(JNIEnv* env,
                                                                                  jclass)
{
  Run(env, std::make_unique<BootParameters>(BootParameters::NANDTitle{Titles::SYSTEM_MENU}), false);
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_ChangeDisc(JNIEnv* env, jclass,
                                                                               jstring jFile)
{
  HostThreadLock guard;
  const std::string path = GetJString(env, jFile);
  __android_log_print(ANDROID_LOG_INFO, DOLPHIN_TAG, "Change Disc: %s", path.c_str());
  auto& system = Core::System::GetInstance();
  system.GetDVDInterface().ChangeDisc(Core::CPUThreadGuard{system}, path);
}

JNIEXPORT jobject JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_GetLogTypeNames(JNIEnv* env,
                                                                                       jclass)
{
  std::map<std::string, std::string> map = Common::Log::LogManager::GetInstance()->GetLogTypes();

  auto map_size = static_cast<jsize>(map.size());
  jobject linked_hash_map =
      env->NewObject(IDCache::GetLinkedHashMapClass(), IDCache::GetLinkedHashMapInit(), map_size);
  for (const auto& entry : map)
  {
    jstring key = ToJString(env, entry.first);
    jstring value = ToJString(env, entry.second);

    jobject result =
        env->CallObjectMethod(linked_hash_map, IDCache::GetLinkedHashMapPut(), key, value);

    env->DeleteLocalRef(key);
    env->DeleteLocalRef(value);
    env->DeleteLocalRef(result);
  }
  return linked_hash_map;
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_ReloadLoggerConfig(JNIEnv*,
                                                                                       jclass)
{
  Common::Log::LogManager::Init();
}

JNIEXPORT jboolean JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_ConvertDiscImage(
    JNIEnv* env, jclass, jstring jInPath, jstring jOutPath, jint jPlatform, jint jFormat,
    jint jBlockSize, jint jCompression, jint jCompressionLevel, jboolean jScrub, jobject jCallback)
{
  const std::string in_path = GetJString(env, jInPath);
  const std::string out_path = GetJString(env, jOutPath);
  const DiscIO::Platform platform = static_cast<DiscIO::Platform>(jPlatform);
  const DiscIO::BlobType format = static_cast<DiscIO::BlobType>(jFormat);
  const DiscIO::WIARVZCompressionType compression =
      static_cast<DiscIO::WIARVZCompressionType>(jCompression);
  const bool scrub = static_cast<bool>(jScrub);

  std::unique_ptr<DiscIO::BlobReader> blob_reader;
  if (scrub)
    blob_reader = DiscIO::ScrubbedBlob::Create(in_path);
  else
    blob_reader = DiscIO::CreateBlobReader(in_path);

  if (!blob_reader)
    return JNI_FALSE;

  jobject jCallbackGlobal = env->NewGlobalRef(jCallback);
  Common::ScopeGuard scope_guard([jCallbackGlobal, env] { env->DeleteGlobalRef(jCallbackGlobal); });

  const auto callback = [&jCallbackGlobal](const std::string& text, float completion) {
    JNIEnv* env = IDCache::GetEnvForThread();

    jstring j_text = ToJString(env, text);
    jboolean result = env->CallBooleanMethod(jCallbackGlobal, IDCache::GetCompressCallbackRun(),
                                             j_text, completion);
    env->DeleteLocalRef(j_text);

    return static_cast<bool>(result);
  };

  bool success = false;

  switch (format)
  {
  case DiscIO::BlobType::PLAIN:
    success = DiscIO::ConvertToPlain(blob_reader.get(), in_path, out_path, callback);
    break;

  case DiscIO::BlobType::GCZ:
    success =
        DiscIO::ConvertToGCZ(blob_reader.get(), in_path, out_path,
                             platform == DiscIO::Platform::WiiDisc ? 1 : 0, jBlockSize, callback);
    break;

  case DiscIO::BlobType::WIA:
  case DiscIO::BlobType::RVZ:
    success = DiscIO::ConvertToWIAOrRVZ(blob_reader.get(), in_path, out_path,
                                        format == DiscIO::BlobType::RVZ, compression,
                                        jCompressionLevel, jBlockSize, callback);
    break;

  default:
    ASSERT(false);
    break;
  }

  return static_cast<jboolean>(success);
}

JNIEXPORT jstring JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_FormatSize(JNIEnv* env,
                                                                                  jclass,
                                                                                  jlong bytes,
                                                                                  jint decimals)
{
  return ToJString(env, UICommon::FormatSize(bytes, decimals));
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_NativeLibrary_SetObscuredPixelsLeft(JNIEnv*, jclass, jint width)
{
  OSD::SetObscuredPixelsLeft(width);
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_NativeLibrary_SetObscuredPixelsTop(JNIEnv*, jclass, jint height)
{
  OSD::SetObscuredPixelsTop(height);
}

JNIEXPORT jboolean JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_IsGameMetadataValid(JNIEnv*,
                                                                                            jclass)
{
  return s_game_metadata_is_valid;
}

JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_NativeLibrary_IsEmulatingWiiUnchecked(JNIEnv*, jclass)
{
  return Core::System::GetInstance().IsWii();
}

JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_NativeLibrary_GetCurrentGameIDUnchecked(JNIEnv* env, jclass)
{
  return ToJString(env, SConfig::GetInstance().GetGameID());
}

JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_NativeLibrary_GetCurrentTitleDescriptionUnchecked(JNIEnv* env,
                                                                                 jclass)
{
  // Prefer showing just the name. If no name is available, show just the game ID.
  std::string description = SConfig::GetInstance().GetTitleName();
  if (description.empty())
    description = SConfig::GetInstance().GetTitleDescription();

  return ToJString(env, description);
}
}
