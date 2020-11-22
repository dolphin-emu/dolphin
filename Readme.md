# Python Scripting support for the Dolphin Emulator

This is a fork of the [Dolphin GameCube and Wii Emulator](https://github.com/dolphin-emu/dolphin)
with added support for **Python Scripting**.
Check out the original repository for general information and build instructions.

## Getting Started

You can run a python script by clicking "Add New Script" in the Scripting panel (View -> Scripting).
Say you have a file `myscript.py`:
```python
from dolphin import event
while True:
    await event.frameadvance()
    print("Hello Dolphin!")
```
Then you should select that file in the file selection dialog.
Alternatively, launch Dolphin from a command line with e.g. `./Dolphin.exe --script myscript.py`
to automatically add a script at startup.

Start a game for the above script to output text.
To be able to see the script's output, enable the `Scripting` log type in the logging configuration (View -> Show Log Configuration).
Everything printed to `stdout` or `stderr` will then be visible in the log (View -> Show Log).


## API documentation

The API is organized as various python module aggregated into one module called `dolphin`.
For example, to access the memory module, import it via `from dolphin import memory`.

For comprehensive documentation of all API functions, please check out the **[dolphin module stubs](dolphin-stubs/)**.
Not only do those `.pyi` stub files serve as documentation for the API surface,
they can also be directly consumed by tools like linters or IDEs, e.g. for auto-completion.

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

It shouldn't, this is a bug. But given the immaturity of this project these things will likely happen occasionally.
Please file an issue!

> **Why does this only exist for Windows?**

There is nothing fundamentally stopping this to work on other OSes, it's just not done yet:
- The build steps are currently only implemented for Visual studio
  (e.g. [Source/VSProps/PyEmbed.props](Source/VSProps/PyEmbed.props), [Source/Core/Scripting/Scripting.vcxproj](Source/Core/Scripting/Scripting.vcxproj)),
  but building for anything but windows requires them to be implemented in CMake.
- Currently only the [Python externals](Externals/python) for Windows x86-64 are bundled.
  There are no embeddable Python distribution for Mac OS or *nix system readily available on python.org,
  so preparing the right externals could be a bit difficult (I haven't tried).

> **Why does it only exist for the x86-64 architecture?**

Basically the same reason as above:
Currently only the [Python externals](Externals/python) for Windows x86-64 are bundled.
There are no embeddable Python distribution for ARM64 readily available on python.org,
so preparing the right externals could be a bit difficult (I haven't tried).
