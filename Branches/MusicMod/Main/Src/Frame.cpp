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


//////////////////////////////////////////////////////////////////////////////////////////
// Include
// ¯¯¯¯¯¯¯¯¯¯
#include <iostream> // System

#include "Core.h" // Core

#include "IniFile.h"		// Common

#include "../../../../Source/Core/DolphinWX/Src/Globals.h" // DolphinWX
#include "../../../../Source/Core/DolphinWX/Src/Frame.h"

#include "../../Common/Console.h" // Local
#include "../../Player/Src/PlayerExport.h" // Player
//////////////////////////////////



//////////////////////////////////////////////////////////////////////////////////////////
// Declarations and definitions
// ¯¯¯¯¯¯¯¯¯¯
namespace MusicMod
{
	bool GlobalMute = false;
	bool GlobalPause = false;
	bool bShowConsole = false;
	int GlobalVolume = 255;
	extern bool dllloaded;
}
//////////////////////////////////



//////////////////////////////////////////////////////////////////////////////////////////
// Change the brightness of a wxBitmap
// ¯¯¯¯¯¯¯¯¯¯
wxBitmap SetBrightness(wxBitmap _Bitmap, int _Brightness, bool Gray)
{
	wxImage _Image = _Bitmap.ConvertToImage();
	wxImage _Image2 = _Bitmap.ConvertToImage();

	wxString Tmp;

	if(_Brightness < 0) _Brightness = 0; // Limits
	if(_Brightness > 255) _Brightness = 255;

	_Brightness = 255 - _Brightness; // Make big values brighter

	// Remove the alpha layer first
	for(int y = 0; y < _Bitmap.GetWidth(); y++)
	{
		for(int x = 0; x < _Bitmap.GetHeight(); x++)
			_Image.SetAlpha(x, y, 255);
	}

	for(int y = 0; y < _Bitmap.GetWidth(); y++)
	{
		//Tmp += wxString::Format("\n %i: ", y);

		for(int x = 0; x < _Bitmap.GetHeight(); x++)
		{
			u8 R = _Image.GetRed(x, y); // Get colors
			u8 G = _Image.GetGreen(x, y);
			u8 B = _Image.GetBlue(x, y);

			//if((x == 5 | x == 6) && y == 15) Tmp += wxString::Format("%03i %03i %03i", R, G, B);
			
			if(_Brightness > 128)
			{
				int Bright = _Brightness - 128;
				R = R - Bright * (R - 0) / 128;
				G = G - Bright * (G - 0) / 128;
				B = B - Bright * (B - 0) / 128;

				// 118 - 72 * 118 / 128 = 118 - 66.3 = 52
				// 119 - = 119 - 66.9 = 52
			}
			else
			{
				int Bright = 128 - _Brightness;
				R = R - Bright * (R - 255) / 128;
				G = G - Bright * (G - 255) / 128;
				B = B - Bright * (B - 255) / 128;
			}

			//if((x == 5 | x == 6) && y == 15) Tmp += wxString::Format("  %03i %03i %03i | ", R, G, B);
			
			_Image.SetRGB(x, y, R, G, B);
		}		
	}
	// Return the alpha
	_Image.SetAlpha(_Image2.GetAlpha(), true);

	// Update the bitmap
	if(Gray)
		return wxBitmap(_Image.ConvertToGreyscale());
	else
		return wxBitmap(_Image);

	//wxMessageBox(Tmp);
}
//////////////////////////////////


void ShowConsole()
{
	StartConsoleWin(100, 2000, "Console"); // Give room for 2000 rows
}

#ifdef MUSICMOD
void
CFrame::MM_InitBitmaps()
{
	// Gray version

	//m_Bitmaps[Toolbar_PluginDSP_Dis] = wxBitmap(SetBrightness(m_Bitmaps[Toolbar_PluginDSP], 165, true));
	m_Bitmaps[Toolbar_PluginDSP_Dis] = wxBitmap(SetBrightness(m_Bitmaps[Toolbar_PluginDSP], 165, true));
	m_Bitmaps[Toolbar_Log_Dis] = wxBitmap(SetBrightness(m_Bitmaps[Toolbar_Log], 165, true));
}



void
CFrame::MM_PopulateGUI()
{	
	// ---------------------------------------
	// Load config
	// ---------------------
	IniFile file;
	file.Load("Plainamp.ini");
	file.Get("Interface", "ShowConsole", &MusicMod::bShowConsole, false);
	// -------


	// ---------------------------------------
	// Make a debugging window
	// ---------------------
	if(MusicMod::bShowConsole) ShowConsole();

	// Write version
	#ifdef _M_X64
		wprintf("64 bit version\n");
	#else
		wprintf("32 bit version\n");
	#endif
	// -----------


	wxToolBar* toolBar = theToolBar; // Shortcut

	toolBar->AddSeparator();


	// ---------------------------------------
	// Draw a rotated music label
	// ---------------------
	wxBitmap m_RotatedText(30, 15);
	wxMemoryDC dc;
	dc.SelectObject(m_RotatedText);
	wxBrush BackgroundGrayBrush(_T("#ece9d8")); // The right color in windows

	// Set outline and fill colors
	dc.SetBackground(BackgroundGrayBrush);
	dc.Clear();
	
	wxFont m_font(8, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);
	dc.SetFont(m_font);
	dc.SetTextForeground(wxColour(*wxLIGHT_GREY));

	dc.DrawText(wxT("Music"), 0, 0);
	m_RotatedText = wxBitmap(m_RotatedText.ConvertToImage().Rotate90(false));

	wxStaticBitmap * m_StaticBitmap = new wxStaticBitmap(toolBar, wxID_ANY, m_RotatedText);

	toolBar->AddControl(m_StaticBitmap);
	// ---------------------------



	mm_ToolMute = toolBar->AddTool(IDM_MUTE, _T("Mute"),   m_Bitmaps[Toolbar_Play], _T("Mute music"));
	mm_ToolPlay = toolBar->AddTool(IDM_MUSIC_PLAY, _T("Play"),   m_Bitmaps[Toolbar_Play], _T("Play or pause music without pausing the game"));

	// This cause the disabled tool bitmap to become some kind of monochrome version
	/*
	mm_ToolMute = new wxToolBarToolBase(toolBar, IDM_MUTE, _T("Mute"), m_Bitmaps[Toolbar_PluginDSP],
		m_Bitmaps[Toolbar_PluginDSP], wxITEM_CHECK, 0, _T("Mute music"));
	toolBar->AddTool(mm_ToolMute);

	mm_ToolPlay = new wxToolBarToolBase(toolBar, IDM_MUSIC_PLAY, _T("Play"), m_Bitmaps[Toolbar_Play],
		m_Bitmaps[Toolbar_Play], wxITEM_NORMAL, 0, _T("Play or pause music without pausing the game"));
	toolBar->AddTool(mm_ToolPlay);
	*/


	// ---------------------
	/* Lots of code to get a label for the slider, in 2.9.0 AddControl accepts a label so then
	   this code can be simplified a lot */
	// ---------
	wxPanel * mm_SliderPanel = new wxPanel(toolBar, IDS_VOLUME_PANEL, wxDefaultPosition, wxDefaultSize);
	wxSlider * mm_Slider = new wxSlider(mm_SliderPanel, IDS_VOLUME, 125, 0, 255, wxDefaultPosition, wxDefaultSize);
	//m_Slider->SetToolTip("Change the music volume");
	wxStaticText * mm_SliderText = new wxStaticText(mm_SliderPanel, IDS_VOLUME_LABEL, _T("Volume"), wxDefaultPosition, wxDefaultSize);
	wxBoxSizer * mm_VolSizer = new wxBoxSizer(wxVERTICAL);
	mm_VolSizer->Add(mm_Slider, 0, wxEXPAND | wxALL, 0);
	mm_VolSizer->Add(mm_SliderText, 0, wxCENTER | wxALL, 0);

	mm_SliderPanel->SetSizer(mm_VolSizer);
	mm_SliderPanel->SetSize(mm_VolSizer->GetMinSize().GetWidth(), mm_VolSizer->GetMinSize().GetHeight());

	toolBar->AddControl((wxControl*)mm_SliderPanel);
	// ---------

	mm_ToolLog = toolBar->AddTool(IDT_LOG, _T("Log"),   m_Bitmaps[Toolbar_Log], _T("Show or hide log"));
}


void
CFrame::MM_UpdateGUI()
{
		// ---------------------------------------------------------------------------------------
		if(MusicMod::GlobalMute)
		{
			//m_pMenuItemMute->SetText(_T("Play"));
			//GetToolBar()->SetToolNormalBitmap(IDM_MUTE, m_Bitmaps[Toolbar_Pause]);
			mm_ToolMute->SetLabel("Unmute");
			mm_ToolMute->SetNormalBitmap(m_Bitmaps[Toolbar_PluginDSP_Dis]);
			//m_ToolMute->SetToggle(true);
		}
		else
		{
			//GetToolBar()->SetToolNormalBitmap(IDM_MUTE, m_Bitmaps[Toolbar_PluginDSP]);
			mm_ToolMute->SetLabel("Mute");
			mm_ToolMute->SetNormalBitmap(m_Bitmaps[Toolbar_PluginDSP]);
		}

		if(MusicMod::GlobalPause)
		{
			//GetToolBar()->SetToolNormalBitmap(IDM_PAUSE, m_Bitmaps[Toolbar_Pause]);
			mm_ToolPlay->SetLabel("Play");
			mm_ToolPlay->SetNormalBitmap(m_Bitmaps[Toolbar_Play]);
		}
		else
		{
			//GetToolBar()->SetToolNormalBitmap(IDM_PAUSE, m_Bitmaps[Toolbar_PluginDSP]);
			mm_ToolPlay->SetLabel("Pause");
			mm_ToolPlay->SetNormalBitmap(m_Bitmaps[Toolbar_Pause]);
		}

		if(MusicMod::bShowConsole)
		{
			mm_ToolLog->SetNormalBitmap(m_Bitmaps[Toolbar_Log]);
		}
		else
		{
			mm_ToolLog->SetNormalBitmap(m_Bitmaps[Toolbar_Log_Dis]);
		}
		// ---------------------------------------------------------------------------------------
}




void
CFrame::MM_OnPlay()
{
	//MessageBox(0, "CFrame::OnPlay > Begin", "", 0);
	wprintf("\nCFrame::OnPlayMusicMod > Begin\n");

	
	if (Core::GetState() != Core::CORE_UNINITIALIZED)
	{
		if (Core::GetState() == Core::CORE_RUN)
		{
			wprintf("CFrame::OnPlayMusicMod > Pause\n");
			if(!MusicMod::GlobalPause) // we may has set this elsewhere
			{
				MusicMod::GlobalPause = true;
				if (MusicMod::dllloaded)
				{
					Player_Pause();
				}

			}
		}
		else
		{
			wprintf("CFrame::OnPlayMusicMod > Play\n");
			if(MusicMod::GlobalPause) // we may has set this elsewhere
			{
				MusicMod::GlobalPause = false;
				if (MusicMod::dllloaded)
				{
					Player_Unpause();
				}
			}
		}
	}
	
}



// =======================================================================================
// Mute music
// ---------------------------------------------------------------------------------------
void
CFrame::MM_OnMute(wxCommandEvent& WXUNUSED (event))
{
	wprintf("CFrame::OnMute > Begin\n");
	//MessageBox(0, "", "", 0);

	if(!MusicMod::GlobalMute)
	{
		if(MusicMod::dllloaded) // avoid crash
		{
			Player_Mute();
		}

		MusicMod::GlobalMute = true;		
		//m_ToolMute->Enable(false);
		//GetToolBar()->EnableTool(IDT_LOG, false);
		UpdateGUI();
	}
	else
	{
		if(MusicMod::dllloaded) // avoid crash
		{
			Player_Mute();
		}
		MusicMod::GlobalMute = false;
		//m_ToolMute->Enable(true);
		UpdateGUI();
	}
}
// =======================================================================================


// =======================================================================================
// Pause music
// ---------------------------------------------------------------------------------------
void
CFrame::MM_OnPause(wxCommandEvent& WXUNUSED (event))
{
	wprintf("CFrame::OnPause > Begin\n");
	//MessageBox(0, "", "", 0);

	if(!MusicMod::GlobalPause)
	{
		if(MusicMod::dllloaded) // avoid crash
		{
			Player_Pause();
		}
		MusicMod::GlobalPause = true;
		UpdateGUI();
	}
	else
	{
		if(MusicMod::dllloaded) // avoid crash
		{
			Player_Pause();
		}
		MusicMod::GlobalPause = false;
		UpdateGUI();
	}
}




// =======================================================================================
// Change volume
// ---------------------------------------------------------------------------------------
void CFrame::MM_OnVolume(wxScrollEvent& event)
{
	//wprintf("CFrame::OnVolume > Begin <%i>\n", event.GetPosition());
	//MessageBox(0, "", "", 0);

	//if(event.GetEventType() == wxEVT_SCROLL_PAGEUP || event.GetEventType() == wxEVT_SCROLL_PAGEDOWN)
	//	return;

	if(MusicMod::dllloaded) // avoid crash
	{

		Player_Volume(event.GetPosition());
		MusicMod::GlobalVolume = event.GetPosition();

		MusicMod::GlobalMute = false; // Unmute to
		mm_ToolMute->Toggle(false);		
		
		if(event.GetEventType() == wxEVT_SCROLL_CHANGED)
		{
			// Only update this on release, to avoid major flickering when changing volume
			UpdateGUI();

			/* Use this to avoid that the focus get stuck on the slider when the main
			window has been replaced */
			this->SetFocus();}
		
	}
}
//=======================================================================================



// =======================================================================================
// Show log
// ---------------------------------------------------------------------------------------
void CFrame::MM_OnLog(wxCommandEvent& event)
{
	//wprintf("CFrame::OnLog > Begin\n");
	//MessageBox(0, "", "", 0);

	if(MusicMod::dllloaded) // Avoid crash
	{
	}

	MusicMod::bShowConsole = !MusicMod::bShowConsole;

	if(MusicMod::bShowConsole)
		{ ShowConsole(); Player_Console(true); }
	else
	{
		#if defined (_WIN32)
			FreeConsole(); Player_Console(false); 
		#endif
	}

	IniFile file;
	file.Load("Plainamp.ini");
	file.Set("Interface", "ShowConsole", MusicMod::bShowConsole);
	file.Save("Plainamp.ini");

	UpdateGUI();
}
//=======================================================================================
#endif // MUSICMOD
