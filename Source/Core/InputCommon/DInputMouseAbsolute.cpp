#include "DInputMouseAbsolute.h"
#include "VideoCommon/OnScreenDisplay.h"

#include "Core/Host.h"
#include "Core/ConfigManager.h"
#include "Core/Config/MainSettings.h"
#include "Common/Config/Config.h"

int win_w = 0, win_h = 0;

namespace prime
{
void InitMouse(IDirectInput8* const idi8)
{
  LPDIRECTINPUTDEVICE8 mo_device = nullptr;

  if (SUCCEEDED(idi8->CreateDevice(GUID_SysMouse, &mo_device, nullptr)) &&
      SUCCEEDED(mo_device->SetDataFormat(&c_dfDIMouse2)) &&
      SUCCEEDED(mo_device->SetCooperativeLevel(nullptr, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE)))
  {
    {
      // Setting this property causes directinput to give the absolute coordinates so we can
      // retrieve more accurate representations of inputs
      DIPROPDWORD dipdw;
      dipdw.diph.dwSize = sizeof(DIPROPDWORD);
      dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
      dipdw.diph.dwObj = 0;
      dipdw.diph.dwHow = DIPH_DEVICE;
      dipdw.dwData = DIPROPAXISMODE_ABS;
      mo_device->SetProperty(DIPROP_AXISMODE, &dipdw.diph);
      auto mouse_input = new DInputMouse();
      mouse_input->Init(mo_device);
      g_mouse_input = mouse_input;
      return;
    }
  }

  if (mo_device)
  {
    mo_device->Release();
  }
}

DInputMouse::DInputMouse()
{
  dx = dy = 0;
  cursor_locked = false;
  m_mo_device = nullptr;
}

void DInputMouse::Init(LPDIRECTINPUTDEVICE8 mo_device)
{
  m_mo_device = mo_device;
  m_mo_device->Acquire();
  // we don't need to check device caps
}

void DInputMouse::UpdateInput()
{
  // safeguard
  if (m_mo_device == nullptr || !Config::Get(Config::PRIMEHACK_ENABLE))
    return;

  DIMOUSESTATE2 input_temp;
  ULONGLONG cur_time = GetTickCount64();

  // Throw out inputs that are too out-of-date
  if (cur_time - last_update > 250)
  {
    m_mo_device->GetDeviceState(sizeof(DIMOUSESTATE2), &state_prev);
  }
  last_update = cur_time;

  // Retrieve input data from our device
  HRESULT hr = m_mo_device->GetDeviceState(sizeof(DIMOUSESTATE2), &input_temp);

  // ensure the device has been acquired
  if (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED)
  {
    m_mo_device->Acquire();
  }

  // Only process inputs when the cursor is locked
  if (SUCCEEDED(hr) && cursor_locked)
  {
    dx += input_temp.lX - state_prev.lX;
    dy += input_temp.lY - state_prev.lY;

    state_prev = input_temp;
  }
}
}  // namespace prime
