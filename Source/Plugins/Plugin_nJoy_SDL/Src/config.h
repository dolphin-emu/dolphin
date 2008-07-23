//////////////////////////////////////////////////////////////////////////////////////////
// Project description
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
// Name: nJoy 
// Description: A Dolphin Compatible Input Plugin
//
// Author: Falcon4ever (nJoy@falcon4ever.com)
// Site: www.multigesture.net
// Copyright (C) 2003-2008 Dolphin Project.
//
//////////////////////////////////////////////////////////////////////////////////////////
//
// Licensetype: GNU General Public License (GPL)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.
//
// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/
//
// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/
//
//////////////////////////////////////////////////////////////////////////////////////////

// whole file... until we fix the GUI
#ifdef _WIN32

#include <windows.h>    // includes basic windows functionality
#include <Windowsx.h>
#include <stdio.h>
#include <commctrl.h>   // includes the common control header

#include "../Res/resource.h"	// includes GUI IDs

#pragma comment(lib, "comctl32.lib")

//////////////////////////////////////////////////////////////////////////////////////////
// Config dialog functions
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯

int OpenConfig(HINSTANCE hInst, HWND _hParent);
BOOL ControllerTab(HWND hDlg, UINT message, UINT wParam, LONG lParam, int controller);
BOOL APIENTRY ControllerTab1(HWND hDlg, UINT message, UINT wParam, LONG lParam);
BOOL APIENTRY ControllerTab2(HWND hDlg, UINT message, UINT wParam, LONG lParam);
BOOL APIENTRY ControllerTab3(HWND hDlg, UINT message, UINT wParam, LONG lParam);
BOOL APIENTRY ControllerTab4(HWND hDlg, UINT message, UINT wParam, LONG lParam);

bool GetButtons(HWND hDlg, int buttonid, int controller);
bool GetHats(HWND hDlg, int buttonid, int controller);
bool GetAxis(HWND hDlg, int buttonid, int controller);

void UpdateVisibleItems(HWND hDlg, int controllertype);

void GetControllerAll(HWND hDlg, int controller);
void SetControllerAll(HWND hDlg, int controller);

int GetButton(HWND hDlg, int item);
void SetButton(HWND hDlg, int item, int value);

//////////////////////////////////////////////////////////////////////////////////////////
// About dialog functions
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯

void OpenAbout(HINSTANCE hInst, HWND _hParent);
BOOL CALLBACK AboutDlg(HWND abouthWnd, UINT message, WPARAM wParam, LPARAM lParam);

#endif
