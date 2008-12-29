#ifndef EVENTHANDER_H
#define EVENTHANDER_H 1
#include <queue>
#include "Event.hpp"

typedef bool (*listenFuncPtr) (sf::Event *);

class EventHandler {

private:
    listenFuncPtr keys[100][6];
    listenFuncPtr mouse[6];
    listenFuncPtr joys[10];
    std::queue<sf::Event> eventQueue;
public:
    bool RegisterEventListener(listenFuncPtr func, int event, int type);
    void Update();
    bool addEvent(sf::Event *);
    
};

#endif
