// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <wx/utils.h>

#include "DolphinWX/DolphinSlider.h"

#ifdef __WXMSW__
#define WIN32_LEAN_AND_MEAN 1
// clang-format off
#include <Windows.h>
#include <CommCtrl.h>
// clang-format on
#endif

static constexpr int SLIDER_MIN_LENGTH = 100;

DolphinSlider::DolphinSlider() = default;
DolphinSlider::~DolphinSlider() = default;

bool DolphinSlider::Create(wxWindow* parent, wxWindowID id, int value, int min_val, int max_val,
                           const wxPoint& pos, const wxSize& size, long style,
                           const wxValidator& validator, const wxString& name)
{
  // Sanitize the style flags.
  // We don't want any label flags because those break DPI scaling on wxMSW,
  // wxWidgets will internally lock the height of the slider to 32 pixels.
  style &= ~wxSL_LABELS;

  return wxSlider::Create(parent, id, value, min_val, max_val, pos, size, style, validator, name);
}

wxSize DolphinSlider::DoGetBestClientSize() const
{
#ifdef __WXMSW__
  int ticks = 0;
  int default_length = FromDIP(SLIDER_MIN_LENGTH);
  if (HasFlag(wxSL_TICKS))
  {
    // NOTE: Ticks do not scale at all (on Win7)
    default_length += 4;
    ticks = 6;
  }

  int metric = 0;
  {
    // We need to determine the maximum thumb size because unfortunately the thumb size
    // is controlled by the theme so may have a varying maximum size limit.
    // NOTE: We can't use ourself because we're const so we can't change our size.
    // NOTE: This is less inefficient then it seems, DoGetBestSize() is only called once
    //   per instance and cached until InvalidateBestSize() is called.
    wxSlider* helper = new wxSlider(GetParent(), wxID_ANY, GetValue(), GetMin(), GetMax(),
                                    wxDefaultPosition, FromDIP(wxSize(100, 100)), GetWindowStyle());
    ::RECT r{};
    ::SendMessageW(reinterpret_cast<HWND>(helper->GetHWND()), TBM_GETTHUMBRECT, 0,
                   reinterpret_cast<LPARAM>(&r));
    helper->Destroy();

    // Breakdown metrics
    int computed_size;
    int scroll_size;
    if (HasFlag(wxSL_VERTICAL))
    {
      // Trackbar thumb does not directly touch the edge, we add the padding
      // a second time to pad the other edge to make it symmetric.
      computed_size = static_cast<int>(r.right + r.left);
      scroll_size = ::GetSystemMetrics(SM_CXVSCROLL);
    }
    else
    {
      computed_size = static_cast<int>(r.bottom + r.top);
      scroll_size = ::GetSystemMetrics(SM_CYHSCROLL);
    }

    // This is based on how Microsoft calculates trackbar sizes in the .Net Framework
    // when using automatic sizing in WinForms.
    int max = scroll_size * 2;

    metric = wxClip(computed_size, scroll_size, max);
  }

  if (HasFlag(wxSL_VERTICAL))
    return wxSize(metric + ticks, default_length);
  return wxSize(default_length, metric + ticks);
#else
  wxSize base_size = wxSlider::DoGetBestClientSize();
  // If the base class is not using DoGetBestClientSize(), fallback to DoGetBestSize()
  if (base_size == wxDefaultSize)
    return wxDefaultSize;
  return CorrectMinSize(base_size);
#endif
}

wxSize DolphinSlider::DoGetBestSize() const
{
  return CorrectMinSize(wxSlider::DoGetBestSize());
}

wxSize DolphinSlider::CorrectMinSize(wxSize size) const
{
  wxSize default_length = FromDIP(wxSize(SLIDER_MIN_LENGTH, SLIDER_MIN_LENGTH));
  // GTK Styles sometimes don't return a default length.
  // NOTE: Vertical is the dominant flag if both are set.
  if (HasFlag(wxSL_VERTICAL))
  {
    if (size.GetHeight() < default_length.GetHeight())
    {
      size.SetHeight(default_length.GetHeight());
    }
  }
  else if (size.GetWidth() < default_length.GetWidth())
  {
    size.SetWidth(default_length.GetWidth());
  }
  return size;
}

#ifdef __WXMSW__
WXLRESULT DolphinSlider::MSWWindowProc(WXUINT msg, WXWPARAM wp, WXLPARAM lp)
{
  if (msg == WM_THEMECHANGED)
    InvalidateBestSize();
  return wxSlider::MSWWindowProc(msg, wp, lp);
}
#endif
