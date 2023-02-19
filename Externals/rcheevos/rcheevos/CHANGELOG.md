# v10.2.0

* add RC_MEMSIZE_16_BITS_BE, RC_MEMSIZE_24_BITS_BE, and RC_MEMSIZE_32_BITS_BE
* add secondary flag for RC_CONDITION_MEASURED that tells the UI when to show progress as raw vs. as a percentage
* add rapi calls for fetch_leaderboard_info, fetch_achievement_info and fetch_game_list
* add hash support for RC_CONSOLE_PSP
* add RCHEEVOS_URL_SSL compile flag to use https in rurl functions
* add space to "PC Engine" label
* update RC_CONSOLE_INTELLIVISION memory map to acknowledge non-8-bit addresses
* standardize to z64 format when hashing RC_CONSOLE_N64
* prevent generating hash for PSX disc when requesting RC_CONSOLE_PLAYSTATION2
* fix wrong error message being returned when a leaderboard was only slightly malformed

# v10.1.0

* add RC_RUNTIME_EVENT_ACHIEVEMENT_UNPRIMED
* add rc_runtime_validate_addresses
* add external memory to memory map for Magnavox Odyssey 2
* fix memory map base address for NeoGeo Pocket
* fix bitcount always returning 0 when used in rich presence

# v10.0.0

* add rapi sublibrary for communicating with server (eliminates need for client-side JSON parsing; client must still
  provide HTTP functionality). rurl is now deprecated
* renamed 'rhash.h' to 'rc_hash.h' to eliminate conflict with system headers, renamed 'rconsoles.h' and 'rurl.h' for 
  consistency
* split non-runtime functions out of 'rcheevos.h' as they're not needed by most clients
* allow ranges in rich presence lookups
* add rc_richpresence_size_lines function to fetch line associated to error when processing rich presence script
* add rc_runtime_invalidate_address function to disable achievements when an unknown address is queried
* add RC_CONDITION_RESET_NEXT_IF
* add RC_CONDITION_SUB_HITS
* support MAXOF operator ($) for leaderboard values using trigger syntax
* allow RC_CONDITION_PAUSE_IF and RC_CONDITION_RESET_IF in leaderboard value expression
* changed track parameter of rc_hash_cdreader_open_track_handler to support three virtual tracks:
  RC_HASH_CDTRACK_FIRST_DATA, RC_HASH_CDTRACK_LAST and RC_HASH_CDTRACK_LARGEST.
* changed offset parameter of rc_hash_filereader_seek_handler and return value of rc_hash_filereader_tell_handler
  from size_t to int64_t to support files larger than 2GB when compiling in 32-bit mode.
* reset to default cd reader if NULL is passed to rc_hash_init_custom_cdreader
* add hash support for RC_CONSOLE_DREAMCAST, RC_CONSOLE_PLAYSTATION_2, RC_CONSOLE_SUPERVISION, and RC_CONSOLE_TIC80
* ignore headers when generating hashs for RC_CONSOLE_PC_ENGINE and RC_CONSOLE_ATARI_7800
* require unique identifier when hashing RC_CONSOLE_SEGA_CD and RC_CONSOLE_SATURN discs
* add expansion memory to RC_CONSOLE_SG1000 memory map
* rename RC_CONSOLE_MAGNAVOX_ODYSSEY -> RC_CONSOLE_MAGNAVOX_ODYSSEY2
* rename RC_CONSOLE_AMIGA_ST -> RC_CONSOLE_ATARI_ST
* add RC_CONSOLE_SUPERVISION, RC_CONSOLE_SHARPX1, RC_CONSOLE_TIC80, RC_CONSOLE_THOMSONTO8
* fix error identifying largest track when track has multiple bins
* fix memory corruption error when cue track has more than 6 INDEXs
* several improvements to data storage for conditions (rc_memref_t and rc_memref_value_t structures have been modified)

# v9.2.0

* fix issue identifying some PC-FX titles where the boot code is not in the first data track
* add enums and labels for RC_CONSOLE_MAGNAVOX_ODYSSEY, RC_CONSOLE_SUPER_CASSETTEVISION, RC_CONSOLE_NEO_GEO_CD,
  RC_CONSOLE_FAIRCHILD_CHANNEL_F, RC_CONSOLE_FM_TOWNS, RC_CONSOLE_ZX_SPECTRUM, RC_CONSOLE_GAME_AND_WATCH,
  RC_CONSOLE_NOKIA_NGAGE, RC_CONSOLE_NINTENDO_3DS

# v9.1.0

* add hash support and memory map for RC_CONSOLE_MSX
* add hash support and memory map for RC_CONSOLE_PCFX
* include parent directory when hashing non-arcade titles in arcade mode
* support absolute paths in m3u
* make cue scanning case-insensitive
* expand SRAM mapping for RC_CONSOLE_WONDERSWAN
* fix display of measured value when another group has an unmeasured hit count
* fix memory read error when hashing file with no extension
* fix possible divide by zero when using RC_CONDITION_ADD_SOURCE/RC_CONDITION_SUB_SOURCE
* fix classification of secondary RC_CONSOLE_SATURN memory region

# v9.0.0

* new size: RC_MEMSIZE_BITCOUNT
* new flag: RC_CONDITION_OR_NEXT
* new flag: RC_CONDITION_TRIGGER
* new flag: RC_CONDITION_MEASURED_IF
* new operators: RC_OPERATOR_MULT / RC_OPERATOR_DIV
* is_bcd removed from memref - now part of RC_MEMSIZE
* add rc_runtime_t and associated functions
* add rc_hash_ functions
* add rc_error_str function
* add game_hash parameter to rc_url_award_cheevo
* remove hash parameter from rc_url_submit_lboard
* add rc_url_ping function
* add rc_console_ functions

# v8.1.0

* new flag: RC_CONDITION_MEASURED
* new flag: RC_CONDITION_ADD_ADDRESS
* add rc_evaluate_trigger - extended version of rc_test_trigger with more granular return codes
* make rc_evaluate_value return a signed int (was unsigned int)
* new formats: RC_FORMAT_MINUTES and RC_FORMAT_SECONDS_AS_MINUTES
* removed " Points" text from RC_FORMAT_SCORE format
* removed RC_FORMAT_OTHER format. "OTHER" format now parses to RC_FORMAT_SCORE
* bugfix: AddHits will now honor AndNext on previous condition

# v8.0.1

* bugfix: prevent null reference exception if rich presence contains condition without display string
* bugfix: 24-bit read from memory should only read 24-bits

# v8.0.0

* support for prior operand type
* support for AndNext condition flag
* support for rich presence
* bugfix: update delta/prior memory values while group is paused
* bugfix: allow floating point number without leading 0
* bugfix: support empty alt groups

# v7.1.1

* Address signed/unsigned mismatch warnings

# v7.1.0

* Added the RC_DISABLE_LUA macro to compile rcheevos without Lua support

# v7.0.2

* Make sure the code is C89-compliant
* Use 32-bit types in Lua
* Only evaluate Lua operands when the Lua state is not `NULL`

# v7.0.1

* Fix the alignment of memory allocations

# v7.0.0

* Removed **rjson**

# v6.5.0

* Added a schema for errors returned by the server

# v6.4.0

* Added an enumeration with the console identifiers used in RetroAchievements

# v6.3.1

* Pass the peek function and the user data to the Lua functions used in operands.

# v6.3.0

* Added **rurl**, an API to build URLs to access RetroAchievements web services.

# v6.2.0

* Added **rjson**, an API to easily decode RetroAchievements JSON files into C structures.

# v6.1.0

* Added support for 24-bit operands with the `'W'` prefix (`RC_OPERAND_24_BITS`)

# v6.0.2

* Only define RC_ALIGNMENT if it has not been already defined

# v6.0.1

* Use `sizeof(void*)` as a better default for `RC_ALIGNMENT`

# v6.0.0

* Simplified API: separate functions to get the buffer size and to parse `memaddr` into the provided buffer
* Fixed crash trying to call `rc_update_condition_pause` during a dry-run
* The callers are now responsible to pass down a scratch buffer to avoid accesses to out-of-scope memory

# v5.0.0

* Pre-compute if a condition has a pause condition in its group
* Added a pre-computed flag that tells if the condition set has at least one pause condition
* Removed the link to the previous condition in a condition set chain

# v4.0.0

* Fixed `ret` not being properly initialized in `rc_parse_trigger`
* Build the unit tests with optimizations and `-Wall` to help catch more issues
* Added `extern "C"` around the inclusion of the Lua headers so that **rcheevos** can be compiled cleanly as C++
* Exposed `rc_parse_value` and `rc_evaluate_value` to be used with rich presence
* Removed the `reset` and `dirty` flags from the external API

# v3.2.0

* Added the ability to reset triggers and leaderboards
* Add a function to parse a format string and return the format enum, and some unit tests for it

# v3.1.0

* Added `rc_format_value` to the API

# v3.0.1

* Fixed wrong 32-bit value on 64-bit platforms

# v3.0.0

* Removed function rc_evaluate_value from the API

# v2.0.0

* Removed leaderboard callbacks in favor of a simpler scheme

# v1.1.2

* Fixed NULL pointer deference when there's an error during the parse

# v1.1.1

* Removed unwanted garbage
* Should be v1.0.1 :/

# v1.0.0

* First version
