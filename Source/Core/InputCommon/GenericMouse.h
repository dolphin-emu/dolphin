#pragma once

#include <stdint.h>

// it's CENTRE f&*k you
#define SET_RENDER_CENTRE(x, y) \
                       win_x = x;\
                       win_y = y; \

extern int win_x;
extern int win_y;

namespace prime
{

class GenericMouse
{
public:
  // Platform dependant implementations are made virtual
  virtual void UpdateInput() = 0;
  virtual void LockCursorToGameWindow() = 0;

  void mousePressEvent(int x, int y);
  void ResetDeltas();
  int32_t GetDeltaVerticalAxis() const;
  int32_t GetDeltaHorizontalAxis() const;

protected:
  bool cursor_locked;
  int32_t dx, dy;
};

extern GenericMouse* g_mouse_input;

}  // namespace prime
