#pragma once

#include <memory>
#include <vector>

#include "Core/PrimeHack/HackManager.h"
#include "Core/PrimeHack/AddressDB.h"
#include "Core/PrimeHack/EmuVariableManager.h"

#include "InputCommon/ControlReference/ControlReference.h"

// Naming scheme will match dolphin as this place acts as an interface between the hack & dolphin proper
namespace prime {
void InitializeHack();

bool CheckBeamCtl(int beam_num);
bool CheckVisorCtl(int visor_num);
bool CheckBeamScrollCtl(bool direction);
bool CheckVisorScrollCtl(bool direction);
bool CheckSpringBallCtl();
bool ImprovedMotionControls();
bool CheckForward();
bool CheckBack();
bool CheckLeft();
bool CheckRight();
bool CheckJump();

bool CheckGrappleCtl();
bool GrappleTappingMode();
bool GrappleCtlBound();

void SetEFBToTexture(bool toggle);
bool UseMPAutoEFB();
bool LockCameraInPuzzles();
bool GetEFBTexture();
bool GetBloom();
bool GetReduceBloom();
float GetBloomIntensity();

bool GetNoclip();
bool GetInvulnerability();
bool GetSkipCutscene();
bool GetRestoreDashing();

bool GetEnableSecondaryGunFX();
bool GetShowGCCrosshair();
u32 GetGCCrosshairColor();

bool GetAutoArmAdjust();
bool GetToggleArmAdjust();
std::tuple<float, float, float> GetArmXYZ();

void UpdateHackSettings();

float GetSensitivity();
void SetSensitivity(float sensitivity);
float GetCursorSensitivity();
void SetCursorSensitivity(float sensitivity);
bool ScaleCursorSensitivity();
void SetScaleCursorSensitivity(bool scale);
float GetFov();
bool InvertedY();
void SetInvertedY(bool inverted);
bool InvertedX();
void SetInvertedX(bool inverted);
bool GetCulling();

bool HandleReticleLockOn();
void SetReticleLock(bool lock);

bool NewMapControlsEnabled();
void SetNewMapControls(bool new_controls);

enum CameraLock { Centre, Angle45, Unlocked };

void SetLockCamera(CameraLock lock);
CameraLock GetLockCamera();

bool CheckPitchRecentre();
bool ControllerMode();

std::pair<std::string, std::string> GetProfiles();
void ChangeControllerProfileAlt(std::string profile);

double GetHorizontalAxis();
double GetVerticalAxis();

std::tuple<bool, bool> GetMenuOptions();

HackManager *GetHackManager();
AddressDB *GetAddressDB();
EmuVariableManager *GetVariableManager();

bool ModPending();
void ClearPendingModfile();
std::string GetPendingModfile();
void SetPendingModfile(std::string const& path);

bool ModSuspended();
void SuspendMod();
void ResumeMod();

void SetMotd(std::string const& motd);
std::string GetMotd();

bool UsingRealWiimote();
}
