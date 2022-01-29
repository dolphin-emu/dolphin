#include "InputCommon/ControllerEmu/ControlGroup/PrimeHackMorph.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"

namespace ControllerEmu
{
  PrimeHackMorph::PrimeHackMorph(const std::string& name_, const std::string& default_selection)
    : ControlGroup(name_, GroupType::PrimeHackMorph), m_selection_value(default_selection)
  {
  }

    // Returns the MorphBall Profile
  const std::string& PrimeHackMorph::GetMorphBallProfileName() const
  {
    return m_selection_value;
  }

  void PrimeHackMorph::SetMorphBallProfileName(std::string& val)
  {
    m_selection_value = val;
  }
}  // namespace ControllerEmu
