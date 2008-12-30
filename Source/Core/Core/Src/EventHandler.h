#ifndef EVENTHANDER_H
#define EVENTHANDER_H 1
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
    void Update();
    bool addEvent(sf::Event *e);
    static bool TestEvent (Keys k, sf::Event e);
    static int wxCharCodeWXToSF(int id);
    static void SFKeyToString(unsigned int keycode, char *keyStr);
};

#endif
