Dolphin VR 4.0-9170
(Unofficial Dolphin build with Oculus Rift and SteamVR support.)
Open Source licence: GPL v2+

The official Dolphin website: https://dolphin-emu.org
See the website for non-VR related help, and the Wiki for game specific information.
Don't both the official Dolphin devs with VR questions or issues, they don't know.

The VR branch: https://github.com/CarlKenner/dolphin/tree/VR-Hydra
Disscusion of the VR branch: https://forums.oculus.com/viewtopic.php?f=42&t=11241

##
Dolphin VR has a lot of options and it can be confusing to set them all correctly. Here's a quick setup
guide to help.

Direct mode is working, but the performance on many games is poor. It is recommended to run in
extended mode! If you do run in direct mode, make sure Aero is enabled. The only judder free configurations for direct mode
found so far are either 'dual core determinism' set to 'fake-completion' or 'synchronize GPU thread' enabled. The mirrored window must also
be enabled and manually resized to 0 pixels by 0 pixles. Testing has been limited so your mileage may vary.

In the 'Config' tab, 'Framelimit' should be set to match your Rift's refresh rate (most likely 75fps).
Games will run 25% too fast, but this will avoid judder. There are two options available to bring the game frame rate back down to
normal speed. The first is the 'Opcode Replay Buffer', which rerenders game frames so headtracking runs at 75fps forcing
the game to maintain its normal speed.  This feature only currently works in around 60% of games, but is the preferred
method.  Synchronous timewarp rotates the image to fake head-tracking at a higher rate.  This can be tried if 'Opcode Replay Buffer' fails
but sometimes introduces artifacts and may judder depending on the game. As a last resort, the the Rift can be
set to run at 60hz from your AMD/Nvidia control panel and still run with low persistence, but flickering can be seen
in bright scenes. Different games run at differnet frame-rates, so choose the correct opcode replay/timewarp setting for the game.\n\n"
Under Graphics->Hacks->EFB Copies, 'Disable' and 'Remove Blank EFB Copy Box' can fix some games
that display black, or large black boxes that can't be removed by the 'Hide Objects Codes'. It can also cause
corruption in some games, so only enable it if it's needed.

Right-clicking on each game will give you the options to adjust VR settings, remove rendered objects, and (in
a limited number of games) disable culling through the use of included AR codes. Culling codes put extra strain
on the emulated CPU as well as your PC.  You may need to override the Dolphin CPU clockspeed under
the advanced section of the 'config' tab to maintain a full game speed. This will only help if gamecube/wii CPU is
not fast enough to render the game without culling.

Objects such as fake 16:9 bars can be removed from the game.  Some games already have hide objects codes,
so make sure to check them if you would like the object removed.  You can find your own codes by starting
with an 8-bit code and clicking the up button until the object disappears.  Then move to 16bit, add two zeros
to the right side of your 8bit code, and continue hitting the up button until the object disappears again.
Continue until you have a long enough code for it to be unique.

Take time to set the VR-Hotkeys. You can create combinations for XInput by right clicking on the buttons. Remember you can also set
a the gamecube/wii controller emulation to not happen during a certain button press, so setting freelook to 'Left
Bumper + Right Analog Stick' and the gamecube C-Stick to '!Left Bumper + Right Analog Stick' works great. If you're
changing the scale in game, use the 'global scale' instead of 'units per metre' to maintain a consistent free look step size.\n\n"
Enjoy and watch for new updates!

How to run:

Optional: Copy sixense_x64.dll, iweardrv.dll, and iwrstdrv.dll into your Dolphin directory.

Oculus Rift DK1 to DK2, and HTC Vive are supported. You need the Oculus 0.6 or 0.7 runtime installed.
Both Direct and Extended modes work now.
Dolphin performs best if it is the only monitor connected to the desktop, but that
also makes the menus more difficult to use.
Plug the Rift in, turn it on, and make sure the service is running and not paused,
and if using DK1 make sure legacy DK1 support is turned off (this is not a legacy application),
before launching Dolphin.

Only Direct3D and OpenGL renderers will work. Direct3D is recommended.
In Graphics, turn OFF Render To Main Window.
In the Graphics options Enhancements tab set the internal resolution to a multiple
of the native. 1x Native is much too blurry, 2x or 2.5x are good choices.
Lower resoultions are faster but blurrier.
Anti-aliasing is apparently not working anymore.
eXternal Frame Buffer is always disabled regardless of what you choose.
The OnScreen Display or debug information doesn't work in VR mode, so don't bother.
The Asynchronous Timewarp option doesn't work any more (since SDK 0.4.3).

Choose a game and click the Play button to start. Acknowledge the warning, then put 
on the Rift. Keep the mouse over the Rift's window, and click to focus input.
Shift+R recenters the view.

Try toggling fullscreen with Alt+Enter if it doesn't look right.

There appears to be a memory leak that slows down the game if you play for a long time.
Try doing a save state and then exit and restart Dolphin and load the save state
if your performance drops. Also there may be issues with stopping a game then starting
another game without restarting Dolphin. Sorry, it's a work in progress.

Per-game Settings:
Virtual Reality needs to be the correct scale, and the 2D elements need to be at
the correct depth. It's different for every game.
You can edit the per-game settings by right clicking a game in the list and
choosing "properties". It doesn't take effect until you restart the game. 
You can also adjust the settings with the keyboard during the game.
And there's now a VR Game tab in the graphics options.

## Free-Look Keys:
(Free-Look must be turned on. These keys often conflict with game controls,
so you may need to map your game controls to other keys or gamepad.)
Shift+R reset free-look position, also resets Rift position.
Shift+WASD to move around. If movement is too slow, the scale is too large.
Shift+Q move down, Shift+E move up.
Shift+9 or Shift+0 halve or double speed. But fix the scale first instead.

## VR 3D Settings Keys:
(Only affects scenes with some 3D perspective elements.)
Shift+R to recenter Rift.
Shift+- and Shift+= to adjust the scale to make the world smaller or bigger!
(Scale is important for VR, so try to get it life-size. Make it smaller first 
until it is toy size, then bigger until it is life-size. Move the camera closer
with Free-Look so you can see close-up to a person.)
Shift+. and Shift+/ to move the HUD closer or further and change its size.
Shift+[ and Shift+] to make the HUD thinner or thicker.
Shift+P and Shift+; to move the camera closer or further from the action.
(Permanent per-game setting, not like Free-look. Also adjusts the HUD size
to compensate for new camera position so aiming is right at Aim-Distance.
Aim distance must be set in game properties.)
Shift+O and Shift+L to tilt the camera up or down by 5 degrees.
(Correct pitch is also important for VR, you want the floor horizontal.
But most third person games angle the camera down at various angles depending on
what is happening in the game. Tilting tha camera up 10 degrees is usually a 
good compromise.)
Esc to stop emulation and give you the option to save VR settings.

## VR 2D Settings:
(Only affects scenes which have no perspective projection and only 2D elements.)
Shift+I and Shift+K to tilt the 2D camera up or down by 5 degrees.
(This is good for top-down or 2.5D menus, intros, or games, for realism.)
Shift+U and Shift+J to move 2D screen out or in by 10cm, without changing size.
(I recommend less than 4 metres away so you can still feel its depth.)
Shift+M and Shift+, to scale 2D screen smaller or bigger.
(It looks like the screen is moving closer or further, but it isn't!
Stereoscopic depth isn't changing, just the size. I use this to make the screen
life size in Virtual Console games. Use Free-look to move closer to a person
to adjust the size to real-life.)
Shift+Y and Shift+H to move the 2D screen up and down by 10cm.
(Move the screen down/camera up for 2.5D games for more realism.
Move the screen up/camera down for 2D platform games to make the ground level
with the real floor. Move the screen up for top-down games so you can see better.)
*NEW* Shift+T and Shift+G to make the 2D screen thicker (more 3D) or thinner.
(Sometimes 2D games can be made 3D if they render sprites at different depths.
A thicker screen can also help fix Z-Fighting issues.)
Esc to stop emulation and give you the option to save VR settings.

## VR debugging keys:
(Don't use these!)
Shift+B and Shift+N to change selected debug layer.

## Other useful keys:
Alt+F5 to disconnect/reconnect virtual Wii remote.
Shift+F1 to Shift+F8 to save game with savestate 1 to 8.
(So you can exit and save and edit your settings.)
F1 to F8 to load game from savestate 1 to 8.
Esc to stop game
(It pauses to ask you to confirm stopping, so you can also use this to pause.
Take off the Rift after pressing this to view the dialog box.)
See the menus for more keys, these and other keys can be adjusted in the menus,
unlike the hardcoded Free-look and VR keys.

## Razer Hydra:
You need sixense_x64.dll (not included) in your folder if you want to use the Hydra.
Exit the Razer Hydra and Sixense system tray icons before playing.

Unlike the Rift, you can plug in or unplug the Razer Hydra at any time.

This probably also supports the new Sixense STEM controller that Sixense is bringing out soon, which is likely to be backwards compatible.

This patch has no effect (dolphin behaves the same as before except for a log message) unless the user has sixense_x64.dll (part of the Sixense driver) installed (or sixense.dll for 32 bit). Users can get it from http://sixense.com/windowssdkdownload and put it in their Dolphin exe folder.

When the dll is present, and a Hydra is connected, and not sitting in the dock, and Wiimote 1 is set to Emulated Wiimote, the Hydra will automatically control Wiimote 1 and its Nuchuk or Classic Controller extensions. It will also control Gamecube controller 1 if it is enabled. While holding the hydra, the actual tilt will override any other emulated tilt, and the actual pointing will override any other emulated IR, and the actual joysticks will override other emulated joysticks, but emulated buttons, swings and shakes from other inputs will still work additively.

It currently uses a fixed control scheme to make it plug-and-play:
## Wiimote:
A: right start button (middle)
B: right bumper or trigger (underneath)
Minus: right 3 button (top left)
Plus: right 4 button (top right)
Home/Recenter IR: push right stick in (top middle)
Dpad: right analog stick (top middle)
1: right 1 button (bottom left)
2: right 2 button (bottom right)
IR pointer: right controller's position in space relative to where you pressed Home
Orientation/Motion sensing: right controller's orientation/motion
Swap extension: push left stick in

## Nunchuk:
C: left bumper (top) or left 1 button (bottom left)
Z: left trigger (bottom) or left 2 button (bottom right)
Stick: left stick
Orientation/Motion sensing: left controller's orientation/motion

## Classic:
L: left analog trigger (bottom left unlike Classic Controller Pro)
ZL: left bumper (top left)
R: right analog trigger
ZR: right bumper
left stick: left stick
right stick: right stick
dpad: left buttons like if tilted inwards, 3=up, 2=down, 1=left, 4=right 
a, b, x, y: right buttons like if tilted inwards: 2=a, 1=b, 4=x, 3=y
minus: left start
plus: right start
home/recenter IR: push right stick in

## Sideways Wiimote (if left controller is docked):
turn right hydra sideways yourself, same as above wiimote controls

## Sideways Wiimote (if holding both controllers):
steering/tilt left-right: angle between controllers (like holding imaginary wheel or sideways wiimote with both hands)
tilt forwards-backwards: tilt controllers (only measures right one) forwards or backwards
motion sensing: shake or swing controllers (only measures right one)
1: right 1 button (bottom left)
2: right 2 button (bottom right)
A: left or right start button
B: left or right bumpers or triggers
minus: right 3 button (top left)
plus: right 4 button (top right)
home/recenter IR: push right stick in

## GameCube:
L: left analog trigger (bottom left like GameCube controller)
Z: left or right bumper (top left or top right)
R: right analog trigger (bottom right)
analog stick: left stick
C-substick: right stick
dpad: left buttons like if tilted inwards, 3=up, 2=down, 1=left, 4=right 
a, b, x, y: right buttons like GameCube layout: 2=a, 1=b, 4=x, 3=y
start: right start

## General:
Cycle between Wiimote/sideways Wiimote/Wiimote+Nunchuk/Classic: push left stick in
(note that doesn't affect the gamecube controller)
Recenter IR pointer: push right stick in (press again to get out of home screen)
Recenter hydra joysticks: dock then undock that controller
(if that doesn't help, disconnect and reconnect nunchuk/classic extension)

## Hydra Troubleshooting:
Hydra isn't recognised: make sure sixense_x64.dll is in Dolphin's exe folder (see above).
Wii cursor isn't showing: push the right analog stick in, then exit the home screen.
IR Pointer doesn't aim smoothly: it uses position rather than angle, so move rather than aiming.
Orientation/motion sensing is messed up: make sure base is straight with cables at back, then dock both controllers and pick them up again. If that doesn't work, make sure you have the Sideways Wiimote setting set correctly in Wiimote settings.
Hands are swapped: The nunchuk should be the controller that says LT (or q7) on the triggers, so hold that one in your left hand.
Joystick is messed up: dock then undock both controllers, then disconnect and reconnect the extension in Wiimote settings.

## The following compatability lists are out of date.

## VR Compatability for GameCube games:
These games have been tested, and had their defaults set to the correct scale.
They are listed in order of compatability from working best to most broken.
Games not listed have not been tested, need to have the scale set manually, and might or might not work.

Metroid Prime (but not the "Player's Choice version" 1.02)
Metroid Prime 2 (dark visor isn't fully functional)
Zelda: Wind Waker (dot in middle of screen)

Animal Crossing GameCube (near-clipping when turning head)
Bomberman Generation (angled camera)
Mario Party 4 (working, angled camera)
Alien Hominid (menus are the wrong scale)
Zelda Twilight Princess GameCube (Effects are missing, map is black, water has no reflections, hawkeye has problem in top left corner) 
Mario Party 5 (white characters at start)
Baldur's Gate Dark Alliance (lights at HUD depth, angled camera)
Hunter: The Reckoning (movies aren't visible, just click start until you see the menu or game)
Pikmin (3D HUD at wrong depth)
Pikmin 2 (3D HUD at wrong depth, far clipping)
Zelda Collector's Edition: OOT (Giant A button behind you)
Zelda Collector's Edition: Majora's Mask (Giant A button behind you)
Sonic Adventures DX (white subtitles and some screens)
Paper Mario (works, a few things render strangely)
Ikagura (HUD doesn't line up right yet)
Donkey Konga 2 (slight menu issues)
Tony Hawks Pro 4 (some intros missing head-tracking)
Four Swords Adventures (z-fighting, letterboxing)
Mario Kart Double Dash (dirty screen)
Soul Calibur 2 (HUD is ocluded by world behind it)
F-Zero GX (sort of works, resets)
Resident Evil 4 (intro weirdness, playable)
Zelda Collector's Edition: Wind Waker Demo (larger, sometimes distorted)
Star Fox Assault (menu depth problems)
Super Smash Bros. Melee (missing textures)
Eternal Darkness (HUD too close, scale not consistent)
Kirby Air (sometimes one eye, change internal resolution during game to fix)
Wave Race Blue Storm (water issues, wrong menu depths)
Need for Speed Underground (messed up menus)
Need for Speed Hot Pursuit 2 (menu and HUD problems)
Pokemon Coluseum (weird doubled menus and HUD, missing textures)
Killer 7 (impossible to proceed past menu)
Beyond Good and Evil (strobing)
Time Splitters (black noise)
Sonic Heroes (most things missing)
Luigi's Mansion (only intro works)

Battalion Wars (crashes when starting a level)
Zelda Collector's Edition: Zelda 1 (broken)
Zelda Collector's Edition: Zelda 2 (broken)
Game Boy Player (freezes)
Star Wars Rogue Squadron III (freezes)
Star Wars Rogue Leader (crashes)
Final Fantasy Crystal (Crash, CPU)
Super Mario Strikers (Crash)
Viewtiful Joe (crash)
Pokemon Box Ruby and Sapphire (crash)
Bloody War Primal Fury (crash)

## VR compatability for Nintendo 64 games:
These games have been tested, and had their defaults set to the correct scale.
They are listed in order of compatability from working best to most broken.
Only Custom Robo V2 has not been tested, needs to have the scale set manually, and might or might not work.

Super Mario Kart 64 on Virtual Console (sometimes flashing skybox)
Majora's Mask in Collector's Edition on GameCube (big A button behind you, too close)
Ocarina Of Time in Collector's Edition on GameCube (big A button behind you, too close, 2D backgrounds)
(K) Super Mario 64 on Virtual Console (very annoying flashing skybox)
Super Mario 64 on GameCube (very annoying flashing skybox)

(K) Kirby 64: The Crystal Shards (unplayable, lots of issues)
Majora's Mask (data is corrupt)
Ocarina Of Time on Virtual Console (data is corrupt)
Sin and Punishment (data is corrupt)
Yoshi's Story (data is corrupt)
Pokemon Snap (data is corrupt)
(K) 1080 Snowboarding (data is corrupt)
(K) Star Fox 64: Lylat Wars (OpenGL crash)
F-Zero X (OpenGL crash)
Paper Mario (OpenGL crash)
Wave Race 64 (OpenGL crash)
Cruis'n USA (OpenGL crash)
Pokémon Puzzle League (OpenGL crash)
Mario Golf (OpenGL crash)
Super Smash Bros. (OpenGL crash)
Ogre Battle 64: Person of Lordly Caliber (OpenGL crash)
Mario Tennis (OpenGL crash)
Mario Party 2 (OpenGL crash)
Bomberman Hero (OpenGL crash)

(Custom Robo V2 is untested)

## VR compatability for Super Nintendo:
These games have been tested, and had their defaults set to the correct scale.
They are listed in order of compatability from working best to most broken.
Unlisted games have not been tested, need to have the scale set manually, and might or might not work.

Donkey Kong Country
Super Castlevania IV
Super Mario RPG
A Link To The Past (low resolution?, wrong aspect ratio)
Super Metroid (wrong aspect ratio)
Super Mario World (wrong aspect ratio)

Chrono Trigger (very broken render to texture)

## VR compatability for Nintendo Entertainment System:
(None are playable)

## System Requirements
* OS
    * Microsoft Windows (Vista or higher).
    * Linux or Apple Mac OS X (10.7 or higher).
    * Unix-like systems other than Linux might work but are not officially supported.
* Processor
    * A CPU with SSE2 support.
    * A modern CPU (3 GHz and Dual Core, not older than 2008) is highly recommended.
* Graphics
    * A reasonably modern graphics card (Direct3D 10.0 / OpenGL 3.0).
    * A graphics card that supports Direct3D 11 / OpenGL 4.4 is recommended.

## Command line usage
(not needed for VR)
`Usage: Dolphin [-h] [-d] [-l] [-e <str>] [-b] [-V <str>] [-A <str>]`  

* -h, --help Show this help message  
* -d, --debugger Opens the debugger  
* -l, --logger Opens the logger  
* -e, --exec=<str> Loads the specified file (DOL,ELF,WAD,GCM,ISO)  
* -b, --batch Exit Dolphin with emulator  
* -V, --video_backend=<str> Specify a video backend  
* -A, --audio_emulation=<str> Low level (LLE) or high level (HLE) audio
* -vr  force Virtual Reality DK2 mode if no HMD is detected

Available DSP emulation engines are HLE (High Level Emulation) and
LLE (Low Level Emulation). HLE is fast but often less accurate while LLE is
slow but close to perfect. Note that LLE has two submodes (Interpreter and
Recompiler), which cannot be selected from the command line.

Available video backends are "D3D" (only available on Windows Vista or higher),
"OGL". There's also "Software Renderer", which uses the CPU for rendering and
is intended for debugging purposes, only.

## Sys Files
* `totaldb.dsy`: Database of symbols (for devs only)
* `GC/font_ansi.bin`: font dumps
* `GC/font_sjis.bin`: font dumps
* `GC/dsp_coef.bin`: DSP dumps
* `GC/dsp_rom.bin`: DSP dumps

The DSP dumps included with Dolphin have been written from scratch and do not
contain any copyrighted material. They should work for most purposes, however
some games implement copy protection by checksumming the dumps. You will need
to dump the DSP files from a console and replace the default dumps if you want
to fix those issues.

## Folder structure
These folders are installed read-only and should not be changed:

* `GameSettings`: per-game default settings database
* `GC`: DSP and font dumps
* `Maps`: symbol tables (dev only)
* `Shaders`: post-processing shaders
* `Themes`: icon themes for GUI
* `Wii`: default Wii NAND contents

## User folder structure
A number of user writeable directories are created for caching purposes or for
allowing the user to edit their contents. On OS X and Linux these folders are
stored in `~/Library/Application Support/Dolphin/` and `~/.dolphin-emu`
respectively. On Windows the user directory is stored in the `My Documents`
folder by default, but there are various way to override this behavior:

* Creating a file called `portable.txt` next to the Dolphin executable will
  store the user directory in a local directory called "User" next to the
  Dolphin executable.
* If the registry string value `LocalUserConfig` exists in
  `HKEY_CURRENT_USER/Dolphin Emulator` and has the value **1**, Dolphin will
  always start in portable mode.
* If the registry string value `UserConfigPath` exists in
  `HKEY_CURRENT_USER/Dolphin Emulator`, the user folders will be stored in the
  directory given by that string. The other two methods will be prioritized
  over this setting.


List of user folders:

* `Cache`: used to cache the ISO list
* `Config`: configuration files
* `Dump`: anything dumped from dolphin
* `GameConfig`: additional settings to be applied per-game
* `GC`: memory cards and system BIOS
* `Load`: custom textures
* `Logs`: logs, if enabled
* `ScreenShots`: screenshots taken via Dolphin
* `StateSaves`: save states
* `Wii`: Wii NAND contents

## Custom textures
Custom textures have to be placed in the user directory under
`Load/Textures/[GameID]/`. You can find the Game ID by right-clicking a game
in the ISO list and selecting "ISO Properties".
