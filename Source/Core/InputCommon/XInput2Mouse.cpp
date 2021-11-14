#include "InputCommon/XInput2Mouse.h"

#include "Core/Host.h"
#include <cstring>
#include <math.h>

int win_w = 0, win_h = 0;

namespace prime
{
bool InitXInput2Mouse(void* const hwnd)
{
  Display* dpy = XOpenDisplay(nullptr);
  int xi_opcode, event, error;

  if (!XQueryExtension(dpy, "XInputExtension", &xi_opcode, &event, &error))
    return false;

  int major = 2, minor = 0;

  if (XIQueryVersion(dpy, &major, &minor) != Success)
    return false;

  XIDeviceInfo* all_masters;
  XIDeviceInfo* current_master;
  int num_masters;

  all_masters = XIQueryDevice(dpy, XIAllMasterDevices, &num_masters);

  if (num_masters < 1)
    return false;

  current_master = &all_masters[0];
  if (current_master->use == XIMasterPointer)
  {
    g_mouse_input = new XInput2Mouse((Window)hwnd, xi_opcode, current_master->deviceid);
  }

  XCloseDisplay(dpy);

  XIFreeDeviceInfo(all_masters);
  return true;
}

XInput2Mouse::XInput2Mouse(Window hwnd, int opcode, int pointer)
  : pointer_deviceid(pointer), xi_opcode(opcode), window(hwnd)
{
  display = XOpenDisplay(nullptr);

  int unused;
  XIDeviceInfo* pointer_device = XIQueryDevice(display, pointer_deviceid, &unused);
  name = std::string(pointer_device->name);
  XIFreeDeviceInfo(pointer_device);

  XIEventMask mask;
  unsigned char mask_buf[(XI_LASTEVENT + 7) / 8];
  mask.mask_len = sizeof(mask_buf);
  mask.mask = mask_buf;
  memset(mask_buf, 0, sizeof(mask_buf));

  XISetMask(mask_buf, XI_RawMotion);

  SelectEventsForDevice(DefaultRootWindow(display), &mask, pointer_deviceid);
}

void XInput2Mouse::UpdateInput()
{
  XFlush(display);
  float delta_x = 0.0f, delta_y = 0.0f;
  double delta_delta;
  XEvent event;
  while (XPending(display))
  {
    XNextEvent(display, &event);

    if (event.xcookie.type != GenericEvent)
      continue;
    if (event.xcookie.extension != xi_opcode)
      continue;
    if (!XGetEventData(display, &event.xcookie))
      continue;

    XIRawEvent* raw_event = (XIRawEvent*)event.xcookie.data;

    if (event.xcookie.evtype == XI_RawMotion)
    {
      if (XIMaskIsSet(raw_event->valuators.mask, 0))
      {
        delta_delta = raw_event->raw_values[0];
        if (delta_delta == delta_delta && 1 + delta_delta != delta_delta)
          delta_x += delta_delta;
      }
      if (XIMaskIsSet(raw_event->valuators.mask, 1))
      {
        delta_delta = raw_event->raw_values[1];
        if (delta_delta == delta_delta && 1 + delta_delta != delta_delta)
          delta_y += delta_delta;
      }
    }
    XFreeEventData(display, &event.xcookie);
  }

  if (cursor_locked)
  {
    this->dx += std::roundf(delta_x);
    this->dy += std::roundf(delta_y);
  }
  LockCursorToGameWindow();
}

void XInput2Mouse::SelectEventsForDevice(Window root_window, XIEventMask* mask, int deviceid)
{
  // Set the event mask for the master device.
  mask->deviceid = deviceid;
  XISelectEvents(display, root_window, mask, 1);

  // Query all the master device's slaves and set the same event mask for
  // those too. There are two reasons we want to do this. For mouse devices,
  // we want the raw motion events, and only slaves (i.e. physical hardware
  // devices) emit those. For keyboard devices, selecting slaves avoids
  // dealing with key focus.

  XIDeviceInfo* all_slaves;
  XIDeviceInfo* current_slave;
  int num_slaves;

  all_slaves = XIQueryDevice(display, XIAllDevices, &num_slaves);

  for (int i = 0; i < num_slaves; i++)
  {
    current_slave = &all_slaves[i];
    if ((current_slave->use != XISlavePointer && current_slave->use != XISlaveKeyboard) ||
      current_slave->attachment != deviceid)
      continue;
    mask->deviceid = current_slave->deviceid;
    XISelectEvents(display, root_window, mask, 1);
  }

  XIFreeDeviceInfo(all_slaves);
}

void XInput2Mouse::LockCursorToGameWindow()
{
  if (Host_RendererHasFocus() && cursor_locked)
  {
    Host_RendererUpdateCursor(true);
  }
  else
  {
    cursor_locked = false;
    Host_RendererUpdateCursor(false);
  }
}

}  // namespace prime
