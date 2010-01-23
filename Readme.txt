Dolphin-emu - The Gamecube / Wii Emulator
==========================================
Homesite: http://dolphin-emu.com/
Project Site: http://code.google.com/p/dolphin-emu

About: Dolphin-emu is a emulator for Gamecube, Wii, Triforce that lets you run Wii/GCN/Tri games on your Windows/Linux/MacOSX PC system

Open Source Release under GPL 2

Project Leaders: F|RES, ector

Team members: zerofrog, vincent.hamm, falc4ever, Sonicadvance1, tmator, shuffle2, gigaherz, mthuurne, drkIIRaziel, masken, DacoTaco, XTra.KrazzY, nakeee, memberTwo.mb2, donkopunchstania, jpeterson, omegadox, lpfaint99, bushing, magumagu9, mizvekov, cooliscool, facugaich


Please read the FAQ before use: http://code.google.com/p/dolphin-emu/wiki/Facts_And_Questions

System Requirements:
* OS: Mircrosoft Windows (2000/XP/Vista or higher) or Linux or Apple Mac OS X.
* Processor: Fast CPU with SSE2 supported (recommended at least 2Ghz). Dual Core for speed boost.
* Graphics: Any graphics card that supports Direct3D 9 or OpenGL 2.1.


[Windows Version]
Dolphin.exe: The main program 
Arguments:
-d		Run Dolphin with the debugger tools
-l		Run Dolphin with the logmanager enabled
-e=<path>	Open an elf file

cg.dll/cgGL.dll: Cg Shading API (http://developer.nvidia.com/object/cg_toolkit.html)
wiiuse.dll: Wiimote Bluetooth API (http://www.wiiuse.net/)
SDL: Simple DirectMedia Layer API (http://www.libsdl.org/)
*.pdb = Program Debug Database (use these symbols with a program debugger)
[DSP Plugins]
Plugin_DSP_LLE.dll: Low Level DSP Emulation
Plugin_DSP_HLE.dll: High Level DSP Emulation (only emulates AX UCodes)
[Video plugins]
Plugin_VideoDX9.dll: Render with Direct3D 9 (outdated video plugin)
Plugin_VideoOGL.dll: Render with OpenGL + Cg Shader Language
[Gamecube Controller Plugins]
Plugin_GCPad.dll: Use keyboard or joypads
[Wiimote plugins]
Plugin_Wiimote.dll: Use native wiimote or keyboard

[Linux Version]
Usage: Dolphin [-h] [-d] [-l] [-e <str>]
  -h, --help     	Prints usage message
  -d, --debugger 	Run Dolphin with the debugger tools
  -l, --logger   	Run Dolphin with the logmanager enabled
  -e, --elf=<path>	Loads an elf file

[Mac Version]
(someone fill this out)

[Sys Files]
totaldb.dsy: Database of symbols (for devs only)
font_ansi.bin/font_sjis.bin: font dumps
setting-usa/jpn/usa.txt: config files for Wii

[Config Folders]
Cache: used to cache the ISO list
Dump: Anything dumped from dolphin will go here
GameConfig: Holds the INI game config files
GC: Gamecube Memory Cards
Logs: logs go here
Maps: symbol tables go here (dev only)
ScreenShots: screenshots are saved here
SaveStates: save states are stored here
Wii: Wii saves and config is stored here
