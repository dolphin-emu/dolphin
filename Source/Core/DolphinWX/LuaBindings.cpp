#include "LuaScripting.h"

//Functions to register with Lua


// Prints a string to the text control of this frame
int printToTextCtrl(lua_State* L)
{
  currentWindow->Log(lua_tostring(L, 1));
  currentWindow->Log("\n");

  return 0;
}

// Steps a frame if the emulator is paused, pauses it otherwise.
int frameAdvance(lua_State* L)
{
  Core::DoFrameStep();
  return 0;
}

int getCurrentFrame(lua_State *L)
{
  lua_pushinteger(L, Movie::GetCurrentFrame());
  return 0;
}

int setAnalog(lua_State* L)
{
  if (lua_gettop(L) != 2)
  {
    currentWindow->Log("Incorrect # of parameters passed to setAnalog.\n");
    return 0;
  }

  u8 xPos = lua_tointeger(L, 1);
  u8 yPos = lua_tointeger(L, 2);

  currentWindow->pad_status->stickX = xPos;
  currentWindow->pad_status->stickY = yPos;

  return 0;
}
