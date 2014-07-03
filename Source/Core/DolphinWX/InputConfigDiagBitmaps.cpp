// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <wx/bitmap.h>
#include <wx/brush.h>
#include <wx/chartype.h>
#include <wx/colour.h>
#include <wx/dcmemory.h>
#include <wx/defs.h>
#include <wx/font.h>
#include <wx/gdicmn.h>
#include <wx/notebook.h>
#include <wx/pen.h>
#include <wx/statbmp.h>
#include <wx/string.h>

#include "DolphinWX/InputConfigDiag.h"
#include "DolphinWX/WxUtils.h"

#include "InputCommon/ControllerEmu.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/ControllerInterface/Device.h"

class wxTimerEvent;

void InputConfigDialog::UpdateBitmaps(wxTimerEvent& WXUNUSED(event))
{
	wxFont small_font(6, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);

	g_controller_interface.UpdateInput();

	// don't want game thread updating input when we are using it here
	std::unique_lock<std::recursive_mutex> lk(g_controller_interface.update_lock, std::try_to_lock);
	if (!lk.owns_lock())
		return;

	GamepadPage* const current_page = (GamepadPage*)m_pad_notebook->GetPage(m_pad_notebook->GetSelection());

	for (ControlGroupBox* g : current_page->control_groups)
	{
		// if this control group has a bitmap
		if (g->static_bitmap)
		{
			wxMemoryDC dc;
			wxBitmap bitmap(g->static_bitmap->GetBitmap());
			dc.SelectObject(bitmap);
			dc.Clear();

			dc.SetFont(small_font);
			dc.SetTextForeground(0xC0C0C0);

			// label for sticks and stuff
			if (64 == bitmap.GetHeight())
				dc.DrawText(StrToWxStr(g->control_group->name).Upper(), 4, 2);

			switch (g->control_group->type)
			{
			case GROUP_TYPE_TILT :
			case GROUP_TYPE_STICK :
			case GROUP_TYPE_CURSOR :
				{
					// this is starting to be a mess combining all these in one case

					float x = 0, y = 0, z = 0;
					float xx, yy;

					switch (g->control_group->type)
					{
					case GROUP_TYPE_STICK :
						((ControllerEmu::AnalogStick*)g->control_group)->GetState(&x, &y, 32.0, 32-1.5);
						break;
					case GROUP_TYPE_TILT :
						((ControllerEmu::Tilt*)g->control_group)->GetState(&x, &y, 32.0, 32-1.5);
						break;
					case GROUP_TYPE_CURSOR :
						((ControllerEmu::Cursor*)g->control_group)->GetState(&x, &y, &z);
						x *= (32-1.5); x+= 32;
						y *= (32-1.5); y+= 32;
						break;
					}

					xx = g->control_group->controls[3]->control_ref->State();
					xx -= g->control_group->controls[2]->control_ref->State();
					yy = g->control_group->controls[1]->control_ref->State();
					yy -= g->control_group->controls[0]->control_ref->State();
					xx *= 32 - 1; xx += 32;
					yy *= 32 - 1; yy += 32;

					// draw the shit

					// ir cursor forward movement
					if (GROUP_TYPE_CURSOR == g->control_group->type)
					{
						if (z)
						{
							dc.SetPen(*wxRED_PEN);
							dc.SetBrush(*wxRED_BRUSH);
						}
						else
						{
							dc.SetPen(*wxGREY_PEN);
							dc.SetBrush(*wxGREY_BRUSH);
						}
						dc.DrawRectangle(0, 31 - z*31, 64, 2);
					}

					// octagon for visual aid for diagonal adjustment
					dc.SetPen(*wxLIGHT_GREY_PEN);
					dc.SetBrush(*wxWHITE_BRUSH);
					if (GROUP_TYPE_STICK == g->control_group->type)
					{
						// outline and fill colors
						wxBrush LightGrayBrush("#dddddd");
						wxPen LightGrayPen("#bfbfbf");
						dc.SetBrush(LightGrayBrush);
						dc.SetPen(LightGrayPen);

						// polygon offset
						float max
							, diagonal
							, box = 64
							, d_of = box / 256.0
							, x_of = box / 2.0;

						if (g->control_group->name == "Main Stick")
						{
							max = (87.0f / 127.0f) * 100;
							diagonal = (55.0f / 127.0f) * 100.0;
						}
						else if (g->control_group->name == "C-Stick")
						{
							max = (74.0f / 127.0f) * 100;
							diagonal = (46.0f / 127.0f) * 100;
						}
						else
						{
							max = (82.0f / 127.0f) * 100;
							diagonal = (58.0f / 127.0f) * 100;
						}

						// polygon corners
						wxPoint Points[8];
						Points[0].x = (int)(0.0 * d_of + x_of); Points[0].y = (int)(max * d_of + x_of);
						Points[1].x = (int)(diagonal * d_of + x_of); Points[1].y = (int)(diagonal * d_of + x_of);
						Points[2].x = (int)(max * d_of + x_of); Points[2].y = (int)(0.0 * d_of + x_of);
						Points[3].x = (int)(diagonal * d_of + x_of); Points[3].y = (int)(-diagonal * d_of + x_of);
						Points[4].x = (int)(0.0 * d_of + x_of); Points[4].y = (int)(-max * d_of + x_of);
						Points[5].x = (int)(-diagonal * d_of + x_of); Points[5].y = (int)(-diagonal * d_of + x_of);
						Points[6].x = (int)(-max * d_of + x_of); Points[6].y = (int)(0.0 * d_of + x_of);
						Points[7].x = (int)(-diagonal * d_of + x_of); Points[7].y = (int)(diagonal * d_of + x_of);

						// draw polygon
						dc.DrawPolygon(8, Points);
					}
					else
					{
						dc.DrawRectangle(16, 16, 32, 32);
					}

					if (GROUP_TYPE_CURSOR != g->control_group->type)
					{
						// deadzone circle
						dc.SetBrush(*wxLIGHT_GREY_BRUSH);
						dc.DrawCircle(32, 32, g->control_group->settings[SETTING_DEADZONE]->value * 32);
					}

					// raw dot
					dc.SetPen(*wxGREY_PEN);
					dc.SetBrush(*wxGREY_BRUSH);
					// i like the dot better than the cross i think
					dc.DrawRectangle(xx - 2, yy - 2, 4, 4);
					//dc.DrawRectangle(xx-1, 64-yy-4, 2, 8);
					//dc.DrawRectangle(xx-4, 64-yy-1, 8, 2);

					// adjusted dot
					if (x!=32 || y!=32)
					{
						dc.SetPen(*wxRED_PEN);
						dc.SetBrush(*wxRED_BRUSH);
						dc.DrawRectangle(x-2, 64-y-2, 4, 4);
						// i like the dot better than the cross i think
						//dc.DrawRectangle(x-1, 64-y-4, 2, 8);
						//dc.DrawRectangle(x-4, 64-y-1, 8, 2);
					}

				}
				break;
			case GROUP_TYPE_FORCE :
				{
					float raw_dot[3];
					float adj_dot[3];
					const float deadzone = 32 * g->control_group->settings[0]->value;

					// adjusted
					((ControllerEmu::Force*)g->control_group)->GetState(adj_dot, 32.0, 32-1.5);

					// raw
					for (unsigned int i=0; i<3; ++i)
					{
						raw_dot[i] = g->control_group->controls[i*2 + 1]->control_ref->State()
							- g->control_group->controls[i*2]->control_ref->State();
						raw_dot[i] *= 32 - 1; raw_dot[i] += 32;
					}

					// deadzone rect for forward/backward visual
					dc.SetBrush(*wxLIGHT_GREY_BRUSH);
					dc.SetPen(*wxLIGHT_GREY_PEN);
					dc.DrawRectangle(0, 32 - deadzone, 64, deadzone * 2);

					// raw forward/background line
					dc.SetPen(*wxGREY_PEN);
					dc.SetBrush(*wxGREY_BRUSH);
					dc.DrawRectangle(0, raw_dot[2] - 1, 64, 2);

					// adjusted forward/background line
					if (adj_dot[2]!=32)
					{
						dc.SetPen(*wxRED_PEN);
						dc.SetBrush(*wxRED_BRUSH);
						dc.DrawRectangle(0, adj_dot[2] - 1, 64, 2);
					}

					// a rectangle, for looks i guess
					dc.SetBrush(*wxWHITE_BRUSH);
					dc.SetPen(*wxLIGHT_GREY_PEN);
					dc.DrawRectangle(16, 16, 32, 32);

					// deadzone square
					dc.SetBrush(*wxLIGHT_GREY_BRUSH);
					dc.DrawRectangle(32 - deadzone, 32 - deadzone, deadzone * 2, deadzone * 2);

					// raw dot
					dc.SetPen(*wxGREY_PEN);
					dc.SetBrush(*wxGREY_BRUSH);
					dc.DrawRectangle(raw_dot[1] - 2, raw_dot[0] - 2, 4, 4);

					// adjusted dot
					if (adj_dot[1]!=32 || adj_dot[0]!=32)
					{
						dc.SetPen(*wxRED_PEN);
						dc.SetBrush(*wxRED_BRUSH);
						dc.DrawRectangle(adj_dot[1]-2, adj_dot[0]-2, 4, 4);
					}

				}
				break;
			case GROUP_TYPE_BUTTONS :
				{
					const unsigned int button_count = ((unsigned int)g->control_group->controls.size());

					// draw the shit
					dc.SetPen(*wxGREY_PEN);

					unsigned int * const bitmasks = new unsigned int[ button_count ];
					for (unsigned int n = 0; n<button_count; ++n)
						bitmasks[n] = (1 << n);

					unsigned int buttons = 0;
					((ControllerEmu::Buttons*)g->control_group)->GetState(&buttons, bitmasks);

					for (unsigned int n = 0; n<button_count; ++n)
					{
						if (buttons & bitmasks[n])
						{
							dc.SetBrush(*wxRED_BRUSH);
						}
						else
						{
							unsigned char amt = 255 - g->control_group->controls[n]->control_ref->State() * 128;
							dc.SetBrush(wxBrush(wxColour(amt, amt, amt)));
						}
						dc.DrawRectangle(n * 12, 0, 14, 12);

						// text
						const std::string name = g->control_group->controls[n]->name;
						// bit of hax so ZL, ZR show up as L, R
						dc.DrawText(StrToWxStr(std::string(1, (name[1] && name[1] < 'a') ? name[1] : name[0])), n*12 + 2, 1);
					}

					delete[] bitmasks;

				}
				break;
			case GROUP_TYPE_TRIGGERS :
				{
					const unsigned int trigger_count = ((unsigned int)(g->control_group->controls.size()));

					// draw the shit
					dc.SetPen(*wxGREY_PEN);
					ControlState deadzone =  g->control_group->settings[0]->value;

					unsigned int* const trigs = new unsigned int[trigger_count];
					((ControllerEmu::Triggers*)g->control_group)->GetState(trigs, 64);

					for (unsigned int n = 0; n < trigger_count; ++n)
					{
						ControlState trig_r = g->control_group->controls[n]->control_ref->State();

						// outline
						dc.SetPen(*wxGREY_PEN);
						dc.SetBrush(*wxWHITE_BRUSH);
						dc.DrawRectangle(0, n*12, 64, 14);

						// raw
						dc.SetBrush(*wxGREY_BRUSH);
						dc.DrawRectangle(0, n*12, trig_r*64, 14);

						// deadzone affected
						dc.SetBrush(*wxRED_BRUSH);
						dc.DrawRectangle(0, n*12, trigs[n], 14);

						// text
						dc.DrawText(StrToWxStr(g->control_group->controls[n]->name), 3, n*12 + 1);
					}

					delete[] trigs;

					// deadzone box
					dc.SetPen(*wxLIGHT_GREY_PEN);
					dc.SetBrush(*wxTRANSPARENT_BRUSH);
					dc.DrawRectangle(0, 0, deadzone*64, trigger_count*14);

				}
				break;
			case GROUP_TYPE_MIXED_TRIGGERS :
				{
					const unsigned int trigger_count = ((unsigned int)(g->control_group->controls.size() / 2));

					// draw the shit
					dc.SetPen(*wxGREY_PEN);
					ControlState thresh = g->control_group->settings[0]->value;

					for (unsigned int n = 0; n < trigger_count; ++n)
					{
						dc.SetBrush(*wxRED_BRUSH);
						ControlState trig_d = g->control_group->controls[n]->control_ref->State();

						ControlState trig_a = trig_d > thresh ? 1
							: g->control_group->controls[n+trigger_count]->control_ref->State();

						dc.DrawRectangle(0, n*12, 64+20, 14);
						if (trig_d <= thresh)
							dc.SetBrush(*wxWHITE_BRUSH);
						dc.DrawRectangle(trig_a*64, n*12, 64+20, 14);
						dc.DrawRectangle(64, n*12, 32, 14);

						// text
						dc.DrawText(StrToWxStr(g->control_group->controls[n+trigger_count]->name), 3, n*12 + 1);
						dc.DrawText(StrToWxStr(std::string(1, g->control_group->controls[n]->name[0])), 64 + 3, n*12 + 1);
					}

					// threshold box
					dc.SetPen(*wxLIGHT_GREY_PEN);
					dc.SetBrush(*wxTRANSPARENT_BRUSH);
					dc.DrawRectangle(thresh*64, 0, 128, trigger_count*14);

				}
				break;
			case GROUP_TYPE_SLIDER:
				{
					const ControlState deadzone = g->control_group->settings[0]->value;

					ControlState state = g->control_group->controls[1]->control_ref->State() - g->control_group->controls[0]->control_ref->State();
					dc.SetPen(*wxGREY_PEN);
					dc.SetBrush(*wxGREY_BRUSH);
					dc.DrawRectangle(31 + state * 30, 0, 2, 14);

					((ControllerEmu::Slider*)g->control_group)->GetState(&state, 1);
					if (state)
					{
						dc.SetPen(*wxRED_PEN);
						dc.SetBrush(*wxRED_BRUSH);
						dc.DrawRectangle(31 + state * 30, 0, 2, 14);
					}

					// deadzone box
					dc.SetPen(*wxLIGHT_GREY_PEN);
					dc.SetBrush(*wxTRANSPARENT_BRUSH);
					dc.DrawRectangle(32 - deadzone * 32, 0, deadzone * 64, 14);
				}
				break;
			default :
				break;
			}

			// box outline
			// Windows XP color
			dc.SetPen(wxPen("#7f9db9"));
			dc.SetBrush(*wxTRANSPARENT_BRUSH);
			dc.DrawRectangle(0, 0, bitmap.GetWidth(), bitmap.GetHeight());

			dc.SelectObject(wxNullBitmap);
			g->static_bitmap->SetBitmap(bitmap);
		}
	}
}
