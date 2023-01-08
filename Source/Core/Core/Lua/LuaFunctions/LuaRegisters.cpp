#include "LuaRegisters.h"
namespace Lua
{
namespace LuaRegisters
{
class luaRegister
{
public:
  inline luaRegister() {}
};

luaRegister* luaRegisterPointer = NULL;

luaRegister* GetLuaRegisterInstance()
{
  if (luaRegisterPointer == NULL)
    luaRegisterPointer = new luaRegister();
  return luaRegisterPointer;
}


void InitLuaRegistersFunctions(lua_State* luaState)
{
  luaRegister** luaRegisterPtrPtr = (luaRegister**)lua_newuserdata(luaState, sizeof(luaRegister*));
  *luaRegisterPtrPtr = GetLuaRegisterInstance();
  luaL_newmetatable(luaState, "LuaRegistersTable");
  lua_pushvalue(luaState, -1);
  lua_setfield(luaState, -2, "__index");

  luaL_Reg luaRegistersFunctions[] = {
    {"getRegister", getRegister},
    {"setRegister",  setRegister},
    {nullptr, nullptr}
  };
  luaL_setfuncs(luaState, luaRegistersFunctions, 0);
  lua_setmetatable(luaState, -2);
  lua_setglobal(luaState, "registers");
}

int getRegister(lua_State* luaState)
{
  return 1;
}

int setRegister(lua_State* luaState)
{
  return 0;
}

}  // namespace LuaRegisters
}  // namespace Lua
