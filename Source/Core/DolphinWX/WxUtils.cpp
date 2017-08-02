// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <wx/app.h>
#include <wx/bitmap.h>
#include <wx/choice.h>
#include <wx/combo.h>
#include <wx/combobox.h>
#include <wx/display.h>
#include <wx/gdicmn.h>
#include <wx/image.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/toolbar.h>
#include <wx/toplevel.h>
#include <wx/utils.h>

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/StringUtil.h"
#include "Core/ConfigManager.h"

#include "DolphinWX/WxUtils.h"

#ifdef _WIN32
#include <Windows.h>
#endif

namespace WxUtils
{
// Launch a file according to its mime type
void Launch(const std::string& filename)
{
  if (!::wxLaunchDefaultBrowser(StrToWxStr(filename)))
  {
    // WARN_LOG
  }
}

// Launch an file explorer window on a certain path
void Explore(const std::string& path)
{
  wxString wxPath = StrToWxStr(path);
#ifndef _WIN32
  // Default to file
  if (!wxPath.Contains("://"))
  {
    wxPath = "file://" + wxPath;
  }
#endif

#ifdef __WXGTK__
  wxPath.Replace(" ", "\\ ");
#endif

  if (!::wxLaunchDefaultBrowser(wxPath))
  {
    // WARN_LOG
  }
}

void ShowErrorDialog(const wxString& error_msg)
{
  wxMessageBox(error_msg, _("Error"), wxOK | wxICON_ERROR);
}

wxBitmap CreateDisabledButtonBitmap(const wxBitmap& original)
{
  wxImage image = original.ConvertToImage();
  return wxBitmap(image.ConvertToDisabled(240), wxBITMAP_SCREEN_DEPTH, original.GetScaleFactor());
}

void AddToolbarButton(wxToolBar* toolbar, int toolID, const wxString& label, const wxBitmap& bitmap,
                      const wxString& shortHelp)
{
  // Must explicitly set the disabled button bitmap because wxWidgets
  // incorrectly desaturates it instead of lightening it.
  toolbar->AddTool(toolID, label, bitmap, WxUtils::CreateDisabledButtonBitmap(bitmap),
                   wxITEM_NORMAL, shortHelp);
}

wxIconBundle GetDolphinIconBundle()
{
  static wxIconBundle s_bundle;
  if (!s_bundle.IsEmpty())
    return s_bundle;

#ifdef _WIN32

  // Convert the Windows ICO file into a wxIconBundle by tearing it apart into each individual
  // sub-icon using the Win32 API. This is necessary because WX uses its own wxIcons internally
  // which (unlike QIcon in Qt) only contain 1 image per icon, hence why wxIconBundle exists.
  HINSTANCE dolphin = GetModuleHandleW(nullptr);
  for (int size : {16, 32, 48, 256})
  {
    // Extract resource from embedded DolphinWX.rc
    HANDLE win32_icon =
        LoadImageW(dolphin, L"\"DOLPHIN\"", IMAGE_ICON, size, size, LR_CREATEDIBSECTION);
    if (win32_icon && win32_icon != INVALID_HANDLE_VALUE)
    {
      wxIcon icon;
      icon.CreateFromHICON(reinterpret_cast<HICON>(win32_icon));
      s_bundle.AddIcon(icon);
    }
  }

#else

  for (const char* fname : {"Dolphin.png", "dolphin_logo.png", "dolphin_logo@2x.png"})
  {
    wxImage image{StrToWxStr(File::GetSysDirectory() + RESOURCES_DIR DIR_SEP + fname),
                  wxBITMAP_TYPE_PNG};
    if (image.IsOk())
    {
      wxIcon icon;
      icon.CopyFromBitmap(image);
      s_bundle.AddIcon(icon);
    }
  }

#endif

  return s_bundle;
}

wxRect GetVirtualScreenGeometry()
{
  wxRect geometry;
  for (unsigned int i = 0, end = wxDisplay::GetCount(); i < end; ++i)
    geometry.Union(wxDisplay(i).GetGeometry());
  return geometry;
}

void SetWindowSizeAndFitToScreen(wxTopLevelWindow* tlw, wxPoint pos, wxSize size,
                                 wxSize default_size)
{
  if (tlw->IsMaximized())
    return;

  // NOTE: Positions can be negative and still be valid. Coordinates are relative to the
  //   primary monitor so if the primary monitor is in the middle then (-1000, 10) is a
  //   valid position on the monitor to the left of the primary. (This does not apply to
  //   sizes obviously)
  wxRect screen_geometry;
  wxRect window_geometry{pos, size};

  if (wxDisplay::GetCount() > 1)
    screen_geometry = GetVirtualScreenGeometry();
  else
    screen_geometry = wxDisplay(0).GetClientArea();

  // Initialize the default size if it is wxDefaultSize or otherwise negative.
  default_size.DecTo(screen_geometry.GetSize());
  default_size.IncTo(tlw->GetMinSize());
  if (!default_size.IsFullySpecified())
    default_size.SetDefaults(wxDisplay(0).GetClientArea().GetSize() / 2);

  // If the position we're given doesn't make sense then go with the current position.
  // (Assuming the window was created with wxDefaultPosition then this should be reasonable)
  if (pos.x - screen_geometry.GetLeft() < -1000 || pos.y - screen_geometry.GetTop() < -1000 ||
      pos.x - screen_geometry.GetRight() > 1000 || pos.y - screen_geometry.GetBottom() > 1000)
  {
    window_geometry.SetPosition(tlw->GetPosition());
  }

  // If the window is bigger than all monitors combined, or negative (uninitialized) then reset it.
  if (window_geometry.IsEmpty() || window_geometry.GetWidth() > screen_geometry.GetWidth() ||
      window_geometry.GetHeight() > screen_geometry.GetHeight())
  {
    window_geometry.SetSize(default_size);
  }

  // Check if the window entirely lives on a single monitor without spanning.
  // If the window does not span multiple screens then we should constrain it within that
  // single monitor instead of the entire virtual desktop space.
  // The benefit to doing this is that we can account for the OS X menu bar and Windows task
  // bar which are treated as invisible when only looking at the virtual desktop instead of
  // an individual screen.
  if (wxDisplay::GetCount() > 1)
  {
    // SPECIAL CASE: If the window is entirely outside the visible area of the desktop then we
    //   put it back on the primary (zero) monitor.
    wxRect monitor_intersection{window_geometry};
    int the_monitor = 0;
    if (!monitor_intersection.Intersect(screen_geometry).IsEmpty())
    {
      std::array<int, 4> monitors{{wxDisplay::GetFromPoint(monitor_intersection.GetTopLeft()),
                                   wxDisplay::GetFromPoint(monitor_intersection.GetTopRight()),
                                   wxDisplay::GetFromPoint(monitor_intersection.GetBottomLeft()),
                                   wxDisplay::GetFromPoint(monitor_intersection.GetBottomRight())}};
      the_monitor = wxNOT_FOUND;
      bool intersected = false;
      for (int one_monitor : monitors)
      {
        if (one_monitor == the_monitor || one_monitor == wxNOT_FOUND)
          continue;
        if (the_monitor != wxNOT_FOUND)
        {
          // The window is spanning multiple screens.
          the_monitor = wxNOT_FOUND;
          break;
        }
        the_monitor = one_monitor;
        intersected = true;
      }
      // If we get wxNOT_FOUND for all corners then there are holes in the virtual desktop and the
      // entire window is lost in one. (e.g. 3 monitors in an 'L', window in top-right)
      if (!intersected)
        the_monitor = 0;
    }
    if (the_monitor != wxNOT_FOUND)
    {
      // We'll only use the client area of this monitor if the window will actually fit.
      // (It may not fit if the window is spilling off the edge so it isn't entirely visible)
      wxRect client_area{wxDisplay(the_monitor).GetClientArea()};
      if (client_area.GetWidth() >= window_geometry.GetWidth() &&
          client_area.GetHeight() >= window_geometry.GetHeight())
      {
        screen_geometry = client_area;
      }
    }
  }

  // The window SHOULD be small enough to fit on the screen, but it might be spilling off an edge
  // so we'll snap it to the nearest edge as necessary.
  if (!screen_geometry.Contains(window_geometry))
  {
    // NOTE: The order is important here, if the window *is* too big to fit then it will snap to
    //   the top-left corner.
    int spill_x = std::max(0, window_geometry.GetRight() - screen_geometry.GetRight());
    int spill_y = std::max(0, window_geometry.GetBottom() - screen_geometry.GetBottom());
    window_geometry.Offset(-spill_x, -spill_y);
    if (window_geometry.GetTop() < screen_geometry.GetTop())
      window_geometry.SetTop(screen_geometry.GetTop());
    if (window_geometry.GetLeft() < screen_geometry.GetLeft())
      window_geometry.SetLeft(screen_geometry.GetLeft());
  }

  tlw->SetSize(window_geometry, wxSIZE_ALLOW_MINUS_ONE);
}

wxSizer* GiveMinSize(wxWindow* window, const wxSize& min_size)
{
  wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);
  int flags = wxEXPAND;
  // On Windows comboboxes will misrender when stretched vertically.
  if (wxDynamicCast(window, wxChoice) || wxDynamicCast(window, wxComboBox) ||
      wxDynamicCast(window, wxComboCtrl))
    flags = wxALIGN_CENTER_VERTICAL;
  sizer->Add(window, 1, flags);
  sizer->SetMinSize(min_size);
  return sizer;
}

wxSizer* GiveMinSizeDIP(wxWindow* window, const wxSize& min_size)
{
  return GiveMinSize(window, window->FromDIP(min_size));
}

wxSize GetTextWidgetMinSize(const wxControl* control, const wxString& value)
{
  return control->GetSizeFromTextSize(control->GetTextExtent(value));
}

wxSize GetTextWidgetMinSize(const wxControl* control, unsigned int value)
{
  return GetTextWidgetMinSize(control, wxString::Format("%u", value));
}

wxSize GetTextWidgetMinSize(const wxControl* control, int value)
{
  return GetTextWidgetMinSize(control, wxString::Format("%d", value));
}

wxSize GetTextWidgetMinSize(const wxSpinCtrl* spinner)
{
  wxSize size = GetTextWidgetMinSize(spinner, spinner->GetMin());
  size.IncTo(GetTextWidgetMinSize(spinner, spinner->GetMax()));
  return size;
}

static wxImage LoadScaledImage(const std::string& file_path, const wxWindow* context,
                               const wxSize& output_size, const wxRect& usable_rect, LSIFlags flags,
                               const wxColour& fill_color)
{
  std::string fpath, fname, fext;
  SplitPath(file_path, &fpath, &fname, &fext);

  const double window_scale_factor = context->GetContentScaleFactor();
  // Compute the total scale factor from the ratio of DIPs to window pixels (FromDIP) and
  // window pixels to framebuffer pixels (GetContentScaleFactor).
  // NOTE: Usually only one of these is meaningful:
  //   - On Windows/GTK2: content_scale = 1.0, FromDIP = 96DPI -> Screen DPI
  //   - On Mac OS X: content_scale = screen_dpi / 96, FromDIP = 96DPI -> 96DPI (no-op)
  // [The 1024 is arbitrarily large to minimise rounding error, it has no significance]
  const double scale_factor = (context->FromDIP(1024) / 1024.0) * window_scale_factor;

  // We search for files on quarter ratios of DIPs to framebuffer pixels.
  // By default, the algorithm prefers to find an exact or bigger size then downscale if
  // needed but will resort to upscaling if a bigger image cannot be found.
  // E.g. A basic retina screen on Mac OS X has a scale_factor of 2.0, so we would look for
  // @2x, @2.25x, @2.5x, @2.75x, @3x, @1.75x, @1.5x, @1.25x, @1x, then give up.
  // (At 125% on Windows the search is @1.25, @1.5, @1.75, @2, @2.25, @1)
  // If flags does not include LSI_SCALE_DOWN (i.e. we would be forced to crop big
  // images instead of scaling them) then we will only accept smaller sizes, i.e.
  // @2x, @1.75, @1.5, @1.25, @1, then give up.
  // NOTE: We do a lot of exact comparisons against floating point here but it's fine
  //   because the numbers involved are all powers of 2 so can be represented exactly.
  wxImage image;
  double selected_image_scale = 1;
  {
    auto image_check = [&](double scale) -> bool {
      std::string path = fpath + fname + StringFromFormat("@%gx", scale) + fext;
      if (!File::Exists(path))
      {
        // Special Case: @1x may not have a suffix at all.
        if (scale != 1.0 || !File::Exists(file_path))
          return false;
        path = file_path;
      }
      if (!image.LoadFile(StrToWxStr(path), wxBITMAP_TYPE_ANY))
        return false;
      selected_image_scale = scale;
      return true;
    };
    const bool prefer_smaller = !(flags & LSI_SCALE_DOWN);
    const double scale_factor_quarter =
        prefer_smaller ? std::floor(scale_factor * 4) / 4 : std::ceil(scale_factor * 4) / 4;
    // Search for bigger sizes first (preferred)
    if (!prefer_smaller)
    {
      // We search within a 'circle' of the exact match limited by scale=1.0.
      // i.e. scale_factor = 1.5, radius = 0.5; scale = 2.5, radius = 1.5.
      // The minimum radius is 1.0.
      double limit = std::max(scale_factor_quarter * 2 - 1, scale_factor_quarter + 1);
      for (double quarter = scale_factor_quarter; quarter <= limit; quarter += 0.25)
      {
        if (image_check(quarter))
          break;
      }
    }
    // If we didn't hit a bigger size then we'll fallback to looking for smaller ones
    if (!image.IsOk())
    {
      double quarter = scale_factor_quarter;
      if (!prefer_smaller)  // So we don't recheck the exact match
        quarter -= 0.25;
      for (; quarter >= 1.0; quarter -= 0.25)
      {
        if (image_check(quarter))
          break;
      }
    }
  }

  // The file apparently does not exist so we give up. Create a white square placeholder instead.
  if (!image.IsOk())
  {
    wxLogError("Could not find resource: %s", StrToWxStr(file_path));
    image.Create(1, 1, false);
    image.Clear(0xFF);
  }

  return ScaleImage(image, selected_image_scale, window_scale_factor, output_size, usable_rect,
                    flags, fill_color);
}

wxBitmap LoadScaledBitmap(const std::string& file_path, const wxWindow* context,
                          const wxSize& output_size, const wxRect& usable_rect, LSIFlags flags,
                          const wxColour& fill_color)
{
  return wxBitmap(LoadScaledImage(file_path, context, output_size, usable_rect, flags, fill_color),
                  wxBITMAP_SCREEN_DEPTH, context->GetContentScaleFactor());
}

wxBitmap LoadScaledResourceBitmap(const std::string& name, const wxWindow* context,
                                  const wxSize& output_size, const wxRect& usable_rect,
                                  LSIFlags flags, const wxColour& fill_color)
{
  std::string path = File::GetSysDirectory() + RESOURCES_DIR DIR_SEP + name + ".png";
  return LoadScaledBitmap(path, context, output_size, usable_rect, flags, fill_color);
}

wxBitmap LoadScaledThemeBitmap(const std::string& name, const wxWindow* context,
                               const wxSize& output_size, const wxRect& usable_rect, LSIFlags flags,
                               const wxColour& fill_color)
{
  std::string path = File::GetThemeDir(SConfig::GetInstance().theme_name) + name + ".png";
  return LoadScaledBitmap(path, context, output_size, usable_rect, flags, fill_color);
}

wxBitmap ScaleImageToBitmap(const wxImage& image, const wxWindow* context,
                            const wxSize& output_size, const wxRect& usable_rect, LSIFlags flags,
                            const wxColour& fill_color)
{
  double scale_factor = context->GetContentScaleFactor();
  return wxBitmap(ScaleImage(image, 1.0, scale_factor, output_size, usable_rect, flags, fill_color),
                  wxBITMAP_SCREEN_DEPTH, scale_factor);
}

wxBitmap ScaleImageToBitmap(const wxImage& image, const wxWindow* context, double source_scale,
                            LSIFlags flags, const wxColour& fill_color)
{
  double scale_factor = context->GetContentScaleFactor();
  return wxBitmap(ScaleImage(image, source_scale, scale_factor, wxDefaultSize, wxDefaultSize, flags,
                             fill_color),
                  wxBITMAP_SCREEN_DEPTH, scale_factor);
}

wxImage ScaleImage(wxImage image, double source_scale_factor, double content_scale_factor,
                   wxSize output_size, wxRect usable_rect, LSIFlags flags,
                   const wxColour& fill_color)
{
  if (!image.IsOk())
  {
    wxFAIL_MSG("WxUtils::ScaleImage expects a valid image.");
    return image;
  }

  if (content_scale_factor != 1.0)
  {
    output_size *= content_scale_factor;
    usable_rect.SetPosition(usable_rect.GetPosition() * content_scale_factor);
    usable_rect.SetSize(usable_rect.GetSize() * content_scale_factor);
  }

  // Fix the output size if it's unset.
  wxSize img_size = image.GetSize();
  if (output_size.GetWidth() < 1)
    output_size.SetWidth(
        static_cast<int>(img_size.GetWidth() * (content_scale_factor / source_scale_factor)));
  if (output_size.GetHeight() < 1)
    output_size.SetHeight(
        static_cast<int>(img_size.GetHeight() * (content_scale_factor / source_scale_factor)));

  // Fix the usable rect. If it's empty then the whole canvas is usable.
  if (usable_rect.IsEmpty())
  {
    // Constructs a temp wxRect 0,0->output_size then move assigns it.
    usable_rect = output_size;
  }
  else if (!usable_rect.Intersects(output_size))
  {
    wxFAIL_MSG("Usable Zone Rectangle is not inside the canvas. Check the output size is correct.");
    image.Create(1, 1, false);
    image.SetRGB(0, 0, fill_color.Red(), fill_color.Green(), fill_color.Blue());
    if (fill_color.Alpha() == wxALPHA_TRANSPARENT)
      image.SetMaskColour(fill_color.Red(), fill_color.Green(), fill_color.Blue());
    usable_rect = output_size;
  }

  // Step 1: Scale the image
  if ((flags & LSI_SCALE) != LSI_SCALE_NONE)
  {
    if (flags & LSI_SCALE_NO_ASPECT)
    {
      // Stretch scale without preserving the aspect ratio.
      bool scale_width = (img_size.GetWidth() > usable_rect.GetWidth() && flags & LSI_SCALE_DOWN) ||
                         (img_size.GetWidth() < usable_rect.GetWidth() && flags & LSI_SCALE_UP);
      bool scale_height =
          (img_size.GetHeight() > usable_rect.GetHeight() && flags & LSI_SCALE_DOWN) ||
          (img_size.GetHeight() < usable_rect.GetHeight() && flags & LSI_SCALE_UP);
      if (scale_width || scale_height)
      {
        // NOTE: Using BICUBIC instead of HIGH because it's the same internally
        //   except that downscaling uses a box filter with awful obvious aliasing
        //   for non-integral scale factors.
        image.Rescale(scale_width ? usable_rect.GetWidth() : img_size.GetWidth(),
                      scale_height ? usable_rect.GetHeight() : img_size.GetHeight(),
                      wxIMAGE_QUALITY_BICUBIC);
      }
    }
    else
    {
      // Scale while preserving the aspect ratio.
      double scale = std::min(static_cast<double>(usable_rect.GetWidth()) / img_size.GetWidth(),
                              static_cast<double>(usable_rect.GetHeight()) / img_size.GetHeight());
      int target_width = static_cast<int>(img_size.GetWidth() * scale);
      int target_height = static_cast<int>(img_size.GetHeight() * scale);
      // Bilinear produces sharper images when upscaling, bicubic tends to smear/blur sharp edges.
      if (scale > 1.0 && flags & LSI_SCALE_UP)
        image.Rescale(target_width, target_height, wxIMAGE_QUALITY_BILINEAR);
      else if (scale < 1.0 && flags & LSI_SCALE_DOWN)
        image.Rescale(target_width, target_height, wxIMAGE_QUALITY_BICUBIC);
    }
    img_size = image.GetSize();
  }

  // Step 2: Resize the canvas to match the output size.
  // NOTE: If NOT using LSI_SCALE_DOWN then this will implicitly crop the image
  if (img_size != output_size || usable_rect.GetPosition() != wxPoint())
  {
    wxPoint base = usable_rect.GetPosition();
    if (flags & LSI_ALIGN_HCENTER)
      base.x += (usable_rect.GetWidth() - img_size.GetWidth()) / 2;
    else if (flags & LSI_ALIGN_RIGHT)
      base.x += usable_rect.GetWidth() - img_size.GetWidth();
    if (flags & LSI_ALIGN_VCENTER)
      base.y += (usable_rect.GetHeight() - img_size.GetHeight()) / 2;
    else if (flags & LSI_ALIGN_BOTTOM)
      base.y += usable_rect.GetHeight() - img_size.GetHeight();

    int r = -1, g = -1, b = -1;
    if (fill_color.Alpha() != wxALPHA_TRANSPARENT)
    {
      r = fill_color.Red();
      g = fill_color.Green();
      b = fill_color.Blue();
    }
    image.Resize(output_size, base, r, g, b);
  }

  return image;
}

}  // namespace

std::string WxStrToStr(const wxString& str)
{
  return str.ToUTF8().data();
}

wxString StrToWxStr(const std::string& str)
{
  // return wxString::FromUTF8Unchecked(str.c_str());
  return wxString::FromUTF8(str.c_str());
}

unsigned long WxStrToUL(const wxString& str)
{
  unsigned long value = 0;
  str.ToULong(&value);
  return value;
}
