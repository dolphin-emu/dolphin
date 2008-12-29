#ifndef EVENTHANDER_H
#define EVENTHANDER_H 1
#include <queue>
#include "Event.hpp"

typedef bool (*listenFuncPtr) (sf::Event *);
enum InputType
{
    KeyboardInput,
    MouseInput,
    JoystickInput
};
 
struct Keys
{
    InputType inputType;
    sf::Event::EventType eventType;
    sf::Key::Code keyCode;
    sf::Mouse::Button mouseButton;
};

class EventHandler {

private:
    listenFuncPtr keys[sf::Key::Count][6];
    listenFuncPtr mouse[sf::Mouse::Count];
    listenFuncPtr joys[sf::Joy::Count];
    std::queue<Keys> eventQueue;
public:
    bool RegisterEventListener(listenFuncPtr func, int event, int type);
    void Update();
    bool addEvent(sf::Event *);
    static bool TestEvent (Keys k, sf::Event e);
    static int wxCharCodeWXToSF(int id);
    static void SFKeyToString(unsigned int keycode, char *keyStr);
};

#endif
