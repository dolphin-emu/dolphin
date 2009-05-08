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
#include "Log.h"

#include "../../../../Source/Core/DolphinWX/Src/Globals.h" // DolphinWX
#include "../../../../Source/Core/DolphinWX/Src/Frame.h"

#include "../../../../Source/Core/DolphinWX/resources/toolbar_plugin_dsp.c" // Icons
#include "../../../../Source/Core/DolphinWX/resources/Boomy.h"
#include "../../../../Source/Core/DolphinWX/resources/Vista.h"
#include "../../../../Source/Core/DolphinWX/resources/KDE.h"
#include "../../../../Source/Core/DolphinWX/resources/X-Plastik.h"

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
	int GlobalVolume = 125;
	extern bool dllloaded;

	void ShowConsole();
	void Init();
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


#ifdef MUSICMOD
void
CFrame::MM_InitBitmaps(int Theme)
{
	// Define the log bitmaps
	switch (Theme)
	{
	case BOOMY:
		m_Bitmaps[Toolbar_Log] = wxGetBitmapFromMemory(Toolbar_Log_png);
		break;
	case VISTA:
		m_Bitmaps[Toolbar_Log] = wxGetBitmapFromMemory(Toolbar_Log1_png);
		break;
	case XPLASTIK:
		m_Bitmaps[Toolbar_Log] = wxGetBitmapFromMemory(Toolbar_Log2_png);
		break;
	case KDE:
		m_Bitmaps[Toolbar_Log] = wxGetBitmapFromMemory(Toolbar_Log3_png);
		break;
	default: PanicAlert("Theme selection went wrong");
	}	

	// Create a gray version
	m_Bitmaps[Toolbar_PluginDSP_Dis] = wxBitmap(SetBrightness(m_Bitmaps[Toolbar_PluginDSP], 165, true));
	m_Bitmaps[Toolbar_Log_Dis] = wxBitmap(SetBrightness(m_Bitmaps[Toolbar_Log], 165, true));

	// Update in case the bitmap has been updated
	//if (GetToolBar() != NULL) TheToolBar->FindById(Toolbar_Log)->SetNormalBitmap(m_Bitmaps[Toolbar_Log]);
}


void
CFrame::MM_PopulateGUI()
{	
	wxToolBar* toolBar = TheToolBar; // Shortcut

	toolBar->AddSeparator();

	MusicMod::Init();

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



	mm_ToolMute = toolBar->AddTool(IDM_MUTE, _T("Mute"),   m_Bitmaps[Toolbar_PluginDSP], _T("Mute music"));
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
	mm_Slider = new wxSlider(mm_SliderPanel, IDS_VOLUME, 125, 0, 255, wxDefaultPosition, wxDefaultSize);
	//mm_Slider->SetToolTip("Change the music volume");
	mm_Slider->SetValue(MusicMod::GlobalVolume);

	wxStaticText * mm_SliderText = new wxStaticText(mm_SliderPanel, IDS_VOLUME_LABEL, _T("Volume"), wxDefaultPosition, wxDefaultSize);
	wxBoxSizer * mm_VolSizer = new wxBoxSizer(wxVERTICAL);
	mm_VolSizer->Add(mm_Slider, 0, wxEXPAND | wxALL, 0);
	mm_VolSizer->Add(mm_SliderText, 0, wxCENTER | wxALL, 0);

	mm_SliderPanel->SetSizer(mm_VolSizer);
	mm_SliderPanel->SetSize(mm_VolSizer->GetMinSize().GetWidth(), mm_VolSizer->GetMinSize().GetHeight());

	toolBar->AddControl((wxControl*)mm_SliderPanel);
	// ---------

	mm_ToolLog = toolBar->AddTool(IDT_LOG, _T("Log"),   m_Bitmaps[Toolbar_Log],
		wxT("Show or hide log. Enable the log window and restart Dolphin to show the DLL status."));
}


//////////////////////////////////////////////////////////////////////////////////////////
// Update GUI
// ¯¯¯¯¯¯¯¯¯¯
void
CFrame::MM_UpdateGUI()
{
		if(MusicMod::GlobalMute)
		{
			mm_ToolMute->SetLabel("Unmute");
			mm_ToolMute->SetNormalBitmap(m_Bitmaps[Toolbar_PluginDSP_Dis]);
		}
		else
		{
			mm_ToolMute->SetLabel("Mute");
			mm_ToolMute->SetNormalBitmap(m_Bitmaps[Toolbar_PluginDSP]);
		}

		if(MusicMod::GlobalPause)
		{
			mm_ToolPlay->SetLabel("Play");
			mm_ToolPlay->SetNormalBitmap(m_Bitmaps[Toolbar_Play]);
		}
		else
		{
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
}
//////////////////////////////////



// =======================================================================================
// Play and stop music
// ---------------------------------------------------------------------------------------
void
CFrame::MM_OnPlay()
{
	//INFO_LOG(AUDIO,"\nCFrame::OnPlayMusicMod > Begin\n");

	// Save the volume
	MusicMod::GlobalVolume = mm_Slider->GetValue();	

	IniFile file;
	file.Load("Plainamp.ini");
	file.Set("Plainamp", "Volume", MusicMod::GlobalVolume);
	file.Save("Plainamp.ini");
	
	if (Core::GetState() != Core::CORE_UNINITIALIZED)
	{
		if (Core::GetState() == Core::CORE_RUN)
		{
			//INFO_LOG(AUDIO,"CFrame::OnPlayMusicMod > Pause\n");
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
			//INFO_LOG(AUDIO,"CFrame::OnPlayMusicMod > Play\n");
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

void
CFrame::MM_OnStop()
{
	Player_Stop();
	MusicMod::GlobalPause = false;
}
// =======================================================================================


// =======================================================================================
// Mute music
// ---------------------------------------------------------------------------------------
void
CFrame::MM_OnMute(wxCommandEvent& WXUNUSED (event))
{
	//INFO_LOG(AUDIO,"CFrame::OnMute > Begin\n");
	//MessageBox(0, "", "", 0);

	if(!MusicMod::GlobalMute)
	{
		if(MusicMod::dllloaded) // avoid crash
		{
			Player_Mute(MusicMod::GlobalVolume);
		}

		MusicMod::GlobalMute = true;
		UpdateGUI();
	}
	else
	{
		if(MusicMod::dllloaded) // avoid crash
		{
			Player_Mute(MusicMod::GlobalVolume);
		}
		MusicMod::GlobalMute = false;
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
	INFO_LOG(AUDIO,"CFrame::OnPause > Begin\n");
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
	//INFO_LOG(AUDIO,"CFrame::OnVolume > Begin <%i>\n", event.GetPosition());
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
	//INFO_LOG(AUDIO,"CFrame::OnLog > Begin\n");
	//MessageBox(0, "", "", 0);

	if(!MusicMod::dllloaded) return; // Avoid crash

	MusicMod::bShowConsole = !MusicMod::bShowConsole;

	if(MusicMod::bShowConsole)
		/* What we do here is run StartConsoleWin() in Common directly after each
		   other first in the exe then in the DLL, sometimes this would give me a rampant memory
		   usage increase until the exe crashed at 700 MB memory usage or something like that.
		   For that reason I'm trying to sleep for a moment between them here. */
		{ MusicMod::ShowConsole(); Sleep(100); Player_Console(true); }
	else
	{
		#if defined (_WIN32)
			Console::Close(); Player_Console(false); 
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
