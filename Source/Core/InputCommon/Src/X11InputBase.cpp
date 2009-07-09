
#include "X11InputBase.h"
namespace InputCommon
{
// Taken from wxw source code
KeySym wxCharCodeWXToX(int id)
{
    KeySym keySym;

    switch (id)
    {
        case WXK_CANCEL:        keySym = XK_Cancel; break;
        case WXK_BACK:          keySym = XK_BackSpace; break;
        case WXK_TAB:           keySym = XK_Tab; break;
        case WXK_CLEAR:         keySym = XK_Clear; break;
        case WXK_RETURN:        keySym = XK_Return; break;
        case WXK_SHIFT:         keySym = XK_Shift_L; break;
        case WXK_CONTROL:       keySym = XK_Control_L; break;
        case WXK_ALT:           keySym = XK_Meta_L; break;
        case WXK_CAPITAL:       keySym = XK_Caps_Lock; break;
        case WXK_MENU :         keySym = XK_Menu; break;
        case WXK_PAUSE:         keySym = XK_Pause; break;
        case WXK_ESCAPE:        keySym = XK_Escape; break;
        case WXK_SPACE:         keySym = ' '; break;
        case WXK_PAGEUP:        keySym = XK_Prior; break;
        case WXK_PAGEDOWN:      keySym = XK_Next; break;
        case WXK_END:           keySym = XK_End; break;
        case WXK_HOME :         keySym = XK_Home; break;
        case WXK_LEFT :         keySym = XK_Left; break;
        case WXK_UP:            keySym = XK_Up; break;
        case WXK_RIGHT:         keySym = XK_Right; break;
        case WXK_DOWN :         keySym = XK_Down; break;
        case WXK_SELECT:        keySym = XK_Select; break;
        case WXK_PRINT:         keySym = XK_Print; break;
        case WXK_EXECUTE:       keySym = XK_Execute; break;
        case WXK_INSERT:        keySym = XK_Insert; break;
        case WXK_DELETE:        keySym = XK_Delete; break;
        case WXK_HELP :         keySym = XK_Help; break;
        case WXK_NUMPAD0:       keySym = XK_KP_0; break; case WXK_NUMPAD_INSERT:     keySym = XK_KP_Insert; break;
        case WXK_NUMPAD1:       keySym = XK_KP_1; break; case WXK_NUMPAD_END:           keySym = XK_KP_End; break;
        case WXK_NUMPAD2:       keySym = XK_KP_2; break; case WXK_NUMPAD_DOWN:      keySym = XK_KP_Down; break;
        case WXK_NUMPAD3:       keySym = XK_KP_3; break; case WXK_NUMPAD_PAGEDOWN:  keySym = XK_KP_Page_Down; break;
        case WXK_NUMPAD4:       keySym = XK_KP_4; break; case WXK_NUMPAD_LEFT:         keySym = XK_KP_Left; break;
        case WXK_NUMPAD5:       keySym = XK_KP_5; break;
        case WXK_NUMPAD6:       keySym = XK_KP_6; break; case WXK_NUMPAD_RIGHT:       keySym = XK_KP_Right; break;
        case WXK_NUMPAD7:       keySym = XK_KP_7; break; case WXK_NUMPAD_HOME:       keySym = XK_KP_Home; break;
        case WXK_NUMPAD8:       keySym = XK_KP_8; break; case WXK_NUMPAD_UP:             keySym = XK_KP_Up; break;
        case WXK_NUMPAD9:       keySym = XK_KP_9; break; case WXK_NUMPAD_PAGEUP:   keySym = XK_KP_Page_Up; break;
        case WXK_NUMPAD_DECIMAL:    keySym = XK_KP_Decimal; break; case WXK_NUMPAD_DELETE:   keySym = XK_KP_Delete; break;
        case WXK_NUMPAD_MULTIPLY:   keySym = XK_KP_Multiply; break;
        case WXK_NUMPAD_ADD:             keySym = XK_KP_Add; break;
        case WXK_NUMPAD_SUBTRACT: keySym = XK_KP_Subtract; break;
        case WXK_NUMPAD_DIVIDE:        keySym = XK_KP_Divide; break;
        case WXK_NUMPAD_ENTER:        keySym = XK_KP_Enter; break;
        case WXK_NUMPAD_SEPARATOR:  keySym = XK_KP_Separator; break;
        case WXK_F1:            keySym = XK_F1; break;
        case WXK_F2:            keySym = XK_F2; break;
        case WXK_F3:            keySym = XK_F3; break;
        case WXK_F4:            keySym = XK_F4; break;
        case WXK_F5:            keySym = XK_F5; break;
        case WXK_F6:            keySym = XK_F6; break;
        case WXK_F7:            keySym = XK_F7; break;
        case WXK_F8:            keySym = XK_F8; break;
        case WXK_F9:            keySym = XK_F9; break;
        case WXK_F10:           keySym = XK_F10; break;
        case WXK_F11:           keySym = XK_F11; break;
        case WXK_F12:           keySym = XK_F12; break;
        case WXK_F13:           keySym = XK_F13; break;
        case WXK_F14:           keySym = XK_F14; break;
        case WXK_F15:           keySym = XK_F15; break;
        case WXK_F16:           keySym = XK_F16; break;
        case WXK_F17:           keySym = XK_F17; break;
        case WXK_F18:           keySym = XK_F18; break;
        case WXK_F19:           keySym = XK_F19; break;
        case WXK_F20:           keySym = XK_F20; break;
        case WXK_F21:           keySym = XK_F21; break;
        case WXK_F22:           keySym = XK_F22; break;
        case WXK_F23:           keySym = XK_F23; break;
        case WXK_F24:           keySym = XK_F24; break;
        case WXK_NUMLOCK:       keySym = XK_Num_Lock; break;
        case WXK_SCROLL:        keySym = XK_Scroll_Lock; break;
        default:                keySym = id <= 255 ? (KeySym)id : 0;
    }

    return keySym;
}

void XKeyToString(unsigned int keycode, char *keyStr) {
  switch (keycode) {
    
  case XK_Left:
    sprintf(keyStr, "LEFT");
    break;
  case XK_Up: 
    sprintf(keyStr, "UP");
    break;
  case XK_Right: 
    sprintf(keyStr, "RIGHT");
    break;
  case XK_Down:
    sprintf(keyStr, "DOWN");
    break;
  case XK_Return:
    sprintf(keyStr, "RETURN");
    break;
  case XK_KP_Enter:
    sprintf(keyStr, "KP ENTER");
    break;
  case XK_KP_Left:
    sprintf(keyStr, "KP LEFT");
    break;
  case XK_KP_Up: 
    sprintf(keyStr, "KP UP");
    break;
  case XK_KP_Right: 
    sprintf(keyStr, "KP RIGHT");
    break;
  case XK_KP_Down:
    sprintf(keyStr, "KP DOWN");
    break;
  case XK_Shift_L:
    sprintf(keyStr, "LShift");
    break;
  case XK_Control_L:
    sprintf(keyStr, "LControl");
    break;
  default:
    sprintf(keyStr, "%c", toupper(keycode));
  }
}
}