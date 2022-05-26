#include "InputCommon/ControllerEmu/ControlGroup/PrimeHackModes.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"

namespace ControllerEmu
{
  PrimeHackModes::PrimeHackModes(const std::string& name_) : ControlGroup(name_, GroupType::PrimeHackMode)
  {
  }

  // Always return controller mode for platforms with input APIs we don't support.
  int PrimeHackModes::GetSelectedDevice() const
  {
    if (GetMouseSupported()) {
      return m_selection_setting.GetValue();
    }
    else {
      return 1;
    }
  }

  // Check to see if we're on a platform with a mouse class.
  bool PrimeHackModes::GetMouseSupported() const
  {
#if defined CIFACE_USE_WIN32 || defined CIFACE_USE_XLIB || defined CIFACE_USE_OSX
    return true;
#else
    return false;
#endif
  }

  void PrimeHackModes::SetSelectedDevice(int val)
  {
    m_selection_setting.SetValue(val);
  }
}  // namespace ControllerEmu
