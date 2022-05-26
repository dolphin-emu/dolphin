#include <ApplicationServices/ApplicationServices.h>
#include "GenericMouse.h"

namespace prime
{

bool InitQuartzInputMouse(uint32_t* windowid);

class QuartzInputMouse: public GenericMouse
{
public:
  explicit QuartzInputMouse(uint32_t* windowid);
  void UpdateInput() override;
  void LockCursorToGameWindow() override;

private:
  CGPoint current_loc, center;
  CGEventRef event{};
  uint32_t* m_windowid;
  CGRect getBounds();
  CGPoint getWindowCenter();
};

}
