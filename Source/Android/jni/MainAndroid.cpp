// Copyright 2003 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <array>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <numeric>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <EGL/egl.h>
#include <android/input.h>
#include <android/log.h>
#include <android/native_window_jni.h>
#include <fmt/format.h>
#include <jni.h>

#include "Common/Assert.h"
#include "Common/CPUDetect.h"
#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/Config/Config.h"
#include "Common/Contains.h"
#include "Common/Event.h"
#include "Common/FileUtil.h"
#include "Common/Flag.h"
#include "Common/IOFile.h"
#include "Common/IniFile.h"
#include "Common/Logging/Log.h"
#include "Common/Logging/LogManager.h"
#include "Common/MsgHandler.h"
#include "Common/ScopeGuard.h"
#include "Common/Version.h"
#include "Common/WindowSystemInfo.h"

#include "Core/AchievementManager.h"
#include "Core/Boot/Boot.h"
#include "Core/BootManager.h"
#include "Core/CommonTitles.h"
#include "Core/Config/MainSettings.h"
#include "Core/Config/UISettings.h"
#include "Core/ConfigLoaders/GameConfigLoader.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/DolphinAnalytics.h"
#include "Core/HW/DVD/DVDInterface.h"
#include "Core/HW/GCPad.h"
#include "Core/HW/SI/SI_Device.h"
#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteReal/WiimoteReal.h"
#include "Core/Host.h"
#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/State.h"
#include "Core/System.h"
#include "jni/NetPlay/NetPlayUICallbacks.h"

#include "DiscIO/Blob.h"
#include "DiscIO/Enums.h"
#include "DiscIO/RiivolutionParser.h"
#include "DiscIO/ScrubbedBlob.h"
#include "DiscIO/Volume.h"

#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ControllerInterface/Android/Android.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/InputConfig.h"

#include "UICommon/GameFile.h"
#include "UICommon/UICommon.h"

#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/Present.h"
#include "VideoCommon/VideoBackendBase.h"

#include "jni/AndroidCommon/AndroidCommon.h"
#include "jni/AndroidCommon/IDCache.h"
#include "jni/AutomaticControllers.h"

namespace
{
constexpr char DOLPHIN_TAG[] = "DolphinEmuNative";

ANativeWindow* s_surf;

Common::Event s_update_main_frame_event;

// This exists to prevent surfaces from being destroyed during the boot process,
// as that can lead to the boot process dereferencing nullptr.
std::mutex s_surface_lock;
std::condition_variable s_surface_cv;

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

static void AutoSetupControllers();

namespace
{
constexpr int NUM_GC_PORTS = 4;

// The controller occupying each GameCube port, for the duration of one emulation session.
// Guarded by s_controller_setup_mutex.
std::mutex s_controller_setup_mutex;
std::array<std::string, NUM_GC_PORTS> s_controller_ports;
bool s_mappings_applied = false;
}  // namespace

namespace AutomaticControllers
{
bool IsActive()
{
  if (!Config::Get(Config::MAIN_AUTOMATIC_CONTROLLERS))
    return false;

  const std::lock_guard setup_lock(s_controller_setup_mutex);
  return std::ranges::any_of(s_controller_ports,
                             [](const std::string& owner) { return !owner.empty(); });
}
}  // namespace AutomaticControllers

void Host_TitleChanged()
{
  s_game_metadata_is_valid = true;

  JNIEnv* env = IDCache::GetEnvForThread();
  env->CallStaticVoidMethod(IDCache::GetNativeLibraryClass(), IDCache::GetOnTitleChanged());

  // On Wii, an ES title change reloads the pad configs from disk right after this callback
  // returns (TitleContext::Update calls SConfig::OnESTitleChanged after SetRunningGameMetadata),
  // clobbering automatically generated controller mappings. Queue a re-application to run
  // once the current CPU-thread callstack, including that reload, has finished; the job is
  // serviced by the emulation run loop.
  Core::QueueHostJob([](Core::System&) { AutoSetupControllers(); }, false);
}

std::unique_ptr<GBAHostInterface> Host_CreateGBAHost(std::weak_ptr<HW::GBA::Core> core)
{
  return nullptr;
}

static bool MsgAlert(const char* caption, const char* text, bool yes_no, Common::MsgType style)
{
  __android_log_print(ANDROID_LOG_ERROR, DOLPHIN_TAG, "[NativeLibrary] Alert: %s", text);

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
  Core::SetState(Core::System::GetInstance(), Core::State::Running);
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_PauseEmulation(
    JNIEnv*, jclass, bool override_achievement_restrictions)
{
  Core::SetState(Core::System::GetInstance(), Core::State::Paused, true,
                 override_achievement_restrictions);
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_StopEmulation(JNIEnv*, jclass)
{
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
  Core::SaveScreenShot();
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_eglBindAPI(JNIEnv*, jclass,
                                                                               jint api)
{
  eglBindAPI(api);
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_SaveState(JNIEnv*, jclass,
                                                                              jint slot)
{
  State::Save(Core::System::GetInstance(), slot);
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_SaveStateAs(JNIEnv* env, jclass,
                                                                                jstring path)
{
  State::SaveAs(Core::System::GetInstance(), GetJString(env, path));
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_LoadState(JNIEnv*, jclass,
                                                                              jint slot)
{
  State::Load(Core::System::GetInstance(), slot);
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_LoadStateAs(JNIEnv* env, jclass,
                                                                                jstring path)
{
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
  return static_cast<jint>(Common::Log::MAX_EFFECTIVE_LOGLEVEL);
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_WipeJitBlockProfilingData(
    JNIEnv* env, jclass native_library_class)
{
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

  s_surface_cv.notify_all();
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_SurfaceDestroyed(JNIEnv*,
                                                                                     jclass)
{
  {
    // If emulation continues running without a valid surface, we will probably crash,
    // so pause emulation until we get a valid surface again. EmulationFragment handles resuming.
    while (s_is_booting.IsSet())
    {
      // Need to wait for boot to finish before we can pause
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
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

  s_surface_cv.notify_all();
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
  WiimoteReal::Refresh();
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_ReloadConfig(JNIEnv*, jclass)
{
  SConfig::GetInstance().LoadSettings();
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_ResetDolphinSettings(JNIEnv*,
                                                                                         jclass)
{
  SConfig::ResetAllSettings();
  UICommon::SetUserDirectory(File::GetUserPath(D_USER_IDX));
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_Initialize(JNIEnv*, jclass)
{
  UICommon::CreateDirectories();
  Common::RegisterMsgAlertHandler(&MsgAlert);
  DolphinAnalytics::AndroidSetGetValFunc(&GetAnalyticValue);

  WiimoteReal::InitAdapterClass();
  UICommon::Init();
  UICommon::InitControllers(WindowSystemInfo(WindowSystemType::Android, nullptr, nullptr, nullptr));

  AchievementManager::GetInstance().Init(nullptr);
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

static bool HasInput(const ciface::Core::Device& device, std::string_view name)
{
  return device.FindInput(name) != nullptr;
}

// Enough to swallow the noise an analog stick reports while it sits idle
constexpr const char* DEAD_ZONE = "10.";

static std::string AxisName(int axis, bool negative)
{
  return fmt::format("Axis {}{}", axis, negative ? '-' : '+');
}

// Android reports an axis either as centered, with a direction each way, or as one way only.
// Which of the two it is depends on the axis alone: the sticks, the hat, AXIS_Z and AXIS_RZ
// are always centered, while AXIS_LTRIGGER, AXIS_RTRIGGER, AXIS_BRAKE and AXIS_GAS are always
// one way. It says nothing about what the control physically is.
static bool HasCenteredAxis(const ciface::Core::Device& device, int axis)
{
  return HasInput(device, AxisName(axis, false)) && HasInput(device, AxisName(axis, true));
}

static bool HasOneWayAxis(const ciface::Core::Device& device, int axis)
{
  return HasInput(device, AxisName(axis, false)) && !HasInput(device, AxisName(axis, true));
}

// Builds an expression that triggers on whichever of the given controls the controller has,
// so that one is enough but several are allowed, e.g. "`Up` | `Axis 16-`". Empty, leaving
// the control unmapped, if it has none of them.
static std::string AnyOf(const ciface::Core::Device& device,
                         std::initializer_list<std::string_view> controls)
{
  std::string result;
  for (std::string_view control : controls)
  {
    if (!HasInput(device, control))
      continue;
    if (!result.empty())
      result += " | ";
    result += fmt::format("`{}`", control);
  }
  return result;
}

// The first of the given axes that the controller reports as one way, if any
static std::string FirstOneWayAxis(const ciface::Core::Device& device,
                                   std::initializer_list<int> axes)
{
  for (int axis : axes)
  {
    if (HasOneWayAxis(device, axis))
      return fmt::format("`{}`", AxisName(axis, false));
  }
  return {};
}

// Generates a standard GameCube mapping for an Android controller from the controls it
// reports. Face buttons follow their physical position rather than their label, which is how
// Android reports them for both Xbox-style and PlayStation-style controllers.
static void GenerateAndroidGamepadConfig(const ciface::Core::Device& device,
                                         Common::IniFile::Section* section)
{
  ciface::Core::DeviceQualifier qualifier;
  qualifier.FromDevice(&device);
  section->Set("Device", qualifier.ToString());

  section->Set("Buttons/A", AnyOf(device, {"Button A"}));
  section->Set("Buttons/B", AnyOf(device, {"Button X"}));
  section->Set("Buttons/X", AnyOf(device, {"Button B"}));
  section->Set("Buttons/Y", AnyOf(device, {"Button Y"}));
  section->Set("Buttons/Z", AnyOf(device, {"Button R1"}));
  section->Set("Buttons/Start", AnyOf(device, {"Start"}));

  section->Set("Main Stick/Up", AnyOf(device, {AxisName(AMOTION_EVENT_AXIS_Y, true)}));
  section->Set("Main Stick/Down", AnyOf(device, {AxisName(AMOTION_EVENT_AXIS_Y, false)}));
  section->Set("Main Stick/Left", AnyOf(device, {AxisName(AMOTION_EVENT_AXIS_X, true)}));
  section->Set("Main Stick/Right", AnyOf(device, {AxisName(AMOTION_EVENT_AXIS_X, false)}));
  section->Set("Main Stick/Dead Zone", std::string(DEAD_ZONE));

  // Which axes the right stick and the analog triggers use differs between controllers, and
  // Android does not say which is which, because it decides whether an axis is centered from
  // the axis alone. Controllers that follow Android's own layout put the right stick on
  // AXIS_Z and AXIS_RZ and the triggers on axes of their own, while controllers that report
  // themselves the way an Xbox controller does put the triggers on AXIS_Z and AXIS_RZ and the
  // right stick on AXIS_RX and AXIS_RY. Having somewhere else for the triggers to be is what
  // tells the two layouts apart.
  const bool has_trigger_axes = HasOneWayAxis(device, AMOTION_EVENT_AXIS_LTRIGGER) ||
                                HasOneWayAxis(device, AMOTION_EVENT_AXIS_RTRIGGER) ||
                                HasOneWayAxis(device, AMOTION_EVENT_AXIS_BRAKE) ||
                                HasOneWayAxis(device, AMOTION_EVENT_AXIS_GAS);
  const bool has_trigger_buttons = HasInput(device, "Button L2") || HasInput(device, "Button R2");
  const bool has_z_rz = HasCenteredAxis(device, AMOTION_EVENT_AXIS_Z) &&
                        HasCenteredAxis(device, AMOTION_EVENT_AXIS_RZ);
  const bool has_rx_ry = HasCenteredAxis(device, AMOTION_EVENT_AXIS_RX) &&
                         HasCenteredAxis(device, AMOTION_EVENT_AXIS_RY);
  const bool z_rz_are_triggers = has_z_rz && has_rx_ry && !has_trigger_axes && !has_trigger_buttons;

  int c_stick_x = -1;
  int c_stick_y = -1;
  if (has_z_rz && !z_rz_are_triggers)
  {
    c_stick_x = AMOTION_EVENT_AXIS_Z;
    c_stick_y = AMOTION_EVENT_AXIS_RZ;
  }
  else if (has_rx_ry)
  {
    c_stick_x = AMOTION_EVENT_AXIS_RX;
    c_stick_y = AMOTION_EVENT_AXIS_RY;
  }

  if (c_stick_x >= 0)
  {
    section->Set("C-Stick/Up", fmt::format("`{}`", AxisName(c_stick_y, true)));
    section->Set("C-Stick/Down", fmt::format("`{}`", AxisName(c_stick_y, false)));
    section->Set("C-Stick/Left", fmt::format("`{}`", AxisName(c_stick_x, true)));
    section->Set("C-Stick/Right", fmt::format("`{}`", AxisName(c_stick_x, false)));
    section->Set("C-Stick/Dead Zone", std::string(DEAD_ZONE));
  }

  std::string l_analog =
      FirstOneWayAxis(device, {AMOTION_EVENT_AXIS_LTRIGGER, AMOTION_EVENT_AXIS_BRAKE});
  std::string r_analog =
      FirstOneWayAxis(device, {AMOTION_EVENT_AXIS_RTRIGGER, AMOTION_EVENT_AXIS_GAS});
  if (z_rz_are_triggers)
  {
    // A trigger on a centered axis rests at one end of it, so the whole of the axis is the
    // travel of the trigger.
    l_analog = fmt::format("`Full {}`", AxisName(AMOTION_EVENT_AXIS_Z, false));
    r_analog = fmt::format("`Full {}`", AxisName(AMOTION_EVENT_AXIS_RZ, false));
  }

  // Controllers without analog triggers report them as buttons instead
  const std::string l_button = AnyOf(device, {"Button L2"});
  const std::string r_button = AnyOf(device, {"Button R2"});
  if (l_analog.empty())
    l_analog = l_button;
  if (r_analog.empty())
    r_analog = r_button;

  section->Set("Triggers/L", l_button.empty() ? l_analog : l_button);
  section->Set("Triggers/R", r_button.empty() ? r_analog : r_button);
  section->Set("Triggers/L-Analog", l_analog);
  section->Set("Triggers/R-Analog", r_analog);

  // The d-pad arrives as buttons, as a hat, or as both
  section->Set("D-Pad/Up", AnyOf(device, {"Up", AxisName(AMOTION_EVENT_AXIS_HAT_Y, true)}));
  section->Set("D-Pad/Down", AnyOf(device, {"Down", AxisName(AMOTION_EVENT_AXIS_HAT_Y, false)}));
  section->Set("D-Pad/Left", AnyOf(device, {"Left", AxisName(AMOTION_EVENT_AXIS_HAT_X, true)}));
  section->Set("D-Pad/Right", AnyOf(device, {"Right", AxisName(AMOTION_EVENT_AXIS_HAT_X, false)}));

  if (device.FindOutput("Motor 0") != nullptr)
    section->Set("Rumble/Motor", std::string("`Motor 0`"));
}

// Whether a device can serve as a GameCube controller: Android's own markers of a gamepad,
// plus the controls that a generated mapping needs.
static bool IsUsableGamepad(const ciface::Core::Device& device,
                            const ciface::Android::DeviceProperties& properties)
{
  return properties.is_gamepad && HasInput(device, "Button A") && HasInput(device, "Axis 0-") &&
         HasInput(device, "Axis 0+") && HasInput(device, "Axis 1-") && HasInput(device, "Axis 1+");
}

// Gives the emulated controllers back the mappings the user configured. Called with the
// setup lock held.
static void RestoreControllerMappings()
{
  if (!s_mappings_applied)
    return;

  s_mappings_applied = false;
  if (Pad::GetConfig()->GetControllerCount() >= NUM_GC_PORTS)
    Pad::LoadConfig();
}

static void AutoSetupControllers()
{
  // Device changes and first inputs each run a setup on a thread of their own
  const std::lock_guard setup_lock(s_controller_setup_mutex);

  if (!Config::Get(Config::MAIN_AUTOMATIC_CONTROLLERS))
  {
    // Turning the setting off gives the ports back and starts the players over
    RestoreControllerMappings();
    if (!std::ranges::all_of(s_controller_ports, &std::string::empty))
    {
      s_controller_ports = {};
      ciface::Android::ForgetDeliveredInput();
      for (int port = 0; port < NUM_GC_PORTS; ++port)
        Config::DeleteKey(Config::LayerType::CurrentRun, Config::GetInfoForSIDevice(port));
    }
    return;
  }

  struct Candidate
  {
    std::shared_ptr<ciface::Core::Device> device;
    ciface::Android::DeviceProperties properties;
  };

  std::vector<Candidate> candidates;
  for (const std::string& device_string : g_controller_interface.GetAllDeviceStrings())
  {
    ciface::Core::DeviceQualifier qualifier;
    qualifier.FromString(device_string);

    const std::shared_ptr<ciface::Core::Device> device =
        g_controller_interface.FindDevice(qualifier);
    if (!device)
      continue;

    const std::optional properties = ciface::Android::GetDeviceProperties(*device);
    if (properties && IsUsableGamepad(*device, *properties))
      candidates.emplace_back(device, *properties);
  }

  // Players claim their port by using their controller: the first controller to send input
  // becomes player one, the next becomes player two, and so on. This is the only order that
  // reflects what the players did, because Android reports the controllers that are already
  // connected in a fixed order of its own. It also settles which of the devices Android
  // reports for one controller to take the input from, since only the one a player is
  // actually using ever sends any.
  std::erase_if(candidates, [](const Candidate& c) { return !c.properties.input_order; });
  std::ranges::sort(candidates, {}, [](const Candidate& c) { return *c.properties.input_order; });

  // Ports are held by the controller rather than by the device it is reported as, because
  // Android renames and renumbers those as controllers come and go.
  const auto candidate_index = [&](const std::string& descriptor) {
    if (descriptor.empty())
      return -1;
    const auto it = std::ranges::find(candidates, descriptor,
                                      [](const Candidate& c) { return c.properties.descriptor; });
    return it == candidates.end() ? -1 : static_cast<int>(it - candidates.begin());
  };

  const std::array previous_ports = s_controller_ports;

  // A controller keeps its port until the setting is turned off or Dolphin is closed, so
  // that leaving a game, or a controller running out of battery, does not shuffle the
  // players around.
  for (const Candidate& candidate : candidates)
  {
    if (candidate.properties.descriptor.empty() ||
        Common::Contains(s_controller_ports, candidate.properties.descriptor))
    {
      continue;
    }

    const auto free_port = std::ranges::find(s_controller_ports, std::string{});
    if (free_port == s_controller_ports.end())
      break;
    *free_port = candidate.properties.descriptor;
  }

  // Take over only the ports this feature has assigned, so that a port the user configured
  // as something else, or left empty, keeps whatever they chose. The current-run layer keeps
  // all of this out of their saved configuration, and while a game is running SerialInterface
  // picks the change up on the CPU thread by itself.
  for (int port = 0; port < NUM_GC_PORTS; ++port)
  {
    const auto& si_device = Config::GetInfoForSIDevice(port);
    if (candidate_index(s_controller_ports[port]) >= 0)
      Config::SetCurrent(si_device, SerialInterface::SIDEVICE_GC_CONTROLLER);
    else
      Config::DeleteKey(Config::LayerType::CurrentRun, si_device);
  }

  // The emulated controllers exist for as long as the process does, but a mapping cannot be
  // applied before they have been created.
  if (Pad::GetConfig()->GetControllerCount() < NUM_GC_PORTS)
    return;

  // Give every port back the mapping the user configured before handing out the generated
  // ones, so that a port whose controller has left is usable again rather than left pointing
  // at a controller that is no longer there. The mappings are handed out again on every pass
  // because a Wii title change reloads them from disk behind our back.
  Pad::LoadConfig();
  s_mappings_applied = true;

  for (int port = 0; port < NUM_GC_PORTS; ++port)
  {
    const int index = candidate_index(s_controller_ports[port]);
    if (index < 0)
      continue;

    Common::IniFile ini;
    Common::IniFile::Section* const section = ini.GetOrCreateSection("Generated");
    GenerateAndroidGamepadConfig(*candidates[index].device, section);

    ControllerEmu::EmulatedController* const controller = Pad::GetConfig()->GetController(port);
    controller->LoadConfig(section);
    controller->UpdateReferences(g_controller_interface);
  }

  if (s_controller_ports == previous_ports)
    return;

  // Tell the players which controller they ended up as, since nothing else says so while a
  // game is running.
  for (int port = 0; port < NUM_GC_PORTS; ++port)
  {
    const int index = candidate_index(s_controller_ports[port]);
    if (index < 0)
      continue;

    const std::string& name = candidates[index].device->GetName();
    INFO_LOG_FMT(CONTROLLERINTERFACE, "GameCube controller {}: {}", port + 1, name);
    OSD::AddMessage(fmt::format("GameCube controller {}: {}", port + 1, name));
  }
}

static void Run(JNIEnv* env, std::unique_ptr<BootParameters>&& boot, bool riivolution)
{
  if (riivolution && std::holds_alternative<BootParameters::Disc>(boot->parameters))
  {
    const std::string& riivolution_dir = File::GetUserPath(D_RIIVOLUTION_IDX);
    const DiscIO::Volume& volume = *std::get<BootParameters::Disc>(boot->parameters).volume;

    AddRiivolutionPatches(boot.get(), DiscIO::Riivolution::GenerateRiivolutionPatchesFromConfig(
                                          riivolution_dir, volume.GetGameID(), volume.GetRevision(),
                                          volume.GetDiscNumber()));
  }

  // Get the port configuration in place before the core reads it at boot. Players keep the
  // ports they have already claimed, so that leaving a game and starting another one does
  // not make everyone claim their place again.
  AutoSetupControllers();

  s_need_nonblocking_alert_msg = true;
  std::unique_lock<std::mutex> surface_guard(s_surface_lock);

  s_surface_cv.wait(surface_guard, []() { return s_surf != nullptr; });

  WindowSystemInfo wsi(WindowSystemType::Android, nullptr, s_surf, s_surf);
  wsi.render_surface_scale = GetRenderSurfaceScale(env);

  if (BootManager::BootCore(Core::System::GetInstance(), std::move(boot), wsi))
  {
    static constexpr int WAIT_STEP = 25;
    while (Core::GetState(Core::System::GetInstance()) == Core::State::Starting)
      std::this_thread::sleep_for(std::chrono::milliseconds(WAIT_STEP));

    AutoSetupControllers();

    // Run the setup again whenever the set of controllers changes, and when a device first
    // delivers input, which is what identifies it as a controller. The notifications arrive
    // on the threads that handle input and hotplug, so the work is queued for the host.
    const auto setup_on_change = [] {
      if (Core::IsRunning(Core::System::GetInstance()))
        Core::QueueHostJob([](Core::System&) { AutoSetupControllers(); });
    };
    static Common::EventHook devices_changed_hook =
        g_controller_interface.RegisterDevicesChangedCallback(setup_on_change);
    static Common::EventHook first_input_hook =
        ciface::Android::RegisterFirstInputCallback(setup_on_change);
  }

  s_is_booting.Clear();
  s_need_nonblocking_alert_msg = false;
  surface_guard.unlock();

  while (Core::IsRunning(Core::System::GetInstance()))
  {
    s_update_main_frame_event.Wait();
    Core::HostDispatchJobs(Core::System::GetInstance());
  }

  s_game_metadata_is_valid = false;
  Core::Shutdown(Core::System::GetInstance());

  {
    const std::lock_guard setup_lock(s_controller_setup_mutex);
    RestoreControllerMappings();
  }

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

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_RunNetPlay(
    JNIEnv* env, jclass, jobjectArray jPaths, jboolean jRiivolution, jlong jBootSessionData)
{
  auto boot_session_data =
      std::unique_ptr<BootSessionData>(reinterpret_cast<BootSessionData*>(jBootSessionData));
  if (!boot_session_data)
  {
    env->CallStaticVoidMethod(IDCache::GetNativeLibraryClass(), IDCache::GetDisplayToastMsg(),
                              ToJString(env, "Netplay: no boot session data"), JNI_TRUE);
    env->CallStaticVoidMethod(IDCache::GetNativeLibraryClass(),
                              IDCache::GetFinishEmulationActivity());
    return;
  }
  Run(env, JStringArrayToVector(env, jPaths), jRiivolution, std::move(*boot_session_data));
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_RunSystemMenu(JNIEnv* env,
                                                                                  jclass)
{
  Run(env, std::make_unique<BootParameters>(BootParameters::NANDTitle{Titles::SYSTEM_MENU}), false);
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_ChangeDisc(JNIEnv* env, jclass,
                                                                               jstring jFile)
{
  const std::string path = GetJString(env, jFile);
  __android_log_print(ANDROID_LOG_INFO, DOLPHIN_TAG, "Change Disc: %s", path.c_str());
  auto& system = Core::System::GetInstance();
  system.GetDVDInterface().ChangeDisc(Core::CPUThreadGuard{system}, path);
}

JNIEXPORT jobjectArray JNICALL
Java_org_dolphinemu_dolphinemu_NativeLibrary_GetLogTypeNames(JNIEnv* env, jclass)
{
  using LogManager = Common::Log::LogManager;

  return VectorToJObjectArray(
      env, LogManager::GetInstance()->GetLogTypes(), IDCache::GetPairClass(),
      [](JNIEnv* env_, const LogManager::LogContainer& log_container) {
        jstring short_name = ToJString(env_, log_container.m_short_name);
        jstring full_name = ToJString(env_, log_container.m_full_name);

        jobject pair = env_->NewObject(IDCache::GetPairClass(), IDCache::GetPairConstructor(),
                                       short_name, full_name);

        env_->DeleteLocalRef(short_name);
        env_->DeleteLocalRef(full_name);

        return pair;
      });
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
