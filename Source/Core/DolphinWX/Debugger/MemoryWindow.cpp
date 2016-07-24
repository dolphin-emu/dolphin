// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <wx/button.h>
#include <wx/listbox.h>
#include <wx/msgdlg.h>
#include <wx/panel.h>
#include <wx/radiobox.h>
#include <wx/radiobut.h>
#include <wx/sizer.h>
#include <wx/srchctrl.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/utils.h>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/StringUtil.h"
#include "Common/SymbolDB.h"
#include "Core/ConfigManager.h"
#include "Core/Debugger/PPCDebugInterface.h"
#include "Core/HW/DSP.h"
#include "Core/HW/Memmap.h"
#include "Core/PowerPC/PowerPC.h"
#include "DolphinWX/Debugger/MemoryView.h"
#include "DolphinWX/Debugger/MemoryWindow.h"
#include "DolphinWX/Globals.h"
#include "DolphinWX/WxUtils.h"

enum
{
  IDM_MEM_ADDRBOX,
  IDM_SYMBOLLIST,
  IDM_SETVALBUTTON,
  IDM_DUMP_MEMORY,
  IDM_DUMP_MEM2,
  IDM_DUMP_FAKEVMEM,
  IDM_VALBOX,
  IDM_DATA_TYPE_RBOX,
  IDM_SEARCH,
  IDM_ASCII,
  IDM_HEX
};

BEGIN_EVENT_TABLE(CMemoryWindow, wxPanel)
EVT_BUTTON(IDM_SETVALBUTTON, CMemoryWindow::SetMemoryValue)
EVT_BUTTON(IDM_DUMP_MEMORY, CMemoryWindow::OnDumpMemory)
EVT_BUTTON(IDM_DUMP_MEM2, CMemoryWindow::OnDumpMem2)
EVT_BUTTON(IDM_DUMP_FAKEVMEM, CMemoryWindow::OnDumpFakeVMEM)
EVT_RADIOBOX(IDM_DATA_TYPE_RBOX, CMemoryWindow::OnDataTypeChanged)
EVT_BUTTON(IDM_SEARCH, CMemoryWindow::OnSearch)
END_EVENT_TABLE()

CMemoryWindow::CMemoryWindow(wxWindow* parent, wxWindowID id, const wxPoint& pos,
                             const wxSize& size, long style, const wxString& name)
    : wxPanel(parent, id, pos, size, style, name)
{
  DebugInterface* di = &PowerPC::debug_interface;

  memview = new CMemoryView(di, this);

  addrbox = new wxSearchCtrl(this, IDM_MEM_ADDRBOX);
  addrbox->Bind(wxEVT_TEXT, &CMemoryWindow::OnAddrBoxChange, this);
  addrbox->SetDescriptiveText(_("Search Address"));

  valbox =
      new wxTextCtrl(this, IDM_VALBOX, "", wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
  valbox->Bind(wxEVT_TEXT_ENTER, &CMemoryWindow::SetMemoryValueFromValBox, this);
  valbox->Bind(wxEVT_TEXT, &CMemoryWindow::OnValueChanged, this);

  const int space3 = FromDIP(3);
  const int space5 = FromDIP(5);

  wxBoxSizer* const search_sizer = new wxBoxSizer(wxVERTICAL);
  search_sizer->Add(addrbox, 0, wxEXPAND);
  search_sizer->Add(valbox, 0, wxEXPAND);
  search_sizer->Add(new wxButton(this, IDM_SETVALBUTTON, _("Set Value")));

  wxBoxSizer* const dump_sizer = new wxBoxSizer(wxVERTICAL);
  dump_sizer->Add(new wxButton(this, IDM_DUMP_MEMORY, _("Dump MRAM")), 0, wxEXPAND);
  dump_sizer->Add(new wxButton(this, IDM_DUMP_MEM2, _("Dump EXRAM")), 0, wxEXPAND);
  if (!SConfig::GetInstance().bMMU)
    dump_sizer->Add(new wxButton(this, IDM_DUMP_FAKEVMEM, _("Dump FakeVMEM")), 0, wxEXPAND);

  wxStaticBoxSizer* const sizerSearchType = new wxStaticBoxSizer(wxVERTICAL, this, _("Search"));
  sizerSearchType->Add(btnSearch = new wxButton(this, IDM_SEARCH, _("Search")));
  sizerSearchType->Add(m_rb_ascii = new wxRadioButton(this, IDM_ASCII, "Ascii", wxDefaultPosition,
                                                      wxDefaultSize, wxRB_GROUP));
  sizerSearchType->Add(m_rb_hex = new wxRadioButton(this, IDM_HEX, _("Hex")));
  m_search_result_msg =
      new wxStaticText(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
                       wxST_NO_AUTORESIZE | wxALIGN_CENTER_HORIZONTAL);
  sizerSearchType->Add(m_search_result_msg, 0, wxEXPAND);

  wxArrayString data_type_options;
  data_type_options.Add(wxString::FromAscii("U8"));
  data_type_options.Add(wxString::FromAscii("U16"));
  data_type_options.Add(wxString::FromAscii("U32"));
  wxRadioBox* rbox_data_type =
      new wxRadioBox(this, IDM_DATA_TYPE_RBOX, _("Data Type"), wxDefaultPosition, wxDefaultSize,
                     data_type_options, 1);

  wxBoxSizer* const sizerRight = new wxBoxSizer(wxVERTICAL);
  sizerRight->Add(search_sizer);
  sizerRight->AddSpacer(space5);
  sizerRight->Add(dump_sizer, 0, wxEXPAND);
  sizerRight->Add(sizerSearchType, 0, wxEXPAND);
  sizerRight->Add(rbox_data_type, 0, wxEXPAND);

  wxBoxSizer* const sizerBig = new wxBoxSizer(wxHORIZONTAL);
  sizerBig->Add(memview, 20, wxEXPAND);
  sizerBig->AddSpacer(space3);
  sizerBig->Add(sizerRight, 0, wxEXPAND | wxTOP | wxBOTTOM, space3);
  sizerBig->AddSpacer(space3);

  SetSizer(sizerBig);
  m_rb_hex->SetValue(true);  // Set defaults
  rbox_data_type->SetSelection(0);

  sizerRight->Fit(this);
  sizerBig->Fit(this);
}

void CMemoryWindow::JumpToAddress(u32 _Address)
{
  memview->Center(_Address);
}

void CMemoryWindow::SetMemoryValueFromValBox(wxCommandEvent& event)
{
  SetMemoryValue(event);
  valbox->SetFocus();
}

void CMemoryWindow::SetMemoryValue(wxCommandEvent& event)
{
  if (!Memory::IsInitialized())
  {
    WxUtils::ShowErrorDialog(_("Cannot set uninitialized memory."));
    return;
  }

  std::string str_addr = WxStrToStr(addrbox->GetValue());
  std::string str_val = WxStrToStr(valbox->GetValue());
  u32 addr;
  u32 val;

  if (!TryParse(std::string("0x") + str_addr, &addr))
  {
    WxUtils::ShowErrorDialog(wxString::Format(_("Invalid address: %s"), str_addr.c_str()));
    return;
  }

  if (!TryParse(std::string("0x") + str_val, &val))
  {
    WxUtils::ShowErrorDialog(wxString::Format(_("Invalid value: %s"), str_val.c_str()));
    return;
  }

  PowerPC::HostWrite_U32(val, addr);
  memview->Refresh();
}

void CMemoryWindow::OnAddrBoxChange(wxCommandEvent& event)
{
  wxString txt = addrbox->GetValue();
  if (txt.size())
  {
    u32 addr;
    sscanf(WxStrToStr(txt).c_str(), "%08x", &addr);
    memview->Center(addr & ~3);
  }

  event.Skip();
}

void CMemoryWindow::Repopulate()
{
  memview->Center(PC);
}

void CMemoryWindow::OnValueChanged(wxCommandEvent&)
{
  m_continue_search = false;
}

static void DumpArray(const std::string& filename, const u8* data, size_t length)
{
  if (data)
  {
    File::IOFile f(filename, "wb");
    f.WriteBytes(data, length);
  }
}

// Write mram to file
void CMemoryWindow::OnDumpMemory(wxCommandEvent& event)
{
  DumpArray(File::GetUserPath(F_RAMDUMP_IDX), Memory::m_pRAM, Memory::REALRAM_SIZE);
}

// Write exram (aram or mem2) to file
void CMemoryWindow::OnDumpMem2(wxCommandEvent& event)
{
  if (SConfig::GetInstance().bWii)
  {
    DumpArray(File::GetUserPath(F_ARAMDUMP_IDX), Memory::m_pEXRAM, Memory::EXRAM_SIZE);
  }
  else
  {
    DumpArray(File::GetUserPath(F_ARAMDUMP_IDX), DSP::GetARAMPtr(), DSP::ARAM_SIZE);
  }
}

// Write fake vmem to file
void CMemoryWindow::OnDumpFakeVMEM(wxCommandEvent& event)
{
  DumpArray(File::GetUserPath(F_FAKEVMEMDUMP_IDX), Memory::m_pFakeVMEM, Memory::FAKEVMEM_SIZE);
}

void CMemoryWindow::OnDataTypeChanged(wxCommandEvent& ev)
{
  static constexpr std::array<MemoryDataType, 3> map{MemoryDataType::U8, MemoryDataType::U16,
                                                     MemoryDataType::U32};
  memview->SetDataType(map.at(ev.GetSelection()));
}

void CMemoryWindow::OnSearch(wxCommandEvent& event)
{
  wxBusyCursor hourglass_cursor;
  u8* ram_ptr = nullptr;
  u32 ram_size = 0;
  // NOTE: We're assuming the base address is zero.
  switch (memview->GetMemoryType())
  {
  case 0:
  default:
    if (Memory::m_pRAM)
    {
      ram_ptr = Memory::m_pRAM;
      ram_size = Memory::REALRAM_SIZE;
    }
    break;
  case 1:
  {
    u8* aram = DSP::GetARAMPtr();
    if (aram)
    {
      ram_ptr = aram;
      ram_size = DSP::ARAM_SIZE;
    }
  }
  break;
  }
  if (!ram_ptr)
  {
    m_search_result_msg->SetLabel(_("Memory Not Ready"));
    return;
  }

  std::vector<u8> search_bytes;
  wxString search_val = valbox->GetValue();

  if (m_rb_hex->GetValue())
  {
    search_val.Trim(true).Trim(false);
    // If there's a trailing nybble, stick a zero in front to make it a byte
    if (search_val.size() & 1)
      search_val.insert(search_val.size() - 1, 1, '0');
    search_bytes.reserve(search_val.size() / 2);

    wxString conversion_buffer(2, ' ');
    for (std::size_t i = 0; i < search_val.size(); i += 2)
    {
      unsigned long byte = 0;
      conversion_buffer[0] = search_val[i];
      conversion_buffer[1] = search_val[i + 1];
      if (!conversion_buffer.ToULong(&byte, 16))
      {
        m_search_result_msg->SetLabel(_("Not Valid Hex"));
        return;
      }
      search_bytes.push_back(static_cast<u8>(byte));
    }
  }
  else
  {
    const auto& bytes = search_val.ToUTF8();
    search_bytes.assign(bytes.data(), bytes.data() + bytes.length());
  }
  search_val.Clear();

  // For completeness
  if (search_bytes.size() > ram_size)
  {
    m_search_result_msg->SetLabel(_("Value Too Large"));
    return;
  }

  if (search_bytes.empty())
  {
    m_search_result_msg->SetLabel(_("No Value Given"));
    return;
  }

  // Search starting from specified address if there is one.
  u32 addr = 0;  // Base address
  {
    wxString addr_val = addrbox->GetValue();
    addr_val.Trim(true).Trim(false);
    if (addr_val.size())
    {
      unsigned long addr_ul = 0;
      if (addr_val.ToULong(&addr_ul, 16))
      {
        addr = static_cast<u32>(addr_ul);
        // Don't find the result we're already looking at
        if (m_continue_search && addr == m_last_search_address)
          addr += 1;
      }
    }
  }
  // If the current address doesn't leave enough bytes to search then we're done.
  if (addr >= ram_size - search_bytes.size())
  {
    m_search_result_msg->SetLabel(_("Address Out of Range"));
    return;
  }
  for (u32 i = addr; i < ram_size - search_bytes.size(); ++i)
  {
    int cmp = std::memcmp(search_bytes.data(), &ram_ptr[i], search_bytes.size());
    if (cmp == 0)
    {
      m_search_result_msg->SetLabel(_("Match Found"));
      // NOTE: SetValue() generates a synthetic wxEVT_TEXT
      addrbox->SetValue(wxString::Format("%08x", i));
      m_last_search_address = i;
      m_continue_search = true;
      return;
    }
  }
  m_search_result_msg->SetLabel(_("No Match"));
}
