// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include <wx/bitmap.h>
#include <wx/brush.h>
#include <wx/colour.h>
#include <wx/dcmemory.h>
#include <wx/font.h>
#include <wx/graphics.h>
#include <wx/notebook.h>
#include <wx/pen.h>
#include <wx/settings.h>
#include <wx/statbmp.h>

#include "DolphinWX/Input/InputConfigDiag.h"
#include "DolphinWX/WxUtils.h"

#include "InputCommon/ControllerEmu/Control/Control.h"
#include "InputCommon/ControllerEmu/ControlGroup/AnalogStick.h"
#include "InputCommon/ControllerEmu/ControlGroup/Buttons.h"
#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "InputCommon/ControllerEmu/ControlGroup/Cursor.h"
#include "InputCommon/ControllerEmu/ControlGroup/Force.h"
#include "InputCommon/ControllerEmu/ControlGroup/Slider.h"
#include "InputCommon/ControllerEmu/ControlGroup/Tilt.h"
#include "InputCommon/ControllerEmu/ControlGroup/Triggers.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"
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
static void DrawOctagon(wxGraphicsContext* gc, ShapePosition p)
{
#if defined(_MSC_VER) && _MSC_VER <= 1800
#define CONSTEXPR(datatype, name, value)                                                           \
  enum name##_enum : datatype { name = value }
#else
#define CONSTEXPR(datatype, name, value) constexpr datatype name = value
#endif
  static CONSTEXPR(int, vertices, 8);
  double radius = p.max;

  wxGraphicsPath path = gc->CreatePath();

  double angle = 2.0 * M_PI / vertices;

  for (int i = 0; i < vertices; i++)
  {
    double a = angle * i;
    double x = radius * cos(a);
    double y = radius * sin(a);
    if (i == 0)
      path.MoveToPoint(x, y);
    else
      path.AddLineToPoint(x, y);
  }
  path.CloseSubpath();

  wxGraphicsMatrix matrix = gc->CreateMatrix();
  matrix.Translate(p.offset, p.offset);
  path.Transform(matrix);

  gc->DrawPath(path);
}

// irregular dodecagon
static void DrawDodecagon(wxGraphicsContext* gc, ShapePosition p)
{
  wxGraphicsPath path = gc->CreatePath();
  path.MoveToPoint(p.dz, p.max);
  path.AddLineToPoint(p.diag, p.diag);
  path.AddLineToPoint(p.max, p.dz);
  path.AddLineToPoint(p.max, -p.dz);
  path.AddLineToPoint(p.diag, -p.diag);
  path.AddLineToPoint(p.dz, -p.max);
  path.AddLineToPoint(-p.dz, -p.max);
  path.AddLineToPoint(-p.diag, -p.diag);
  path.AddLineToPoint(-p.max, -p.dz);
  path.AddLineToPoint(-p.max, p.dz);
  path.AddLineToPoint(-p.diag, p.diag);
  path.AddLineToPoint(-p.dz, p.max);
  path.CloseSubpath();

  wxGraphicsMatrix matrix = gc->CreateMatrix();
  matrix.Translate(p.offset, p.offset);
  path.Transform(matrix);

  gc->DrawPath(path);
}

static void DrawCenteredRectangle(wxGraphicsContext* gc, double x, double y, double w, double h)
{
  x -= w / 2;
  y -= h / 2;
  gc->DrawRectangle(x, y, w, h);
}

#define VIS_BITMAP_SIZE 64

#define VIS_NORMALIZE(a) ((a / 2.0) + 0.5)
#define VIS_COORD(a) ((VIS_NORMALIZE(a)) * VIS_BITMAP_SIZE)

#define COORD_VIS_SIZE 4

static void DrawCoordinate(wxGraphicsContext* gc, ControlState x, ControlState y)
{
  double xc = VIS_COORD(x);
  double yc = VIS_COORD(y);
  DrawCenteredRectangle(gc, xc, yc, COORD_VIS_SIZE, COORD_VIS_SIZE);
}

static void DrawButton(const std::vector<unsigned int>& bitmasks, unsigned int buttons,
                       unsigned int n, wxGraphicsContext* gc, ControlGroupBox* g, unsigned int row,
                       const wxGraphicsMatrix& null_matrix)
{
  if (buttons & bitmasks[(row * 8) + n])
  {
    gc->SetBrush(*wxRED_BRUSH);
  }
  else
  {
    unsigned char amt = 255 - g->control_group->controls[(row * 8) + n]->control_ref->State() * 128;
    gc->SetBrush(wxBrush(wxColour(amt, amt, amt)));
  }
  gc->DrawRectangle(n * 12, (row == 0) ? 0 : (row * 11), 14, 12);

  // text
  const std::string name = g->control_group->controls[(row * 8) + n]->ui_name;
  // Matrix transformation needs to be disabled so we don't draw scaled/zoomed text.
  wxGraphicsMatrix old_matrix = gc->GetTransform();
  gc->SetTransform(null_matrix);
  // bit of hax so ZL, ZR show up as L, R
  gc->DrawText(wxUniChar((name[1] && name[1] < 'a') ? name[1] : name[0]), (n * 12 + 2) * g->m_scale,
               (1 + (row == 0 ? 0 : row * 11)) * g->m_scale);
  gc->SetTransform(old_matrix);
}

static void DrawControlGroupBox(wxGraphicsContext* gc, ControlGroupBox* g)
{
  wxGraphicsMatrix null_matrix = gc->GetTransform();
  wxGraphicsMatrix scale_matrix = null_matrix;
  scale_matrix.Scale(g->m_scale, g->m_scale);

  gc->SetTransform(scale_matrix);

  switch (g->control_group->type)
  {
  case ControllerEmu::GroupType::Tilt:
  case ControllerEmu::GroupType::Stick:
  case ControllerEmu::GroupType::Cursor:
  {
    // this is starting to be a mess combining all these in one case

    ControlState x = 0, y = 0, z = 0;

    switch (g->control_group->type)
    {
    case ControllerEmu::GroupType::Stick:
      ((ControllerEmu::AnalogStick*)g->control_group)->GetState(&x, &y);
      break;
    case ControllerEmu::GroupType::Tilt:
      ((ControllerEmu::Tilt*)g->control_group)->GetState(&x, &y);
      break;
    case ControllerEmu::GroupType::Cursor:
      ((ControllerEmu::Cursor*)g->control_group)->GetState(&x, &y, &z);
      break;
    case ControllerEmu::GroupType::Other:
    case ControllerEmu::GroupType::MixedTriggers:
    case ControllerEmu::GroupType::Buttons:
    case ControllerEmu::GroupType::Force:
    case ControllerEmu::GroupType::Extension:
    case ControllerEmu::GroupType::Triggers:
    case ControllerEmu::GroupType::Slider:
      break;
    }

    // ir cursor forward movement
    if (g->control_group->type == ControllerEmu::GroupType::Cursor)
    {
      gc->SetBrush(z ? *wxRED_BRUSH : *wxGREY_BRUSH);
      wxGraphicsPath path = gc->CreatePath();
      path.AddRectangle(0, 31 - z * 31, 64, 2);
      gc->FillPath(path);
    }

    // input zone
    gc->SetPen(*wxLIGHT_GREY_PEN);
    if (g->control_group->type == ControllerEmu::GroupType::Stick)
    {
      gc->SetBrush(wxColour(0xDDDDDD));  // Light Gray

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
        DrawOctagon(gc, p);
      else
        DrawDodecagon(gc, p);
    }
    else
    {
      gc->SetBrush(*wxWHITE_BRUSH);
      gc->DrawRectangle(16, 16, 32, 32);
    }

    if (g->control_group->type != ControllerEmu::GroupType::Cursor)
    {
      const int deadzone_idx = g->control_group->type == ControllerEmu::GroupType::Stick ?
                                   ControllerEmu::AnalogStick::SETTING_DEADZONE :
                                   0;

      wxGraphicsPath path = gc->CreatePath();
      path.AddCircle(VIS_BITMAP_SIZE / 2, VIS_BITMAP_SIZE / 2,
                     g->control_group->numeric_settings[deadzone_idx]->GetValue() *
                         VIS_BITMAP_SIZE / 2);
      gc->SetBrush(*wxLIGHT_GREY_BRUSH);
      gc->FillPath(path);
    }

    // raw dot
    ControlState xx, yy;
    xx = g->control_group->controls[3]->control_ref->State();
    xx -= g->control_group->controls[2]->control_ref->State();
    yy = g->control_group->controls[1]->control_ref->State();
    yy -= g->control_group->controls[0]->control_ref->State();

    gc->SetPen(*wxTRANSPARENT_PEN);
    gc->SetBrush(*wxGREY_BRUSH);
    DrawCoordinate(gc, xx, yy);

    // adjusted dot
    if (x != 0 || y != 0)
    {
      gc->SetBrush(*wxRED_BRUSH);
      // XXX: The adjusted values flip the Y axis to be in the format
      // the Wii expects. Should this be in WiimoteEmu.cpp instead?
      DrawCoordinate(gc, x, -y);
    }
  }
  break;

  case ControllerEmu::GroupType::Force:
  {
    ControlState raw_dot[3];
    ControlState adj_dot[3];
    const ControlState deadzone = g->control_group->numeric_settings[0]->GetValue();

    // adjusted
    ((ControllerEmu::Force*)g->control_group)->GetState(adj_dot);

    // raw
    for (unsigned int i = 0; i < 3; ++i)
    {
      raw_dot[i] = (g->control_group->controls[i * 2 + 1]->control_ref->State() -
                    g->control_group->controls[i * 2]->control_ref->State());
    }

    // deadzone rect for forward/backward visual
    gc->SetPen(*wxTRANSPARENT_PEN);
    gc->SetBrush(*wxLIGHT_GREY_BRUSH);
    int deadzone_height = deadzone * VIS_BITMAP_SIZE;
    DrawCenteredRectangle(gc, 0, VIS_BITMAP_SIZE / 2, VIS_BITMAP_SIZE, deadzone_height);

#define LINE_HEIGHT 2
    int line_y;

    // raw forward/background line
    gc->SetBrush(*wxGREY_BRUSH);
    line_y = VIS_COORD(raw_dot[2]);
    DrawCenteredRectangle(gc, VIS_BITMAP_SIZE / 2, line_y, VIS_BITMAP_SIZE, LINE_HEIGHT);

    // adjusted forward/background line
    if (adj_dot[2] != 0.0)
    {
      gc->SetBrush(*wxRED_BRUSH);
      line_y = VIS_COORD(adj_dot[2]);
      DrawCenteredRectangle(gc, VIS_BITMAP_SIZE / 2, line_y, VIS_BITMAP_SIZE, LINE_HEIGHT);
    }

#define DEADZONE_RECT_SIZE 32

    // empty deadzone square
    gc->SetPen(*wxLIGHT_GREY_PEN);
    gc->SetBrush(*wxWHITE_BRUSH);
    DrawCenteredRectangle(gc, VIS_BITMAP_SIZE / 2, VIS_BITMAP_SIZE / 2, DEADZONE_RECT_SIZE,
                          DEADZONE_RECT_SIZE);

    // deadzone square
    gc->SetPen(*wxTRANSPARENT_PEN);
    gc->SetBrush(*wxLIGHT_GREY_BRUSH);
    int dz_size = (deadzone * DEADZONE_RECT_SIZE);
    DrawCenteredRectangle(gc, VIS_BITMAP_SIZE / 2, VIS_BITMAP_SIZE / 2, dz_size, dz_size);

    // raw dot
    gc->SetBrush(*wxGREY_BRUSH);
    DrawCoordinate(gc, raw_dot[1], raw_dot[0]);

    // adjusted dot
    if (adj_dot[1] != 0 && adj_dot[0] != 0)
    {
      gc->SetBrush(*wxRED_BRUSH);
      DrawCoordinate(gc, adj_dot[1], adj_dot[0]);
    }
  }
  break;

  case ControllerEmu::GroupType::Buttons:
  {
    const unsigned int button_count = static_cast<unsigned int>(g->control_group->controls.size());
    std::vector<unsigned int> bitmasks(button_count);

    // draw the shit
    gc->SetPen(*wxGREY_PEN);

    for (unsigned int n = 0; n < button_count; ++n)
      bitmasks[n] = (1 << n);

    unsigned int buttons = 0;
    ((ControllerEmu::Buttons*)g->control_group)->GetState(&buttons, bitmasks.data());

    // Draw buttons in rows of 8
    for (unsigned int row = 0; row < ceil((float)button_count / 8.0f); row++)
    {
      unsigned int buttons_to_draw = 8;
      if ((button_count - row * 8) <= 8)
        buttons_to_draw = button_count - row * 8;

      for (unsigned int n = 0; n < buttons_to_draw; ++n)
      {
        DrawButton(bitmasks, buttons, n, gc, g, row, null_matrix);
      }
    }
  }
  break;

  case ControllerEmu::GroupType::Triggers:
  {
    const unsigned int trigger_count = static_cast<unsigned int>(g->control_group->controls.size());
    std::vector<ControlState> trigs(trigger_count);

    // draw the shit
    gc->SetPen(*wxGREY_PEN);

    ControlState deadzone = g->control_group->numeric_settings[0]->GetValue();
    ((ControllerEmu::Triggers*)g->control_group)->GetState(trigs.data());

    for (unsigned int n = 0; n < trigger_count; ++n)
    {
      ControlState trig_r = g->control_group->controls[n]->control_ref->State();

      // outline
      gc->SetBrush(*wxWHITE_BRUSH);
      gc->DrawRectangle(0, n * 12, 64, 14);

      // raw
      gc->SetBrush(*wxGREY_BRUSH);
      gc->DrawRectangle(0, n * 12, trig_r * 64, 14);

      // deadzone affected
      gc->SetBrush(*wxRED_BRUSH);
      gc->DrawRectangle(0, n * 12, trigs[n] * 64, 14);

      // text
      // We don't want the text to be scaled/zoomed
      gc->SetTransform(null_matrix);
      gc->DrawText(StrToWxStr(g->control_group->controls[n]->ui_name), 3 * g->m_scale,
                   (n * 12 + 1) * g->m_scale);
      gc->SetTransform(scale_matrix);
    }

    // deadzone box
    gc->SetPen(*wxLIGHT_GREY_PEN);
    gc->SetBrush(*wxTRANSPARENT_BRUSH);
    gc->DrawRectangle(0, 0, deadzone * 64, trigger_count * 14);
  }
  break;

  case ControllerEmu::GroupType::MixedTriggers:
  {
    const unsigned int trigger_count = ((unsigned int)(g->control_group->controls.size() / 2));

    // draw the shit
    gc->SetPen(*wxGREY_PEN);
    ControlState thresh = g->control_group->numeric_settings[0]->GetValue();

    for (unsigned int n = 0; n < trigger_count; ++n)
    {
      gc->SetBrush(*wxRED_BRUSH);

      ControlState trig_d = g->control_group->controls[n]->control_ref->State();

      ControlState trig_a =
          trig_d > thresh ? 1 : g->control_group->controls[n + trigger_count]->control_ref->State();

      gc->DrawRectangle(0, n * 12, 64 + 20, 14);
      if (trig_d <= thresh)
        gc->SetBrush(*wxWHITE_BRUSH);
      gc->DrawRectangle(trig_a * 64, n * 12, 64 + 20, 14);
      gc->DrawRectangle(64, n * 12, 32, 14);

      // text
      // We don't want the text to be scaled/zoomed
      gc->SetTransform(null_matrix);
      gc->DrawText(StrToWxStr(g->control_group->controls[n + trigger_count]->ui_name),
                   3 * g->m_scale, (n * 12 + 1) * g->m_scale);
      gc->DrawText(StrToWxStr(std::string(1, g->control_group->controls[n]->ui_name[0])),
                   (64 + 3) * g->m_scale, (n * 12 + 1) * g->m_scale);
      gc->SetTransform(scale_matrix);
    }

    // threshold box
    gc->SetPen(*wxLIGHT_GREY_PEN);
    gc->SetBrush(*wxTRANSPARENT_BRUSH);
    gc->DrawRectangle(thresh * 64, 0, 128, trigger_count * 14);
  }
  break;

  case ControllerEmu::GroupType::Slider:
  {
    const ControlState deadzone = g->control_group->numeric_settings[0]->GetValue();

    ControlState state = g->control_group->controls[1]->control_ref->State() -
                         g->control_group->controls[0]->control_ref->State();
    gc->SetPen(*wxTRANSPARENT_PEN);
    gc->SetBrush(*wxGREY_BRUSH);
    gc->DrawRectangle(31 + state * 30, 0, 2, 14);

    ControlState adj_state;
    ((ControllerEmu::Slider*)g->control_group)->GetState(&adj_state);
    if (state)
    {
      gc->SetBrush(*wxRED_BRUSH);
      gc->DrawRectangle(31 + adj_state * 30, 0, 2, 14);
    }

    // deadzone box
    gc->SetPen(*wxLIGHT_GREY_PEN);
    gc->SetBrush(*wxTRANSPARENT_BRUSH);
    gc->DrawRectangle(32 - deadzone * 32, 0, deadzone * 64, 14);
  }
  break;

  default:
    break;
  }
  gc->SetTransform(null_matrix);
}

static void DrawBorder(wxGraphicsContext* gc, double scale)
{
  double pen_width = std::round(scale);  // Pen width = 1px * scale

  // Use the window caption bar color as a safe accent color.
  wxPen border_pen(wxSystemSettings::GetColour(wxSYS_COLOUR_ACTIVECAPTION),
                   static_cast<int>(pen_width));
  border_pen.SetCap(wxCAP_PROJECTING);

  double width, height;
  gc->GetSize(&width, &height);

  wxGraphicsPath path = gc->CreatePath();
  path.AddRectangle(pen_width / 2, pen_width / 2, width - pen_width, height - pen_width);
  gc->SetPen(border_pen);
  gc->StrokePath(path);
}

void InputConfigDialog::UpdateBitmaps(wxTimerEvent& WXUNUSED(event))
{
  wxFont small_font(6, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);
  const wxColour font_color{0xB8B8B8};

  g_controller_interface.UpdateInput();

  wxMemoryDC dc;
  const auto lock = ControllerEmu::EmulatedController::GetStateLock();
  for (ControlGroupBox* g : control_groups)
  {
    // Only if this control group has a bitmap
    if (!g->static_bitmap)
      continue;

    wxBitmap bitmap(g->static_bitmap->GetBitmap());
    // NOTE: Selecting the bitmap inherits the bitmap's ScaleFactor onto the DC as well.
    dc.SelectObjectAsSource(bitmap);
    dc.SetBackground(*wxWHITE_BRUSH);
    dc.Clear();

#ifdef __WXGTK20__
    int dc_height = 0;
    dc.SetFont(small_font);
    dc.GetTextExtent(g->control_group->name, nullptr, &dc_height);
#endif

    std::unique_ptr<wxGraphicsContext> gc{wxGraphicsContext::Create(dc)};
    gc->DisableOffset();
    gc->SetFont(small_font, font_color);

#ifdef __WXGTK20__
    double gc_height = 0;
    gc->GetTextExtent(g->control_group->name, nullptr, &gc_height);
    // On GTK2, wx creates a new empty Cairo/Pango context for the graphics context instead
    // of reusing the wxMemoryDC one, this causes it to forget the screen DPI so fonts stop
    // scaling, we need to scale it manually instead.
    if (std::ceil(gc_height) < dc_height)
    {
      wxFont fixed_font(small_font);
      fixed_font.SetPointSize(static_cast<int>(fixed_font.GetPointSize() * g->m_scale));
      gc->SetFont(fixed_font, font_color);
    }
#endif

    DrawControlGroupBox(gc.get(), g);
    DrawBorder(gc.get(), g->m_scale);

    // label for sticks and stuff
    if (g->HasBitmapHeading())
      gc->DrawText(StrToWxStr(g->control_group->name).Upper(), 4 * g->m_scale, 2 * g->m_scale);

    gc.reset();
    dc.SelectObject(wxNullBitmap);
    g->static_bitmap->SetBitmap(bitmap);
  }
}
