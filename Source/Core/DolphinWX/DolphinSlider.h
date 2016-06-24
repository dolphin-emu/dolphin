// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "wx/slider.h"

// wxSlider has several bugs, including not scaling with DPI.
// This extended slider class tries to paper over the flaws.
class DolphinSlider : public wxSlider
{
public:
	DolphinSlider()
	{
	}

	DolphinSlider(const DolphinSlider&) = delete;

	DolphinSlider(wxWindow* parent, wxWindowID id,
	              int value, int min_value, int max_value,
	              const wxPoint& pos = wxDefaultPosition,
	              const wxSize& size = wxDefaultSize,
	              long style = wxSL_HORIZONTAL,
	              const wxValidator& validator = wxDefaultValidator,
	              const wxString& name = wxSliderNameStr)
	{
		Create(parent, id, value, min_value, max_value, pos, size, style, validator, name);
	}

	DolphinSlider& operator=(const DolphinSlider&) = delete;

	bool Create(wxWindow* parent, wxWindowID id,
	            int value, int min_value, int max_value,
	            const wxPoint& pos = wxDefaultPosition,
	            const wxSize& size = wxDefaultSize,
	            long style = wxSL_HORIZONTAL,
	            const wxValidator& validator = wxDefaultValidator,
	            const wxString& name = wxSliderNameStr);

protected:
	// The base implementation in wxMSW::wxSlider is borked.
	// This is called by GetEffectiveMinSize() which is used by
	// wxSizers to decide the size of the widget for generating layout.
	wxSize DoGetBestSize() const override;
};

