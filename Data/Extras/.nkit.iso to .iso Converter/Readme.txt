- - - - - -__- - -.- - - -.- - -.- - -.- - \|/.- - -.- - -.-__- -.- - - -
 .  .  ___/ |__ .    __ .   .  .  .     . --O--.  __  . ___/ | _____ .  .
       \__ +   \ ___/ |__      __    .  __ /|\___/ |__  \__  |/ +  /   .
  .   . /   | + \\__     \ ___/ |__.___/ |__  \__     \ ./   |    / .
    .  /+   |    \/  _|   \\__     \\__     \  /   |   \/ +   +  /_   .
.     /     |+    \  \_    \/   |   \/   |   \/    |    \    |     \ .  .
   . /    + |   +  \_ |     \_  |    \_  |    \_   |     \_+ |  +   \ .
.  _/  +    |_______/ |______/  |_____/  ______/   _______/  |       \_ .
  .\      + |==\______|===\_____|==\_____|==\______|==\______|+    +  /
- - \_______| - - - - - - -  Proudly Presents  - - - - - - - |_______/ - -

      NKit - Tools to Recover / Preserve Wii and GameCube Disc Images


  .
.     .  .

Overview
----------------------------------------------------------------------------
NKit is a Nintendo ToolKit that can Recover and Preserve Wii and GameCube
disc images 

Recovery is the ability to rebuild source images to match the known good
         images verified by Redump 

Preserve is the ability to shrink any image and convert it back to the
         source iso

NKit can convert to ISO and NKit format. The NKit format is designed to
shrink an image to it's smallest while ensuring it can be restored back to
the original source data. NKit images are also playable by Dolphin 

Read the Docs :) - https://wiki.gbatemp.net/wiki/NKit

  .
.     .  .
Features
----------------------------------------------------------------------------

- Recovery and Preservation of GameCube and Wii images
- NKit Format (smallest format, Dolphin compatible, GC is h/w compatible)
- GC NKit format aligns audio and tgc files to 32k for playability
- Wii NKit format removes hashes and encryption (Dolphin Compatible)
- Reusable library for use in other projects
- Test Mode
- Summary log of all conversions
- Rename images that match Dat file entries to a configurable mask
- Supports 100% of Redump images (Unlicensed, All Regions, Multi Disc etc)
- Scrubbed and Hacked preservation support

Formats
- GCZ support
- ISO.DEC read support
- WBFS read support
- ISO support
- RVT-R Wii ISO read support
- NKit Format support (smallest format and Dolphin compatible)
- Read the above images from Rar/Zip/7zip etc

Recovery Abilities
- Insert missing Wii Update (inc rare extra data), Channel / VC partitions
- Replace Brickblocked Update partitions
- Auto fixes modified disc headers (where the Data header remains intact)
- Fixes rare corrupt Wii partition table (caused by WBM)
- Fixes scrubbed trailing file 0's
- Fixes truncated Wii images (where the Data partition is intact)
- Fixes slightly overdumped images (from descramble tools)
- Fixes Wii Data partitions moved before 0xF800000 to save space
- Fixes compacted GC images
- Fixes moved and reordered GC files
- Fixes modified GC headers (inc. title, region hacks)
- Fixes mod chip modified GC apploaders
- Fixes GC images with non conformant junk
- Fixes modified Wii region and ratings
- Support for GC images with junk not generated with the image ID

  .
.     .  .
Requirements
- Windows | Linux | Mac
- .Net or Mono 4.6.1 (Will be ported to dotnet core 3.0 soon)
- Wine for Apps under Linux and Mac
  .
.     .  .
