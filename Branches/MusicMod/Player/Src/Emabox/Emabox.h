/*//////////////////////////////////////////////////////////////////////////////
// ExtraMessageBox
// 
// Copyright © 2006  Sebastian Pipping <webmaster@hartwork.org>
// 
// -->  http://www.hartwork.org
// 
// This source code is released under the GNU General Public License (GPL).
// See GPL.txt for details. Any non-GPL usage is strictly forbidden.
//////////////////////////////////////////////////////////////////////////////*/


#ifndef EXTRA_MESSAGE_BOX_H
#define EXTRA_MESSAGE_BOX_H 1

#include "EmaboxConfig.h"
#include <windows.h>
#include <tchar.h>



/*
== TYPE =============================================================================
#define MB_TYPEMASK                   15                                         1111
#define MB_OK                         0                                          0000
#define MB_OKCANCEL                   1                                          0001
#define MB_ABORTRETRYIGNORE           2                                          0010
#define MB_YESNOCANCEL                3                                          0011
#define MB_YESNO                      4                                          0100
#define MB_RETRYCANCEL                5                                          0101
#define MB_CANCELTRYCONTINUE          6                                          0110
*/

#define MB_YESNOCANCELALL             7                                       /* 0111 */
#define MB_YESNOALL                   8                                       /* 1000 */

/*
== ICON =============================================================================
#define MB_ICONMASK                   240                                    11110000
#define MB_ICONERROR                  16                                     00010000
#define MB_ICONHAND                   16                                     00010000
#define MB_ICONSTOP                   16                                     00010000
#define MB_ICONQUESTION               32                                     00100000
#define MB_ICONEXCLAMATION            0x30                                   00110000
#define MB_ICONWARNING                0x30                                   00110000
#define MB_ICONINFORMATION            64                                     01000000
#define MB_ICONASTERISK               64                                     01000000
#define MB_USERICON                   128                                    10000000

== DEFAULT BUTTON ===================================================================
#define MB_DEFMASK                    3840                               111100000000
#define MB_DEFBUTTON1                 0                                  000000000000
#define MB_DEFBUTTON2                 256                                000100000000
#define MB_DEFBUTTON3                 512                                001000000000
#define MB_DEFBUTTON4                 0x300                              001100000000
*/

#define MB_DEFBUTTON5                 1024                            /* 010000000000 */
#define MB_DEFBUTTON6                 1280                            /* 010100000000 */

/*
== MODE =============================================================================
#define MB_MODEMASK                   0x00003000                       11000000000000
#define MB_APPLMODAL                  0                                00000000000000
#define MB_SYSTEMMODAL                4096                             01000000000000
#define MB_TASKMODAL                  0x2000                           10000000000000

== MISC =============================================================================
#define MB_MISCMASK                   0x0000C000                     1100000000000000
#define MB_HELP                       0x4000                         0100000000000000
#define MB_NOFOCUS                    0x00008000                     1000000000000000

== FLAGS ============================================================================
#define MB_SETFOREGROUND              0x10000                       10000000000000000
#define MB_DEFAULT_DESKTOP_ONLY       0x20000                      100000000000000000
#define MB_TOPMOST                    0x40000                     1000000000000000000
#define MB_SERVICE_NOTIFICATION_NT3X  0x00040000                  1000000000000000000
#define MB_SERVICE_NOTIFICATION       0x00040000                  1000000000000000000
#define MB_TOPMOST                    0x40000                     1000000000000000000
#define MB_RIGHT                      0x80000                    10000000000000000000
#define MB_RTLREADING                 0x100000                  100000000000000000000
#define MB_SERVICE_NOTIFICATION       0x00200000               1000000000000000000000

== EXTRA FLAGS ======================================================================
*/

#define MB_EXTRAMASC                  0xF0000000  /* 11110000000000000000000000000000 */

#define MB_CHECKMASC                  0xC0000000  /* 11000000000000000000000000000000 */
#define MB_CHECKNONE                  0           /* 00000000000000000000000000000000 */
#define MB_CHECKNEVERAGAIN            0x40000000  /* 01000000000000000000000000000000 */
#define MB_CHECKREMEMBERCHOICE        0x80000000  /* 10000000000000000000000000000000 */

#define MB_CLOSEMASK                  0x30000000  /* 00110000000000000000000000000000 */
#define MB_NORMALCLOSE                0           /* 00000000000000000000000000000000 */
#define MB_DISABLECLOSE               0x10000000  /* 00010000000000000000000000000000 */
#define MB_NOCLOSE                    0x20000000  /* 00100000000000000000000000000000 */



/* Function aliases */
#define ExtraMessageBoxLive      EmaBoxLive
#define ExtraMessageBoxDie       EmaBoxDie
#define ExtraMessageBox          EmaBox
#define ExtraMessageBoxEx        EmaBoxEx
#define ExtraMessageBoxIndirect  EmaBoxIndirect



int EmaBoxLive();

int EmaBoxDie();

int EmaBox(
	HWND hWnd,
	LPCTSTR lpText,
	LPCTSTR lpCaption,
	UINT uType,
	int * pbCheckRes
);

int EmaBoxEx(
	HWND hWnd,
	LPCTSTR lpText,
	LPCTSTR lpCaption,
	UINT uType,
	WORD wLanguageId,
	int * pbCheckRes
);

int EmaBoxIndirect(
	const LPMSGBOXPARAMS lpMsgBoxParams,
	int * pbCheckRes
);



#endif /* EXTRA_MESSAGE_BOX_H */
