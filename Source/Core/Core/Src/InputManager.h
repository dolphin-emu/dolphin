#ifndef INPUTMANAGER_H
#define INPUTMANAGER_H

class InputManager {

public:
    bool Init();
    bool Shutdown();

    InputManager(): sdlInit(false) {}
    ~InputManager() {}

private:
    bool sdlInit;
};
#endif
