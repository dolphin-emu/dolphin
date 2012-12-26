// Copyright (C) 2003 Dolphin Project.

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

#include "VideoConfig.h"
#include "Host.h"
#include "RenderBase.h"

#include "VertexShaderManager.h"
#include "../GLInterface.h"
#include "WX.h"

void cInterfaceWX::Swap()
{
	GLWin.glCanvas->SwapBuffers();
}

void cInterfaceWX::UpdateFPSDisplay(const char *text)
{
	// Handled by Host_UpdateTitle()
}

// Create rendering window.
//		Call browser: Core.cpp:EmuThread() > main.cpp:Video_Initialize()
bool cInterfaceWX::Create(void *&window_handle)
{
	int _tx, _ty, _twidth, _theight;
	Host_GetRenderWindowSize(_tx, _ty, _twidth, _theight);

	// Control window size and picture scaling
	s_backbuffer_width = _twidth;
	s_backbuffer_height = _theight;

	GLWin.panel = (wxPanel *)window_handle;
	GLWin.glCanvas = new wxGLCanvas(GLWin.panel, wxID_ANY, NULL,
		wxPoint(0, 0), wxSize(_twidth, _theight));
	GLWin.glCanvas->Show(true);
	if (GLWin.glCtxt == NULL) // XXX dirty hack
		GLWin.glCtxt = new wxGLContext(GLWin.glCanvas);
}

bool cInterfaceWX::MakeCurrent()
{
	return GLWin.glCanvas->SetCurrent(*GLWin.glCtxt);
}

// Update window width, size and etc. Called from Render.cpp
void cInterfaceWX::Update()
{
	int width, height;

	GLWin.panel->GetSize(&width, &height);
	if (width == s_backbuffer_width && height == s_backbuffer_height)
		return;

	GLWin.glCanvas->SetFocus();
	GLWin.glCanvas->SetSize(0, 0, width, height);
	GLWin.glCtxt->SetCurrent(*GLWin.glCanvas);
	s_backbuffer_width = width;
	s_backbuffer_height = height;
}

// Close backend
void cInterfaceWX::Shutdown()
{
	GLWin.glCanvas->Hide();
}


