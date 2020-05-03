#include "InputCommon/ControllerEmu/ControlGroup/PrimeHackModes.h"

namespace ControllerEmu
{
  PrimeHackModes::PrimeHackModes(const std::string& name_) : ControlGroup(name_, GroupType::PrimeHack)
  {
  }

  int PrimeHackModes::GetSelectedDevice() const
  {
    return m_selection_setting.GetValue();
  }

  void PrimeHackModes::SetSelectedDevice(int val)
  {
    m_selection_setting.SetValue(val);
  }
}  // namespace ControllerEmu
