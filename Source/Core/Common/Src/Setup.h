// Copyright (C) 2003-2008 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/


////////////////////////////////////////////////////////////////////////////////////////
// File description
/* ¯¯¯¯¯¯¯¯¯

   Compilation settings. This file can be kept on the ignore list in your SVN program. It
   allows local optional settings depending on what works on your computer.

////////////////////////*/



////////////////////////////////////////////////////////////////////////////////////////
// Settings
// ¯¯¯¯¯¯¯¯¯

// This may fix a problem with Stop and Start that I described in the comments to revision 2,139
//#define SETUP_FREE_PLUGIN_ON_BOOT

// This may fix a semi-frequent hanging that occured when I used single core and render to main frame
//#define SETUP_AVOID_SINGLE_CORE_HANG_ON_STOP

//////////////////////////