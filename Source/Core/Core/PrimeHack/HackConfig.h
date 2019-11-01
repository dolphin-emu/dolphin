#pragma once

#include <memory>
#include <vector>

#include "Core/PrimeHack/HackManager.h"
#include "InputCommon/ControlReference/ControlReference.h"

namespace prime
{
  void InitializeHack(std::string const& mkb_device_name, std::string const& mkb_device_source);

  // PRECONDITION:  For all following functions, InitializeHack has been called
  std::vector<std::unique_ptr<ControlReference>>& GetMutableControls();
  void RefreshControlDevices();

  bool CheckBeamCtl(int beam_num);
  bool CheckVisorCtl(int visor_num);
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

  std::string const& GetCtlDeviceName();
  std::string const& GetCtlDeviceSource();

  HackManager *GetHackManager();
}
