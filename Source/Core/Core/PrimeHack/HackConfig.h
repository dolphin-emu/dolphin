#pragma once

#include <memory>
#include <vector>

#include "Core/PrimeHack/HackManager.h"
#include "InputCommon/ControlReference/ControlReference.h"

// Naming scheme will match dolphin as this place acts as an interface between the hack & dolphin proper
namespace prime {
void InitializeHack(std::string const& mkb_device_name, std::string const& mkb_device_source);

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

void SetEFBToTexture(bool toggle);
bool UseMPAutoEFB();
bool GetEFBTexture();
bool GetBloom();

bool GetNoclip();
bool GetInvulnerability();
bool GetSkipCutscene();

bool GetEnableSecondaryGunFX();

bool GetAutoArmAdjust();
bool GetToggleArmAdjust();
std::tuple<float, float, float> GetArmXYZ();

void UpdateHackSettings();

float GetSensitivity();
void SetSensitivity(float sensitivity);
float GetCursorSensitivity();
void SetCursorSensitivity(float sensitivity);
float GetFov();
void SetFov(float fov);
bool InvertedY();
void SetInvertedY(bool inverted);
bool InvertedX();
void SetInvertedX(bool inverted);
bool GetCulling();

double GetHorizontalAxis();
double GetVerticalAxis();

std::string const& GetCtlDeviceName();
std::string const& GetCtlDeviceSource();

HackManager *GetHackManager();
}
