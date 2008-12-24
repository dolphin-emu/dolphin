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

#include "Globals.h"
#include "IniFile.h"
#include "svnrev.h"
#include "Config.h"
#include "Render.h"
#include "nGLUtil.h"

GLWindow *glWin = NULL;

void OpenGL_SwapBuffers()
{
    glWin->SwapBuffers();
}

void OpenGL_SetWindowText(const char *text) 
{
    glWin->SetWindowText(text);
}

unsigned int Callback_PeekMessages()
{
    return glWin->PeekMessages();
}

void UpdateFPSDisplay(const char *text)
{
    char temp[512];
    sprintf(temp, "SVN R%s: GL: %s", SVN_REV_STR, text);
    OpenGL_SetWindowText(temp);

}

// =======================================================================================
// Create window. Called from main.cpp
bool OpenGL_Create(SVideoInitialize &_VideoInitialize, 
		   int width, int height) 
{
    g_VideoInitialize.pPeekMessages = &Callback_PeekMessages;
    g_VideoInitialize.pUpdateFPSDisplay = &UpdateFPSDisplay;

    if (strncasecmp(g_Config.iBackend, "sdl", 10) == 0)
	glWin = new SDLWindow(width, height);
    else if (strncasecmp(g_Config.iBackend, "x11", 10) == 0)
	glWin = new X11Window(width, height);
    else if (strncasecmp(g_Config.iBackend, "wxgl", 10) == 0)
	glWin = new WXGLWindow(width, height);
    else
	PanicAlert("Invalid backend %s", g_Config.iBackend);
    
    return (glWin?true:false);
}

bool OpenGL_MakeCurrent()
{
    return glWin->MakeCurrent();
}


// =======================================================================================
// Update window width, size and etc. Called from Render.cpp
// ----------------
void OpenGL_Update()
{
    glWin->Update();
}



// =======================================================================================
// Close plugin
// ----------------
void OpenGL_Shutdown()
{
    delete glWin;
}

u32 OpenGL_GetWidth() {
    return glWin->GetWidth();
}

u32 OpenGL_GetHeight() {
    return glWin->GetHeight();
}

void OpenGL_SetSize(u32 width, u32 height) {
    glWin->SetSize(width, height);
}

int OpenGL_GetXoff() {
    return glWin->GetXoff();
}

int OpenGL_GetYoff() {
    return glWin->GetYoff();
}

float OpenGL_GetXmax() {
    return glWin->GetXmax();
}

float OpenGL_GetYmax() {
    return glWin->GetYmax();
}

void OpenGL_AddBackends(ConfigDialog *frame) {
    if(SDLWindow::valid())
	frame->AddRenderBackend("SDL");
    if(X11Window::valid())
	frame->AddRenderBackend("X11");
    if(WXGLWindow::valid())
	frame->AddRenderBackend("WXGL");
}

void OpenGL_AddResolutions(ConfigDialog *frame) {
    // TODO get resolution iter
}
