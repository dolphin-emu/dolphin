
#ifndef XINPUTBASE_H
#define XINPUTBASE_H

#include <X11/X.h>
#include <X11/keysym.h>
#include <wx/wx.h>
namespace InputCommon
{
KeySym wxCharCodeWXToX(int id);
void XKeyToString(unsigned int keycode, char *keyStr);
}
#endif
