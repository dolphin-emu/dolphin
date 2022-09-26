#include "HackConfig.h"

#include <array>
#include <string>
#include <mutex>

#include "Common/IniFile.h"
#include "Common/CommonPaths.h"
#include "Common/FileSearch.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/StringUtil.h"

#include "Core/PrimeHack/PrimeUtils.h"
#include "Core/PrimeHack/EmuVariableManager.h"

#include "Core/PrimeHack/Mods/AutoEFB.h"
#include "Core/PrimeHack/Mods/CutBeamFxMP1.h"
#include "Core/PrimeHack/Mods/DisableBloom.h"
#include "Core/PrimeHack/Mods/BloomIntensityMP3.h"
#include "Core/PrimeHack/Mods/FpsControls.h"
#include "Core/PrimeHack/Mods/RestoreDashing.h"
#include "Core/PrimeHack/Mods/Invulnerability.h"
#include "Core/PrimeHack/Mods/MapController.h"
#include "Core/PrimeHack/Mods/Motd.h"
#include "Core/PrimeHack/Mods/Noclip.h"
#include "Core/PrimeHack/Mods/SkipCutscene.h"
#include "Core/PrimeHack/Mods/SpringballButton.h"
#include "Core/PrimeHack/Mods/STRGPatch.h"
#include "Core/PrimeHack/Mods/ViewModifier.h"
#include "Core/PrimeHack/Mods/ContextSensitiveControls.h"
#include "Core/PrimeHack/Mods/FriendVouchers.h"
#include "Core/PrimeHack/Mods/PortalSkipMP2.h"
#include "Core/PrimeHack/Mods/DisableHudMemoPopup.h"
#include "Core/PrimeHack/Mods/ElfModLoader.h"
#include "Core/PrimeHack/Mods/UnlockHypermode.h"

#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"
#include "Core/HW/GCPad.h"
#include "Core/ConfigManager.h"
#include "Core/Config/MainSettings.h"
#include "Core/Config/GraphicsSettings.h"
#include "Core/Host.h"

#include "Common/Config/Config.h"

#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/ControllerEmu/ControlGroup/PrimeHackAltProfile.h"
#include "InputCommon/InputConfig.h"

#include "VideoCommon/VideoConfig.h"

constexpr const char* PROFILES_DIR = "Profiles/";

namespace prime {
namespace {
float sensitivity;
float cursor_sensitivity;
float camera_fov;

bool inverted_x = false;
bool inverted_y = false;
bool scale_cursor_sens = false;
HackManager hack_mgr;
AddressDB addr_db;
EmuVariableManager var_mgr;
bool is_running = false;
CameraLock lock_camera = CameraLock::Unlocked;
bool reticle_lock = false;
bool new_map_controls = false;

std::string pending_modfile = "";
bool mod_suspended = false;

std::string trilogy_motd = "Thanks for using PrimeHack!\nPlease see our wiki for help!";
std::mutex motd_lock;
}

void InitializeHack() {
  if (is_running) return; is_running = true;
  PrimeMod::set_hack_manager(GetHackManager());
  PrimeMod::set_address_database(GetAddressDB());
  init_db(*GetAddressDB());

  // Create all mods
  hack_mgr.add_mod("auto_efb", std::make_unique<AutoEFB>());
  hack_mgr.add_mod("cut_beam_fx_mp1", std::make_unique<CutBeamFxMP1>());
  hack_mgr.add_mod("bloom_modifier", std::make_unique<DisableBloom>());
  hack_mgr.add_mod("bloom_intensity", std::make_unique<BloomIntensityMP3>());
  hack_mgr.add_mod("fps_controls", std::make_unique<FpsControls>());
  hack_mgr.add_mod("invulnerability", std::make_unique<Invulnerability>());
  hack_mgr.add_mod("noclip", std::make_unique<Noclip>());
  hack_mgr.add_mod("skip_cutscene", std::make_unique<SkipCutscene>());
  hack_mgr.add_mod("restore_dashing", std::make_unique<RestoreDashing>());
  hack_mgr.add_mod("springball_button", std::make_unique<SpringballButton>());
  hack_mgr.add_mod("fov_modifier", std::make_unique<ViewModifier>());
  hack_mgr.add_mod("context_sensitive_controls", std::make_unique<ContextSensitiveControls>());
  hack_mgr.add_mod("portal_skip_mp2", std::make_unique<PortalSkipMP2>());
  hack_mgr.add_mod("friend_vouchers_cheat", std::make_unique<FriendVouchers>());
  hack_mgr.add_mod("disable_hudmemo_popup", std::make_unique<DisableHudMemoPopup>());
  hack_mgr.add_mod("elf_mod_loader", std::make_unique<ElfModLoader>());
  hack_mgr.add_mod("unlock_hypermode", std::make_unique<UnlockHypermode>());
  hack_mgr.add_mod("map_controller", std::make_unique<MapController>());
  hack_mgr.add_mod("motd", std::make_unique<Motd>());
  hack_mgr.add_mod("strg_patch", std::make_unique<STRGPatch>());

  hack_mgr.enable_mod("skip_cutscene");
  hack_mgr.enable_mod("fov_modifier");
  hack_mgr.enable_mod("bloom_modifier");
  hack_mgr.enable_mod("bloom_intensity");
  hack_mgr.enable_mod("map_controller");
  hack_mgr.enable_mod("motd");
  hack_mgr.enable_mod("strg_patch");

  // Enable no PrimeHack control mods
  if (!Config::Get(Config::PRIMEHACK_ENABLE))
  {
    return;
  }

  hack_mgr.enable_mod("fps_controls");
  hack_mgr.enable_mod("springball_button");
  hack_mgr.enable_mod("context_sensitive_controls");
  hack_mgr.enable_mod("elf_mod_loader");
}

bool CheckBeamCtl(int beam_num) {
  return Wiimote::CheckBeam(beam_num);
}

bool CheckVisorCtl(int visor_num) {
  return Wiimote::CheckVisor(visor_num);
}

bool CheckVisorScrollCtl(bool direction) {
  return Wiimote::CheckVisorScroll(direction);
}

bool CheckBeamScrollCtl(bool direction) {
  return Wiimote::CheckBeamScroll(direction);
}

bool CheckSpringBallCtl() {
  return Wiimote::CheckSpringBall();
}

bool ImprovedMotionControls() {
  return Wiimote::CheckImprovedMotions();
}

bool CheckForward() {
  if (hack_mgr.get_active_game() >= Game::PRIME_1_GCN) {
    return Pad::CheckForward();
  }
  else {
    return Wiimote::CheckForward();
  }
}

bool CheckBack() {
  if (hack_mgr.get_active_game() >= Game::PRIME_1_GCN) {
    return Pad::CheckBack();
  }
  else {
    return Wiimote::CheckBack();
  }
}

bool CheckLeft() {
  if (hack_mgr.get_active_game() >= Game::PRIME_1_GCN) {
    return Pad::CheckLeft();
  }
  else {
    return Wiimote::CheckLeft();
  }
}

bool CheckRight() {
  if (hack_mgr.get_active_game() >= Game::PRIME_1_GCN) {
    return Pad::CheckRight();
  }
  else {
    return Wiimote::CheckRight();
  }
}

bool CheckJump() {
  if (hack_mgr.get_active_game() >= Game::PRIME_1_GCN) {
    return Pad::CheckJump();
  }
  else {
    return Wiimote::CheckJump();
  }
}

bool CheckGrappleCtl() {
  return Wiimote::CheckGrapple();
}

bool GrappleTappingMode() {
  return Wiimote::UseGrappleTapping();
}

bool GrappleCtlBound() {
  return Wiimote::GrappleCtlBound();
}

void SetEFBToTexture(bool toggle) {
  return Config::SetCurrent(Config::GFX_HACK_SKIP_EFB_COPY_TO_RAM, toggle);
}

bool UseMPAutoEFB() {
  return Config::Get(Config::AUTO_EFB);
}

bool LockCameraInPuzzles() {
  return Config::Get(Config::LOCKCAMERA_IN_PUZZLES);
}

bool GetEFBTexture() {
  return Config::Get(Config::GFX_HACK_SKIP_EFB_COPY_TO_RAM);
}

bool GetBloom() {
  return Config::Get(Config::DISABLE_BLOOM);
}

bool GetReduceBloom() {
  return Config::Get(Config::REDUCE_BLOOM);
}

float GetBloomIntensity() {
  return Config::Get(Config::BLOOM_INTENSITY);
}

bool GetEnableSecondaryGunFX() {
  return Config::Get(Config::ENABLE_SECONDARY_GUNFX);
}

bool GetShowGCCrosshair() {
  return Config::Get(Config::GC_SHOW_CROSSHAIR);
}

u32 GetGCCrosshairColor() {
  return Config::Get(Config::GC_CROSSHAIR_COLOR_RGBA);
}

bool GetAutoArmAdjust() {
  return Config::Get(Config::ARMPOSITION_MODE) == 0;
}

bool GetToggleArmAdjust() {
  return Config::Get(Config::TOGGLE_ARM_REPOSITION);
}

std::tuple<float, float, float> GetArmXYZ() {
  float x = Config::Get(Config::ARMPOSITION_LEFTRIGHT) / 100.f;
  float y = Config::Get(Config::ARMPOSITION_FORWARDBACK) / 100.f;
  float z = Config::Get(Config::ARMPOSITION_UPDOWN) / 100.f;

  return std::make_tuple(x, y, z);
}


std::pair<std::string, std::string> GetProfiles() {
  auto* group = static_cast<ControllerEmu::PrimeHackAltProfile*>(
    Wiimote::GetWiimoteGroup(0, WiimoteEmu::WiimoteGroup::AltProfileControls));

  const std::string alt_profname = group->GetAltProfileName();
  std::string alt_profile_path;
  std::string main_profile_path;

  if (!alt_profname.empty() && (alt_profname != std::string("Disabled"))) {
    alt_profile_path = File::GetUserPath(D_CONFIG_IDX) + PROFILES_DIR +
                         Wiimote::GetConfig()->GetProfileName() + "/" + group->GetAltProfileName() +
      ".ini";
  } else {
    alt_profile_path = "";
  }

  main_profile_path = File::GetUserPath(D_CONFIG_IDX) + WIIMOTE_INI_NAME + "_Backup.ini";

  return { alt_profile_path, main_profile_path };
}

void ChangeControllerProfileAlt(std::string profile_path)
{
  IniFile ini;
  ini.Load(profile_path);

  std::string profile_name = ini.GetSection("Wiimote1") ? "Wiimote1" : "Profile";

  Wiimote::GetConfig()->GetController(0)->LoadConfig(ini.GetOrCreateSection(profile_name));
  Wiimote::GetConfig()->GetController(0)->UpdateReferences(g_controller_interface);
}

void UpdateHackSettings() {
  double camera, cursor;
  bool invertx, inverty, scale_sens = false, lock = false, new_controls;

  if (hack_mgr.get_active_game() >= Game::PRIME_1_GCN)
    std::tie<double, double, bool, bool, bool>(camera, cursor, invertx, inverty, new_controls) =
      Pad::PrimeSettings();
  else
    std::tie<double, double, bool, bool, bool, bool>(camera, cursor, invertx, inverty, scale_sens, lock, new_controls) =
      Wiimote::PrimeSettings();

  SetSensitivity((float)camera);
  SetCursorSensitivity((float)cursor);
  SetInvertedX(invertx);
  SetInvertedY(inverty);
  SetScaleCursorSensitivity(scale_sens);
  SetReticleLock(lock);
  SetNewMapControls(new_controls);
}

float GetSensitivity() {
  return sensitivity;
}

void SetSensitivity(float sens) {
  sensitivity = sens;
}

bool HandleReticleLockOn()
{
  return reticle_lock;
}

bool NewMapControlsEnabled()
{
  return new_map_controls;
}

void SetNewMapControls(bool new_controls)
{
  new_map_controls = new_controls;
}

void SetReticleLock(bool lock)
{
  reticle_lock = lock;
}

float GetCursorSensitivity() {
  return cursor_sensitivity;
}

void SetCursorSensitivity(float sens) {
  cursor_sensitivity = sens;
}

float GetFov() {
  return Config::Get(Config::FOV);
}

bool InvertedY() {
  return inverted_y;
}

void SetInvertedY(bool inverted) {
  inverted_y = inverted;
}

bool InvertedX() {
  return inverted_x;
}

void SetInvertedX(bool inverted) {
  inverted_x = inverted;
}

bool ScaleCursorSensitivity() {
  return scale_cursor_sens;
}

void SetScaleCursorSensitivity(bool scale) {
  scale_cursor_sens = scale;
}

bool CheckPitchRecentre() {
  if (ControllerMode()) {
    if (hack_mgr.get_active_game() >= Game::PRIME_1_GCN) {
      return Pad::CheckPitchRecentre();
    } else {
      return Wiimote::CheckPitchRecentre();
    }
  }

  return false;
}

bool ControllerMode() {
  if (hack_mgr.get_active_game() == Game::INVALID_GAME)
    return true;

  if (hack_mgr.get_active_game() >= Game::PRIME_1_GCN) {
    return Pad::PrimeUseController();
  }
  else {
    return Wiimote::PrimeUseController();
  }
}

double GetHorizontalAxis() {
  if (hack_mgr.get_active_game() >= Game::PRIME_1_GCN) {
    if (Pad::PrimeUseController()) {
      return std::get<0>(Pad::GetPrimeStickXY());
    }
  }
  else if (Wiimote::PrimeUseController()) {
    return std::get<0>(Wiimote::GetPrimeStickXY());
  }

  if (!Host_RendererHasFocus())
    return 0;

  return static_cast<double>(g_mouse_input->GetDeltaHorizontalAxis());
}

double GetVerticalAxis() {
  if (hack_mgr.get_active_game() >= Game::PRIME_1_GCN) {
    if (Pad::PrimeUseController()) {
      return std::get<1>(Pad::GetPrimeStickXY());
    }
  }
  else if (Wiimote::PrimeUseController()) {
    return std::get<1>(Wiimote::GetPrimeStickXY());
  }

  if (!Host_RendererHasFocus())
    return 0;

  return static_cast<double>(g_mouse_input->GetDeltaVerticalAxis());
}

bool GetCulling() {
  return Config::Get(Config::TOGGLE_CULLING);
}

void SetLockCamera(CameraLock lock) {
  lock_camera = lock;
}

CameraLock GetLockCamera() {
  return lock_camera;
}

std::tuple<bool, bool> GetMenuOptions() {
  return Wiimote::GetBVMenuOptions();
}

AddressDB* GetAddressDB() {
  return &addr_db;
}

EmuVariableManager* GetVariableManager() {
  return &var_mgr;
}

HackManager* GetHackManager() {
  return &hack_mgr;
}

bool ModPending() {
  return !pending_modfile.empty();
}

void ClearPendingModfile() {
  pending_modfile.clear();
}

std::string GetPendingModfile() {
  return pending_modfile;
}

void SetPendingModfile(std::string const& path) {
  pending_modfile = path;
}

bool ModSuspended() {
  return mod_suspended;
}

void SuspendMod() {
  mod_suspended = true;
}

void ResumeMod() {
  mod_suspended = false;
}

void SetMotd(std::string const& motd) {
  auto lock = std::lock_guard<std::mutex>(motd_lock);
  trilogy_motd = motd;
}

std::string GetMotd() {
  auto lock = std::lock_guard<std::mutex>(motd_lock);
  return trilogy_motd;
}

bool UsingRealWiimote() {
  return Wiimote::GetSource(0) == WiimoteSource::Real;
}
}  // namespace prime
