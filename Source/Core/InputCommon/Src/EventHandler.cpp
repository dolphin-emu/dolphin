#include "EventHandler.h"

#if defined HAVE_WX && HAVE_WX
#include <wx/wx.h>
#endif

EventHandler *EventHandler::m_Instance = 0;

EventHandler::EventHandler() {
    for (int i=0; i<NUMKEYS; i++)
		for (int j=0; j<NUMMODS; j++)
			keys[i][j] = NULL;
    //    memset(keys, sizeof(listenFuncPtr) * NUMKEYS*NUMMODS, 0);
    memset(mouse, sizeof(listenFuncPtr) * (sf::Mouse::Count+1), 0);
    memset(joys, sizeof(listenFuncPtr) * (sf::Joy::Count+1), 0);

 }

EventHandler::~EventHandler() {
}

EventHandler *EventHandler::GetInstance() {
    // fprintf(stderr, "handler instance %p\n", m_Instance);

    if (! m_Instance)
	m_Instance = new EventHandler();
    return m_Instance;
}

void EventHandler::Destroy() {
    if (m_Instance)
       	delete m_Instance;
    // fprintf(stderr, "deleting instance %p\n", m_Instance);
    m_Instance = 0;
}

bool EventHandler::RegisterEventListener(listenFuncPtr func, Keys key) {
    if (key.inputType == KeyboardInput) {
	// fprintf(stderr, "Registering %d:%d  %p %p \n", key.keyCode, key.mods, func, this);
	if (key.keyCode == sf::Key::Count || key.mods >= NUMMODS ||  
	    key.keyCode >= NUMKEYS) 
	    return false;
	if (keys[key.keyCode][key.mods] && keys[key.keyCode][key.mods] != func)
	    return false
;
	keys[key.keyCode][key.mods] = func;
    } else if (key.inputType == MouseInput) {
	if (mouse[key.mouseButton])
	    return false;
	mouse[key.mouseButton] = func;
    }
    
    return true;
}

bool EventHandler::RemoveEventListener(Keys key) {
    if (key.inputType == KeyboardInput) {
	if ((key.keyCode == sf::Key::Count || key.keyCode >= NUMKEYS
             || key.mods >= NUMMODS) && ! keys[key.keyCode][key.mods])
	    return false;
	keys[key.keyCode][key.mods] = NULL;
    } else if (key.inputType == MouseInput) {
	if (! mouse[key.mouseButton])
	    return false;
	mouse[key.mouseButton] = NULL;
    }
    
    return true; 
}

void EventHandler::Update() {
    for (unsigned int i = 0; i < eventQueue.size();i++) {
	sf::Event ev = eventQueue.front();
	eventQueue.pop();
	fprintf(stderr, "Updating event type %d code %d mod %d func %p %p\n", ev.Type, ev.Key.Code, ev.Key.Alt+2*ev.Key.Shift+4*ev.Key.Control, keys[ev.Key.Code][ev.Key.Alt+2*ev.Key.Shift+4*ev.Key.Control], this);
	if(keys[ev.Key.Code][ev.Key.Alt+2*ev.Key.Shift+4*ev.Key.Control])
	    keys[ev.Key.Code][ev.Key.Alt+2*ev.Key.Shift+4*ev.Key.Control](ev);
    }
}

bool EventHandler::addEvent(sf::Event *ev) {
    eventQueue.push(*ev);
    fprintf(stderr, "Got event type %d code %d %p\n", ev->Type, ev->Key.Code, this); 
    return true;
}


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

#if defined HAVE_WX && HAVE_WX 
// Taken from wxw source code
sf::Key::Code EventHandler::wxCharCodeToSF(int id)
{
    sf::Key::Code sfKey;

    switch (id) {
//  case WXK_CANCEL:          sfKey = sf::Key::Cancel; break;
//  case WXK_BACK:            sfKey = sf::Key::BackSpace; break;
    case WXK_TAB:             sfKey = sf::Key::Tab; break;
//  case WXK_CLEAR:           sfKey = sf::Key::Clear; break;
    case WXK_RETURN:          sfKey = sf::Key::Return; break;
    case WXK_SHIFT:           sfKey = sf::Key::LShift; break;
    case WXK_CONTROL:         sfKey = sf::Key::LControl; break;
    case WXK_ALT:             sfKey = sf::Key::LAlt; break;
//  case WXK_CAPITAL:         sfKey = sf::Key::Caps_Lock; break;
    case WXK_MENU :           sfKey = sf::Key::Menu; break;
    case WXK_PAUSE:           sfKey = sf::Key::Pause; break;
    case WXK_ESCAPE:          sfKey = sf::Key::Escape; break;
    case WXK_SPACE:           sfKey = sf::Key::Space; break;
    case WXK_PAGEUP:          sfKey = sf::Key::PageUp; break;
    case WXK_PAGEDOWN:        sfKey = sf::Key::PageDown; break;
    case WXK_END:             sfKey = sf::Key::End; break;
    case WXK_HOME :           sfKey = sf::Key::Home; break;
    case WXK_LEFT :           sfKey = sf::Key::Left; break;
    case WXK_UP:              sfKey = sf::Key::Up; break;
    case WXK_RIGHT:           sfKey = sf::Key::Right; break;
    case WXK_DOWN :           sfKey = sf::Key::Down; break;
//  case WXK_SELECT:          sfKey = sf::Key::Select; break;
//  case WXK_PRINT:           sfKey = sf::Key::Print; break;
//  case WXK_EXECUTE:         sfKey = sf::Key::Execute; break;
    case WXK_INSERT:          sfKey = sf::Key::Insert; break;
    case WXK_DELETE:          sfKey = sf::Key::Delete; break;
//  case WXK_HELP :           sfKey = sf::Key::Help; break;
    case WXK_NUMPAD0:         sfKey = sf::Key::Numpad0; break; 
    case WXK_NUMPAD_INSERT:   sfKey = sf::Key::Insert; break;
    case WXK_NUMPAD1:         sfKey = sf::Key::Numpad1; break; 
    case WXK_NUMPAD_END:      sfKey = sf::Key::End; break;
    case WXK_NUMPAD2:         sfKey = sf::Key::Numpad2; break; 
    case WXK_NUMPAD_DOWN:     sfKey = sf::Key::Down; break;
    case WXK_NUMPAD3:         sfKey = sf::Key::Numpad3; break; 
    case WXK_NUMPAD_PAGEDOWN: sfKey = sf::Key::PageDown; break;
    case WXK_NUMPAD4:         sfKey = sf::Key::Numpad4; break; 
    case WXK_NUMPAD_LEFT:     sfKey = sf::Key::Left; break;
    case WXK_NUMPAD5:         sfKey = sf::Key::Numpad5; break;
    case WXK_NUMPAD6:         sfKey = sf::Key::Numpad6; break; 
    case WXK_NUMPAD_RIGHT:    sfKey = sf::Key::Right; break;
    case WXK_NUMPAD7:         sfKey = sf::Key::Numpad7; break; 
    case WXK_NUMPAD_HOME:     sfKey = sf::Key::Home; break;
    case WXK_NUMPAD8:         sfKey = sf::Key::Numpad8; break; 
    case WXK_NUMPAD_UP:       sfKey = sf::Key::Up; break;
    case WXK_NUMPAD9:         sfKey = sf::Key::Numpad9; break; 
    case WXK_NUMPAD_PAGEUP:   sfKey = sf::Key::PageUp; break;
//  case WXK_NUMPAD_DECIMAL:  sfKey = sf::Key::Decimal; break; 
    case WXK_NUMPAD_DELETE:   sfKey = sf::Key::Delete; break;
    case WXK_NUMPAD_MULTIPLY: sfKey = sf::Key::Multiply; break;
    case WXK_NUMPAD_ADD:      sfKey = sf::Key::Add; break;
    case WXK_NUMPAD_SUBTRACT: sfKey = sf::Key::Subtract; break;
    case WXK_NUMPAD_DIVIDE:   sfKey = sf::Key::Divide; break;
    case WXK_NUMPAD_ENTER:    sfKey = sf::Key::Return; break;
//  case WXK_NUMPAD_SEPARATOR:sfKey = sf::Key::Separator; break;
    case WXK_F1:              sfKey = sf::Key::F1; break;
    case WXK_F2:              sfKey = sf::Key::F2; break;
    case WXK_F3:              sfKey = sf::Key::F3; break;
    case WXK_F4:              sfKey = sf::Key::F4; break;
    case WXK_F5:              sfKey = sf::Key::F5; break;
    case WXK_F6:              sfKey = sf::Key::F6; break;
    case WXK_F7:              sfKey = sf::Key::F7; break;
    case WXK_F8:              sfKey = sf::Key::F8; break;
    case WXK_F9:              sfKey = sf::Key::F9; break;
    case WXK_F10:             sfKey = sf::Key::F10; break;
    case WXK_F11:             sfKey = sf::Key::F11; break;
    case WXK_F12:             sfKey = sf::Key::F12; break;
    case WXK_F13:             sfKey = sf::Key::F13; break;
    case WXK_F14:             sfKey = sf::Key::F14; break;
    case WXK_F15:             sfKey = sf::Key::F15; break;
//  case WXK_NUMLOCK:         sfKey = sf::Key::Num_Lock; break;
//  case WXK_SCROLL:          sfKey = sf::Key::Scroll_Lock; break;
    default:

        // To lower (will tolower work on windows?)
        if (id >= 'A' && id <= 'Z') 
            id = id - 'A' + 'a';
        
	if ((id >= 'a' && id <= 'z') || 
	    (id  >= '0' && id <= '9')) 
	    sfKey = (sf::Key::Code)id;   
	else 
	    sfKey = sf::Key::Count; // Invalid key
        
    }
    
    return sfKey;
}
#endif

void EventHandler::SFKeyToString(sf::Key::Code keycode, char *keyStr) {
    switch (keycode) {
/*  case sf::Key::A = 'a': sprintf(keyStr, "UP"); break;
    case sf::Key::B = 'b': sprintf(keyStr, "UP"); break;
    case sf::Key::C = 'c': sprintf(keyStr, "UP"); break;
    case sf::Key::D = 'd': sprintf(keyStr, "UP"); break;
    case sf::Key::E = 'e': sprintf(keyStr, "UP"); break;
    case sf::Key::F = 'f': sprintf(keyStr, "UP"); break;
    case sf::Key::G = 'g': sprintf(keyStr, "UP"); break;
    case sf::Key::H = 'h': sprintf(keyStr, "UP"); break;
    case sf::Key::I = 'i': sprintf(keyStr, "UP"); break;
    case sf::Key::J = 'j': sprintf(keyStr, "UP"); break;
    case sf::Key::K = 'k': sprintf(keyStr, "UP"); break;
    case sf::Key::L = 'l': sprintf(keyStr, "UP"); break;
    case sf::Key::M = 'm': sprintf(keyStr, "UP"); break;
    case sf::Key::N = 'n': sprintf(keyStr, "UP"); break;
    case sf::Key::O = 'o': sprintf(keyStr, "UP"); break;
    case sf::Key::P = 'p': sprintf(keyStr, "UP"); break;
    case sf::Key::Q = 'q': sprintf(keyStr, "UP"); break;
    case sf::Key::R = 'r': sprintf(keyStr, "UP"); break;
    case sf::Key::S = 's': sprintf(keyStr, "UP"); break;
    case sf::Key::T = 't': sprintf(keyStr, "UP"); break;
    case sf::Key::U = 'u': sprintf(keyStr, "UP"); break;
    case sf::Key::V = 'v': sprintf(keyStr, "UP"); break;
    case sf::Key::W = 'w': sprintf(keyStr, "UP"); break;
    case sf::Key::X = 'x': sprintf(keyStr, "UP"); break;
    case sf::Key::Y = 'y': sprintf(keyStr, "UP"); break;
    case sf::Key::Z = 'z': sprintf(keyStr, "UP"); break;
    case sf::Key::Num0 = '0': sprintf(keyStr, "UP"); break;
    case sf::Key::Num1 = '1': sprintf(keyStr, "UP"); break;
    case sf::Key::Num2 = '2': sprintf(keyStr, "UP"); break;
    case sf::Key::Num3 = '3': sprintf(keyStr, "UP"); break;
    case sf::Key::Num4 = '4': sprintf(keyStr, "UP"); break;
    case sf::Key::Num5 = '5': sprintf(keyStr, "UP"); break;
    case sf::Key::Num6 = '6': sprintf(keyStr, "UP"); break;
    case sf::Key::Num7 = '7': sprintf(keyStr, "UP"); break;
    case sf::Key::Num8 = '8': sprintf(keyStr, "UP"); break;
    case sf::Key::Num9 = '9': sprintf(keyStr, "UP"); break;*/ 
    case sf::Key::Escape:      sprintf(keyStr, "Escape"); break;
    case sf::Key::LControl:    sprintf(keyStr, "LControl"); break;
    case sf::Key::LShift:      sprintf(keyStr, "LShift"); break;
    case sf::Key::LAlt:        sprintf(keyStr, "LAlt"); break;
    case sf::Key::LSystem:     sprintf(keyStr, "LSystem"); break;
    case sf::Key::RControl:    sprintf(keyStr, "RControl"); break;
    case sf::Key::RShift:      sprintf(keyStr, "RShift"); break;
    case sf::Key::RAlt:        sprintf(keyStr, "RAlt"); break;
    case sf::Key::RSystem:     sprintf(keyStr, "RSystem"); break;
    case sf::Key::Menu:        sprintf(keyStr, "Menu"); break;
    case sf::Key::LBracket:    sprintf(keyStr, "LBracket"); break;
    case sf::Key::RBracket:    sprintf(keyStr, "RBracket"); break;
    case sf::Key::SemiColon:   sprintf(keyStr, ";"); break;
    case sf::Key::Comma:       sprintf(keyStr, ","); break;
    case sf::Key::Period:      sprintf(keyStr, "."); break;
    case sf::Key::Quote:       sprintf(keyStr, "\'"); break;
    case sf::Key::Slash:       sprintf(keyStr, "/"); break;
    case sf::Key::BackSlash:   sprintf(keyStr, "\\"); break;
    case sf::Key::Tilde:       sprintf(keyStr, "~"); break;
    case sf::Key::Equal:       sprintf(keyStr, "="); break;
    case sf::Key::Dash:        sprintf(keyStr, "-"); break;
    case sf::Key::Space:       sprintf(keyStr, "Space"); break;
    case sf::Key::Return:      sprintf(keyStr, "Return"); break;
    case sf::Key::Back:        sprintf(keyStr, "Back"); break;
    case sf::Key::Tab:         sprintf(keyStr, "Tab"); break;
    case sf::Key::PageUp:      sprintf(keyStr, "Page Up"); break;
    case sf::Key::PageDown:    sprintf(keyStr, "Page Down"); break;
    case sf::Key::End:         sprintf(keyStr, "End"); break;
    case sf::Key::Home:        sprintf(keyStr, "Home"); break;
    case sf::Key::Insert:      sprintf(keyStr, "Insert"); break;
    case sf::Key::Delete:      sprintf(keyStr, "Delete"); break;
    case sf::Key::Add:         sprintf(keyStr, "+"); break;
    case sf::Key::Subtract:    sprintf(keyStr, "-"); break;
    case sf::Key::Multiply:    sprintf(keyStr, "*"); break;
    case sf::Key::Divide:      sprintf(keyStr, "/"); break;
    case sf::Key::Left:        sprintf(keyStr, "Left"); break;
    case sf::Key::Right:       sprintf(keyStr, "Right"); break;
    case sf::Key::Up:          sprintf(keyStr, "Up"); break;
    case sf::Key::Down:        sprintf(keyStr, "Down"); break;
    case sf::Key::Numpad0:     sprintf(keyStr, "NP 0"); break;
    case sf::Key::Numpad1:     sprintf(keyStr, "NP 1"); break;
    case sf::Key::Numpad2:     sprintf(keyStr, "NP 2"); break;
    case sf::Key::Numpad3:     sprintf(keyStr, "NP 3"); break;
    case sf::Key::Numpad4:     sprintf(keyStr, "NP 4"); break;
    case sf::Key::Numpad5:     sprintf(keyStr, "NP 5"); break;
    case sf::Key::Numpad6:     sprintf(keyStr, "NP 6"); break;
    case sf::Key::Numpad7:     sprintf(keyStr, "NP 7"); break;
    case sf::Key::Numpad8:     sprintf(keyStr, "NP 8"); break;
    case sf::Key::Numpad9:     sprintf(keyStr, "NP 9"); break;
    case sf::Key::F1:          sprintf(keyStr, "F1"); break;
    case sf::Key::F2:          sprintf(keyStr, "F2"); break;
    case sf::Key::F3:          sprintf(keyStr, "F3"); break;
    case sf::Key::F4:          sprintf(keyStr, "F4"); break;
    case sf::Key::F5:          sprintf(keyStr, "F5"); break;
    case sf::Key::F6:          sprintf(keyStr, "F6"); break;
    case sf::Key::F7:          sprintf(keyStr, "F7"); break;
    case sf::Key::F8:          sprintf(keyStr, "F8"); break;
    case sf::Key::F9:          sprintf(keyStr, "F9"); break;
    case sf::Key::F10:         sprintf(keyStr, "F10"); break;
    case sf::Key::F11:         sprintf(keyStr, "F11"); break;
    case sf::Key::F12:         sprintf(keyStr, "F12"); break;
    case sf::Key::F13:         sprintf(keyStr, "F13"); break;
    case sf::Key::F14:         sprintf(keyStr, "F14"); break;
    case sf::Key::F15:         sprintf(keyStr, "F15"); break;
    case sf::Key::Pause:       sprintf(keyStr, "Pause"); break;
    default:                   
	if (keycode > sf::Key::Escape)
	    sprintf(keyStr, "Invalid Key");
	else
	    sprintf(keyStr, "%c", toupper(keycode));
	break;
    }
}

class EventHandlerCleaner
{
public:
    ~EventHandlerCleaner()
    {
	//EventHandler::Destroy();
    }
} EventHandlerCleanerInst;
