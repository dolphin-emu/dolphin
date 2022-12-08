# Python Scripting support for the Dolphin Emulator

This is a fork of the [Dolphin GameCube and Wii Emulator](https://github.com/dolphin-emu/dolphin)
with added support for **Python Scripting**.
Check out the original repository for general information and build instructions.

## Getting Started

You can run a python script by clicking "Add New Script" in the Scripting panel (View -> Scripting).
Say you have a file `myscript.py`:
```python
from dolphin import event, gui

red = 0xffff0000
frame_counter = 0
while True:
    await event.frameadvance()
    frame_counter += 1
    # draw on screen
    gui.draw_text((10, 10), red, f"Frame: {frame_counter}")
    # print to console
    if frame_counter % 60 == 0:
        print(f"The frame count has reached {frame_counter}")
```
Then you should select that file in the file selection dialog.
Alternatively, launch Dolphin from a command line with e.g. `./Dolphin.exe --script myscript.py`
to automatically add a script at startup.

Start a game for the above script to start drawing a frame counter in the top left corner.

To be able to see the script's output, enable the `Scripting` log type in the logging configuration (View -> Show Log Configuration) and set the verbosity to "Error" or lower (not "Notice").
Everything printed to `stdout` or `stderr` will then be visible in the log (View -> Show Log).


## API documentation

The API is organized as various python module aggregated into one module called `dolphin`.
For example, to access the memory module, import it via `from dolphin import memory`.
For comprehensive documentation of all API functions, please check out the **[dolphin module stubs](python-stubs/dolphin)**.
The stub files serve as documentation for the API surface.

Additionally, if you are using an IDE and place the `dolphin` stub module directory somewhere it gets recognized as a python module
(e.g. next to the python scripts you are working on) they get recognized and can give you useful features like auto-completion.

If both the stub files and the rest of this section fail to explain something,
please let me know and I will attempt to improve the documentation.

### events

All events are implemented in two different paradigms: as awaitable coroutines and as a callback you can register.
The function to register a callback is always the event name prefixed with `on_`.
```python
from dolphin import event

# callback style
def my_callback():
    print("next frame")
event.on_frameadvance(my_callback)

# async style
while True:
    await event.frameadvance()
    print("next frame")
```

Each event can only have one listener attached at a time.
Repeated calls to `event.on_*()` will unregister the previous listener.
Registering `None` also unregisters the listener.

Events can return any number of values.
Depending on the paradigm used those must be part of the callback function's signature,
or are returned by the await statement as a tuple.
```python
from dolphin import event

# callback style
def my_callback(is_write: bool, addr: int, value: int):
    if is_write:
        print(f"{addr:08x} was changed to {value}")
event.on_memorybreakpoint(my_callback)

# async style.
# If the event has more than 1 argument, the result is returned as a tuple.
(is_write, addr, value) = await event.memorybreakpoint()
```


## Debugging scripts

You can theoretically debug embedded scripts the same way you could debug any other python application running remotely.
I tried using [debugpy](https://github.com/microsoft/debugpy) from within Visual Studio Code and that worked fine.
See [Python debug configurations in Visual Studio Code](https://code.visualstudio.com/docs/python/debugging)
for a good guide on how to get started.
Here are some pitfalls I encountered:

- Dolphin may call into the python code from a different thread than the thread
  the initial script execution was invoked from.
  For example, if you want to add breakpoints to emulation events you need to call
  `debugpy.debug_this_thread()` once after some emulation event occurred.
  Alternatively you can add `breakpoint()` calls to your python code.
- you need to call `debugpy.configure(python="/path/to/python")` before
  `debugpy.listen(5678)`, otherwise you get a timeout and the debugger won't work.
  See https://github.com/microsoft/debugpy/issues/262 for more details.
- `debugpy` will print errors regarding internal generated filenames not being absolute.
  That looks scary but doesn't seem to be a problem during debugging.
  See https://bugs.python.org/issue20443 (probably, I haven't tried it with 3.9 yet)


## FAQ / Q&A

> **Why isn't there a function for X?**

The Scripting API is very minimal right now, but nothing speaks against it growing.
Please open an issue describing your needs and I will try to add it!

> **Why does the emulator crash or hang?**

You may be using a library in your script that does not support python subinterpreters (e.g. SciPy or NumPy, see https://github.com/Felk/dolphin/issues/9).
Please try running dolphin with the `--no-python-subinterpreters` command-line option.
If that does not help, please file an issue!

> **Why does it only exist for the x86-64 architecture?**

There is nothing fundamentally stopping this to work on ARM as well,
but currently only the Python [externals](Externals) for Windows x86-64 are bundled.
There are no embeddable Python distribution for ARM64 readily available on python.org,
so preparing the right externals could be a bit difficult (I haven't tried).
