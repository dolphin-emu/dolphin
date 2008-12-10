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
#include "Frame.h"
#include "FileUtil.h"
#include "StringUtil.h"

#include "GameListCtrl.h"
#include "BootManager.h"

#include "Common.h"
#include "Config.h"
#include "Core.h"
#include "State.h"
#include "ConfigMain.h"
#include "PluginManager.h"
#include "MemcardManager.h"
#include "CheatsWindow.h"
#include "AboutDolphin.h"

#include <wx/statusbr.h>


namespace WiimoteLeds
{
	int LED_SIZE_X = 8;
	int LED_SIZE_Y = 8;
	int SPEAKER_SIZE_X = 8;
	int SPEAKER_SIZE_Y = 8;

	int ConnectionStatusWidth = 103; // This needs to be wider, for vista at least
	int ConnectionStatusOnlyAdj = 7;
	int RightmostMargin = 6;
	int SpIconMargin = 11;
	int LedIconMargin = 11;

	// Leds only
	static const int LdWidthsOn[] =
	{
		-1,
		ConnectionStatusWidth,
		(LedIconMargin + LED_SIZE_X) * 4 + RightmostMargin
	};	
	static const int StylesFieldOn[] = { wxSB_NORMAL, wxSB_NORMAL, wxSB_NORMAL };	
	
	// Speakers only
	static const int SpWidthsOn[] =
	{
		-1,
		ConnectionStatusWidth,
		( SpIconMargin + SPEAKER_SIZE_X ) * 3 + RightmostMargin
	};	
	static const int SpStylesFieldOn[] = { wxSB_NORMAL, wxSB_NORMAL, wxSB_NORMAL };	

	// Both
	static const int LdSpWidthsOn[] =
	{
		-1,
		ConnectionStatusWidth,
		(SpIconMargin + SPEAKER_SIZE_X) * 3,
		(LedIconMargin + LED_SIZE_X) * 4 + RightmostMargin
	};	
	static const int LdSpStylesFieldOn[] = { wxSB_NORMAL, wxSB_NORMAL, wxSB_NORMAL };	

	// Only the Wiimote connection Status
	static const int WidthsOff[] =
	{
		-1,
		ConnectionStatusWidth + ConnectionStatusOnlyAdj + RightmostMargin
	};
	static const int StylesFieldOff[] = { wxSB_NORMAL, wxSB_NORMAL };

	// GC mode
	static const int WidthsOffGC[] = { -1 };
	static const int StylesFieldOffGC[] = { wxSB_NORMAL };
};

// =======================================================
// Modify status bar
// -------------
void CFrame::ModifyStatusBar()
{
	// Get settings
	bool LedsOn = SConfig::GetInstance().m_LocalCoreStartupParameter.bWiiLeds;
	bool SpeakersOn = SConfig::GetInstance().m_LocalCoreStartupParameter.bWiiSpeakers;

	// Don't use this for GC games, or before a game is loaded
	if(!SConfig::GetInstance().m_LocalCoreStartupParameter.bWii)
		{ LedsOn = false; SpeakersOn = false; }

	// Declarations
	int Fields;
	int *Widths;
	int *StylesFields;

	// ---------------------------------------
	// Leds only
	// -------------
	if(LedsOn && !SpeakersOn)
	{
		Fields = 3;
		Widths = (int*)WiimoteLeds::LdWidthsOn;
		StylesFields = (int*)WiimoteLeds::StylesFieldOn;
	}
	// ---------------------------------------
	// Speaker only
	// -------------
	else if(!LedsOn && SpeakersOn)
	{
		Fields = 3;
		Widths = (int*)WiimoteLeds::SpWidthsOn;
		StylesFields = (int*)WiimoteLeds::SpStylesFieldOn;
	}
	// ---------------------------------------
	// Both on
	// -------------
	else if(LedsOn && SpeakersOn)
	{
		Fields = 4;
		Widths = (int*)WiimoteLeds::LdSpWidthsOn;
		StylesFields = (int*)WiimoteLeds::LdSpStylesFieldOn;
	}
	// ---------------------------------------
	// Both off
	// -------------
	else if(!LedsOn && !SpeakersOn)
	{
		Fields = 2;
		Widths = (int*)WiimoteLeds::WidthsOff;
		StylesFields = (int*)WiimoteLeds::StylesFieldOff;

		// Maybe we should even go down to one field
		if(!SConfig::GetInstance().m_LocalCoreStartupParameter.bWii)
		{
			Fields = 1;
			Widths = (int*)WiimoteLeds::WidthsOffGC;
			StylesFields = (int*)WiimoteLeds::StylesFieldOffGC;
		}
	}

	// Update the settings
	m_pStatusBar->SetFieldsCount(Fields);
	m_pStatusBar->SetStatusWidths(Fields, Widths);
	m_pStatusBar->SetStatusStyles(Fields, StylesFields);

	/* Destroy and create all, and check for HaveLeds and HaveSpeakers in case we have
	   gotten a confirmed on or off setting, in which case we don't do anything */
	if(!LedsOn && HaveLeds) CreateDestroy(DESTROYLEDS);
	if(!SpeakersOn && HaveSpeakers) CreateDestroy(DESTROYSPEAKERS);
	if(LedsOn && !HaveLeds) CreateDestroy(CREATELEDS);
	if(SpeakersOn && !HaveSpeakers) CreateDestroy(CREATESPEAKERS);

	DoMoveIcons();
	m_pStatusBar->Refresh(); // avoid small glitches that can occur
}


// =======================================================
// Create and destroy leds and speakers icons
// -------------
void CFrame::CreateDestroy(int Case)
{
	switch(Case)
	{
	case CREATELEDS:
		{
			CreateLeds();
			UpdateLeds();		
			HaveLeds = true;
			break;
		}
	case DESTROYLEDS:
		{
			for(int i = 0; i < 4; i++)
			{
				m_StatBmp[i]->Destroy();		
			}
			HaveLeds = false;
			break;
		}
	case CREATESPEAKERS:
		{
			CreateSpeakers();
			HaveSpeakers = true;	
			break;
		}
		
	case DESTROYSPEAKERS:
	{
		for(int i = 4; i < 7; i++)
		{
			m_StatBmp[i]->Destroy();		
		}
		HaveSpeakers = false;
		break;
	}
	} // end of switch

	DoMoveIcons();
}
// =============


// =======================================================
// Create and update leds
// -------------
void CFrame::CreateLeds()
{
	// Begin with blank ones
	memset(&g_Leds, 0, sizeof(g_Leds));
	for(int i = 0; i < 4; i++)
	{
		m_StatBmp[i] = new wxStaticBitmap(m_pStatusBar, wxID_ANY,
			CreateBitmapForLeds((bool)g_Leds[i]));
	}
}
// Update leds
void CFrame::UpdateLeds()
{
	for(int i = 0; i < 4; i++)
	{
		m_StatBmp[i]->SetBitmap(CreateBitmapForLeds((bool)g_Leds[i]));
	}
}
// ==============


// =======================================================
// Create and speaker icons
// -------------
void CFrame::CreateSpeakers()
{
	// Begin with blank ones
	memset(&g_Speakers, 0, sizeof(g_Speakers));
	memset(&g_Speakers_, 0, sizeof(g_Speakers_));
	for(int i = 0; i < 3; i++)
	{
		m_StatBmp[i+4] = new wxStaticBitmap(m_pStatusBar, wxID_ANY,
			CreateBitmapForSpeakers(i, (bool)g_Speakers[i]));
	}
}
// Update icons
void CFrame::UpdateSpeakers()
{
	/*if(!SConfig::GetInstance().m_LocalCoreStartupParameter.bNTSC)
		{ PanicAlert("Not NTSC");}

	if(!SConfig::GetInstance().m_LocalCoreStartupParameter.bWii)
		{ PanicAlert("Not Wii");}*/
	for(int i = 0; i < 3; i++)
	{
		m_StatBmp[i+4]->SetBitmap(CreateBitmapForSpeakers(i, (bool)g_Speakers[i]));
	}
}
// ==============


// =======================================================
// Create the Leds bitmap
// -------------
wxBitmap CFrame::CreateBitmapForLeds(bool On)
{
	wxBitmap bitmap(WiimoteLeds::LED_SIZE_X, WiimoteLeds::LED_SIZE_Y);
	wxMemoryDC dc;
	dc.SelectObject(bitmap);

	// Set outline and fill colors
	wxBrush LightBlueBrush(_T("#0383f0"));
	wxPen LightBluePen(_T("#80c5fd"));
	wxPen LightGrayPen(_T("#909090"));
	dc.SetPen(On ? LightBluePen : LightGrayPen);
	dc.SetBrush(On ? LightBlueBrush : *wxWHITE_BRUSH);

	dc.Clear();
	dc.DrawRectangle(0, 0, WiimoteLeds::LED_SIZE_X, WiimoteLeds::LED_SIZE_Y);
	dc.SelectObject(wxNullBitmap);
	return bitmap;
}


// =======================================================
// Create the Speaker bitmap
// -------------
wxBitmap CFrame::CreateBitmapForSpeakers(int BitmapType, bool On)
{
	wxBitmap bitmap(WiimoteLeds::LED_SIZE_X, WiimoteLeds::LED_SIZE_Y);
	wxMemoryDC dc;
	dc.SelectObject(bitmap);
	wxBrush BackgroundGrayBrush(_T("#ece9d8")); // the right color in windows

	switch(BitmapType)
	{
	case 0:
	{
		// Set outline and fill colors
		dc.SetPen(On ? *wxMEDIUM_GREY_PEN : *wxMEDIUM_GREY_PEN);
		dc.SetBrush(On ? *wxGREEN_BRUSH : *wxWHITE_BRUSH);
		dc.SetBackground(BackgroundGrayBrush);
		dc.Clear();
		dc.DrawEllipse(0, 0, WiimoteLeds::SPEAKER_SIZE_X, WiimoteLeds::SPEAKER_SIZE_Y);
		dc.SelectObject(wxNullBitmap);
		return bitmap;
	}
	case 1:
	{
		// Set outline and fill colors
		dc.SetPen(On ? *wxMEDIUM_GREY_PEN : *wxMEDIUM_GREY_PEN);
		dc.SetBrush(On ? *wxBLUE_BRUSH : *wxWHITE_BRUSH);
		dc.SetBackground(BackgroundGrayBrush);
		dc.Clear();
		dc.DrawEllipse(0, 0, WiimoteLeds::SPEAKER_SIZE_X, WiimoteLeds::SPEAKER_SIZE_Y);
		dc.SelectObject(wxNullBitmap);
		return bitmap;
	}
	case 2:
	{
		// Set outline and fill colors
		dc.SetPen(On ? *wxMEDIUM_GREY_PEN : *wxMEDIUM_GREY_PEN);
		dc.SetBrush(On ? *wxGREEN_BRUSH : *wxWHITE_BRUSH);
		dc.SetBackground(BackgroundGrayBrush);
		dc.Clear();
		dc.DrawEllipse(0, 0, WiimoteLeds::SPEAKER_SIZE_X, WiimoteLeds::SPEAKER_SIZE_Y);
		dc.SelectObject(wxNullBitmap);
		return bitmap;
	}
	}

        return bitmap;
}


// =======================================================
// Move the bitmaps
// -------------
void CFrame::MoveIcons(wxSizeEvent& event)
{
	DoMoveIcons();
	event.Skip();
}

void CFrame::DoMoveIcons()
{
	if(HaveLeds) MoveLeds();
	if(HaveSpeakers) MoveSpeakers();
}

void CFrame::MoveLeds()
{
    wxRect Rect;
	// Get the bitmap field coordinates
    m_pStatusBar->GetFieldRect((HaveLeds && HaveSpeakers) ? 3 : 2, Rect);
    wxSize Size = m_StatBmp[0]->GetSize(); // Get the bitmap size

	//wxMessageBox(wxString::Format("%i", Rect.x));
	int x = Rect.x + 10;
	int Dist = WiimoteLeds::LED_SIZE_X + 7;
	int y = Rect.y + (Rect.height - Size.y) / 2;

	for(int i = 0; i < 4; i++)
	{
		if(i > 0) x = m_StatBmp[i-1]->GetPosition().x + Dist;
		m_StatBmp[i]->Move(x, y);
	}	
}

void CFrame::MoveSpeakers()
{
    wxRect Rect;
	m_pStatusBar->GetFieldRect(2, Rect); // Get the bitmap field coordinates

	// Get the actual bitmap size, currently it's the same as SPEAKER_SIZE_Y
    wxSize Size = m_StatBmp[4]->GetSize();

	//wxMessageBox(wxString::Format("%i", Rect.x));
	int x = Rect.x + 9;
	int Dist = WiimoteLeds::SPEAKER_SIZE_X + 7;
	int y = Rect.y + (Rect.height - Size.y) / 2;

	for(int i = 0; i < 3; i++)
	{
		if(i > 0) x = m_StatBmp[i-1+4]->GetPosition().x + Dist;
		m_StatBmp[i+4]->Move(x, y);
	}	
}
// ==============
