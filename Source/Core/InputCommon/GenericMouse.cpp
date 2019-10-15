#include "GenericMouse.h"

namespace prime
{

void GenericMouse::mousePressEvent(int x, int y)
{
  int xClick = x, yClick = y;
  if (xClick >= 0 && yClick >= 0)
  {
    cursor_locked = true;
  }
}

void GenericMouse::ResetDeltas()
{
  dx = dy = 0;
}

int32_t GenericMouse::GetDeltaHorizontalAxis() const
{
  return dx;  
}

int32_t GenericMouse::GetDeltaVerticalAxis() const
{
  return dy;
}

GenericMouse* g_mouse_input;

}  // namespace prime
