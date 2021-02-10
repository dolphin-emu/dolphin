#pragma once

#include <memory>
#include <vector>

#include "Core/PrimeHack/HackManager.h"
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
bool GrappleCtlBound();

void SetEFBToTexture(bool toggle);
bool UseMPAutoEFB();
bool LockCameraInPuzzles();
bool GetEFBTexture();
bool GetBloom();

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
float GetFov();
bool InvertedY();
void SetInvertedY(bool inverted);
bool InvertedX();
void SetInvertedX(bool inverted);
bool GetCulling();

bool HandleReticleLockOn();
void SetReticleLock(bool lock);

enum CameraLock { Centre, Angle45, Unlocked };

void SetLockCamera(CameraLock lock);
CameraLock GetLockCamera();

bool CheckPitchRecentre();

double GetHorizontalAxis();
double GetVerticalAxis();

std::tuple<bool, bool> GetMenuOptions();

HackManager *GetHackManager();
EmuVariableManager *GetVariableManager();
}
