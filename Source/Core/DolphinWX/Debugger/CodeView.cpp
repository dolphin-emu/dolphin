// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include <wx/brush.h>
#include <wx/chartype.h>
#include <wx/clipbrd.h>
#include <wx/colour.h>
#include <wx/control.h>
#include <wx/dataobj.h>
#include <wx/dcclient.h>
#include <wx/defs.h>
#include <wx/event.h>
#include <wx/gdicmn.h>
#include <wx/graphics.h>
#include <wx/menu.h>
#include <wx/menuitem.h>
#include <wx/pen.h>
#include <wx/setup.h>
#include <wx/string.h>
#include <wx/textdlg.h>
#include <wx/translation.h>
#include <wx/windowid.h>

#include "Common/CommonTypes.h"
#include "Common/DebugInterface.h"
#include "Common/StringUtil.h"
#include "Common/SymbolDB.h"
#include "Core/Core.h"
#include "Core/Host.h"
#include "DolphinWX/Globals.h"
#include "DolphinWX/WxUtils.h"
#include "DolphinWX/Debugger/CodeView.h"
#include "DolphinWX/Debugger/DebuggerUIUtil.h"

wxDEFINE_EVENT(wxEVT_CODEVIEW_CHANGE, wxCommandEvent);

enum
{
	IDM_GOTOINMEMVIEW = 12000,
	IDM_COPYADDRESS,
	IDM_COPYHEX,
	IDM_COPYCODE,
	IDM_INSERTBLR, IDM_INSERTNOP,
	IDM_RUNTOHERE,
	IDM_JITRESULTS,
	IDM_FOLLOWBRANCH,
	IDM_RENAMESYMBOL,
	IDM_PATCHALERT,
	IDM_COPYFUNCTION,
	IDM_ADDFUNCTION,
};

CCodeView::CCodeView(DebugInterface* debuginterface, SymbolDB *symboldb,
		wxWindow* parent, wxWindowID Id)
	: wxControl(parent, Id),
	  m_debugger(debuginterface),
	  m_symbol_db(symboldb),
	  m_plain(false),
	  m_curAddress(debuginterface->GetPC()),
	  m_align(debuginterface->GetInstructionSize(0)),
	  m_rowHeight(13),
	  m_selection(0),
	  m_oldSelection(0),
	  m_selecting(false),
	  m_lx(-1),
	  m_ly(-1)
{
	Bind(wxEVT_ERASE_BACKGROUND, &CCodeView::OnErase, this);
	Bind(wxEVT_PAINT, &CCodeView::OnPaint, this);
	Bind(wxEVT_MOUSEWHEEL, &CCodeView::OnScrollWheel, this);
	Bind(wxEVT_LEFT_DOWN, &CCodeView::OnMouseDown, this);
	Bind(wxEVT_LEFT_UP, &CCodeView::OnMouseUpL, this);
	Bind(wxEVT_MOTION, &CCodeView::OnMouseMove, this);
	Bind(wxEVT_RIGHT_DOWN, &CCodeView::OnMouseDown, this);
	Bind(wxEVT_RIGHT_UP, &CCodeView::OnMouseUpR, this);
	Bind(wxEVT_MENU, &CCodeView::OnPopupMenu, this);
	Bind(wxEVT_SIZE, &CCodeView::OnResize, this);
}

int CCodeView::YToAddress(int y)
{
	wxRect rc = GetClientRect();
	int ydiff = y - rc.height / 2 - m_rowHeight / 2;
	ydiff = (int)(floorf((float)ydiff / (float)m_rowHeight)) + 1;
	return m_curAddress + ydiff * m_align;
}

void CCodeView::OnMouseDown(wxMouseEvent& event)
{
	int x = event.m_x;
	int y = event.m_y;

	if (x > 16)
	{
		m_oldSelection = m_selection;
		m_selection = YToAddress(y);
		// SetCapture(wnd);
		bool oldselecting = m_selecting;
		m_selecting = true;

		if (!oldselecting || (m_selection != m_oldSelection))
			Refresh();
	}
	else
	{
		ToggleBreakpoint(YToAddress(y));
	}

	event.Skip();
}

void CCodeView::OnScrollWheel(wxMouseEvent& event)
{
	const bool scroll_down = (event.GetWheelRotation() < 0);
	const int num_lines = event.GetLinesPerAction();

	if (scroll_down)
	{
		m_curAddress += num_lines * 4;
	}
	else
	{
		m_curAddress -= num_lines * 4;
	}

	Refresh();
	event.Skip();
}

void CCodeView::ToggleBreakpoint(u32 address)
{
	m_debugger->ToggleBreakpoint(address);
	Refresh();

	// Propagate back to the parent window to update the breakpoint list.
	wxCommandEvent evt(wxEVT_HOST_COMMAND, IDM_UPDATEBREAKPOINTS);
	GetEventHandler()->AddPendingEvent(evt);
}

void CCodeView::OnMouseMove(wxMouseEvent& event)
{
	wxRect rc = GetClientRect();

	if (event.m_leftDown && event.m_x > 16)
	{
		if (event.m_y < 0)
		{
			m_curAddress -= m_align;
			Refresh();
		}
		else if (event.m_y > rc.height)
		{
			m_curAddress += m_align;
			Refresh();
		}
		else
		{
			OnMouseDown(event);
		}
	}

	event.Skip();
}

void CCodeView::RaiseEvent()
{
	wxCommandEvent ev(wxEVT_CODEVIEW_CHANGE, GetId());
	ev.SetEventObject(this);
	ev.SetInt(m_selection);
	GetEventHandler()->ProcessEvent(ev);
}

void CCodeView::OnMouseUpL(wxMouseEvent& event)
{
	if (event.m_x > 16)
	{
		m_curAddress = YToAddress(event.m_y);
		m_selecting = false;
		Refresh();
	}

	RaiseEvent();
	event.Skip();
}

u32 CCodeView::AddrToBranch(u32 addr)
{
	std::string disasm = m_debugger->Disassemble(addr);
	size_t pos = disasm.find("->0x");

	if (pos != std::string::npos)
	{
		std::string hex = disasm.substr(pos + 2);
		return std::stoul(hex, nullptr, 16);
	}

	return 0;
}

void CCodeView::InsertBlrNop(int Blr)
{
	// Check if this address has been modified
	int find = -1;
	for (u32 i = 0; i < m_blrList.size(); i++)
	{
		if (m_blrList.at(i).address == m_selection)
		{
			find = i;
			break;
		}
	}

	// Save the old value
	if (find >= 0)
	{
		m_debugger->WriteExtraMemory(0, m_blrList.at(find).oldValue, m_selection);
		m_blrList.erase(m_blrList.begin() + find);
	}
	else
	{
		BlrStruct temp;
		temp.address = m_selection;
		temp.oldValue = m_debugger->ReadMemory(m_selection);
		m_blrList.push_back(temp);
		if (Blr == 0)
			m_debugger->InsertBLR(m_selection, 0x4e800020);
		else
			m_debugger->InsertBLR(m_selection, 0x60000000);
	}
	Refresh();
}

void CCodeView::OnPopupMenu(wxCommandEvent& event)
{
#if wxUSE_CLIPBOARD
	wxTheClipboard->Open();
#endif

	switch (event.GetId())
	{
		case IDM_GOTOINMEMVIEW:
			// CMemoryDlg::Goto(selection);
			break;

#if wxUSE_CLIPBOARD
		case IDM_COPYADDRESS:
			wxTheClipboard->SetData(new wxTextDataObject(wxString::Format("%08x", m_selection)));
			break;

		case IDM_COPYCODE:
			{
				std::string disasm = m_debugger->Disassemble(m_selection);
				wxTheClipboard->SetData(new wxTextDataObject(StrToWxStr(disasm)));
			}
			break;

		case IDM_COPYHEX:
			{
				std::string temp = StringFromFormat("%08x", m_debugger->ReadInstruction(m_selection));
				wxTheClipboard->SetData(new wxTextDataObject(StrToWxStr(temp)));
			}
			break;


		case IDM_COPYFUNCTION:
			{
				Symbol *symbol = m_symbol_db->GetSymbolFromAddr(m_selection);
				if (symbol)
				{
					std::string text;
					text = text + symbol->name + "\r\n";
					// we got a function
					u32 start = symbol->address;
					u32 end = start + symbol->size;
					for (u32 addr = start; addr != end; addr += 4)
					{
						std::string disasm = m_debugger->Disassemble(addr);
						text += StringFromFormat("%08x: ", addr) + disasm + "\r\n";
					}
					wxTheClipboard->SetData(new wxTextDataObject(StrToWxStr(text)));
				}
			}
			break;
#endif

		case IDM_RUNTOHERE:
			m_debugger->SetBreakpoint(m_selection);
			m_debugger->RunToBreakpoint();
			Refresh();
			break;

		// Insert blr or restore old value
		case IDM_INSERTBLR:
			InsertBlrNop(0);
			Refresh();
			break;
		case IDM_INSERTNOP:
			InsertBlrNop(1);
			Refresh();
			break;

		case IDM_JITRESULTS:
			{
				// Propagate back to the parent window and tell it
				// to flip to the JIT tab for the current address.
				wxCommandEvent jit_event(wxEVT_HOST_COMMAND, IDM_UPDATEJITPANE);
				GetEventHandler()->AddPendingEvent(jit_event);
			}
			break;

		case IDM_FOLLOWBRANCH:
			{
				u32 dest = AddrToBranch(m_selection);
				if (dest)
				{
					Center(dest);
					RaiseEvent();
				}
			}
			break;

		case IDM_ADDFUNCTION:
			m_symbol_db->AddFunction(m_selection);
			Host_NotifyMapLoaded();
			break;

		case IDM_RENAMESYMBOL:
			{
				Symbol *symbol = m_symbol_db->GetSymbolFromAddr(m_selection);
				if (symbol)
				{
					wxTextEntryDialog input_symbol(this, _("Rename symbol:"),
							wxGetTextFromUserPromptStr,
							StrToWxStr(symbol->name));
					if (input_symbol.ShowModal() == wxID_OK)
					{
						symbol->name = WxStrToStr(input_symbol.GetValue());
						Refresh(); // Redraw to show the renamed symbol
					}
					Host_NotifyMapLoaded();
				}
			}
			break;

		case IDM_PATCHALERT:
			break;
	}

#if wxUSE_CLIPBOARD
	wxTheClipboard->Close();
#endif
	event.Skip();
}

void CCodeView::OnMouseUpR(wxMouseEvent& event)
{
	bool isSymbol = m_symbol_db->GetSymbolFromAddr(m_selection) != nullptr;
	// popup menu
	wxMenu menu;
	//menu->Append(IDM_GOTOINMEMVIEW, "&Goto in mem view");
	menu.Append(IDM_FOLLOWBRANCH, _("&Follow branch"))->Enable(AddrToBranch(m_selection) ? true : false);
	menu.AppendSeparator();
#if wxUSE_CLIPBOARD
	menu.Append(IDM_COPYADDRESS, _("Copy &address"));
	menu.Append(IDM_COPYFUNCTION, _("Copy &function"))->Enable(isSymbol);
	menu.Append(IDM_COPYCODE, _("Copy &code line"));
	menu.Append(IDM_COPYHEX, _("Copy &hex"));
	menu.AppendSeparator();
#endif
	menu.Append(IDM_RENAMESYMBOL, _("Rename &symbol"))->Enable(isSymbol);
	menu.AppendSeparator();
	menu.Append(IDM_RUNTOHERE, _("&Run To Here"))->Enable(Core::IsRunning());
	menu.Append(IDM_ADDFUNCTION, _("&Add function"))->Enable(Core::IsRunning());
	menu.Append(IDM_JITRESULTS, _("PPC vs X86"))->Enable(Core::IsRunning());
	menu.Append(IDM_INSERTBLR, _("Insert &blr"))->Enable(Core::IsRunning());
	menu.Append(IDM_INSERTNOP, _("Insert &nop"))->Enable(Core::IsRunning());
	menu.Append(IDM_PATCHALERT, _("Patch alert"))->Enable(Core::IsRunning());
	PopupMenu(&menu);
	event.Skip();
}

void CCodeView::OnErase(wxEraseEvent& event)
{}

void CCodeView::OnPaint(wxPaintEvent& event)
{
	// -------------------------
	// General settings
	// -------------------------
	std::unique_ptr<wxGraphicsContext> ctx(wxGraphicsContext::Create(wxPaintDC(this)));
	wxRect rc = GetClientRect();

	ctx->SetFont(DebuggerFont, *wxBLACK);

	wxDouble w,h;
	ctx->GetTextExtent("0WJyq", &w, &h);

	if (h > m_rowHeight)
		m_rowHeight = h;

	ctx->GetTextExtent("W", &w, &h);
	int charWidth = w;

	struct branch
	{
		int src, dst, srcAddr;
	};

	branch branches[256];
	int numBranches = 0;
	// TODO: Add any drawing code here...
	int width   = rc.width;
	int numRows = ((rc.height / m_rowHeight) / 2) + 2;
	// ------------

	// -------------------------
	// Colors and brushes
	// -------------------------

	const wxColour bgColor = *wxWHITE;
	wxPen nullPen(bgColor);
	wxPen currentPen(*wxBLACK_PEN);
	wxPen selPen(*wxGREY_PEN);
	nullPen.SetStyle(wxTRANSPARENT);
	currentPen.SetStyle(wxSOLID);
	wxBrush currentBrush(*wxLIGHT_GREY_BRUSH);
	wxBrush pcBrush(*wxGREEN_BRUSH);
	wxBrush bpBrush(*wxRED_BRUSH);

	wxBrush bgBrush(bgColor);
	wxBrush nullBrush(bgColor);
	nullBrush.SetStyle(wxTRANSPARENT);

	ctx->SetPen(nullPen);
	ctx->SetBrush(bgBrush);
	ctx->DrawRectangle(0, 0, 16, rc.height);
	ctx->DrawRectangle(0, 0, rc.width, 5);
	// ------------

	// -----------------------------
	// Walk through all visible rows
	// -----------------------------
	for (int i = -numRows; i <= numRows; i++)
	{
		unsigned int address = m_curAddress + (i * m_align);

		int rowY1 = (rc.height / 2) + (m_rowHeight * i) - (m_rowHeight / 2);
		int rowY2 = (rc.height / 2) + (m_rowHeight * i) + (m_rowHeight / 2);

		wxString temp = wxString::Format("%08x", address);
		u32 color = m_debugger->GetColor(address);
		wxBrush rowBrush(wxColour(color >> 16, color >> 8, color));
		ctx->SetBrush(nullBrush);
		ctx->SetPen(nullPen);
		ctx->DrawRectangle(0, rowY1, 16, rowY2 - rowY1 + 2);

		if (m_selecting && (address == m_selection))
			ctx->SetPen(selPen);
		else
			ctx->SetPen(i == 0 ? currentPen : nullPen);

		if (address == m_debugger->GetPC())
			ctx->SetBrush(pcBrush);
		else
			ctx->SetBrush(rowBrush);

		ctx->DrawRectangle(16, rowY1, width, rowY2 - rowY1 + 1);
		ctx->SetBrush(currentBrush);
		if (!m_plain)
		{
			// the address text is dark red
			ctx->SetFont(DebuggerFont, wxColour("#600000"));
			ctx->DrawText(temp, 17, rowY1);
			ctx->SetFont(DebuggerFont, *wxBLACK);
		}

		// If running
		if (m_debugger->IsAlive())
		{
			std::vector<std::string> dis;
			SplitString(m_debugger->Disassemble(address), '\t', dis);
			dis.resize(2);

			static const size_t VALID_BRANCH_LENGTH = 10;
			const std::string& opcode   = dis[0];
			const std::string& operands = dis[1];
			std::string desc;

			// look for hex strings to decode branches
			std::string hex_str;
			size_t pos = operands.find("0x8");
			if (pos != std::string::npos)
			{
				hex_str = operands.substr(pos);
			}

			if (hex_str.length() == VALID_BRANCH_LENGTH)
			{
				u32 offs = std::stoul(hex_str, nullptr, 16);

				branches[numBranches].src = rowY1 + (m_rowHeight / 2);
				branches[numBranches].srcAddr = (address / m_align);
				branches[numBranches++].dst = (int)(rowY1 + ((s64)(u32)offs - (s64)(u32)address) * m_rowHeight / m_align + m_rowHeight / 2);
				desc = StringFromFormat("-->%s", m_debugger->GetDescription(offs).c_str());

				// the -> arrow illustrations are purple
				ctx->SetFont(DebuggerFont, wxTheColourDatabase->Find("PURPLE"));
			}
			else
			{
				ctx->SetFont(DebuggerFont, *wxBLACK);
			}

			ctx->DrawText(StrToWxStr(operands), 17 + 17*charWidth, rowY1);
			// ------------

			// Show blr as its' own color
			if (opcode == "blr")
				ctx->SetFont(DebuggerFont, wxTheColourDatabase->Find("DARK GREEN"));
			else
				ctx->SetFont(DebuggerFont, wxTheColourDatabase->Find("VIOLET"));

			ctx->DrawText(StrToWxStr(opcode), 17 + (m_plain ? 1*charWidth : 9*charWidth), rowY1);

			if (desc.empty())
			{
				desc = m_debugger->GetDescription(address);
			}

			if (!m_plain)
			{
				ctx->SetFont(DebuggerFont, *wxBLUE);

				//char temp[256];
				//UnDecorateSymbolName(desc,temp,255,UNDNAME_COMPLETE);
				if (!desc.empty())
				{
					ctx->DrawText(StrToWxStr(desc), 17 + 35 * charWidth, rowY1);
				}
			}

			// Show red breakpoint dot
			if (m_debugger->IsBreakpoint(address))
			{
				ctx->SetBrush(bpBrush);
				ctx->DrawRectangle(2, rowY1 + 1, 11, 11);
			}
		}
	} // end of for
	// ------------

	// -------------------------
	// Colors and brushes
	// -------------------------
	ctx->SetPen(currentPen);

	for (int i = 0; i < numBranches; i++)
	{
		int x = 17 + 49 * charWidth + (branches[i].srcAddr % 9) * 8;
		MoveTo(x-2, branches[i].src);

		if (branches[i].dst < rc.height + 400 && branches[i].dst > -400)
		{
			LineTo(ctx, x+2, branches[i].src);
			LineTo(ctx, x+2, branches[i].dst);
			LineTo(ctx, x-4, branches[i].dst);

			MoveTo(x, branches[i].dst - 4);
			LineTo(ctx, x-4, branches[i].dst);
			LineTo(ctx, x+1, branches[i].dst+5);
		}
		//else
		//{
			// This can be re-enabled when there is a scrollbar or
			// something on the codeview (the lines are too long)

			//LineTo(ctx, x+4, branches[i].src);
			//MoveTo(x+2, branches[i].dst-4);
			//LineTo(ctx, x+6, branches[i].dst);
			//LineTo(ctx, x+1, branches[i].dst+5);
		//}

		//LineTo(ctx, x, branches[i].dst+4);
		//LineTo(ctx, x-2, branches[i].dst);
	}
	// ------------
}

void CCodeView::LineTo(std::unique_ptr<wxGraphicsContext>& ctx, int x, int y)
{
	std::vector<wxPoint2DDouble> points { wxPoint2DDouble(m_lx, m_ly), wxPoint2DDouble(x, y) };

	ctx->DrawLines(points.size(), points.data());
	m_lx = x;
	m_ly = y;
}

void CCodeView::OnResize(wxSizeEvent& event)
{
	Refresh();
	event.Skip();
}
