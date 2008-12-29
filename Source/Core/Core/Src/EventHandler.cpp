#include "EventHandler.h"
#include <wx/wx.h>

bool EventHandler::TestEvent (Keys k, sf::Event e)
{
    //Mouse event
    if (k.inputType==MouseInput && k.eventType==e.Type && k.mouseButton==e.MouseButton.Button)
    {
        return (true);
    }
    //Keyboard event
    if (k.inputType==KeyboardInput && k.eventType==e.Type && k.keyCode==e.Key.Code)
    {
        return (true);
    }
    return (false);
}
 
// Taken from wxw source code
int wxCharCodeWXToX(int id)
{
    int sfKey;

    switch (id) {
	//case WXK_CANCEL:        sfKey = sf::Key::Cancel; break;
	//case WXK_BACK:          sfKey = sf::Key::BackSpace; break;
    case WXK_TAB:           sfKey = sf::Key::Tab; break;
	//case WXK_CLEAR:         sfKey = sf::Key::Clear; break;
    case WXK_RETURN:        sfKey = sf::Key::Return; break;
    case WXK_SHIFT:         sfKey = sf::Key::LShift; break;
    case WXK_CONTROL:       sfKey = sf::Key::LControl; break;
    case WXK_ALT:           sfKey = sf::Key::LAlt; break;
	//case WXK_CAPITAL:       sfKey = sf::Key::Caps_Lock; break;
    case WXK_MENU :         sfKey = sf::Key::Menu; break;
    case WXK_PAUSE:         sfKey = sf::Key::Pause; break;
    case WXK_ESCAPE:        sfKey = sf::Key::Escape; break;
    case WXK_SPACE:         sfKey = sf::Key::Space; break;
    case WXK_PAGEUP:        sfKey =sf::Key::PageUp; break;
    case WXK_PAGEDOWN:      sfKey =sf::Key::PageDown; break;
    case WXK_END:           sfKey =sf::Key::End; break;
    case WXK_HOME :         sfKey =sf::Key::Home; break;
    case WXK_LEFT :         sfKey =sf::Key::Left; break;
    case WXK_UP:            sfKey =sf::Key::Up; break;
    case WXK_RIGHT:         sfKey =sf::Key::Right; break;
    case WXK_DOWN :         sfKey =sf::Key::Down; break;
	//case WXK_SELECT:        sfKey =sf::Key::Select; break;
	//case WXK_PRINT:         sfKey =sf::Key::Print; break;
	//case WXK_EXECUTE:       sfKey =sf::Key::Execute; break;
    case WXK_INSERT:        sfKey =sf::Key::Insert; break;
    case WXK_DELETE:        sfKey =sf::Key::Delete; break;
	//case WXK_HELP :         sfKey =sf::Key::Help; break;
    case WXK_NUMPAD0:       sfKey =sf::Key::Numpad0; break; 
    case WXK_NUMPAD_INSERT: sfKey =sf::Key::Insert; break;
    case WXK_NUMPAD1:       sfKey =sf::Key::Numpad1; break; 
    case WXK_NUMPAD_END:    sfKey =sf::Key::End; break;
    case WXK_NUMPAD2:       sfKey =sf::Key::Numpad2; break; 
    case WXK_NUMPAD_DOWN:   sfKey =sf::Key::Down; break;
    case WXK_NUMPAD3:       sfKey =sf::Key::Numpad3; break; 
    case WXK_NUMPAD_PAGEDOWN: sfKey =sf::Key::PageDown; break;
    case WXK_NUMPAD4:       sfKey =sf::Key::Numpad4; break; 
    case WXK_NUMPAD_LEFT:   sfKey =sf::Key::Left; break;
    case WXK_NUMPAD5:       sfKey =sf::Key::Numpad5; break;
    case WXK_NUMPAD6:       sfKey =sf::Key::Numpad6; break; 
    case WXK_NUMPAD_RIGHT:  sfKey =sf::Key::Right; break;
    case WXK_NUMPAD7:       sfKey =sf::Key::Numpad7; break; 
    case WXK_NUMPAD_HOME:   sfKey =sf::Key::Home; break;
    case WXK_NUMPAD8:       sfKey =sf::Key::Numpad8; break; 
    case WXK_NUMPAD_UP:     sfKey =sf::Key::Up; break;
    case WXK_NUMPAD9:       sfKey =sf::Key::Numpad9; break; 
    case WXK_NUMPAD_PAGEUP:   sfKey =sf::Key::PageUp; break;
	//case WXK_NUMPAD_DECIMAL:  sfKey =sf::Key::Decimal; break; 
    case WXK_NUMPAD_DELETE:   sfKey =sf::Key::Delete; break;
    case WXK_NUMPAD_MULTIPLY: sfKey =sf::Key::Multiply; break;
    case WXK_NUMPAD_ADD:      sfKey =sf::Key::Add; break;
    case WXK_NUMPAD_SUBTRACT: sfKey =sf::Key::Subtract; break;
    case WXK_NUMPAD_DIVIDE:   sfKey =sf::Key::Divide; break;
    case WXK_NUMPAD_ENTER:    sfKey =sf::Key::Return; break;
	//case WXK_NUMPAD_SEPARATOR: sfKey =sf::Key::Separator; break;
    case WXK_F1:            sfKey =sf::Key::F1; break;
    case WXK_F2:            sfKey =sf::Key::F2; break;
    case WXK_F3:            sfKey =sf::Key::F3; break;
    case WXK_F4:            sfKey =sf::Key::F4; break;
    case WXK_F5:            sfKey =sf::Key::F5; break;
    case WXK_F6:            sfKey =sf::Key::F6; break;
    case WXK_F7:            sfKey =sf::Key::F7; break;
    case WXK_F8:            sfKey =sf::Key::F8; break;
    case WXK_F9:            sfKey =sf::Key::F9; break;
    case WXK_F10:           sfKey =sf::Key::F10; break;
    case WXK_F11:           sfKey =sf::Key::F11; break;
    case WXK_F12:           sfKey =sf::Key::F12; break;
    case WXK_F13:           sfKey =sf::Key::F13; break;
    case WXK_F14:           sfKey =sf::Key::F14; break;
    case WXK_F15:           sfKey =sf::Key::F15; break;
	//case WXK_NUMLOCK:       sfKey =sf::Key::Num_Lock; break;
	//case WXK_SCROLL:        sfKey =sf::Key::Scroll_Lock; break;
    default:                sfKey = id <= 255 ? id : 0;
    }

    return sfKey;
}

void EventHandler::SFKeyToString(unsigned int keycode, char *keyStr) {
    switch (keycode) {
	
    case sf::Key::Return:
	sprintf(keyStr, "RETURN");
	break;
    case sf::Key::Left:
	sprintf(keyStr, "LEFT");
	break;
    case sf::Key::Up: 
	sprintf(keyStr, "UP");
	break;
    case sf::Key::Right: 
	sprintf(keyStr, "RIGHT");
	break;
    case sf::Key::Down:
	sprintf(keyStr, "DOWN");
	break;
    case sf::Key::LShift:
    case sf::Key::RShift:
	sprintf(keyStr, "Shift");
	break;
    case sf::Key::LControl:
    case sf::Key::RControl:
	sprintf(keyStr, "Control");
    case sf::Key::LAlt:
    case sf::Key::RAlt:
	sprintf(keyStr, "Alt");
	break;
    default:
	sprintf(keyStr, "%c", toupper(keycode));
    }
}
