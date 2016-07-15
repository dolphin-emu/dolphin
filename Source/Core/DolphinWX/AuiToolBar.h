// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <wx/aui/auibar.h>
#include <wx/window.h>

// This fixes wxAuiToolBar setting itself to 21 pixels wide regardless of content
// because of a wxWidgets issue as described here: https://dolp.in/pr4013#issuecomment-233096214
// It overrides DoSetSize() to remove the clamping in the original WX code
// which is causing display issues on Linux and OS X.
class DolphinAuiToolBar : public wxAuiToolBar
{
public:
  using wxAuiToolBar::wxAuiToolBar;

protected:
  void DoSetSize(int x, int y, int width, int height, int size_flags) override
  {
    wxWindow::DoSetSize(x, y, width, height, size_flags);
  }
};
