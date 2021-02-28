#include "HackConfig.h"

#include <array>
#include <string>

#include "Common/IniFile.h"
#include "Core/PrimeHack/PrimeUtils.h"
#include "Core/PrimeHack/EmuVariableManager.h"

#include "Core/PrimeHack/Mods/AutoEFB.h"
#include "Core/PrimeHack/Mods/CutBeamFxMP1.h"
#include "Core/PrimeHack/Mods/DisableBloom.h"
#include "Core/PrimeHack/Mods/FpsControls.h"
#include "Core/PrimeHack/Mods/RestoreDashing.h"
#include "Core/PrimeHack/Mods/Invulnerability.h"
#include "Core/PrimeHack/Mods/Noclip.h"
#include "Core/PrimeHack/Mods/SkipCutscene.h"
#include "Core/PrimeHack/Mods/SpringballButton.h"
#include "Core/PrimeHack/Mods/ViewModifier.h"
#include "Core/PrimeHack/Mods/ContextSensitiveControls.h"
#include "Core/PrimeHack/Mods/FriendVouchers.h"
#include "Core/PrimeHack/Mods/PortalSkipMP2.h"

#include "InputCommon/ControllerInterface/ControllerInterface.h"

#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"
#include "Core/HW/GCPad.h"
#include "Core/ConfigManager.h"
#include "Core/Config/GraphicsSettings.h"
#include "VideoCommon/VideoConfig.h"

namespace prime {
namespace {
float sensitivity;
float cursor_sensitivity;
float camera_fov;

bool inverted_x = false;
bool inverted_y = false;
HackManager hack_mgr;
EmuVariableManager var_mgr;
bool is_running = false;
CameraLock lock_camera = CameraLock::Unlocked;
bool reticle_lock = false;
}

void InitializeHack() {
  if (is_running) return; is_running = true;

  // Create all mods
  hack_mgr.add_mod("auto_efb", std::make_unique<AutoEFB>());
  hack_mgr.add_mod("cut_beam_fx_mp1", std::make_unique<CutBeamFxMP1>());
  hack_mgr.add_mod("disable_bloom", std::make_unique<DisableBloom>());
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

  hack_mgr.enable_mod("skip_cutscene");
  hack_mgr.enable_mod("fov_modifier");

  // Enable no PrimeHack control mods
  if (!SConfig::GetInstance().bEnablePrimeHack) {
    return;
  }


  hack_mgr.enable_mod("fps_controls");
  hack_mgr.enable_mod("springball_button");
  hack_mgr.enable_mod("context_sensitive_controls");
  hack_mgr.enable_mod("portal_skip_mp2");
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
  return Wiimote::CheckForward();
}

bool CheckBack() {
  return Wiimote::CheckBack();
}

bool CheckLeft() {
  return Wiimote::CheckLeft();
}

bool CheckRight() {
  return Wiimote::CheckRight();
}

bool CheckJump() {
  return Wiimote::CheckJump();
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

bool GetNoclip() {
  return SConfig::GetInstance().bPrimeNoclip;
}

bool GetInvulnerability() {
  return SConfig::GetInstance().bPrimeInvulnerability;
}

bool GetSkipCutscene() {
  return SConfig::GetInstance().bPrimeSkipCutscene;
}

bool GetRestoreDashing() {
  return SConfig::GetInstance().bPrimeRestoreDashing;
}

bool GetEFBTexture() {
  return Config::Get(Config::GFX_HACK_SKIP_EFB_COPY_TO_RAM);
}

bool GetBloom() {
  return Config::Get(Config::DISABLE_BLOOM);
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

void UpdateHackSettings() {
  double camera, cursor;
  bool invertx, inverty, lock = false;

  if (hack_mgr.get_active_game() >= Game::PRIME_1_GCN)
    std::tie<double, double, bool, bool>(camera, cursor, invertx, inverty) =
      Pad::PrimeSettings();
  else
    std::tie<double, double, bool, bool, bool>(camera, cursor, invertx, inverty, lock) =
      Wiimote::PrimeSettings();

  SetSensitivity((float)camera);
  SetCursorSensitivity((float)cursor);
  SetInvertedX(invertx);
  SetInvertedY(inverty);
  SetReticleLock(lock);
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

bool CheckPitchRecentre() {
  if (hack_mgr.get_active_game() >= Game::PRIME_1_GCN) {
    if (Pad::PrimeUseController()) {
      return Pad::CheckPitchRecentre();
    } 
  }
  else if (Wiimote::PrimeUseController()) {
    return Wiimote::CheckPitchRecentre();
  }

  return false;
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

EmuVariableManager* GetVariableManager() {
  return &var_mgr;
}

HackManager* GetHackManager() {
  return &hack_mgr;
}
}  // namespace prime
