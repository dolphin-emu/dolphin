// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <limits>
#include <set>
#include <sstream>
#include <type_traits>

#include "Common/Logging/Log.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/ControllerInterface/DInput/DInput.h"
#include "InputCommon/ControllerInterface/DInput/DInputJoystick.h"
#include "InputCommon/ControllerInterface/DInput/XInputFilter.h"

namespace ciface::DInput
{
constexpr DWORD DATA_BUFFER_SIZE = 32;

struct GUIDComparator
{
  bool operator()(const GUID& left, const GUID& right) const
  {
    static_assert(std::is_trivially_copyable_v<GUID>);

    return memcmp(&left, &right, sizeof(left)) < 0;
  }
};

static std::set<GUID, GUIDComparator> s_guids_in_use;

void InitJoystick(IDirectInput8* const idi8, HWND hwnd)
{
  std::list<DIDEVICEINSTANCE> joysticks;
  idi8->EnumDevices(DI8DEVCLASS_GAMECTRL, DIEnumDevicesCallback, (LPVOID)&joysticks,
                    DIEDFL_ATTACHEDONLY);

  std::unordered_set<DWORD> xinput_guids = GetXInputGUIDS();
  for (DIDEVICEINSTANCE& joystick : joysticks)
  {
    // Skip XInput Devices
    if (xinput_guids.count(joystick.guidProduct.Data1))
    {
      continue;
    }

    // Skip devices we are already using.
    if (s_guids_in_use.count(joystick.guidInstance))
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
          WARN_LOG(
              PAD,
              "DInput: Failed to acquire device exclusively. Force feedback will be unavailable.");
          // Fall back to non-exclusive mode, with no rumble
          if (FAILED(
                  js_device->SetCooperativeLevel(nullptr, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE)))
          {
            // PanicAlert("SetCooperativeLevel failed!");
            js_device->Release();
            continue;
          }
        }

        s_guids_in_use.insert(joystick.guidInstance);
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
    // Try to set a range with 16 bits of precision:
    range.lMin = std::numeric_limits<s16>::min();
    range.lMax = std::numeric_limits<s16>::max();
    m_device->SetProperty(DIPROP_RANGE, &range.diph);
    // Not all devices support setting DIPROP_RANGE so we must GetProperty right back.
    // This also checks that the axis is present.
    // Note: Even though not all devices support setting DIPROP_RANGE, some require it.
    if (SUCCEEDED(m_device->GetProperty(DIPROP_RANGE, &range.diph)))
    {
      const LONG base = (range.lMin + range.lMax) / 2;
      const LONG& ax = (&m_state_in.lX)[offset];

      // each axis gets a negative and a positive input instance associated with it
      AddAnalogInputs(new Axis(offset, ax, base, range.lMin - base),
                      new Axis(offset, ax, base, range.lMax - base));
    }
  }

  // Force feedback:
  std::list<DIDEVICEOBJECTINSTANCE> objects;
  if (SUCCEEDED(m_device->EnumObjects(DIEnumDeviceObjectsCallback, (LPVOID)&objects, DIDFT_AXIS)))
  {
    const int num_ff_axes =
        std::count_if(std::begin(objects), std::end(objects), [](DIDEVICEOBJECTINSTANCE& pdidoi) {
          return pdidoi.dwFlags && DIDOI_FFACTUATOR;
        });
    InitForceFeedback(m_device, num_ff_axes);
  }

  // Zero inputs:
  m_state_in = {};

  // Set hats to center:
  // "The center position is normally reported as -1" -MSDN
  std::fill(std::begin(m_state_in.rgdwPOV), std::end(m_state_in.rgdwPOV), -1);
}

Joystick::~Joystick()
{
  DIDEVICEINSTANCE info = {};
  info.dwSize = sizeof(info);
  if (SUCCEEDED(m_device->GetDeviceInfo(&info)))
  {
    s_guids_in_use.erase(info.guidInstance);
  }
  else
  {
    ERROR_LOG(PAD, "DInputJoystick: GetDeviceInfo failed.");
  }

  DeInitForceFeedback();

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

bool Joystick::IsValid() const
{
  return SUCCEEDED(m_device->Acquire());
}

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
  return ControlState(m_axis - m_base) / m_range;
}

ControlState Joystick::Button::GetState() const
{
  return ControlState(m_button > 0);
}

ControlState Joystick::Hat::GetState() const
{
  // "Some drivers report the centered position of the POV indicator as 65,535.
  // Determine whether the indicator is centered as follows" -MSDN
  const bool is_centered = (0xffff == LOWORD(m_hat));

  if (is_centered)
    return 0;

  return (abs((int)(m_hat / 4500 - m_direction * 2 + 8) % 8 - 4) > 2);
}
}  // namespace ciface::DInput
