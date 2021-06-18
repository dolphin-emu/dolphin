#pragma once
#include "GenericMouse.h"
#include "InputCommon/ControllerInterface/DInput/DInput8.h"

namespace prime
{
void InitMouse(IDirectInput8* const idi8);

/* DInputMouse -
Retrieves mouse input using DirectInput8, intended use outside of standard controller interface
Input data updated synchronously through UpdateInput method
Input data retrieved though GetDeltaAxis
*/
class DInputMouse : public GenericMouse
{
public:
  DInputMouse();
  // Initialize this class with a device
  void Init(LPDIRECTINPUTDEVICE8 mo_device);

  void UpdateInput() override;

  // Handled by Dolphin for Windows
  void LockCursorToGameWindow() override {};

private:
  DIMOUSESTATE2 state_prev;
  ULONGLONG last_update;
  LPDIRECTINPUTDEVICE8 m_mo_device;
};

}  // namespace prime
