This is the dolphinWatch fork. If you want the normal readme go look at https://github.com/dolphin-emu/dolphin .

## What is DolphinWatch

DolphinWatch is a fork of the [Dolphin Wii and Gamecube Emulator](http://dolphin-emu.org) that allows for programatically control of Dolphin via a TCP socket connection. It was made for [TwitchPlaysPokemon](https://www.twitch.tv/twitchplayspokemon) to be able to automate Pokémon Battle Revolution matches. It originated [here](https://redd.it/3ithlh).

Communication happens through text-based commands over said socket connection. Execution of commands is asynchronous to the emulation, and they get polled roughly 20 times per second. Frame-perfect control is therefore impossible with DolphinWatch. An integrated scripting layer, like the integrated LUA-scripting known to many handheld emulators like VBA, would be suitable for frame-perfect control. But sadly Dolphin does not have such functionality (yet, or I didn't find it).

## Building

There are no automated builds for DolphinWatch, so you'd have to build it for yourself to get the latest version. Instructions for this are on the main repository linked at the start. Some prebuilt Windows binaries might get linked in this repo once in a while, you can use those too.

## How to get started

To start Dolphin with the DolphinWatch extension, run it with the command-line flag `/w` (`-w` if you're not stuck on Windows) followed by the port to use. We usually use port 6000, so start it from the command line with `Dolphin.exe /w 6000` Dolphin then starts an internal DolphinWatch server on that port clients can connect to.
Once a server is running, a socket connection can be opened. The text-based protocol that is used to control is documented below. There is a [Python implementation](https://github.com/TwitchPlaysPokemon/PyDolphinWatch) of DolphinWatch that can be used too.

## The protocol

DolphinWatch used a text-based protocol to communicate. Commands sent through the socket connection are of the following format:

```COMMAND arguments...\n```

Instead of a newline `\n` terminating a command, `;` can be used too. The only difference is that commands separated by `;` are guaranteed to be executed in the same emulation-frame, while commands separated by newlines `\n` are not. Arguments must therefore not contain these characters: ` ;\n`.

Memory addresses are local to the platform's emulated address space.
Most commands do not return anything. This also means that malformed commands might only trigger a log-error in Dolphin.


### client to server

#### `WRITE <mode> <addr> <val>`

This command usually is used to write single values.

mode
  : can either be 8, 16 or 32, depending on how many bits to write

addr
  : address where to write

val
  : number to write at given address. Must be an integer (numbers bigger than `<mode>` will get silently cast)


#### `WRITE_MULTI <addr> <val>...`

This command usually is used to write sequences or strings.

addr:
  : address where to write

val:
  : 32-bit number to write given address. all 32-bit `<val>`s are written consecutively, starting at `<addr>`


#### `READ <mode> <addr>`

This command usually is used to read single values. If no error occured, the server will send a `MEM` command for that address.

mode
  : can either be 8, 16 or 32, depending on how many bits to read

addr
  : address where to read from

#### `READ_MULTI`

There is no READ_MULTI. Not sure why, but there just isn't one right now.


#### `SUBSCRIBE <mode> <addr>`

This command usually is used to continuously read single values. If no error occured, the server will send a `MEM` command for that address. The server will also continue to send a `MEM` command each time the value at the given address changes.

mode
  : can either be 8, 16 or 32, depending on how many bits to read

addr
  : address where to subscribe to


#### `SUBSCRIBE_MULTI <size> <addr>...`

This command usually is used to continuously read sequences or strings. If no error occured, the server will send a `MEM_MULTI` command for that address space (`<addr>` and `<size>` consecutive 32-bit words). The server will also continue to send a `MEM_MULTI` command each time any of the values within the given address space changes.

size:
  : number of 32-bit values to subscribe to. all 32-bit values are read consecutively, starting at `<addr>`

addr:
  : address where to subscribe to


#### `UNSUBSCRIBE <addr>`

Undoes a previous `SUBSCRIBE` at address `<addr>`. The server will no longer send `MEM` commands when the value changes.

addr
  : address where to unsubscribe from


#### `UNSUBSCRIBE_MULTI <addr>`

Undoes a previous `SUBSCRIBE_MULTI` at address `<addr>`. The server will no longer send `MEM_MULTI` commands when any value in the address space changes.

addr
  : address where to unsubscribe from

#### `BUTTONSTATES_WII <i_wiimote> <states>`

Sets the buttonstates (pressed or not) of a Wiimote. 

i_wiimote
  : index of the Wiimote, between 0 and 3.

states
  : buttonstates represented as a 16-bit number. [Click here for the data-layout](http://wiibrew.org/wiki/Wiimote#Buttons)

*Implementation detail*: DolphinWatch hijacks the respective emulated wiimote to do this. All user-input on that wiimote will be ignored and replaced by these buttonstates for roughly 500ms.

#### `BUTTONSTATES_GC <i_gcpad> <states> <sx> <sy> <ssx> <ssy>`

Sets the buttonstates and joystick-positions of a Gamecube-Controller

i_gcpad
  : index of the Gamecube-Controller, between 0 and 3

states
  : buttonstates represented as a 16-bit number. [Click here for the data-layout](http://pastebin.com/raw.php?i=4txWae07)

sx
  : x-position of the main joystick, as a float between -1.0 and 1.0

sy
  : y-position of the main joystick, as a float between -1.0 and 1.0

ssx
  : x-position of the sub-joystick (c-stick), as a float between -1.0 and 1.0

ssy
  : y-position of the sub-joystick (c-stick), as a float between -1.0 and 1.0

#### `PAUSE`

Pauses emulation. Does nothing if already paused.

#### `RESUME`

Resumes emulation. Does nothing if not paused.

#### `RESET`

Resets the emulation. (Might not work properly with Wii games)

#### `SAVE <filename>`

Makes a savestate and saves it at the given location.

filename
  : location of the file to save to. `<filename>` can contain spaces because it is the last (and only) argument. `<filename>` must not contain any of these characters: `?"<>|`.

#### `LOAD <filename>`

`<filename>` underlies the same restrictions as in the SAVE command. Loads a savestate from the given location.
The server will either send "SUCCESS" or "FAIL" after this command was processed. This behavious is guaranteed, and it's recommended for the client to synchronously wait for this response before sending further commands.

#### `VOLUME <volume>`

Sets Dolphin's volume. Must be a number between 0 and 100.

#### `SPEED <speed>`

Sets Dolphin's emulation speed. Must be a float. Normal speed is 1, half speed is 0.5 etc, and 0 means unlimited.

#### `STOP`

Stops the emulation. There is no command to start a different emulation, so this pretty much is a one-way command for shutdown.

#### `INSERT <filename>`

`<filename>` underlies the same restrictions as in the SAVE command. Changes the inserted game.
**This command is not working right now and does nothing. [Why?](https://bugs.dolphin-emu.org/issues/9019)**

### server to client

#### `SUCCESS`

Indicating success of a previously transmitted command that returns whether it succeeded or not (currently only used by LOAD).

#### `FAIL`

Indicating failure of a previously transmitted command that returns whether it succeeded or not (currently only used by LOAD).

#### `MEM <addr> <val>`

Reports the current value at the given address. Size of the returned value depends on what <mode> was submitted to either READ or SUBSCRIBE.

addr
  : address where the value was read from

val
  : value at that address, as an integer.

#### `MEM_MULTI <addr> <val>...`

Reports the current values starting at the given address. 

addr
  : address where the values were read from.

val
  : consecutive 32-bit values read starting from <addr>
