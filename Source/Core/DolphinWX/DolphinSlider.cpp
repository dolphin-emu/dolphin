// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinWX/DolphinSlider.h"
#include "DolphinWX/WxUtils.h"


bool DolphinSlider::Create(wxWindow* parent, wxWindowID id, int value, int min_val, int max_val,
                           const wxPoint& pos, const wxSize& size, long style,
                           const wxValidator& validator, const wxString& name)
{
	// Sanitise the style flags.
	// We don't want any label flags because those break DPI scaling on wxMSW,
	// wxWidgets will internally lock the height of the slider to 32 pixels.
	style &= ~wxSL_LABELS;

	return wxSlider::Create(parent, id, value, min_val, max_val, pos, size, style, validator, name);
}

wxSize DolphinSlider::DoGetBestSize() const
{
#ifdef _WIN32
	// Convert the 96DPI pixel size into screen pixel size.
	return WxUtils::FromDIP(wxSlider::DoGetBestSize(), this);
#else
	// GTK Styles sometimes don't return a default length.
	wxSize best_size = wxSlider::DoGetBestSize();
	// Vertical is the dominant flag if both are set.
	if (HasFlag(wxSL_VERTICAL))
	{
		if (best_size.GetHeight() < 1)
		{
			best_size.SetHeight(WxUtils::FromDIP(100, this, wxVERTICAL));
		}
	}
	else if (best_size.GetWidth() < 1)
	{
		best_size.SetWidth(WxUtils::FromDIP(100, this, wxHORIZONTAL));
	}
	return best_size;
#endif
}
