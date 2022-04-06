// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/ControllerInterface/DInput/DInputKeyboardMouse.h"

#include <algorithm>

#include "Common/Logging/Log.h"
#include "Core/Core.h"

#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/ControllerInterface/DInput/DInput.h"

// (lower would be more sensitive) user can lower sensitivity by setting range
// seems decent here ( at 8 ), I don't think anyone would need more sensitive than this
// and user can lower it much farther than they would want to with the range
#define MOUSE_AXIS_SENSITIVITY 8

// if input hasn't been received for this many ms, mouse input will be skipped
// otherwise it is just some crazy value
#define DROP_INPUT_TIME 250

namespace ciface::DInput
{
extern double cursor_sensitivity = 15.0;
extern unsigned char center_mouse_key = 'K';
extern double snapping_distance = 4.5;
class RelativeMouseAxis final : public Core::Device::RelativeInput
{
public:
  std::string GetName() const override
  {
    return fmt::format("RelativeMouse {}{}", char('X' + m_index), (m_scale > 0) ? '+' : '-');
  }

  RelativeMouseAxis(u8 index, bool positive, const RelativeMouseState* state)
      : m_state(*state), m_index(index), m_scale(positive * 2 - 1)
  {
  }

  ControlState GetState() const override
  {
    return ControlState(m_state.GetValue().data[m_index] * m_scale);
  }

private:
  const RelativeMouseState& m_state;
  const u8 m_index;
  const s8 m_scale;
};

static const struct
{
  const BYTE code;
  const char* const name;
} named_keys[] = {
#include "InputCommon/ControllerInterface/DInput/NamedKeys.h"  // NOLINT
};

// Prevent duplicate keyboard/mouse devices. Modified by more threads.
static bool s_keyboard_mouse_exists;
static HWND s_hwnd;

void InitKeyboardMouse(IDirectInput8* const idi8, HWND hwnd)
{
  if (s_keyboard_mouse_exists)
    return;

  s_hwnd = hwnd;

  // Mouse and keyboard are a combined device, to allow shift+click and stuff
  // if that's dumb, I will make a VirtualDevice class that just uses ranges of inputs/outputs from
  // other devices
  // so there can be a separated Keyboard and mouse, as well as combined KeyboardMouse

  LPDIRECTINPUTDEVICE8 kb_device = nullptr;
  LPDIRECTINPUTDEVICE8 mo_device = nullptr;

  // These are "virtual" system devices, so they are always there even if we have no physical
  // mouse and keyboard plugged into the computer
  if (SUCCEEDED(idi8->CreateDevice(GUID_SysKeyboard, &kb_device, nullptr)) &&
      SUCCEEDED(kb_device->SetDataFormat(&c_dfDIKeyboard)) &&
      SUCCEEDED(kb_device->SetCooperativeLevel(nullptr, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE)) &&
      SUCCEEDED(idi8->CreateDevice(GUID_SysMouse, &mo_device, nullptr)) &&
      SUCCEEDED(mo_device->SetDataFormat(&c_dfDIMouse2)) &&
      SUCCEEDED(mo_device->SetCooperativeLevel(nullptr, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE)))
  {
    g_controller_interface.AddDevice(std::make_shared<KeyboardMouse>(kb_device, mo_device));
    return;
  }

  ERROR_LOG_FMT(CONTROLLERINTERFACE, "KeyboardMouse device failed to be created");

  if (kb_device)
    kb_device->Release();
  if (mo_device)
    mo_device->Release();
}

void SetKeyboardMouseWindow(HWND hwnd)
{
  s_hwnd = hwnd;
}

KeyboardMouse::~KeyboardMouse()
{
  s_keyboard_mouse_exists = false;

  // Independently of the order in which we do these, if we put a breakpoint on Unacquire() (or in
  // any place in the call stack before this), when refreshing devices from the UI, on the second
  // attempt, it will get stuck in an infinite (while) loop inside dinput8.dll. Given that it can't
  // be otherwise be reproduced (not even with sleeps), we can just ignore the problem.

  // kb
  m_kb_device->Unacquire();
  m_kb_device->Release();
  // mouse
  m_mo_device->Unacquire();
  m_mo_device->Release();
}

KeyboardMouse::KeyboardMouse(const LPDIRECTINPUTDEVICE8 kb_device,
                             const LPDIRECTINPUTDEVICE8 mo_device)
    : m_kb_device(kb_device), m_mo_device(mo_device), m_last_update(GetTickCount()), current_state()
{
  s_keyboard_mouse_exists = true;

  if (FAILED(m_kb_device->Acquire()))
    WARN_LOG_FMT(CONTROLLERINTERFACE, "Keyboard device failed to acquire. We'll retry later");
  if (FAILED(m_mo_device->Acquire()))
    WARN_LOG_FMT(CONTROLLERINTERFACE, "Mouse device failed to acquire. We'll retry later");

  // KEYBOARD
  // add keys
  for (u8 i = 0; i < sizeof(named_keys) / sizeof(*named_keys); ++i)
    AddInput(new Key(i, current_state.keyboard[named_keys[i].code]));

  // Add combined left/right modifiers with consistent naming across platforms.
  AddCombinedInput("Alt", {"LMENU", "RMENU"});
  AddCombinedInput("Shift", {"LSHIFT", "RSHIFT"});
  AddCombinedInput("Ctrl", {"LCONTROL", "RCONTROL"});

  // MOUSE
  DIDEVCAPS mouse_caps = {};
  mouse_caps.dwSize = sizeof(mouse_caps);
  m_mo_device->GetCapabilities(&mouse_caps);
  // mouse buttons
  for (u8 i = 0; i < mouse_caps.dwButtons; ++i)
    AddInput(new Button(i, current_state.mouse.rgbButtons[i]));
  // mouse axes
  for (unsigned int i = 0; i < mouse_caps.dwAxes; ++i)
  {
    const LONG& ax = (&current_state.mouse.lX)[i];

    // each axis gets a negative and a positive input instance associated with it
    AddInput(new Axis(i, ax, (2 == i) ? -1 : -MOUSE_AXIS_SENSITIVITY));
    AddInput(new Axis(i, ax, -(2 == i) ? 1 : MOUSE_AXIS_SENSITIVITY));
  }

  //  Cursor Position
  //  X input (0 index_of_closest_octagon_point (first value in cursor construction), as far as I
  //  can tell) (false means the direction is negative) Left (-X) (Negative Direction)
  AddInput(new Cursor(0, current_state.cursor.x, false));
  // Right (+X) (Positive Direction)
  AddInput(new Cursor(0, current_state.cursor.x, true));

  // Y input (1 index_of_closest_octagon_point (first value in cursor construction), as far as I can
  // tell) (false means the direction is negative) Up (-Y) (Negative Direction)
  AddInput(new Cursor(1, current_state.cursor.y, false));
  // Down (+Y) (Positive Direction)
  AddInput(new Cursor(1, current_state.cursor.y, true));
  // Raw relative mouse movement.
  for (unsigned int i = 0; i != mouse_caps.dwAxes; ++i)
  {
    AddInput(new RelativeMouseAxis(i, false, &current_state.relative_mouse));
    AddInput(new RelativeMouseAxis(i, true, &current_state.relative_mouse));
  }
}

void KeyboardMouse::Generate_Octagon_Points(POINT octagon_points[8])
{
  // the enum which addresses the octagon_points is in the header if you need the actual value of
  // the enum
  double fraction_of_screen_to_lock_mouse_in_x = screen_width / (cursor_sensitivity * screen_ratio);
  double fraction_of_screen_to_lock_mouse_in_y = screen_height / cursor_sensitivity;
  // I did my best to make sure this as close to the actual gamecube octagonal gates using desmos,
  // but there definitely could be a mathmatical way to make these better
  //
  // Update: I asked on the GCC modder discord and someone said 0.7 and my personal eyeballing was
  // 0.75 so I am inclined to believe them.
  constexpr double percentage_reduction_for_corner_gates = 0.7;

  // Southernmost point is cardinal direction and we want to make sure we're inputting the maximum
  // value in the negative Y direction so we floor the calculation to make sure the point the mouse
  // gets held in is the max-value cardinal direction. erring on the side of too far down instead of
  // not far enough. This will be repeated for the cardinal directions.
  octagon_points[NORTH] =
      POINT{center_of_screen.x,
            static_cast<long>(floor(center_of_screen.y - fraction_of_screen_to_lock_mouse_in_y))};

  octagon_points[NORTH_EAST] =
      POINT{static_cast<long>(center_of_screen.x + (fraction_of_screen_to_lock_mouse_in_x *
                                                    percentage_reduction_for_corner_gates)),
            static_cast<long>(center_of_screen.y - (fraction_of_screen_to_lock_mouse_in_y *
                                                    percentage_reduction_for_corner_gates))};

  octagon_points[EAST] =
      POINT{static_cast<long>(ceil(center_of_screen.x + fraction_of_screen_to_lock_mouse_in_x)),
            center_of_screen.y};

  octagon_points[SOUTH_EAST] =
      POINT{static_cast<long>(center_of_screen.x + (fraction_of_screen_to_lock_mouse_in_x *
                                                    percentage_reduction_for_corner_gates)),
            static_cast<long>(center_of_screen.y + (fraction_of_screen_to_lock_mouse_in_y *
                                                    percentage_reduction_for_corner_gates))};

  octagon_points[SOUTH] =
      POINT{center_of_screen.x,
            static_cast<long>(ceil(center_of_screen.y + fraction_of_screen_to_lock_mouse_in_y))};

  octagon_points[SOUTH_WEST] =
      POINT{static_cast<long>(center_of_screen.x - (fraction_of_screen_to_lock_mouse_in_x *
                                                    percentage_reduction_for_corner_gates)),
            static_cast<long>(center_of_screen.y + (fraction_of_screen_to_lock_mouse_in_y *
                                                    percentage_reduction_for_corner_gates))};

  octagon_points[WEST] =
      POINT{static_cast<long>(floor(center_of_screen.x - fraction_of_screen_to_lock_mouse_in_x)),
            center_of_screen.y};

  octagon_points[NORTH_WEST] =
      POINT{static_cast<long>(center_of_screen.x - (fraction_of_screen_to_lock_mouse_in_x *
                                                    percentage_reduction_for_corner_gates)),
            static_cast<long>(center_of_screen.y - (fraction_of_screen_to_lock_mouse_in_y *
                                                    percentage_reduction_for_corner_gates))};
}

bool KeyboardMouse::Point_Is_Inside_Octagon(const POINT& mouse_point, const POINT octagon_points[8])
{
  // This function uses the raycasting method to figure out if it's inside the octagon
  // casting a ray horizontally in one direction from the mouse point and if the
  // casted ray intersects with the edges of the octagon an even number of times (0 counts)
  // the point is outside of the octagon. If the ray intersects an odd number of times, the
  // ray is inside of the polygon. Jordan Curve Thereom is the name I was told
  // https://erich.realtimerendering.com/ptinpoly/
  unsigned int number_of_intersections = 0;

  for (int i = 0; i < 8; i++)
  {
    int next_index = i + 1;
    if (i == 7)
      next_index = 0;

    double rise = static_cast<double>(octagon_points[next_index].y) -
                  static_cast<double>(octagon_points[i].y);
    double run = static_cast<double>(octagon_points[next_index].x) -
                 static_cast<double>(octagon_points[i].x);
    double slope = rise / run;

    // double temp_1 = mouse_point.y - octagon_points[i].y;
    // double temp_2 = temp_1 / slope;
    // double temp_3 = temp_2 + octagon_points[i].x;

    double temp_x = ((mouse_point.y - octagon_points[i].y) / slope) + octagon_points[i].x;

    if (temp_x < mouse_point.x)
    {
      continue;
    }

    if (i < 2 || i > 5)
    {
      if (temp_x >= octagon_points[i].x && temp_x < octagon_points[next_index].x)
      {
        number_of_intersections++;
      }
    }
    else if (i >= 2 && i <= 5)
    {
      if (temp_x <= octagon_points[i].x && temp_x > octagon_points[next_index].x)
      {
        number_of_intersections++;
      }
    }
  }

  if (number_of_intersections % 2 == 0)
  {
    return false;
  }
  else
  {
    return true;
  }
}

double KeyboardMouse::Calculate_Distance_between_Points(const POINT& first_point,
                                                        const POINT& second_point)
{
  // Pythagorean theoreom used to calculate the distance between points
  // the hypotenuse of the right triangle is the distance.
  // the x_difference and y_difference are the two legs of the triangle.
  double x_difference = first_point.x - second_point.x;
  double y_difference = first_point.y - second_point.y;

  return sqrt((x_difference * x_difference) + (y_difference * y_difference));
}

long KeyboardMouse::Find_Second_Line_Point(const POINT& mouse_point, const POINT octagon_points[8],
                                           long index_of_min_octagon_point)
{
  if (mouse_point.y < octagon_points[index_of_min_octagon_point].y)
  {
    switch (index_of_min_octagon_point)
    {
    case SOUTH:

      break;
    case SOUTH_EAST:

      break;
    case EAST:
      return NORTH_EAST;
      break;
    case NORTH_EAST:

      break;
    case NORTH:

      break;
    case NORTH_WEST:

      break;
    case WEST:
      return NORTH_WEST;
      break;
    case SOUTH_WEST:

      break;
    }
  }
  if (mouse_point.x < octagon_points[index_of_min_octagon_point].x)
  {
    switch (index_of_min_octagon_point)
    {
    case SOUTH:
      return SOUTH_WEST;
      break;
    case SOUTH_EAST:
      return SOUTH;
      break;
    case EAST:

      break;
    case NORTH_EAST:
      return NORTH;
      break;
    case NORTH:
      return NORTH_WEST;
      break;
    case NORTH_WEST:
      return WEST;
      break;
    case WEST:

      break;
    case SOUTH_WEST:
      return WEST;
      break;
    }
  }
  if (mouse_point.y > octagon_points[index_of_min_octagon_point].y)
  {
    switch (index_of_min_octagon_point)
    {
    case SOUTH:

      break;
    case SOUTH_EAST:

      break;
    case EAST:
      return SOUTH_EAST;
      break;
    case NORTH_EAST:

      break;
    case NORTH:

      break;
    case NORTH_WEST:

      break;
    case WEST:
      return SOUTH_WEST;
      break;
    case SOUTH_WEST:

      break;
    }
  }
  if (mouse_point.x > octagon_points[index_of_min_octagon_point].x)
  {
    switch (index_of_min_octagon_point)
    {
    case SOUTH:
      return SOUTH_EAST;
      break;
    case SOUTH_EAST:
      return EAST;
      break;
    case EAST:

      break;
    case NORTH_EAST:
      return EAST;
      break;
    case NORTH:
      return NORTH_EAST;
      break;
    case NORTH_WEST:
      return NORTH;
      break;
    case WEST:

      break;
    case SOUTH_WEST:
      return SOUTH;
      break;
    }
  }
  return 0;
}

void KeyboardMouse::Move_Mouse_Point_Along_Gate(POINT& mouse_point, const POINT octagon_points[8])
{
  // Sage 4/2/2022: Gameplan is to move the mouse towards the nearest gate if the distance the mouse
  // is outside of the octagon is large enough to overcome friction.The amount the mouse will move
  // towards the gate will be the distance outside of the octagon minus friction. This is to
  // simulate the force required to get a real stick into gates while keeping the ability to hit the
  // values in between gates consistently. If the distance outside of the octagon is not large
  // enough to overcome friction, the mouse will get moved to the closest point inside of the
  // octagon. This is almost always going to be directly on the line between octagon points. I may
  // need to bring the mouse a tiny bit inside of the octagon instead of directly on the nearest
  // line if the section of the code that determines whether or not the mouse is inside of the
  // octagon struggles with the mouse_point being directly on the line.
  long distance_to_octagon_points[8] = {0};
  for (int i = 0; i < 8; i++)
  {
    distance_to_octagon_points[i] =
        Calculate_Distance_between_Points(mouse_point, octagon_points[i]);
  }

  long min_distance = distance_to_octagon_points[0];
  long index_of_closest_octagon_point = 0;
  for (int i = 0; i < 8; i++)
  {
    if (distance_to_octagon_points[i] < min_distance)
    {
      min_distance = distance_to_octagon_points[i];
      index_of_closest_octagon_point = i;
    }
  }

  long index_of_second_line_point =
      Find_Second_Line_Point(mouse_point, octagon_points, index_of_closest_octagon_point);

  double rise = static_cast<double>(octagon_points[index_of_second_line_point].y) -
                static_cast<double>(octagon_points[index_of_closest_octagon_point].y);
  double run = static_cast<double>(octagon_points[index_of_second_line_point].x) -
               static_cast<double>(octagon_points[index_of_closest_octagon_point].x);
  double slope_of_octagon_line = rise / run;
  double y_intercept_of_octagon_line =
      slope_of_octagon_line * (0 - octagon_points[index_of_closest_octagon_point].x) +
      octagon_points[index_of_closest_octagon_point].y;

  rise = static_cast<double>(center_of_screen.y) - static_cast<double>(mouse_point.y);
  run = static_cast<double>(center_of_screen.x) - static_cast<double>(mouse_point.x);
  double slope_of_line_from_center_to_mouse = rise / run;
  if (abs(run) < 1 /*Magic number is range around 0 where we just say the slope is zero*/)
  {
    if (mouse_point.y < center_of_screen.y)
    {
      mouse_point = octagon_points[NORTH];
    }
    if (mouse_point.y > center_of_screen.y)
    {
      mouse_point = octagon_points[SOUTH];
    }
    return;
  }
  double y_intercept_of_line_from_center_to_mouse =
      slope_of_line_from_center_to_mouse * (0 - center_of_screen.x) + center_of_screen.y;

  POINT point_of_intersection = {0};

  point_of_intersection.x = static_cast<long>(
      round(((y_intercept_of_octagon_line - y_intercept_of_line_from_center_to_mouse) /
             (slope_of_line_from_center_to_mouse - slope_of_octagon_line))));
  point_of_intersection.y = static_cast<long>(
      round((slope_of_octagon_line * point_of_intersection.x) + y_intercept_of_octagon_line));

  if (Calculate_Distance_between_Points(
          mouse_point, octagon_points[index_of_closest_octagon_point]) < snapping_distance)
  {
    point_of_intersection = octagon_points[index_of_closest_octagon_point];
  }

  mouse_point = point_of_intersection;
}

void KeyboardMouse::Lock_Mouse_In_Jail(POINT& mouse_point)
{
  // Sage 3/30/2022: Locks the mouse into an octagon (like the case on a real controller)
  //
  //				  The plan to make the octagonal locking happen is to rotate the point so
  //				  four octagon edges mark the top, bottom, and sides of the octagon
  //				  (try to make one of the flat edges on a real controller the bottom instead of the
  //				  notch). Then the octagon is just a square with its corners cut off. So, we generate a
  //square 				  and cut the corners of the square off in order to make the octagon. Then we check if 				  the
  //point is in those corners that were cut off and if it is, we move it towards the center 				  until it
  //is inside the octagon.

  POINT octagon_points[8] = {0};

  Generate_Octagon_Points(octagon_points);

  if (Point_Is_Inside_Octagon(mouse_point, octagon_points))
  {
    return;
  }
  else
  {
    Move_Mouse_Point_Along_Gate(mouse_point, octagon_points);
  }

   /* //Sage 3 / 30 / 2022 : Locks the mouse into a square
   //Sage 4/3/2022: Please do not remove this code. It is an earlier version of the mouse jail
   //and I'd like it here if I ever want to go back.

   double fraction_of_screen_to_lock_mouse_in_x = screen_width / (cursor_sensitivity *
   screen_ratio); double fraction_of_screen_to_lock_mouse_in_y = screen_height /
   cursor_sensitivity ;
  // bind x value of mouse pos to within a fraction of the screen from center
   if (mouse_point.x > (center_of_screen.x + fraction_of_screen_to_lock_mouse_in_x))
  	mouse_point.x = static_cast<long>(center_of_screen.x + fraction_of_screen_to_lock_mouse_in_x);

   if (mouse_point.x < (center_of_screen.x - fraction_of_screen_to_lock_mouse_in_x))
  	mouse_point.x = static_cast<long>(center_of_screen.x - fraction_of_screen_to_lock_mouse_in_x);

  // bind y value of mouse pos to within a fraction of the screen from center
   if (mouse_point.y > (center_of_screen.y + fraction_of_screen_to_lock_mouse_in_y))
  	mouse_point.y = static_cast<long>(center_of_screen.y + fraction_of_screen_to_lock_mouse_in_y);

   if (mouse_point.y < (center_of_screen.y - fraction_of_screen_to_lock_mouse_in_y))
  	mouse_point.y = static_cast<long>(center_of_screen.y - fraction_of_screen_to_lock_mouse_in_y);*/
}

void KeyboardMouse::UpdateCursorInput()
{
  POINT temporary_point = {0, 0};
  GetCursorPos(&temporary_point);

  // Sage 3/20/2022: This was for debugging before I figured out how to get the main frame renderer
  // focus
  /*if (!(GetAsyncKeyState(release_mouse_from_screen_jail) & 0x8000)
    && ::Core::IsRunningAndStarted()
    && main_frame->RendererHasFocus())*/
  if (::Core::IsRunningAndStarted() && render_widget.hasFocus())
  {
    // Sage 3/20/2020: I hate the way this Show Cursor function works. This is the only function I
    // have ever actively despised

    ShowCursor(FALSE);  // decrement so I can acquire the actual value
    int show_cursor_number =
        ShowCursor(TRUE);  // acquire actual value on increment so the number is unchanged
    if (show_cursor_number >=
        0)  // check to see if the number is greater than or equal to 0 because the cursor is shown
            // if the number is greater than or equal to 0
      ShowCursor(FALSE);  // decrement the value so the cursor is actually invisible

    Lock_Mouse_In_Jail(temporary_point);

    SetCursorPos(temporary_point.x, temporary_point.y);
  }
  else
  {
    ShowCursor(FALSE);  // decrement so I can acquire the actual value
    int show_cursor_number =
        ShowCursor(TRUE);         // acquire actual value on increment so the number is unchanged
    if (show_cursor_number <= 0)  // check to see if the number is less than or equal to 0 because
                                  // the cursor is shown if the number is greater than or equal to 0
      ShowCursor(TRUE);  // decrement the value so the cursor is actually invisible
  }

  // See If Origin Reset Is Pressed
  if (player_requested_mouse_center ||
      (::Core::GetState() == ::Core::State::Uninitialized &&
       main_frame->RendererHasFocus()))  // Sage 3/20/2022: I don't think this works very well with
                                         // boot to pause, but it does work with normal boot
  {
    // Move cursor to the center of the screen if the origin reset key is pressed
    SetCursorPos(center_of_screen.x, center_of_screen.y);
    temporary_point.x = center_of_screen.x;
    temporary_point.y = center_of_screen.y;
  }

  // Sage 3/7/2022: Everything more than assigning point.x and point.y directly to the controller's
  // state is to normalize the coordinates since it seems like dolphin wants the inputs from -1.0
  // to 1.0
  current_state.cursor.x = ((ControlState)((temporary_point.x) / (ControlState)screen_width) - 0.5) *
       (cursor_sensitivity * screen_ratio);
  current_state.cursor.y = ((ControlState)((temporary_point.y) / (ControlState)screen_height) - 0.5) *
       cursor_sensitivity;
}

void KeyboardMouse::UpdateInput()
{
  UpdateCursorInput();

  DIMOUSESTATE2 tmp_mouse;

  // if mouse position hasn't been updated in a short while, skip a dev state
  //DWORD cur_time = GetTickCount();
  //if (cur_time - m_last_update > DROP_INPUT_TIME)
  //{
  //  // set axes to zero
  //  current_state.mouse = {};
  //  current_state.relative_mouse = {};

  //  // skip this input state
  //  m_mo_device->GetDeviceState(sizeof(tmp_mouse), &tmp_mouse);
  //}

 /* m_last_update = cur_time;*/

  HRESULT mo_hr = m_mo_device->GetDeviceState(sizeof(tmp_mouse), &tmp_mouse);
  if (DIERR_INPUTLOST == mo_hr || DIERR_NOTACQUIRED == mo_hr)
  {
    INFO_LOG_FMT(CONTROLLERINTERFACE, "Mouse device failed to get state");
    if (FAILED(m_mo_device->Acquire()))
      INFO_LOG_FMT(CONTROLLERINTERFACE, "Mouse device failed to re-acquire, we'll retry later");
  }
  else if (SUCCEEDED(mo_hr))
  {
    current_state.relative_mouse.Move({tmp_mouse.lX, tmp_mouse.lY, tmp_mouse.lZ});
    current_state.relative_mouse.Update();

    // need to smooth out the axes, otherwise it doesn't work for shit
    for (unsigned int i = 0; i < 3; ++i)
      ((&current_state.mouse.lX)[i] += (&tmp_mouse.lX)[i]) /= 2;

    // copy over the buttons
    std::copy_n(tmp_mouse.rgbButtons, std::size(tmp_mouse.rgbButtons), current_state.mouse.rgbButtons);
  }

  HRESULT kb_hr = m_kb_device->GetDeviceState(sizeof(current_state.keyboard), &current_state.keyboard);
  if (kb_hr == DIERR_INPUTLOST || kb_hr == DIERR_NOTACQUIRED)
  {
    INFO_LOG_FMT(CONTROLLERINTERFACE, "Keyboard device failed to get state");
    if (SUCCEEDED(m_kb_device->Acquire()))
      m_kb_device->GetDeviceState(sizeof(current_state.keyboard), &current_state.keyboard);
    else
      INFO_LOG_FMT(CONTROLLERINTERFACE, "Keyboard device failed to re-acquire, we'll retry later");
  }

  if (GetAsyncKeyState(center_mouse_key) && 0x8000)
    player_requested_mouse_center = true;
  else
    player_requested_mouse_center = false;
}

std::string KeyboardMouse::GetName() const
{
  return "Keyboard Mouse";
}

std::string KeyboardMouse::GetSource() const
{
  return DINPUT_SOURCE_NAME;
}

// Give this device a higher priority to make sure it shows first
int KeyboardMouse::GetSortPriority() const
{
  return 5;
}

bool KeyboardMouse::IsVirtualDevice() const
{
  return true;
}

// names
std::string KeyboardMouse::Key::GetName() const
{
  return named_keys[m_index].name;
}

std::string KeyboardMouse::Button::GetName() const
{
  return std::string("Click ") + char('0' + m_index);
}

std::string KeyboardMouse::Axis::GetName() const
{
  static char tmpstr[] = "Axis ..";
  tmpstr[5] = (char)('X' + m_index);
  tmpstr[6] = (m_range < 0 ? '-' : '+');
  return tmpstr;
}

std::string KeyboardMouse::Cursor::GetName() const
{
  static char tmpstr[] = "Cursor ..";
  tmpstr[7] = (char)('X' + m_index);
  tmpstr[8] = (m_positive ? '+' : '-');
  return tmpstr;
}

// get/set state
ControlState KeyboardMouse::Key::GetState() const
{
  return (m_key != 0);
}

ControlState KeyboardMouse::Button::GetState() const
{
  return (m_button != 0);
}

ControlState KeyboardMouse::Axis::GetState() const
{
  return ControlState(m_axis) / m_range;
}

ControlState KeyboardMouse::Cursor::GetState() const
{
  return m_axis / (m_positive ? 1.0 : -1.0);
}
}  // namespace ciface::DInput
