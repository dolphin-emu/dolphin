// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <wx/colour.h>
#include <wx/gdicmn.h>
#include <wx/string.h>

class wxControl;
class wxBitmap;
class wxImage;
class wxSizer;
class wxSpinCtrl;
class wxToolBar;
class wxTopLevelWindow;
class wxWindow;

namespace WxUtils
{
// Launch a file according to its mime type
void Launch(const std::string& filename);

// Launch an file explorer window on a certain path
void Explore(const std::string& path);

// Displays a wxMessageBox geared for errors
void ShowErrorDialog(const wxString& error_msg);

// From a wxBitmap, creates the corresponding disabled version for toolbar buttons
wxBitmap CreateDisabledButtonBitmap(const wxBitmap& original);

// Helper function to add a button to a toolbar
void AddToolbarButton(wxToolBar* toolbar, int toolID, const wxString& label, const wxBitmap& bitmap,
                      const wxString& shortHelp);

// Gets a complete set of window icons at all relevant sizes, use with wxTopLevelWindow::SetIcons
wxIconBundle GetDolphinIconBundle();

// Get the dimensions of the virtual desktop that spans all monitors.
// Matches GetSystemMetrics(SM_XVIRTUALSCREEN), etc on Windows.
wxRect GetVirtualScreenGeometry();

// Takes a top-level window and resizes / repositions it so it fits on the screen.
// Supports spanning multiple monitors if there are multiple monitors.
// Will snap to edge if the window is small enough to fit but spills over the boundary.
void SetWindowSizeAndFitToScreen(wxTopLevelWindow* tlw, wxPoint pos, wxSize size,
                                 wxSize default_size = wxDefaultSize);

// wxSizers use the minimum size of a widget when computing layout instead of the best size.
// The best size is only taken when the minsize is -1,-1 (i.e. undefined).
// This means that elements with a MinSize specified will always have that exact size unless
// wxEXPAND-ed.
// This problem can be resolved by wrapping the widget in a sizer and setting the minimum size on
// the sizer instead. Sizers will always use the best size of the widget, treating their own MinSize
// as a floor which is usually what you want.
wxSizer* GiveMinSize(wxWindow* window, const wxSize& min_size);
wxSizer* GiveMinSizeDIP(wxWindow* window, const wxSize& min_size);

// Compute the proper size for a text widget (wxTextCtrl, wxChoice, wxSpinCtrl, etc)
// Based on the text it will be required to hold. This gives the element the minimum
// width to hold the largest text value instead of being arbitrarily wide.
wxSize GetTextWidgetMinSize(const wxControl* control, const wxString& value);
wxSize GetTextWidgetMinSize(const wxControl* control, unsigned int value);
wxSize GetTextWidgetMinSize(const wxControl* control, int value);
wxSize GetTextWidgetMinSize(const wxSpinCtrl* spinner);

enum LSIFlags : unsigned int
{
  LSI_SCALE_NONE = 0,  // Disable scaling, only resize canvas
  LSI_SCALE_UP = 1,    // Scale up if needed, but crop instead of scaling down
  LSI_SCALE_DOWN = 2,  // Scale down if needed, only expand canvas instead of scaling up
  LSI_SCALE = LSI_SCALE_UP | LSI_SCALE_DOWN,  // Scale either way as needed.
  LSI_SCALE_NO_ASPECT = 8,                    // Disable preserving the aspect ratio of the image.

  LSI_ALIGN_LEFT = 0,        // Place image at the left edge of canvas
  LSI_ALIGN_RIGHT = 0x10,    // Place image at the right edge of canvas
  LSI_ALIGN_HCENTER = 0x20,  // Place image in the horizontal center of canvas
  LSI_ALIGN_TOP = 0,         // Place image at the top of the canvas
  LSI_ALIGN_BOTTOM = 0x40,   // Place image at the bottom of the canvas
  LSI_ALIGN_VCENTER = 0x80,  // Place image in the vertical center of canvas

  LSI_ALIGN_CENTER = LSI_ALIGN_HCENTER | LSI_ALIGN_VCENTER,

  LSI_DEFAULT = LSI_SCALE | LSI_ALIGN_CENTER
};
constexpr LSIFlags operator|(LSIFlags left, LSIFlags right)
{
  return static_cast<LSIFlags>(static_cast<unsigned int>(left) | right);
}
constexpr LSIFlags operator&(LSIFlags left, LSIFlags right)
{
  return static_cast<LSIFlags>(static_cast<unsigned int>(left) & right);
}

// Swiss army knife loader function for preparing a scaled resource image file.
// Only the path and context are mandatory, other parameters can be ignored.
// NOTE: All size parameters are in window pixels, not DIPs or framebuffer pixels.
// output_size = size of image canvas if different from native image size. E.g. 96x32
// usable_rect = part of image canvas that is considered usable. E.g. 0,0 -> 32,32
//    Usable zone is helpful if the canvas is bigger than the area which will be drawn on screen.
// flags = See LSIFlags
// fill_color = Color to fill the unused canvas area (due to aspect ratio or usable_rect).
wxBitmap LoadScaledBitmap(const std::string& file_path, const wxWindow* context,
                          const wxSize& output_size = wxDefaultSize,
                          const wxRect& usable_rect = wxDefaultSize, LSIFlags flags = LSI_DEFAULT,
                          const wxColour& fill_color = wxTransparentColour);
wxBitmap LoadScaledResourceBitmap(const std::string& name, const wxWindow* context,
                                  const wxSize& output_size = wxDefaultSize,
                                  const wxRect& usable_rect = wxDefaultSize,
                                  LSIFlags flags = LSI_DEFAULT,
                                  const wxColour& fill_color = wxTransparentColour);
wxBitmap LoadScaledThemeBitmap(const std::string& name, const wxWindow* context,
                               const wxSize& output_size = wxDefaultSize,
                               const wxRect& usable_rect = wxDefaultSize,
                               LSIFlags flags = LSI_DEFAULT,
                               const wxColour& fill_color = wxTransparentColour);

// Variant of LoadScaledBitmap to scale an image that didn't come from a file.
wxBitmap ScaleImageToBitmap(const wxImage& image, const wxWindow* context,
                            const wxSize& output_size = wxDefaultSize,
                            const wxRect& usable_rect = wxDefaultSize, LSIFlags flags = LSI_DEFAULT,
                            const wxColour& fill_color = wxTransparentColour);

// Rescales image to screen DPI.
// "Source scale" is essentially the image's DPI as a ratio to 96DPI, e.g. 144DPI image has a
// scale of 1.5.
wxBitmap ScaleImageToBitmap(const wxImage& image, const wxWindow* context, double source_scale,
                            LSIFlags flags = LSI_DEFAULT,
                            const wxColour& fill_color = wxTransparentColour);

// Internal scaling engine behind all the Scaling functions.
// Exposes all control parameters instead of infering them from other sources.
// "Content scale" is a factor applied to output_size and usable_rect internally to convert them
// to framebuffer pixel sizes.
// NOTE: Source scale factor only matters if you don't explicitly specify the output size.
wxImage ScaleImage(wxImage image, double source_scale_factor = 1.0,
                   double content_scale_factor = 1.0, wxSize output_size = wxDefaultSize,
                   wxRect usable_rect = wxDefaultSize, LSIFlags flags = LSI_DEFAULT,
                   const wxColour& fill_color = wxTransparentColour);

}  // namespace

std::string WxStrToStr(const wxString& str);
wxString StrToWxStr(const std::string& str);
unsigned long WxStrToUL(const wxString& str);
