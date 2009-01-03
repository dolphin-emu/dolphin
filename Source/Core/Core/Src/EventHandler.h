#ifndef EVENTHANDER_H
#define EVENTHANDER_H 1
#include "Common.h"
#include <queue>
#include "Event.hpp"

typedef bool (*listenFuncPtr) (sf::Event);
enum InputType
{
    KeyboardInput,
    MouseInput,
    JoystickInput
};
 
enum Modifiers {
    UseAlt = 1,
    UseShift = 2,
    UseCtrl = 4
};

struct Keys
{
    InputType inputType;
    sf::Event::EventType eventType; 
    sf::Key::Code keyCode;
    int mods;
    sf::Mouse::Button mouseButton;
};

class EventHandler {

private:
    listenFuncPtr keys[sf::Key::Count][8];
    listenFuncPtr mouse[sf::Mouse::Count];
    listenFuncPtr joys[sf::Joy::Count];
    std::queue<sf::Event> eventQueue;
public:
    bool RegisterEventListener(listenFuncPtr func, Keys key);
    bool RemoveEventListener(Keys key);
    void Update();
    bool addEvent(sf::Event *e);
    static bool TestEvent (Keys k, sf::Event e);
#if defined HAVE_WX && HAVE_WX
    static sf::Key::Code wxCharCodeToSF(int id);
#endif
    static void SFKeyToString(sf::Key::Code keycode, char *keyStr);
};

extern EventHandler *eventHandler;

#endif
