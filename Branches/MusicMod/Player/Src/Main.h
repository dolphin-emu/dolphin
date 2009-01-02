////////////////////////////////////////////////////////////////////////////////
// Plainamp, Open source Winamp core
// 
// Copyright © 2005  Sebastian Pipping <webmaster@hartwork.org>
// 
// -->  http://www.hartwork.org
// 
// This source code is released under the GNU General Public License (GPL).
// See GPL.txt for details. Any non-GPL usage is strictly forbidden.
////////////////////////////////////////////////////////////////////////////////


#ifndef PA_MAIN_H
#define PA_MAIN_H



#include "Global.h"
#include "Winamp/wa_msgids.h"



// TODO



#define MENU_MAIN_WINDOWS_CONSOLE      41
#define MENU_MAIN_WINDOWS_MANAGER      42

#define MENU_MAIN_CONTEXT_MANAGER      WINAMP_MAIN_WINDOW // first window, for gen_dl
#define MENU_MAIN_CONTEXT_CONSOLE      WINAMP_OPTIONS_VIDEO // last window, for gen_template


#define PLAINAMP_TOGGLE_CONSOLE  50001
#define PLAINAMP_TOGGLE_MANAGER  50002




extern HWND WindowMain;
extern HMENU main_context_menu;



bool BuildMainWindow();
void About( HWND hParent );
LRESULT CALLBACK WndprocMain( HWND hwnd, UINT message, WPARAM wp, LPARAM lp );



#endif // PA_MAIN_H
