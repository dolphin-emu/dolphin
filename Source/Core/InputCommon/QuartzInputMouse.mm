#include "QuartzInputMouse.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "Core/Host.h"


int win_w = 0, win_h = 0;

namespace prime
{

bool InitCocoaInputMouse(uint32_t* windowid)
{
  g_mouse_input = new CocoaInputMouse(windowid);
  return true;
}

CocoaInputMouse::CocoaInputMouse(uint32_t* windowid)
{
  m_windowid = windowid;
  origin = last_loc = getWindowCenter(m_windowid);
}

void CocoaInputMouse::UpdateInput() 
{
  event = CGEventCreate(nil);
  current_loc = CGEventGetLocation(event);
  CFRelease(event);
  origin = getWindowCenter(m_windowid);

  if (cursor_locked)
  {
      this->dx += current_loc.x - origin.x;
      this->dy += current_loc.y - origin.y;
      if (!isFullScreen()) {
        this->dy -= WINDOW_CHROME_OFFSET;
      }
      last_loc = current_loc;
  }
  LockCursorToGameWindow();

}

void CocoaInputMouse::LockCursorToGameWindow()
{
  if (Host_RendererHasFocus() && cursor_locked)
  {
    Host_RendererUpdateCursor(true);
    CGDisplayHideCursor(CGMainDisplayID());
  }
  else
  {
    cursor_locked = false;
    Host_RendererUpdateCursor(false);
    CGDisplayShowCursor(CGMainDisplayID());
  }
}

bool CocoaInputMouse::isFullScreen()
{
  // TODO: Right now the game must be played on the main display.
  auto bounds = getBounds(m_windowid);
  auto mainDisplayId = CGMainDisplayID();
  auto width = CGDisplayPixelsWide(mainDisplayId);
  auto height = CGDisplayPixelsHigh(mainDisplayId);
  return (bounds.size.width == width && bounds.size.height == height);
}

CGRect getBounds(uint32_t* m_windowid)
{
  CGRect bounds = CGRectZero;
  CGWindowID windowid[1] = {*m_windowid};
  
  CFArrayRef windowArray = CFArrayCreate(nullptr, (const void**)windowid, 1, nullptr);
  CFArrayRef windowDescriptions = CGWindowListCreateDescriptionFromArray(windowArray);
  CFDictionaryRef windowDescription =
      static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(windowDescriptions, 0));

  if (CFDictionaryContainsKey(windowDescription, kCGWindowBounds))
  {
    CFDictionaryRef boundsDictionary =
        static_cast<CFDictionaryRef>(CFDictionaryGetValue(windowDescription, kCGWindowBounds));

    if (boundsDictionary != nullptr)
      CGRectMakeWithDictionaryRepresentation(boundsDictionary, &bounds);
  }

  CFRelease(windowDescriptions);
  CFRelease(windowArray);
  return bounds;
}

CGPoint getWindowCenter(uint32_t* m_windowid)
{
  auto bounds = getBounds(m_windowid);
  double x = bounds.origin.x + (bounds.size.width / 2);
  double y = bounds.origin.y + (bounds.size.height / 2);
  CGPoint center = {x, y};
  return center;
}

}