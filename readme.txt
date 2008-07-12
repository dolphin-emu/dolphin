Dolphin
=======
Open Source Release under GPL 2

by F|RES & ector

Contributions by:
zerofrog, yaz0r, tmbinc

DX9 gfx plugin by ector
OpenGL gfx plugin by zerofrog

System Requirements:
* Windows 2000 or higher. The Linux build is at least 80% done, it starts,
  but won't run games yet. There's a bug somewhere.
* Fast CPU. Dual core a plus.
* Any reasonable modern GPU, support for pixel shaders 2.0 required.

New Features
* Interprets Action Replay codes
* Buggy framerate limiter, deactivated
* Full Xbox 360 Controller support, with rumble

Dolphin is a Gamecube emulator. It also has preliminary support for Wii and 
Triforce emulation.

Gamecube compatibility is about the same as previous releases.

Wii compatibility is very low. Some games show their intro movies, that's it.
There is preliminary Wiimote emulation but it does not work yet.

We haven't worked on Dolphin for a long time, except for a short burst adding
the basic Wii support, and we don't think we will finish it, so we hereby 
release it, in its current unfinished state, to the world.

Usage Notes
-----------
The GUI should be pretty much self-explanatory.

To attempt to boot Wii ISOs, you need the disc master key. It should be
called "masterkey.bin". Put it in the Wii subdirectory.

To use Action Replay codes, follow the examples in the Patches subdirectory.
Use + in front of a cheat name to activate it. The cheats can be named 
anything.

Code Notes
----------
Dolphin is written in C++. It's compiled with Visual Studio 2005. An
installation of the DirectX SDK is required.

To tweak settings for the JIT/Dynarec, see Core/PowerPC/Jit64/JitCache.cpp,
in InitCache().

We have a scons build system set up, but if anyone wants to create an 
automake solution, go ahead, we'd appreciate it.

We are looking for new source code maintainers.