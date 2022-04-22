#pragma once

#include <stdint.h>

extern int win_w, win_h;

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

class NullMouse : public GenericMouse {
public:
  NullMouse() {}
  void UpdateInput() override {}
  void LockCursorToGameWindow() override {}
};

extern GenericMouse* g_mouse_input;

}  // namespace prime
