# **rcheevos**

**rcheevos** is a set of C code, or a library if you will, that tries to make it easier for emulators to process [RetroAchievements](https://retroachievements.org) data, providing support for achievements and leaderboards for their players.

Keep in mind that **rcheevos** does *not* provide HTTP network connections. Clients must get data from RetroAchievements, and pass the response down to **rcheevos** for processing.

Not all structures defined by **rcheevos** can be created via the public API, but are exposed to allow interactions beyond just creation, destruction, and testing, such as the ones required by UI code that helps to create them.

Finally, **rcheevos** does *not* allocate or manage memory by itself. All structures that can be returned by it have a function to determine the number of bytes needed to hold the structure, and another one that actually builds the structure using a caller-provided buffer to bake it.

## Lua

RetroAchievements is considering the use of the [Lua](https://www.lua.org) language to expand the syntax supported for creating achievements. The current expression-based implementation is often limiting on newer systems.

At this point, to enable Lua support, you must compile with an additional compilation flag: `HAVE_LUA`, as neither the backend nor the UI for editing achievements are currently Lua-enabled.

> **rcheevos** does *not* create or maintain a Lua state, you have to create your own state and provide it to **rcheevos** to be used when Lua-coded achievements are found. Calls to **rcheevos** may allocate and/or free additional memory as part of the Lua runtime.

Lua functions used in trigger operands receive two parameters: `peek`, which is used to read from the emulated system's memory, and `userdata`, which must be passed to `peek`. `peek`'s signature is the same as its C counterpart:

```lua
function peek(address, num_bytes, userdata)
```

## API

An understanding about how achievements are developed may be useful, you can read more about it [here](http://docs.retroachievements.org/Developer-docs/).

Most of the exposed APIs are documented [here](https://github.com/RetroAchievements/rcheevos/wiki)

### User Configuration

There's only one thing that can be configured by users of **rcheevos**: `RC_ALIGNMENT`. This macro holds the alignment of allocations made in the buffer provided to the parsing functions, and the default value is `sizeof(void*)`.

If your platform will benefit from a different value, define a new value for it on your compiler flags before compiling the code. It has to be a power of 2, but no checking is done.

### Return values

Any function in the rcheevos library that returns a success indicator will return one of the following values.

These are in `rc_error.h`.

```c
enum {
  RC_OK = 0,
  RC_INVALID_LUA_OPERAND = -1,
  RC_INVALID_MEMORY_OPERAND = -2,
  RC_INVALID_CONST_OPERAND = -3,
  RC_INVALID_FP_OPERAND = -4,
  RC_INVALID_CONDITION_TYPE = -5,
  RC_INVALID_OPERATOR = -6,
  RC_INVALID_REQUIRED_HITS = -7,
  RC_DUPLICATED_START = -8,
  RC_DUPLICATED_CANCEL = -9,
  RC_DUPLICATED_SUBMIT = -10,
  RC_DUPLICATED_VALUE = -11,
  RC_DUPLICATED_PROGRESS = -12,
  RC_MISSING_START = -13,
  RC_MISSING_CANCEL = -14,
  RC_MISSING_SUBMIT = -15,
  RC_MISSING_VALUE = -16,
  RC_INVALID_LBOARD_FIELD = -17,
  RC_MISSING_DISPLAY_STRING = -18,
  RC_OUT_OF_MEMORY = -19,
  RC_INVALID_VALUE_FLAG = -20,
  RC_MISSING_VALUE_MEASURED = -21,
  RC_MULTIPLE_MEASURED = -22,
  RC_INVALID_MEASURED_TARGET = -23,
  RC_INVALID_COMPARISON = -24,
  RC_INVALID_STATE = -25,
  RC_INVALID_JSON = -26
};
```

To convert the return code into something human-readable, pass it to:
```c
const char* rc_error_str(int ret);
```

### Console identifiers

This enumeration uniquely identifies each of the supported platforms in RetroAchievements.

These are in `rc_consoles.h`.

```c
enum {
  RC_CONSOLE_MEGA_DRIVE = 1,
  RC_CONSOLE_NINTENDO_64 = 2,
  RC_CONSOLE_SUPER_NINTENDO = 3,
  RC_CONSOLE_GAMEBOY = 4,
  RC_CONSOLE_GAMEBOY_ADVANCE = 5,
  RC_CONSOLE_GAMEBOY_COLOR = 6,
  RC_CONSOLE_NINTENDO = 7,
  RC_CONSOLE_PC_ENGINE = 8,
  RC_CONSOLE_SEGA_CD = 9,
  RC_CONSOLE_SEGA_32X = 10,
  RC_CONSOLE_MASTER_SYSTEM = 11,
  RC_CONSOLE_PLAYSTATION = 12,
  RC_CONSOLE_ATARI_LYNX = 13,
  RC_CONSOLE_NEOGEO_POCKET = 14,
  RC_CONSOLE_GAME_GEAR = 15,
  RC_CONSOLE_GAMECUBE = 16,
  RC_CONSOLE_ATARI_JAGUAR = 17,
  RC_CONSOLE_NINTENDO_DS = 18,
  RC_CONSOLE_WII = 19,
  RC_CONSOLE_WII_U = 20,
  RC_CONSOLE_PLAYSTATION_2 = 21,
  RC_CONSOLE_XBOX = 22,
  RC_CONSOLE_MAGNAVOX_ODYSSEY2 = 23,
  RC_CONSOLE_POKEMON_MINI = 24,
  RC_CONSOLE_ATARI_2600 = 25,
  RC_CONSOLE_MS_DOS = 26,
  RC_CONSOLE_ARCADE = 27,
  RC_CONSOLE_VIRTUAL_BOY = 28,
  RC_CONSOLE_MSX = 29,
  RC_CONSOLE_COMMODORE_64 = 30,
  RC_CONSOLE_ZX81 = 31,
  RC_CONSOLE_ORIC = 32,
  RC_CONSOLE_SG1000 = 33,
  RC_CONSOLE_VIC20 = 34,
  RC_CONSOLE_AMIGA = 35,
  RC_CONSOLE_ATARI_ST = 36,
  RC_CONSOLE_AMSTRAD_PC = 37,
  RC_CONSOLE_APPLE_II = 38,
  RC_CONSOLE_SATURN = 39,
  RC_CONSOLE_DREAMCAST = 40,
  RC_CONSOLE_PSP = 41,
  RC_CONSOLE_CDI = 42,
  RC_CONSOLE_3DO = 43,
  RC_CONSOLE_COLECOVISION = 44,
  RC_CONSOLE_INTELLIVISION = 45,
  RC_CONSOLE_VECTREX = 46,
  RC_CONSOLE_PC8800 = 47,
  RC_CONSOLE_PC9800 = 48,
  RC_CONSOLE_PCFX = 49,
  RC_CONSOLE_ATARI_5200 = 50,
  RC_CONSOLE_ATARI_7800 = 51,
  RC_CONSOLE_X68K = 52,
  RC_CONSOLE_WONDERSWAN = 53,
  RC_CONSOLE_CASSETTEVISION = 54,
  RC_CONSOLE_SUPER_CASSETTEVISION = 55,
  RC_CONSOLE_NEO_GEO_CD = 56,
  RC_CONSOLE_FAIRCHILD_CHANNEL_F = 57,
  RC_CONSOLE_FM_TOWNS = 58,
  RC_CONSOLE_ZX_SPECTRUM = 59,
  RC_CONSOLE_GAME_AND_WATCH = 60,
  RC_CONSOLE_NOKIA_NGAGE = 61,
  RC_CONSOLE_NINTENDO_3DS = 62,
  RC_CONSOLE_SUPERVISION = 63,
  RC_CONSOLE_SHARPX1 = 64,
  RC_CONSOLE_TIC80 = 65,
  RC_CONSOLE_THOMSONTO8 = 66,
  RC_CONSOLE_PC6000 = 67,
  RC_CONSOLE_PICO = 68,
  RC_CONSOLE_MEGADUCK = 69,
  RC_CONSOLE_ZEEBO = 70
};
```

## Runtime support

The runtime encapsulates a set of achievements, leaderboards, and rich presence for a game and manages processing them for each frame. When important things occur, events are raised for the caller via a callback.

These are in `rc_runtime.h`.

The `rc_runtime_t` structure uses several forward-defines. If you need access to the actual contents of any of the forward-defined structures, those definitions are in `rc_runtime_types.h`

```c
typedef struct rc_runtime_t {
  rc_runtime_trigger_t* triggers;
  unsigned trigger_count;
  unsigned trigger_capacity;

  rc_runtime_lboard_t* lboards;
  unsigned lboard_count;
  unsigned lboard_capacity;

  rc_runtime_richpresence_t* richpresence;

  rc_memref_value_t* memrefs;
  rc_memref_value_t** next_memref;

  rc_value_t* variables;
  rc_value_t** next_variable;
}
rc_runtime_t;
```

The runtime must first be initialized.
```c
void rc_runtime_init(rc_runtime_t* runtime);
```

Then individual achievements, leaderboards, and even rich presence can be loaded into the runtime. These functions return RC_OK, or one of the negative value error codes listed above.
```c
int rc_runtime_activate_achievement(rc_runtime_t* runtime, unsigned id, const char* memaddr, lua_State* L, int funcs_idx);
int rc_runtime_activate_lboard(rc_runtime_t* runtime, unsigned id, const char* memaddr, lua_State* L, int funcs_idx);
int rc_runtime_activate_richpresence(rc_runtime_t* runtime, const char* script, lua_State* L, int funcs_idx);
```

The runtime should be called once per frame to evaluate the state of the active achievements/leaderboards:
```c
void rc_runtime_do_frame(rc_runtime_t* runtime, rc_runtime_event_handler_t event_handler, rc_runtime_peek_t peek, void* ud, lua_State* L);
```

The `event_handler` is a callback function that is called for each event that occurs when processing the frame.
```c
typedef struct rc_runtime_event_t {
  unsigned id;
  int value;
  char type;
}
rc_runtime_event_t;

typedef void (*rc_runtime_event_handler_t)(const rc_runtime_event_t* runtime_event);
```

The `event.type` field will be one of the following:
* RC_RUNTIME_EVENT_ACHIEVEMENT_ACTIVATED (id=achievement id)
  An achievement starts in the RC_TRIGGER_STATE_WAITING state and cannot trigger until it has been false for at least one frame. This event indicates the achievement is no longer waiting and may trigger on a future frame.
* RC_RUNTIME_EVENT_ACHIEVEMENT_PAUSED (id=achievement id)
  One or more conditions in the achievement have disabled the achievement.
* RC_RUNTIME_EVENT_ACHIEVEMENT_RESET (id=achievement id)
  One or more conditions in the achievement have reset any progress captured in the achievement.
* RC_RUNTIME_EVENT_ACHIEVEMENT_TRIGGERED (id=achievement id)
  All conditions for the achievement have been met and the user should be informed.
  NOTE: If `rc_runtime_reset` is called without deactivating the achievement, it may trigger again.
* RC_RUNTIME_EVENT_ACHIEVEMENT_PRIMED (id=achievement id)
  All non-trigger conditions for the achievement have been met. This typically indicates the achievement is a challenge achievement and the challenge is active.
* RC_RUNTIME_EVENT_LBOARD_STARTED (id=leaderboard id, value=leaderboard value)
  The leaderboard's start condition has been met and the user should be informed that a leaderboard attempt has started.
* RC_RUNTIME_EVENT_LBOARD_CANCELED (id=leaderboard id, value=leaderboard value)
  The leaderboard's cancel condition has been met and the user should be informed that a leaderboard attempt has failed.
* RC_RUNTIME_EVENT_LBOARD_UPDATED (id=leaderboard id, value=leaderboard value)
  The leaderboard value has changed.
* RC_RUNTIME_EVENT_LBOARD_TRIGGERED (id=leaderboard id, value=leaderboard value)
  The leaderboard's submit condition has been met and the user should be informed that a leaderboard attempt was successful. The value should be submitted.
* RC_RUNTIME_EVENT_ACHIEVEMENT_DISABLED (id=achievement id)
  The achievement has been disabled by a call to `rc_invalidate_address`.
* RC_RUNTIME_EVENT_LBOARD_DISABLED (id=leaderboard id)
  The achievement has been disabled by a call to `rc_invalidate_address`.

When an achievement triggers, it should be deactivated so it won't trigger again:
```c
void rc_runtime_deactivate_achievement(rc_runtime_t* runtime, unsigned id);
```
Additionally, the unlock should be submitted to the server.

When a leaderboard triggers, it should not be deactivated in case the player wants to try again for a better score. The value should be submitted to the server. 

For `RC_RUNTIME_EVENT_LBOARD_UPDATED` and `RC_RUNTIME_EVENT_LBOARD_TRIGGERED` events, there is a helper function to call if you wish to display the leaderboard value on screen.

```c
int rc_runtime_format_lboard_value(char* buffer, int size, int value, int format);
```

`rc_runtime_do_frame` also periodically updates the rich presense string (every 60 frames). To get the current value, call
```c
const char* rc_runtime_get_richpresence(const rc_runtime_t* runtime);
```

When the game is reset, the runtime should also be reset:
```c
void rc_runtime_reset(rc_runtime_t* runtime);
```

This ensures any active achievements/leaderboards are set back to their initial states and prevents unexpected triggers when the memory changes in atypical way.

## Server Communication

**rapi** builds URLs to access many RetroAchievements web services. Its purpose it to just to free the developer from having to URL-encode parameters and build correct URLs that are valid for the server.

**rapi** does *not* make HTTP requests.

NOTE: **rapi** is a replacement for **rurl**. **rurl** has been deprecated.

These are in `rc_api_user.h`, `rc_api_runtime.h` and `rc_api_common.h`.

The basic process of making an **rapi** call is to initialize a params object, call a function to convert it to a URL, send that to the server, then pass the response to a function to convert it into a response object, and handle the response values.

An example can be found on the [rc_api_init_login_request](https://github.com/RetroAchievements/rcheevos/wiki/rc_api_init_login_request#example) page.

### Functions

Please see the [wiki](https://github.com/RetroAchievements/rcheevos/wiki) for details on the functions exposed for **rapi**.

## Game Identification

**rhash** provides logic for generating a RetroAchievements hash for a given game. There are two ways to use the API - you can pass the filename and let rhash open and process the file, or you can pass the buffered copy of the file to rhash if you've already loaded it into memory.

These are in `rc_hash.h`.

```c
  int rc_hash_generate_from_buffer(char hash[33], int console_id, uint8_t* buffer, size_t buffer_size);
  int rc_hash_generate_from_file(char hash[33], int console_id, const char* path);
```



