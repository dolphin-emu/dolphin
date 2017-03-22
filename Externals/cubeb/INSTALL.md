# Build instructions for libcubeb

You must have CMake v3.1 or later installed.

1. `git clone --recursive https://github.com/kinetiknz/cubeb.git`
2. `mkdir cubeb-build`
3. `cd cubeb-build`
3. `cmake ../cubeb`
4. `cmake --build .`
5. `ctest`

# Windows build notes

Windows builds can use Microsoft Visual Studio 2015 (the default) or MinGW-w64
with Win32 threads (by passing `cmake -G` to generate the appropriate build
configuration).  To build with MinGW-w64, install the following items:

- Download and install MinGW-w64 with Win32 threads.
- Download and install CMake.
- Run MinGW-w64 Terminal from the Start Menu.
- Follow the build steps above, but at step 3 run:
  `cmake -G "MinGW Makefiles" ..`
- Continue the build steps above.

