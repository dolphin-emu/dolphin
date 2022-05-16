#include <ApplicationServices/ApplicationServices.h>
#include "GenericMouse.h"

namespace prime
{

bool InitCocoaInputMouse(uint32_t* windowid);
CGRect getBounds(uint32_t* m_windowid);
CGPoint getWindowCenter(uint32_t* m_windowid);

class CocoaInputMouse: public GenericMouse
{
public:
  CocoaInputMouse(uint32_t* windowid);
  void UpdateInput() override;
  void LockCursorToGameWindow() override;

private:
  bool isFullScreen();
  CGPoint current_loc, last_loc, origin;
  CGEventRef event;
  static const uint8 WINDOW_CHROME_OFFSET = 13;
  uint32_t* m_windowid;
};

}
