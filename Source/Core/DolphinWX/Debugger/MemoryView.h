// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <wx/control.h>
#include "Common/CommonTypes.h"

class DebugInterface;

enum class MemoryDataType
{
  U8,
  U16,
  U32,
  ASCII,
  FloatingPoint
};

wxDECLARE_EVENT(DOLPHIN_EVT_MEMORY_VIEW_DATA_TYPE_CHANGED, wxCommandEvent);

class CMemoryView : public wxControl
{
public:
  CMemoryView(DebugInterface* debuginterface, wxWindow* parent);

  u32 GetSelection() const { return selection; }
  int GetMemoryType() const { return memory; }
  void Center(u32 addr)
  {
    curAddress = addr;
    Refresh();
  }

  void SetDataType(MemoryDataType data_type);
  MemoryDataType GetDataType() const { return m_data_type; }
  void SetMemCheckOptions(bool read, bool write, bool log)
  {
    memCheckRead = read;
    memCheckWrite = write;
    memCheckLog = log;
  }

private:
  int YToAddress(int y);
  bool IsHexMode() const
  {
    return m_data_type != MemoryDataType::ASCII && m_data_type != MemoryDataType::FloatingPoint;
  }

  wxString ReadMemoryAsString(u32 address) const;

  void OnPaint(wxPaintEvent& event);
  void OnMouseDownL(wxMouseEvent& event);
  void OnMouseMove(wxMouseEvent& event);
  void OnMouseUpL(wxMouseEvent& event);
  void OnMouseDownR(wxMouseEvent& event);
  void OnScrollWheel(wxMouseEvent& event);
  void OnPopupMenu(wxCommandEvent& event);
  void OnResize(wxSizeEvent& event);

  static constexpr int LEFT_COL_WIDTH = 16;

  DebugInterface* debugger;

  unsigned int align;
  int rowHeight;
  int m_left_col_width;

  u32 selection;
  u32 oldSelection;
  bool selecting;

  int memory;
  int curAddress;

  bool memCheckRead;
  bool memCheckWrite;
  bool memCheckLog;

  MemoryDataType m_data_type;
  MemoryDataType m_last_hex_type = MemoryDataType::U8;
};
