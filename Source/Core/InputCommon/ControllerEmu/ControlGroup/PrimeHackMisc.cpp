#include "InputCommon/ControllerEmu/ControlGroup/PrimeHackMisc.h"

namespace ControllerEmu
{
  PrimeHackMisc::PrimeHackMisc(const std::string& name_) : ControlGroup(name_, GroupType::PrimeHack)
  {
  }

  int PrimeHackMisc::GetSelectedDevice() const
  {
    return m_selection_setting.GetValue();
  }

  void PrimeHackMisc::SetSelectedDevice(int val)
  {
    m_selection_setting.SetValue(val);
  }
}  // namespace ControllerEmu
