#include "QuartzInputMouse.h"
#include "Core/Host.h"

/**
 * This interface works by centering the cursor within the window
 * ever time the `UpdateInput` method is called. It then calculates
 * the delta based on how far the mouse has moved from the center of
 * the screen before it is re-centered.
 */


int win_w = 0, win_h = 0;

namespace prime
{

bool InitQuartzInputMouse(uint32_t* windowid)
{
  g_mouse_input = new QuartzInputMouse(windowid);
  return true;
}

QuartzInputMouse::QuartzInputMouse(uint32_t* windowid)
{
  m_windowid = windowid;
  center = current_loc = getWindowCenter();
}

void QuartzInputMouse::UpdateInput()
{
  event = CGEventCreate(nil);
  current_loc = CGEventGetLocation(event);
  CFRelease(event);
  center = getWindowCenter();
  if (Host_RendererHasFocus() && cursor_locked)
  {
    this->dx += current_loc.x - center.x;
    this->dy += current_loc.y - center.y;
  }
  LockCursorToGameWindow();
}

void QuartzInputMouse::LockCursorToGameWindow()
{
  if (Host_RendererHasFocus() && cursor_locked)
  {
    // Hack to avoid short bit of input suppression after warp
    // Credit/explanation: https://stackoverflow.com/a/17559012/7341382
      CGWarpMouseCursorPosition(center);
      CGAssociateMouseAndMouseCursorPosition(true);
      CGDisplayHideCursor(CGMainDisplayID());
  }
  else
  {
    cursor_locked = false;
    Host_RendererUpdateCursor(false);
    CGAssociateMouseAndMouseCursorPosition(true);
    CGDisplayShowCursor(CGMainDisplayID());
  }
}

CGRect QuartzInputMouse::getBounds()
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

CGPoint QuartzInputMouse::getWindowCenter()
{
  const auto bounds = getBounds();
  const double x = bounds.origin.x + (bounds.size.width / 2);
  const double y = bounds.origin.y + (bounds.size.height / 2);
  return CGPointMake(x, y);
}

}
