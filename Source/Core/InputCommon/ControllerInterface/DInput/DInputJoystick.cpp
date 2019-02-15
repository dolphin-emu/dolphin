// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <map>
#include <sstream>

#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/ControllerInterface/DInput/DInput.h"
#include "InputCommon/ControllerInterface/DInput/DInputJoystick.h"
#include "InputCommon/ControllerInterface/DInput/XInputFilter.h"

namespace ciface
{
namespace DInput
{
#define DATA_BUFFER_SIZE 32

void InitJoystick(IDirectInput8* const idi8, HWND hwnd)
{
  std::list<DIDEVICEINSTANCE> joysticks;
  idi8->EnumDevices(DI8DEVCLASS_GAMECTRL, DIEnumDevicesCallback, (LPVOID)&joysticks,
                    DIEDFL_ATTACHEDONLY);

  std::unordered_set<DWORD> xinput_guids = GetXInputGUIDS();
  for (DIDEVICEINSTANCE& joystick : joysticks)
  {
    // skip XInput Devices
    if (xinput_guids.count(joystick.guidProduct.Data1))
    {
      continue;
    }

    LPDIRECTINPUTDEVICE8 js_device;
    if (SUCCEEDED(idi8->CreateDevice(joystick.guidInstance, &js_device, nullptr)))
    {
      if (SUCCEEDED(js_device->SetDataFormat(&c_dfDIJoystick)))
      {
        if (FAILED(js_device->SetCooperativeLevel(GetAncestor(hwnd, GA_ROOT),
                                                  DISCL_BACKGROUND | DISCL_EXCLUSIVE)))
        {
          // PanicAlert("SetCooperativeLevel(DISCL_EXCLUSIVE) failed!");
          // fall back to non-exclusive mode, with no rumble
          if (FAILED(
                  js_device->SetCooperativeLevel(nullptr, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE)))
          {
            // PanicAlert("SetCooperativeLevel failed!");
            js_device->Release();
            continue;
          }
        }

        auto js = std::make_shared<Joystick>(js_device);
        // only add if it has some inputs/outputs
        if (js->Inputs().size() || js->Outputs().size())
          g_controller_interface.AddDevice(std::move(js));
      }
      else
      {
        // PanicAlert("SetDataFormat failed!");
        js_device->Release();
      }
    }
  }
}

Joystick::Joystick(/*const LPCDIDEVICEINSTANCE lpddi, */ const LPDIRECTINPUTDEVICE8 device)
    : m_device(device)
//, m_name(TStringToString(lpddi->tszInstanceName))
{
  // seems this needs to be done before GetCapabilities
  // polled or buffered data
  DIPROPDWORD dipdw;
  dipdw.diph.dwSize = sizeof(DIPROPDWORD);
  dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
  dipdw.diph.dwObj = 0;
  dipdw.diph.dwHow = DIPH_DEVICE;
  dipdw.dwData = DATA_BUFFER_SIZE;
  // set the buffer size,
  // if we can't set the property, we can't use buffered data
  m_buffered = SUCCEEDED(m_device->SetProperty(DIPROP_BUFFERSIZE, &dipdw.diph));

  // seems this needs to be done after SetProperty of buffer size
  m_device->Acquire();

  // get joystick caps
  DIDEVCAPS js_caps;
  js_caps.dwSize = sizeof(js_caps);
  if (FAILED(m_device->GetCapabilities(&js_caps)))
    return;

  // max of 32 buttons and 4 hats / the limit of the data format I am using
  js_caps.dwButtons = std::min((DWORD)32, js_caps.dwButtons);
  js_caps.dwPOVs = std::min((DWORD)4, js_caps.dwPOVs);

  // m_must_poll = (js_caps.dwFlags & DIDC_POLLEDDATAFORMAT) != 0;

  // buttons
  for (u8 i = 0; i != js_caps.dwButtons; ++i)
    AddInput(new Button(i, m_state_in.rgbButtons[i]));

  // hats
  for (u8 i = 0; i != js_caps.dwPOVs; ++i)
  {
    // each hat gets 4 input instances associated with it, (up down left right)
    for (u8 d = 0; d != 4; ++d)
      AddInput(new Hat(i, m_state_in.rgdwPOV[i], d));
  }

  // get up to 6 axes and 2 sliders
  DIPROPRANGE range;
  range.diph.dwSize = sizeof(range);
  range.diph.dwHeaderSize = sizeof(range.diph);
  range.diph.dwHow = DIPH_BYOFFSET;
  // screw EnumObjects, just go through all the axis offsets and try to GetProperty
  // this should be more foolproof, less code, and probably faster
  for (unsigned int offset = 0; offset < DIJOFS_BUTTON(0) / sizeof(LONG); ++offset)
  {
    range.diph.dwObj = offset * sizeof(LONG);
    // try to set some nice power of 2 values (128) to match the GameCube controls
    range.lMin = -(1 << 7);
    range.lMax = (1 << 7);
    m_device->SetProperty(DIPROP_RANGE, &range.diph);
    // but I guess not all devices support setting range
    // so I getproperty right afterward incase it didn't set.
    // This also checks that the axis is present
    if (SUCCEEDED(m_device->GetProperty(DIPROP_RANGE, &range.diph)))
    {
      const LONG base = (range.lMin + range.lMax) / 2;
      const LONG& ax = (&m_state_in.lX)[offset];

      // each axis gets a negative and a positive input instance associated with it
      AddAnalogInputs(new Axis(offset, ax, base, range.lMin - base),
                      new Axis(offset, ax, base, range.lMax - base));
    }
  }

  // force feedback
  std::list<DIDEVICEOBJECTINSTANCE> objects;
  if (SUCCEEDED(m_device->EnumObjects(DIEnumDeviceObjectsCallback, (LPVOID)&objects, DIDFT_AXIS)))
  {
    InitForceFeedback(m_device, (int)objects.size());
  }

  ZeroMemory(&m_state_in, sizeof(m_state_in));
  // set hats to center
  memset(m_state_in.rgdwPOV, 0xFF, sizeof(m_state_in.rgdwPOV));
}

Joystick::~Joystick()
{
  m_device->Unacquire();
  m_device->Release();
}

std::string Joystick::GetName() const
{
  return GetDeviceName(m_device);
}

std::string Joystick::GetSource() const
{
  return DINPUT_SOURCE_NAME;
}

// update IO

void Joystick::UpdateInput()
{
  HRESULT hr = 0;

  // just always poll,
  // MSDN says if this isn't needed it doesn't do anything
  m_device->Poll();

  if (m_buffered)
  {
    DIDEVICEOBJECTDATA evtbuf[DATA_BUFFER_SIZE];
    DWORD numevents = DATA_BUFFER_SIZE;
    hr = m_device->GetDeviceData(sizeof(*evtbuf), evtbuf, &numevents, 0);

    if (SUCCEEDED(hr))
    {
      for (LPDIDEVICEOBJECTDATA evt = evtbuf; evt != (evtbuf + numevents); ++evt)
      {
        // all the buttons are at the end of the data format
        // they are bytes rather than longs
        if (evt->dwOfs < DIJOFS_BUTTON(0))
          *(DWORD*)(((BYTE*)&m_state_in) + evt->dwOfs) = evt->dwData;
        else
          ((BYTE*)&m_state_in)[evt->dwOfs] = (BYTE)evt->dwData;
      }

      // seems like this needs to be done maybe...
      if (DI_BUFFEROVERFLOW == hr)
        hr = m_device->GetDeviceState(sizeof(m_state_in), &m_state_in);
    }
  }
  else
  {
    hr = m_device->GetDeviceState(sizeof(m_state_in), &m_state_in);
  }

  // try reacquire if input lost
  if (DIERR_INPUTLOST == hr || DIERR_NOTACQUIRED == hr)
    m_device->Acquire();
}

// get name

std::string Joystick::Button::GetName() const
{
  std::ostringstream ss;
  ss << "Button " << (int)m_index;
  return ss.str();
}

std::string Joystick::Axis::GetName() const
{
  std::ostringstream ss;
  // axis
  if (m_index < 6)
  {
    ss << "Axis " << (char)('X' + (m_index % 3));
    if (m_index > 2)
      ss << 'r';
  }
  // slider
  else
  {
    ss << "Slider " << (int)(m_index - 6);
  }

  ss << (m_range < 0 ? '-' : '+');
  return ss.str();
}

std::string Joystick::Hat::GetName() const
{
  static char tmpstr[] = "Hat . .";
  tmpstr[4] = (char)('0' + m_index);
  tmpstr[6] = "NESW"[m_direction];
  return tmpstr;
}

// get / set state

ControlState Joystick::Axis::GetState() const
{
  return std::max(0.0, ControlState(m_axis - m_base) / m_range);
}

ControlState Joystick::Button::GetState() const
{
  return ControlState(m_button > 0);
}

ControlState Joystick::Hat::GetState() const
{
  // can this func be simplified ?
  // hat centered code from MSDN
  if (0xFFFF == LOWORD(m_hat))
    return 0;

  return (abs((int)(m_hat / 4500 - m_direction * 2 + 8) % 8 - 4) > 2);
}
}
}
