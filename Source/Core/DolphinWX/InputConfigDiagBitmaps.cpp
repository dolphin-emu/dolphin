// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <wx/bitmap.h>
#include <wx/brush.h>
#include <wx/colour.h>
#include <wx/dcmemory.h>
#include <wx/font.h>
#include <wx/notebook.h>
#include <wx/pen.h>
#include <wx/statbmp.h>

#include "DolphinWX/InputConfigDiag.h"
#include "DolphinWX/WxUtils.h"

#include "InputCommon/ControllerEmu.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/ControllerInterface/Device.h"

struct ShapePosition
{
	double max;
	double diag;
	double box;
	double scale;

	double dz;
	double range;

	wxCoord offset;
};

// regular octagon
static void DrawOctagon(wxDC* dc, ShapePosition p)
{
	const int vertices = 8;
	double radius = p.max;

	wxPoint point[vertices];

	double angle = 2.0 * M_PI / vertices;

	for (int i = 0; i < vertices; i++)
	{
		double a = (angle * i);
		double x = radius * cos(a);
		double y = radius * sin(a);
		point[i].x = x;
		point[i].y = y;
	}

	dc->DrawPolygon(vertices, point, p.offset, p.offset);
}

// irregular dodecagon
static void DrawDodecagon(wxDC* dc, ShapePosition p)
{
	const int vertices = 12;

	wxPoint point[vertices];
	point[0].x = p.dz; point[0].y = p.max;
	point[1].x = p.diag; point[1].y = p.diag;
	point[2].x = p.max; point[2].y = p.dz;

	point[3].x = p.max; point[3].y = -p.dz;
	point[4].x = p.diag; point[4].y = -p.diag;
	point[5].x = p.dz; point[5].y = -p.max;

	point[6].x = -p.dz; point[6].y = -p.max;
	point[7].x = -p.diag; point[7].y = -p.diag;
	point[8].x = -p.max; point[8].y = -p.dz;

	point[9].x = -p.max; point[9].y = p.dz;
	point[10].x = -p.diag; point[10].y = p.diag;
	point[11].x = -p.dz; point[11].y = p.max;

	dc->DrawPolygon(vertices, point, p.offset, p.offset);
}

static void DrawCenteredRectangle(wxDC &dc, int x, int y, int w, int h)
{
	x -= w / 2;
	y -= h / 2;
	dc.DrawRectangle(x, y, w, h);
}

#define VIS_BITMAP_SIZE 64

#define VIS_NORMALIZE(a) ((a / 2.0) + 0.5)
#define VIS_COORD(a) ((VIS_NORMALIZE(a)) * VIS_BITMAP_SIZE)

#define COORD_VIS_SIZE 4

static void DrawCoordinate(wxDC &dc, ControlState x, ControlState y)
{
	int xc = VIS_COORD(x);
	int yc = VIS_COORD(y);
	DrawCenteredRectangle(dc, xc, yc, COORD_VIS_SIZE, COORD_VIS_SIZE);
}

static void DrawButton(unsigned int* const bitmasks, unsigned int buttons, unsigned int n, wxDC& dc, ControlGroupBox* g, unsigned int row)
{
	if (buttons & bitmasks[(row * 8) + n])
	{
		dc.SetBrush(*wxRED_BRUSH);
	}
	else
	{
		unsigned char amt = 255 - g->control_group->controls[(row * 8) + n]->control_ref->State() * 128;
		dc.SetBrush(wxBrush(wxColour(amt, amt, amt)));
	}
	dc.DrawRectangle(n * 12, (row == 0) ? 0 : (row * 11), 14, 12);

	// text
	const std::string name = g->control_group->controls[(row * 8) + n]->name;
	// bit of hax so ZL, ZR show up as L, R
	dc.DrawText(StrToWxStr(std::string(1, (name[1] && name[1] < 'a') ? name[1] : name[0])), n * 12 + 2, 1 + ((row == 0) ? 0 : (row * 11)));
}

static void DrawControlGroupBox(wxDC &dc, ControlGroupBox *g)
{
	switch (g->control_group->type)
	{
	case GROUP_TYPE_TILT :
	case GROUP_TYPE_STICK :
	case GROUP_TYPE_CURSOR :
	{
		// this is starting to be a mess combining all these in one case

		ControlState x = 0, y = 0, z = 0;

		switch (g->control_group->type)
		{
		case GROUP_TYPE_STICK :
			((ControllerEmu::AnalogStick*)g->control_group)->GetState(&x, &y);
			break;
		case GROUP_TYPE_TILT :
			((ControllerEmu::Tilt*)g->control_group)->GetState(&x, &y);
			break;
		case GROUP_TYPE_CURSOR :
			((ControllerEmu::Cursor*)g->control_group)->GetState(&x, &y, &z);
			break;
		}

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

		// input zone
		dc.SetPen(*wxLIGHT_GREY_PEN);
		dc.SetBrush(*wxWHITE_BRUSH);
		if (GROUP_TYPE_STICK == g->control_group->type)
		{
			// outline and fill colors
			wxBrush LightGrayBrush("#dddddd");
			wxPen LightGrayPen("#bfbfbf");
			dc.SetBrush(LightGrayBrush);
			dc.SetPen(LightGrayPen);

			ShapePosition p;
			p.box = 64;
			p.offset = p.box / 2;
			p.range = 256;
			p.scale = p.box / p.range;
			p.dz = 15 * p.scale;
			bool octagon = false;

			if (g->control_group->name == "Main Stick")
			{
				p.max = 87 * p.scale;
				p.diag = 55 * p.scale;
			}
			else if (g->control_group->name == "C-Stick")
			{
				p.max = 74 * p.scale;
				p.diag = 46 * p.scale;
			}
			else
			{
				p.scale = 1;
				p.max = 32;
				octagon = true;
			}

			if (octagon)
				DrawOctagon(&dc, p);
			else
				DrawDodecagon(&dc, p);
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
		{
		ControlState xx, yy;
		xx = g->control_group->controls[3]->control_ref->State();
		xx -= g->control_group->controls[2]->control_ref->State();
		yy = g->control_group->controls[1]->control_ref->State();
		yy -= g->control_group->controls[0]->control_ref->State();

		dc.SetPen(*wxGREY_PEN);
		dc.SetBrush(*wxGREY_BRUSH);
		DrawCoordinate(dc, xx, yy);
		}

		// adjusted dot
		if (x != 0 || y != 0)
		{
			dc.SetPen(*wxRED_PEN);
			dc.SetBrush(*wxRED_BRUSH);
			// XXX: The adjusted values flip the Y axis to be in the format
			// the Wii expects. Should this be in WiimoteEmu.cpp instead?
			DrawCoordinate(dc, x, -y);
		}
	}
	break;
	case GROUP_TYPE_FORCE :
	{
		ControlState raw_dot[3];
		ControlState adj_dot[3];
		const ControlState deadzone = g->control_group->settings[0]->value;

		// adjusted
		((ControllerEmu::Force*)g->control_group)->GetState(adj_dot);

		// raw
		for (unsigned int i=0; i<3; ++i)
		{
			raw_dot[i] = (g->control_group->controls[i*2 + 1]->control_ref->State() -
				      g->control_group->controls[i*2]->control_ref->State());
		}

		// deadzone rect for forward/backward visual
		dc.SetBrush(*wxLIGHT_GREY_BRUSH);
		dc.SetPen(*wxLIGHT_GREY_PEN);
		int deadzone_height = deadzone * VIS_BITMAP_SIZE;
		DrawCenteredRectangle(dc, 0, VIS_BITMAP_SIZE / 2, VIS_BITMAP_SIZE, deadzone_height);

#define LINE_HEIGHT 2
		int line_y;

		// raw forward/background line
		dc.SetPen(*wxGREY_PEN);
		dc.SetBrush(*wxGREY_BRUSH);
		line_y = VIS_COORD(raw_dot[2]);
		DrawCenteredRectangle(dc, VIS_BITMAP_SIZE / 2, line_y, VIS_BITMAP_SIZE, LINE_HEIGHT);

		// adjusted forward/background line
		if (adj_dot[2] != 0.0)
		{
			dc.SetPen(*wxRED_PEN);
			dc.SetBrush(*wxRED_BRUSH);
			line_y = VIS_COORD(adj_dot[2]);
			DrawCenteredRectangle(dc, VIS_BITMAP_SIZE / 2, line_y, VIS_BITMAP_SIZE, LINE_HEIGHT);
		}

#define DEADZONE_RECT_SIZE 32

		// empty deadzone square
		dc.SetBrush(*wxWHITE_BRUSH);
		dc.SetPen(*wxLIGHT_GREY_PEN);
		DrawCenteredRectangle(dc, VIS_BITMAP_SIZE / 2, VIS_BITMAP_SIZE / 2, DEADZONE_RECT_SIZE, DEADZONE_RECT_SIZE);

		// deadzone square
		dc.SetBrush(*wxLIGHT_GREY_BRUSH);
		int dz_size = (deadzone * DEADZONE_RECT_SIZE);
		DrawCenteredRectangle(dc, VIS_BITMAP_SIZE / 2, VIS_BITMAP_SIZE / 2, dz_size, dz_size);

		// raw dot
		dc.SetPen(*wxGREY_PEN);
		dc.SetBrush(*wxGREY_BRUSH);
		DrawCoordinate(dc, raw_dot[1], raw_dot[0]);

		// adjusted dot
		if (adj_dot[1] != 0 && adj_dot[0] != 0)
		{
			dc.SetPen(*wxRED_PEN);
			dc.SetBrush(*wxRED_BRUSH);
			DrawCoordinate(dc, adj_dot[1], adj_dot[0]);
		}

	}
	break;
	case GROUP_TYPE_BUTTONS :
	{
		unsigned int button_count = ((unsigned int)g->control_group->controls.size());

		// draw the shit
		dc.SetPen(*wxGREY_PEN);

		unsigned int* const bitmasks = new unsigned int[button_count];
		for (unsigned int n = 0; n<button_count; ++n)
			bitmasks[n] = (1 << n);

		unsigned int buttons = 0;
		((ControllerEmu::Buttons*)g->control_group)->GetState(&buttons, bitmasks);

		// Draw buttons in rows of 8
		for (unsigned int row = 0; row < ceil((float)button_count / 8.0f); row++)
		{
			unsigned int buttons_to_draw = 8;
			if ((button_count - row * 8) <= 8)
				buttons_to_draw = button_count - row * 8;

			for (unsigned int n = 0; n < buttons_to_draw; ++n)
			{
				DrawButton(bitmasks, buttons, n, dc, g, row);
			}
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

		ControlState* const trigs = new ControlState[trigger_count];
		((ControllerEmu::Triggers*)g->control_group)->GetState(trigs);

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
			dc.DrawRectangle(0, n*12, trigs[n]*64, 14);

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

		ControlState adj_state;
		((ControllerEmu::Slider*)g->control_group)->GetState(&adj_state);
		if (state)
		{
			dc.SetPen(*wxRED_PEN);
			dc.SetBrush(*wxRED_BRUSH);
			dc.DrawRectangle(31 + adj_state * 30, 0, 2, 14);
		}

		// deadzone box
		dc.SetPen(*wxLIGHT_GREY_PEN);
		dc.SetBrush(*wxTRANSPARENT_BRUSH);
		dc.DrawRectangle(32 - deadzone * 32, 0, deadzone * 64, 14);
	}
	break;
	default:
	break;
	}
}

void InputConfigDialog::UpdateBitmaps(wxTimerEvent& WXUNUSED(event))
{
	wxFont small_font(6, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);

	g_controller_interface.UpdateInput();

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

			DrawControlGroupBox(dc, g);

			// box outline
			// Windows XP color
			dc.SetPen(wxPen("#7f9db9"));
			dc.SetBrush(*wxTRANSPARENT_BRUSH);
			dc.DrawRectangle(0, 0, bitmap.GetWidth(), bitmap.GetHeight());

			// label for sticks and stuff
			if (64 == bitmap.GetHeight())
				dc.DrawText(StrToWxStr(g->control_group->name).Upper(), 4, 2);

			dc.SelectObject(wxNullBitmap);
			g->static_bitmap->SetBitmap(bitmap);
		}
	}
}
