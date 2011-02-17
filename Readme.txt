Dolphin-emu - The Gamecube / Wii Emulator
==========================================
Homesite: http://dolphin-emulator.com/
Project Site: http://code.google.com/p/dolphin-emu

Dolphin-emu is a emulator for Gamecube, Wii, Triforce that lets
you run Wii/GCN/Tri games on your Windows/Linux/Mac PC system.

Open Source Release under GPL 2

Project Leaders: F|RES, ector

Team members: http://code.google.com/p/dolphin-emu/people/

Please read the FAQ before use:

http://code.google.com/p/dolphin-emu/wiki/Facts_And_Questions

System Requirements:
* OS: Microsoft Windows (2000/XP/Vista or higher) or Linux or Apple Mac OS X.
* Processor: Fast CPU with SSE2 supported (recommended at least 2Ghz).
  Dual Core for speed boost.
* Graphics: Any graphics card that supports Direct3D 9 or OpenGL 2.1.

[Command line usage]
Usage: Dolphin [-h] [-d] [-l] [-e <str>] [-b] [-V <str>] [-A <str>]
  -h, --help                	Show this help message
  -d, --debugger            	Opens the debugger
  -l, --logger              	Opens the logger
  -e, --exec=<str>          	Loads the specified file (DOL,ELF,WAD,GCM,ISO)
  -b, --batch             	Exit Dolphin with emulator
  -V, --video_backend=<str>  	Specify a video plugin
  -A, --audio_emulation=<str>  	Low level (LLE) or high level (HLE) audio

[Libraries]
Cg: Cg Shading API (http://developer.nvidia.com/object/cg_toolkit.html)
*.pdb = Program Debug Database (use these symbols with a program debugger)

[DSP Plugins]
Plugin_DSP_HLE: High Level DSP Emulation
Plugin_DSP_LLE: Low Level DSP Emulation

[Video Backends]
Direct3D9: Render with Direct3D 9
Direct3D11: Render with Direct3D 11
OpenGL: Render with OpenGL + Cg Shader Language

[Sys Files]
totaldb.dsy: Database of symbols (for devs only)
font_ansi.bin/font_sjis.bin: font dumps
setting-usa/jpn/usa.txt: config files for Wii

[Support Folders]
Cache: used to cache the ISO list
Config: emulator configuration files
Dump: anything dumped from dolphin will go here
GC: Gamecube memory cards
GameConfig: holds the INI game config files
Load: high resolution textures
Logs: logs go here
Maps: symbol tables go here (dev only)
OpenCL: OpenCL code
ScreenShots: screenshots are saved here
Shaders: post-processing shaders
StateSaves: save states are stored here
Wii: Wii saves and config is stored here
