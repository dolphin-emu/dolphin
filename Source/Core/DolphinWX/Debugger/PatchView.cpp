// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinWX/Debugger/PatchView.h"

#include <iomanip>
#include <sstream>

#include "Common/CommonTypes.h"
#include "Common/StringUtil.h"
#include "Core/PowerPC/PowerPC.h"
#include "DolphinWX/Debugger/DebuggerUIUtil.h"
#include "DolphinWX/WxUtils.h"

PatchView::PatchView(wxWindow* parent, const wxWindowID id)
    : wxListCtrl(parent, id, wxDefaultPosition, wxDefaultSize,
                 wxLC_REPORT | wxSUNKEN_BORDER | wxLC_ALIGN_LEFT | wxLC_SINGLE_SEL |
                     wxLC_SORT_ASCENDING)
{
  SetFont(DebuggerFont);
  Refresh();
}

void PatchView::Repopulate()
{
  ClearAll();

  InsertColumn(0, _("Active"));
  InsertColumn(1, _("Address"));
  InsertColumn(2, _("Value"));

  u32 patch_id = 0;
  for (const auto& patch : PowerPC::debug_interface.GetPatches())
  {
    const wxString is_enabled =
        (patch.is_enabled == Common::Debug::MemoryPatch::State::Enabled) ? _("on") : _("off");
    const int item = InsertItem(0, is_enabled);
    SetItem(item, 1, StringFromFormat("%08x", patch.address));

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (u8 b : patch.value)
    {
      oss << std::setw(2) << static_cast<u32>(b);
    }
    SetItem(item, 2, oss.str());

    SetItemData(item, patch_id);
    patch_id += 1;
  }

  Refresh();
}

void PatchView::ToggleCurrentSelection()
{
  const int item = GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
  if (item < 0)
    return;

  const u32 patch_id = static_cast<u32>(GetItemData(item));
  const auto& patch = PowerPC::debug_interface.GetPatch(patch_id);
  if (patch.is_enabled == Common::Debug::MemoryPatch::State::Enabled)
    PowerPC::debug_interface.DisablePatch(patch_id);
  else
    PowerPC::debug_interface.EnablePatch(patch_id);
  Repopulate();
}

void PatchView::DeleteCurrentSelection()
{
  const int item = GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
  if (item < 0)
    return;

  const u32 patch_id = static_cast<u32>(GetItemData(item));
  PowerPC::debug_interface.RemovePatch(patch_id);
  Repopulate();
}
