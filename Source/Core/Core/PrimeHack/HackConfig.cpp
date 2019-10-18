#include "HackConfig.h"

#include <array>
#include <string>

#include "Common/IniFile.h"
#include "Core/PrimeHack/AimMods.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"

#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"

namespace prime
{
static std::vector<std::unique_ptr<ControlReference>> control_list;
static float sensitivity;
static float cursor_sensitivity;
static float camera_fov;

static std::string device_name, device_source;
static bool inverted_y = false;
static bool inverted_x = false;
static HackManager hack_mgr;

void InitializeHack(std::string const& mkb_device_name, std::string const& mkb_device_source)
{
  // Create mods for all games/regions.
  hack_mgr.add_mod(std::make_unique<MP1NTSC>());
  hack_mgr.add_mod(std::make_unique<MP1PAL>());
  hack_mgr.add_mod(std::make_unique<MP2NTSC>());
  hack_mgr.add_mod(std::make_unique<MP2PAL>());
  hack_mgr.add_mod(std::make_unique<MP3NTSC>());
  hack_mgr.add_mod(std::make_unique<MP3PAL>());
  hack_mgr.add_mod(std::make_unique<MenuNTSC>());
  hack_mgr.add_mod(std::make_unique<MenuPAL>());

  device_name = mkb_device_name;
  device_source = mkb_device_source;
}

void RefreshControlDevices()
{
  for (int i = 0; i < control_list.size(); i++)
  {
    control_list[i]->UpdateReference(
        g_controller_interface,
        ciface::Core::DeviceQualifier(device_source.c_str(), 0, device_name.c_str()));
  }
}

bool CheckBeamCtl(int beam_num)
{
  return Wiimote::CheckBeam(beam_num);
}

bool CheckVisorCtl(int visor_num)
{
  // return control_list[visor_num + 4]->State() > 0.5;
  return Wiimote::CheckVisor(visor_num);
}

void UpdateHackSettings()
{
  std::tuple<double, double, double, bool, bool> t = Wiimote::PrimeSettings();
  double camera = std::get<0>(t);
  double cursor = std::get<1>(t);
  double fov = std::get<2>(t);
  bool invertx = std::get<3>(t);
  bool inverty = std::get<4>(t);

  SetSensitivity((float)camera);
  SetCursorSensitivity((float)cursor);
  SetFov((float)fov);
  SetInvertedX(invertx);
  SetInvertedY(inverty);
}

std::vector<std::unique_ptr<ControlReference>>& GetMutableControls()
{
  return control_list;
}

float GetSensitivity()
{
  return sensitivity;
}

void SetSensitivity(float sens)
{
  sensitivity = sens;
}

float GetCursorSensitivity()
{
  return cursor_sensitivity;
}

void SetCursorSensitivity(float sens)
{
  cursor_sensitivity = sens;
}

float GetFov()
{
  return camera_fov;
}

void SetFov(float fov)
{
  camera_fov = fov;
}

bool InvertedY()
{
  return inverted_y;
}

void SetInvertedY(bool inverted)
{
  inverted_y = inverted;
}

bool InvertedX()
{
  return inverted_x;
}

void SetInvertedX(bool inverted)
{
  inverted_x = inverted;
}

std::string const& GetCtlDeviceName()
{
  return device_name;
}

std::string const& GetCtlDeviceSource()
{
  return device_source;
}

HackManager* GetHackManager()
{
  return &hack_mgr;
}
}  // namespace prime
