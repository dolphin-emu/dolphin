#include "InputCommon/ControllerEmu/ControlGroup/PrimeHackAltProfile.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"

namespace ControllerEmu
{
  PrimeHackAltProfile::PrimeHackAltProfile(const std::string& name_, const std::string& default_selection)
    : ControlGroup(name_, GroupType::PrimeHackAltProfile), m_selection_value(default_selection)
  {
  }

    // Returns the MorphBall Profile
  const std::string& PrimeHackAltProfile::GetAltProfileName() const
  {
    return m_selection_value;
  }

  void PrimeHackAltProfile::SetAltProfileName(std::string& val)
  {
    m_selection_value = val;
  }
}  // namespace ControllerEmu
