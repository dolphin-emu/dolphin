#pragma once

#include <memory>
#include <vector>

#include "Core/PrimeHack/HackManager.h"
#include "InputCommon/ControlReference/ControlReference.h"

namespace prime
{
  void InitializeHack(std::string const& mkb_device_name, std::string const& mkb_device_source);

  bool CheckBeamCtl(int beam_num);
  bool CheckVisorCtl(int visor_num);
  bool CheckBeamScrollCtl(bool direction);
  bool CheckSpringBallCtl();

  void SetEFBToTexture(bool toggle);
  bool UseMPAutoEFB();
  bool GetEFBTexture();
  bool GetBloom();

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

  std::string const& GetCtlDeviceName();
  std::string const& GetCtlDeviceSource();

  HackManager *GetHackManager();
}
