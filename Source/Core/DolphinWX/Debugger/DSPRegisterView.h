// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <wx/grid.h>

#include "Common/CommonTypes.h"

class CDSPRegTable final : public wxGridTableBase
{
public:
  CDSPRegTable() = default;

  int GetNumberCols() override { return 2; }
  int GetNumberRows() override { return 32; }
  bool IsEmptyCell(int row, int col) override { return false; }
  wxString GetValue(int row, int col) override;
  void SetValue(int row, int col, const wxString&) override;
  wxGridCellAttr* GetAttr(int, int, wxGridCellAttr::wxAttrKind) override;
  void UpdateCachedRegs();

private:
  u64 m_CachedCounter = 0;
  std::array<u16, 32> m_CachedRegs{};
  std::array<bool, 32> m_CachedRegHasChanged{};

  DECLARE_NO_COPY_CLASS(CDSPRegTable)
};

class DSPRegisterView final : public wxGrid
{
public:
  explicit DSPRegisterView(wxWindow* parent, wxWindowID id = wxID_ANY);
  void Repopulate();

private:
  // Owned by wx. Deleted implicitly upon destruction.
  CDSPRegTable* m_register_table;
};
