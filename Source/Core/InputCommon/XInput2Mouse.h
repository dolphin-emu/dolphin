#pragma once

extern "C" {
#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>
#include <X11/keysym.h>
}

#include <string>

#include "InputCommon/GenericMouse.h"

namespace prime
{

bool InitXInput2Mouse(void* const hwnd);

class XInput2Mouse : public GenericMouse
{
public:
  XInput2Mouse(Window hwnd, int opcode, int pointer);

  void UpdateInput() override;
  void LockCursorToGameWindow() override;

private:

  void SelectEventsForDevice(Window root_window, XIEventMask* mask, int deviceid);
  const int pointer_deviceid;
  int xi_opcode;

  std::string name;
  Window window;
  Display* display;
};

}
