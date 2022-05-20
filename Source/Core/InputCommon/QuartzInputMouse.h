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
  explicit CocoaInputMouse(uint32_t* windowid);
  void UpdateInput() override;
  void LockCursorToGameWindow() override;

private:
  CGPoint current_loc, center;
  CGEventRef event;
  uint32_t* m_windowid;
};

}
