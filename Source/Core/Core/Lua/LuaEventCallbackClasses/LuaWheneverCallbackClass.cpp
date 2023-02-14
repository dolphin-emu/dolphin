#include "Core/Lua/LuaEventCallbackClasses/LuaWheneverCallbackClass.h"

#include <array>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

#include "Core/Lua/LuaHelperClasses/LuaColonCheck.h"
#include "Core/Lua/LuaVersionResolver.h"

namespace Lua::LuaWheneverCallback
{

struct LuaThreadToFunctionReference
{
  lua_State* lua_thread;
  int function_reference;
};

class LuaWhenever
{
public:
  inline LuaWhenever() {}
};

static std::vector<LuaThreadToFunctionReference> list_of_functions_to_run;
static bool list_of_functions_is_empty = true;
static std::unique_ptr<LuaWhenever> instance = nullptr;
static std::thread lua_whenever_callback_runner_thread;
static std::mutex list_of_functions_lock;
static std::condition_variable list_of_functions_condition_variable;
static std::mt19937_64 rng_generator;
static int temp_int;
static std::mutex* general_lua_lock;
static bool initialized_whenever_thread = false;

LuaWhenever* GetInstance()
{
  if (instance == nullptr)
    instance = std::make_unique<LuaWhenever>(LuaWhenever());
  return instance.get();
}

static const char* class_name = "Whenever";

void InitLuaWheneverCallbackFunctions(lua_State* lua_state, const std::string& lua_api_version,
                                      std::mutex* new_general_lua_lock)
{
  general_lua_lock = new_general_lua_lock;
  rng_generator.seed(time(nullptr));
  LuaWhenever** lua_whenever_instance_ptr_ptr =
      (LuaWhenever**)lua_newuserdata(lua_state, sizeof(LuaWhenever*));
  *lua_whenever_instance_ptr_ptr = GetInstance();
  luaL_newmetatable(lua_state, "LuaWheneverFunctionsMetaTable");
  lua_pushvalue(lua_state, -1);
  lua_setfield(lua_state, -2, "__index");
  std::array lua_whenever_functions_list_with_versions_attached = {

      luaL_Reg_With_Version({"register", "1.0", Register}),
      luaL_Reg_With_Version({"unregister", "1.0", Unregister})};

  std::unordered_map<std::string, std::string> deprecated_functions_map;
  AddLatestFunctionsForVersion(lua_whenever_functions_list_with_versions_attached,
                               lua_api_version, deprecated_functions_map, lua_state);
  lua_setglobal(lua_state, class_name);
  if (!initialized_whenever_thread)
    lua_whenever_callback_runner_thread = std::thread(RunCallbacks);
  initialized_whenever_thread = true;
}

int Register(lua_State* lua_state)
{
  list_of_functions_is_empty = false;
  LuaColonOperatorTypeCheck(lua_state, class_name, "register", "(functionName)");
  lua_pushvalue(lua_state, 2);
  int new_function_reference = luaL_ref(lua_state, LUA_REGISTRYINDEX);
  lua_State* new_lua_thread = lua_newthread(lua_state);
  list_of_functions_to_run.push_back({new_lua_thread, new_function_reference});
  lua_pushinteger(lua_state, new_function_reference); // used to unregister later on.
  list_of_functions_condition_variable.notify_one();
  return 1;
}

int Unregister(lua_State* lua_state)
{
  LuaColonOperatorTypeCheck(lua_state, class_name, "unregister",
                            "(uniqueFunctionNumberReturnedByRegisterFunction)");
  int reference_of_function_to_delete = luaL_checkinteger(lua_state, 2);
  for (auto it = list_of_functions_to_run.begin(); it != list_of_functions_to_run.end(); ++it)
  {
    if (it->function_reference == reference_of_function_to_delete)
    {
      luaL_unref(lua_state, LUA_REGISTRYINDEX, reference_of_function_to_delete);
      list_of_functions_to_run.erase(it);
      break;
    }
  }
  if (list_of_functions_to_run.size() == 0)
    list_of_functions_is_empty = true;
  else
    list_of_functions_is_empty = false;
  list_of_functions_condition_variable.notify_one();
  return 0;
}

// This function will be called by a seperate thread which just runs Whenever callbacks.
int RunCallbacks()
{
  while (true)
  {
    if (list_of_functions_to_run.empty())
    {
      std::unique_lock<std::mutex> un_l(*general_lua_lock);
      list_of_functions_condition_variable.wait_for(un_l, std::chrono::seconds(3));
    }
    else
    {
      general_lua_lock->lock();
      size_t list_index = rng_generator() % list_of_functions_to_run.size();
      lua_State* next_lua_thread = list_of_functions_to_run[list_index].lua_thread;
      int next_function_reference = list_of_functions_to_run[list_index].function_reference;
      lua_rawgeti(next_lua_thread, LUA_REGISTRYINDEX, next_function_reference);
      lua_resume(next_lua_thread, nullptr, 0, &temp_int);
      general_lua_lock->unlock();
    }

  }
}
}  // namespace Lua::LuaWheneverCallback
