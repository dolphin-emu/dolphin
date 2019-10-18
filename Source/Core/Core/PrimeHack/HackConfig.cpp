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
static const std::string config_path = "hack_config.ini";
static std::string device_name, device_source;
static bool inverted_y = false;
static bool inverted_x = false;
static HackManager hack_mgr;

static std::array<std::string, 4> beam_binds = {"`1` & !E", "`2` & !E", "`3` & !E", "`4` & !E"};
static std::array<std::string, 4> visor_binds = {"", "E & `1`", "E & `2`", "E & `3`"};

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

void SaveStateToIni()
{
  IniFile cfg_file;
  cfg_file.Load(config_path, true);

  auto* sens_section = cfg_file.GetOrCreateSection("mouse");
  sens_section->Set("sensitivity", sensitivity);
  sens_section->Set("cursor_sensitivity", cursor_sensitivity);
  auto* misc_section = cfg_file.GetOrCreateSection("misc");
  misc_section->Set("fov", camera_fov);
  misc_section->Set("inverted_y", inverted_y);
  misc_section->Set("inverted_x", inverted_x);
  auto* beam_section = cfg_file.GetOrCreateSection("beam");
  auto* visor_section = cfg_file.GetOrCreateSection("visor");

  for (int i = 0; i < 4; i++)
  {
    std::string control = std::string("index_") + std::to_string(i);
    beam_section->Set(control, control_list[i]->GetExpression());
    visor_section->Set(control, control_list[i + 4]->GetExpression());
  }
  cfg_file.Save(config_path);
}

void LoadStateFromIni()
{
  IniFile cfg_file;
  cfg_file.Load(config_path, true);

  auto* sens_section = cfg_file.GetOrCreateSection("mouse");
  sens_section->Get("sensitivity", &sensitivity);
  sens_section->Get("cursor_sensitivity", &cursor_sensitivity);
  auto* misc_section = cfg_file.GetOrCreateSection("misc");
  misc_section->Get("fov", &camera_fov);
  misc_section->Get("inverted_y", &inverted_y);
  misc_section->Get("inverted_x", &inverted_x);
  auto* beam_section = cfg_file.GetOrCreateSection("beam");
  auto* visor_section = cfg_file.GetOrCreateSection("visor");

  for (int i = 0; i < 4; i++)
  {
    std::string control = std::string("index_") + std::to_string(i);
    std::string expr;
    beam_section->Get(control, &expr);
    control_list[i]->SetExpression(expr);
    visor_section->Get(control, &expr);
    control_list[i + 4]->SetExpression(expr);
  }
  cfg_file.Save(config_path);
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
