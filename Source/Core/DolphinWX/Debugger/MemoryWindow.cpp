// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>
#include <wx/button.h>
#include <wx/checkbox.h>
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
#include "Common/File.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/StringUtil.h"
#include "Common/SymbolDB.h"
#include "Core/ConfigManager.h"
#include "Core/Debugger/PPCDebugInterface.h"
#include "Core/HW/DSP.h"
#include "Core/HW/Memmap.h"
#include "Core/PowerPC/PowerPC.h"
#include "DolphinWX/Debugger/BreakpointWindow.h"
#include "DolphinWX/Debugger/CodeWindow.h"
#include "DolphinWX/Debugger/MemoryView.h"
#include "DolphinWX/Debugger/MemoryWindow.h"
#include "DolphinWX/Globals.h"
#include "DolphinWX/WxUtils.h"

enum
{
  IDM_ADDRESS_SEARCH_CTRL,
  IDM_SYMBOL_LIST,
  IDM_SET_VALUE_BUTTON,
  IDM_DUMP_MEMORY,
  IDM_DUMP_MEM2,
  IDM_DUMP_FAKEVMEM,
  IDM_VALUE_TEXT_CTRL,
  IDM_DATA_TYPE_RBOX,
  IDM_FIND_NEXT,
  IDM_FIND_PREVIOUS,
  IDM_ASCII,
  IDM_HEX,
  IDM_MEMCHECK_OPTIONS_CHANGE
};

CMemoryWindow::CMemoryWindow(wxWindow* parent, wxWindowID id, const wxPoint& pos,
                             const wxSize& size, long style, const wxString& name)
    : wxPanel(parent, id, pos, size, style, name)
{
  CreateGUI();
}

void CMemoryWindow::CreateGUI()
{
  m_memory_view = new CMemoryView(&PowerPC::debug_interface, this);
  m_memory_view->Bind(DOLPHIN_EVT_MEMORY_VIEW_DATA_TYPE_CHANGED, &CMemoryWindow::OnDataTypeChanged,
                      this);

  const int space3 = FromDIP(3);

  wxBoxSizer* const main_sizer = new wxBoxSizer(wxHORIZONTAL);
  main_sizer->Add(m_memory_view, 20, wxEXPAND);
  main_sizer->AddSpacer(space3);
  main_sizer->Add(CreateRightHandSideSizer(), 0, wxEXPAND | wxTOP | wxBOTTOM, space3);
  main_sizer->AddSpacer(space3);

  SetSizerAndFit(main_sizer);
}

wxSizer* CMemoryWindow::CreateRightHandSideSizer()
{
  wxArrayString data_type_options;
  data_type_options.Add("U8");
  data_type_options.Add("U16");
  data_type_options.Add("U32");
  data_type_options.Add("ASCII");
  data_type_options.Add("Float32");
  m_rbox_data_type = new wxRadioBox(this, IDM_DATA_TYPE_RBOX, _("Data Type"), wxDefaultPosition,
                                    wxDefaultSize, data_type_options, 1);
  m_rbox_data_type->Bind(wxEVT_RADIOBOX, &CMemoryWindow::OnDataTypeChanged, this);
  m_rbox_data_type->SetSelection(static_cast<int>(m_memory_view->GetDataType()));

  const int space5 = FromDIP(5);

  auto* const right_sizer = new wxBoxSizer(wxVERTICAL);
  right_sizer->Add(CreateSearchSizer(), 0, wxEXPAND);
  right_sizer->AddSpacer(space5);
  right_sizer->Add(CreateDumpSizer(), 0, wxEXPAND);
  right_sizer->Add(CreateSearchTypeSizer(), 0, wxEXPAND);
  right_sizer->Add(m_rbox_data_type, 0, wxEXPAND);
  right_sizer->Add(CreateMemcheckOptionSizer(), 0, wxEXPAND);

  return right_sizer;
}

wxSizer* CMemoryWindow::CreateSearchSizer()
{
  m_address_search_ctrl = new wxSearchCtrl(this, IDM_ADDRESS_SEARCH_CTRL);
  m_address_search_ctrl->Bind(wxEVT_TEXT, &CMemoryWindow::OnSearchAddressChanged, this);
  m_address_search_ctrl->SetDescriptiveText(_("Search Address"));

  m_value_text_ctrl = new wxTextCtrl(this, IDM_VALUE_TEXT_CTRL, "", wxDefaultPosition,
                                     wxDefaultSize, wxTE_PROCESS_ENTER);
  m_value_text_ctrl->Bind(wxEVT_TEXT_ENTER, &CMemoryWindow::OnSetMemoryValueFromValBox, this);
  m_value_text_ctrl->Bind(wxEVT_TEXT, &CMemoryWindow::OnValueChanged, this);

  auto* const set_value_button = new wxButton(this, IDM_SET_VALUE_BUTTON, _("Set Value"));
  set_value_button->Bind(wxEVT_BUTTON, &CMemoryWindow::OnSetMemoryValue, this);

  auto* const search_sizer = new wxBoxSizer(wxVERTICAL);
  search_sizer->Add(m_address_search_ctrl, 0, wxEXPAND);
  search_sizer->Add(m_value_text_ctrl, 0, wxEXPAND);
  search_sizer->Add(set_value_button);

  return search_sizer;
}

wxSizer* CMemoryWindow::CreateDumpSizer()
{
  auto* const dump_mram_button = new wxButton(this, IDM_DUMP_MEMORY, _("Dump MRAM"));
  dump_mram_button->Bind(wxEVT_BUTTON, &CMemoryWindow::OnDumpMemory, this);

  auto* const dump_exram_button = new wxButton(this, IDM_DUMP_MEM2, _("Dump EXRAM"));
  dump_exram_button->Bind(wxEVT_BUTTON, &CMemoryWindow::OnDumpMem2, this);

  auto* const dump_sizer = new wxBoxSizer(wxVERTICAL);
  dump_sizer->Add(dump_mram_button, 0, wxEXPAND);
  dump_sizer->Add(dump_exram_button, 0, wxEXPAND);

  if (!SConfig::GetInstance().bMMU)
  {
    auto* const dump_fake_vmem_button = new wxButton(this, IDM_DUMP_FAKEVMEM, _("Dump FakeVMEM"));
    dump_fake_vmem_button->Bind(wxEVT_BUTTON, &CMemoryWindow::OnDumpFakeVMEM, this);

    dump_sizer->Add(dump_fake_vmem_button, 0, wxEXPAND);
  }

  return dump_sizer;
}

wxSizer* CMemoryWindow::CreateSearchTypeSizer()
{
  m_btn_find_next = new wxButton(this, IDM_FIND_NEXT, _("Find Next"));
  m_btn_find_next->Bind(wxEVT_BUTTON, &CMemoryWindow::OnFindNext, this);

  m_btn_find_previous = new wxButton(this, IDM_FIND_PREVIOUS, _("Find Previous"));
  m_btn_find_previous->Bind(wxEVT_BUTTON, &CMemoryWindow::OnFindPrevious, this);

  m_rb_ascii =
      new wxRadioButton(this, IDM_ASCII, "Ascii", wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
  m_rb_hex = new wxRadioButton(this, IDM_HEX, _("Hex"));
  m_rb_hex->SetValue(true);

  m_search_result_msg =
      new wxStaticText(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
                       wxST_NO_AUTORESIZE | wxALIGN_CENTER_HORIZONTAL);

  auto* const search_type_sizer = new wxStaticBoxSizer(wxVERTICAL, this, _("Search"));
  search_type_sizer->Add(m_btn_find_next);
  search_type_sizer->Add(m_btn_find_previous);
  search_type_sizer->Add(m_rb_ascii);
  search_type_sizer->Add(m_rb_hex);
  search_type_sizer->Add(m_search_result_msg, 0, wxEXPAND);

  return search_type_sizer;
}

wxSizer* CMemoryWindow::CreateMemcheckOptionSizer()
{
  // i18n: This string is used for a radio button that represents the type of
  // memory breakpoint that gets triggered when a read operation or write operation occurs.
  // The string is not a command to read and write something or to allow reading and writing.
  m_read_write_radio_btn = new wxRadioButton(this, IDM_MEMCHECK_OPTIONS_CHANGE, _("Read and write"),
                                             wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
  m_read_write_radio_btn->Bind(wxEVT_RADIOBUTTON, &CMemoryWindow::OnMemCheckOptionChange, this);

  // i18n: This string is used for a radio button that represents the type of
  // memory breakpoint that gets triggered when a read operation occurs.
  // The string does not mean "read-only" in the sense that something cannot be written to.
  m_read_radio_btn = new wxRadioButton(this, IDM_MEMCHECK_OPTIONS_CHANGE, _("Read only"));
  m_read_radio_btn->Bind(wxEVT_RADIOBUTTON, &CMemoryWindow::OnMemCheckOptionChange, this);

  // i18n: This string is used for a radio button that represents the type of
  // memory breakpoint that gets triggered when a write operation occurs.
  // The string does not mean "write-only" in the sense that something cannot be read from.
  m_write_radio_btn = new wxRadioButton(this, IDM_MEMCHECK_OPTIONS_CHANGE, _("Write only"));
  m_write_radio_btn->Bind(wxEVT_RADIOBUTTON, &CMemoryWindow::OnMemCheckOptionChange, this);

  m_log_checkbox = new wxCheckBox(this, IDM_MEMCHECK_OPTIONS_CHANGE, _("Log"));
  m_log_checkbox->Bind(wxEVT_CHECKBOX, &CMemoryWindow::OnMemCheckOptionChange, this);
  m_log_checkbox->SetValue(true);

  auto* const memcheck_options_sizer =
      new wxStaticBoxSizer(wxVERTICAL, this, _("Memory breakpoint options"));
  memcheck_options_sizer->Add(m_read_write_radio_btn);
  memcheck_options_sizer->Add(m_read_radio_btn);
  memcheck_options_sizer->Add(m_write_radio_btn);
  memcheck_options_sizer->Add(m_log_checkbox);

  return memcheck_options_sizer;
}

void CMemoryWindow::JumpToAddress(u32 address)
{
  m_memory_view->Center(address);
}

void CMemoryWindow::OnSetMemoryValueFromValBox(wxCommandEvent& event)
{
  OnSetMemoryValue(event);
  m_value_text_ctrl->SetFocus();
}

void CMemoryWindow::OnSetMemoryValue(wxCommandEvent& event)
{
  if (!Memory::IsInitialized())
  {
    WxUtils::ShowErrorDialog(_("Cannot set uninitialized memory."));
    return;
  }

  std::string str_addr = WxStrToStr(m_address_search_ctrl->GetValue());
  u32 addr;
  if (!TryParse("0x" + str_addr, &addr))
  {
    WxUtils::ShowErrorDialog(wxString::Format(_("Invalid address: %s"), str_addr.c_str()));
    return;
  }

  std::string str_val = WxStrToStr(m_value_text_ctrl->GetValue());
  u32 val;
  if (!TryParse("0x" + str_val, &val))
  {
    WxUtils::ShowErrorDialog(wxString::Format(_("Invalid value: %s"), str_val.c_str()));
    return;
  }

  PowerPC::HostWrite_U32(val, addr);
  m_memory_view->Refresh();
}

void CMemoryWindow::OnSearchAddressChanged(wxCommandEvent& event)
{
  wxString txt = m_address_search_ctrl->GetValue();
  if (txt.size())
  {
    u32 addr;
    sscanf(WxStrToStr(txt).c_str(), "%08x", &addr);
    m_memory_view->Center(addr & ~3);
  }

  event.Skip();
}

void CMemoryWindow::Repopulate()
{
  m_memory_view->Center(PC);
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
  static constexpr std::array<MemoryDataType, 5> map{{MemoryDataType::U8, MemoryDataType::U16,
                                                      MemoryDataType::U32, MemoryDataType::ASCII,
                                                      MemoryDataType::FloatingPoint}};
  if (ev.GetId() == IDM_DATA_TYPE_RBOX)
  {
    m_memory_view->SetDataType(map.at(ev.GetSelection()));
  }
  else
  {
    // Event from the CMemoryView indicating type was changed.
    auto itr = std::find(map.begin(), map.end(), static_cast<MemoryDataType>(ev.GetInt()));
    int idx = -1;
    if (itr != map.end())
      idx = static_cast<int>(itr - map.begin());
    m_rbox_data_type->SetSelection(idx);
  }
}

void CMemoryWindow::OnFindNext(wxCommandEvent& event)
{
  wxBusyCursor hourglass_cursor;
  Search(SearchType::FindNext);
}

void CMemoryWindow::OnFindPrevious(wxCommandEvent& event)
{
  wxBusyCursor hourglass_cursor;
  Search(SearchType::FindPrevious);
}

void CMemoryWindow::Search(SearchType search_type)
{
  u8* ram_ptr = nullptr;
  std::size_t ram_size = 0;
  // NOTE: We're assuming the base address is zero.
  switch (m_memory_view->GetMemoryType())
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
  wxString search_val = m_value_text_ctrl->GetValue();

  if (m_rb_hex->GetValue())
  {
    search_val.Trim(true).Trim(false);
    // If there's a trailing nybble, stick a zero in front to make it a byte
    if (search_val.size() & 1)
      search_val.insert(0, 1, '0');
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
    wxString addr_val = m_address_search_ctrl->GetValue();
    addr_val.Trim(true).Trim(false);
    if (!addr_val.empty())
    {
      unsigned long addr_ul = 0;
      if (addr_val.ToULong(&addr_ul, 16))
      {
        addr = static_cast<u32>(addr_ul);
        // Don't find the result we're already looking at
        if (m_continue_search && addr == m_last_search_address &&
            search_type == SearchType::FindNext)
        {
          addr += 1;
        }
      }
    }
  }

  // If the current address doesn't leave enough bytes to search then we're done.
  if (addr >= ram_size - search_bytes.size())
  {
    m_search_result_msg->SetLabel(_("Address Out of Range"));
    return;
  }

  const u8* ptr;
  const u8* end;
  if (search_type == SearchType::FindNext)
  {
    const u8* begin = &ram_ptr[addr];
    end = &ram_ptr[ram_size - search_bytes.size() + 1];
    ptr = std::search(begin, end, search_bytes.begin(), search_bytes.end());
  }
  else
  {
    const u8* begin = ram_ptr;
    end = &ram_ptr[addr + search_bytes.size() - 1];
    ptr = std::find_end(begin, end, search_bytes.begin(), search_bytes.end());
  }

  if (ptr != end)
  {
    m_search_result_msg->SetLabel(_("Match Found"));
    u32 offset = static_cast<u32>(ptr - ram_ptr);
    // NOTE: SetValue() generates a synthetic wxEVT_TEXT
    m_address_search_ctrl->SetValue(wxString::Format("%08x", offset));
    m_last_search_address = offset;
    m_continue_search = true;
    return;
  }

  m_search_result_msg->SetLabel(_("No Match"));
}

void CMemoryWindow::OnMemCheckOptionChange(wxCommandEvent& event)
{
  if (m_read_write_radio_btn->GetValue())
  {
    m_memory_view->SetMemCheckOptions(true, true, m_log_checkbox->GetValue());
  }
  else
  {
    m_memory_view->SetMemCheckOptions(m_read_radio_btn->GetValue(), m_write_radio_btn->GetValue(),
                                      m_log_checkbox->GetValue());
  }
}
