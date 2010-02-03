nJoy v0.3 by Falcon4ever 2008
A Dolphin Compatible Input Plugin
Copyright (C) 2003 Dolphin Project.

Changelog
==========================================================================

0.3: 3nd version of nJoy
 - Release: July 2008
 - Support for four players
 - Misc. improvements

0.2: 2nd version of nJoy
 - Release: 14th July 2005
 - Now using SDL instead of DirectInput9
 - Config/INI support
 - Adjustable deadzone
 - First public release

0.1: First version of nJoy
 - Private Release: january 2005
 - Using DirectInput9
 - Specially made for the Logitech Rumblepad 2, but other joypads work too
 - Advanced debug window, activated during gameplay
 - No deadzone configurable
 - No config/Ini
  
The Author
==========================================================================
* Falcon4ever (nJoy@falcon4ever.com)

System and Software Requirements	
==========================================================================
Dolphin 
  The latest Dolphin release, avaible at www.dolphin-emu.com

SDL.dll	(SDL-1.2.13)
  (included with this release) 
  latest version avaible at www.libsdl.org

A Joystick
  A Windows 9x compatible input device


Plugin Information
==========================================================================
nJoy was written in C++, compiled with Microsoft Visual Studio 2005 Professional Edition. 
nJoy uses SDL for joysticks, mouse and keyboard. 
For the graphical interface plain Win32 code was used.

How to install
==========================================================================
Just unzip the content of the zipfile to your dolphin plugin dir and place
sdl.dll in the root dir.

example config:
[C:]
 |
 +-Dolphin         Dir
  +-DolphinWx.exe  File
  +-SDL.dll        File
  |
  +-Plugins        Dir
   +-nJoy.dll      File


FAQ
==========================================================================

What's SDL???
  SDL is the Simple DirectMedia Layer written by Sam Lantinga.
  It provides an API for audio, video, input ...
  For more information go to http://www.libsdl.org/

Where can I download the latest releases??? 
  nJoy will be released @ www.multigesture.net

Can I mirror this file???
   Sure, just don't forget to add a link to:
   www.multigesture.net OR www.dolphin-emu.com
   --------------------    -------------------
Why should I use nJoy instead of the default input plugin???
  At this moment the default plugin only supports keyboard input.
  nJoy supports Joysticks. And besides that, if you have an GC-adapter
  you can use your original GC controllers with dolphin !

Could you add [insert feature here] please???
  no.

But perhaps...
  NO!

Hmm... There is coming smoke out of my pc, wtf?
  err, this plugin comes without any warranty,
  use it at own risk :)

What should I do if my question isn't listed here???
  Just panic, call 911 or leave a message on:
  (1) Emutalk http://www.emutalk.net/forumdisplay.php?f=100
  (2) NGemu http://forums.ngemu.com/dolphin-discussion/

Thanks / Greetings
==========================================================================

Special Thanks too:
F|RES & ector

Greetings too:
`plot`, Absolute0, Aprentice, Bositman, Brice, ChaosCode, CKemu, 
CoDeX, Dave2001, dn, drk||Raziel, Florin, Gent, Gigaherz, Hacktarux, 
icepir8, JegHegy, Linker, Linuzappz, Martin64, Muad, Knuckles, Raziel, 
Refraction, Rudy_x, Shadowprince, Snake785, Saqib, vEX, yaz0r, 
Zilmar, Zenogais and ZeZu.

AAaannd everyone else I forgot ;)...
