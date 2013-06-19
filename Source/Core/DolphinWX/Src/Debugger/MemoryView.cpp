// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "DebuggerUIUtil.h"
#include "Common.h"
#include "Host.h"
#include "PowerPC/PowerPC.h"
#include "HW/Memmap.h"

#include "MemoryView.h"
#include "../WxUtils.h"

#include <wx/event.h>
#include <wx/clipbrd.h>

enum
{
	IDM_GOTOINMEMVIEW = 12000,
	IDM_COPYADDRESS,
	IDM_COPYHEX,
	IDM_COPYCODE,
	IDM_RUNTOHERE,
	IDM_DYNARECRESULTS,
	IDM_TOGGLEMEMORY,
	IDM_VIEWASFP,
	IDM_VIEWASASCII,
	IDM_VIEWASHEX,
};

BEGIN_EVENT_TABLE(CMemoryView, wxControl)
	EVT_PAINT(CMemoryView::OnPaint)
	EVT_LEFT_DOWN(CMemoryView::OnMouseDownL)
	EVT_LEFT_UP(CMemoryView::OnMouseUpL)
	EVT_MOTION(CMemoryView::OnMouseMove)
	EVT_RIGHT_DOWN(CMemoryView::OnMouseDownR)
	EVT_MENU(-1, CMemoryView::OnPopupMenu)
	EVT_SIZE(CMemoryView::OnResize)
END_EVENT_TABLE()

CMemoryView::CMemoryView(DebugInterface* debuginterface, wxWindow* parent)
	: wxControl(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize)
	, curAddress(debuginterface->getPC())
	, debugger(debuginterface)
	, align(debuginterface->getInstructionSize(0))
	, rowHeight(13)
	, selection(0)
	, oldSelection(0)
	, selecting(false)
	, memory(0)
	, viewAsType(VIEWAS_FP)
{
}

int CMemoryView::YToAddress(int y)
{
	wxRect rc = GetClientRect();
	int ydiff = y - rc.height / 2 - rowHeight / 2;
	ydiff = (int)(floorf((float)ydiff / (float)rowHeight)) + 1;
	return curAddress + ydiff * align;
}

void CMemoryView::OnMouseDownL(wxMouseEvent& event)
{
	int x = event.m_x;
	int y = event.m_y;

	if (x > 16)
	{
		oldSelection = selection;
		selection = YToAddress(y);
		bool oldselecting = selecting;
		selecting = true;

		if (!oldselecting || (selection != oldSelection))
			Refresh();
	}
	else
	{
		debugger->toggleMemCheck(YToAddress(y));

		Refresh();
		Host_UpdateBreakPointView();
	}

	event.Skip(true);
}

void CMemoryView::OnMouseMove(wxMouseEvent& event)
{
	wxRect rc = GetClientRect();

	if (event.m_leftDown && event.m_x > 16)
	{
		if (event.m_y < 0)
		{
			curAddress -= align;
			Refresh();
		}
		else if (event.m_y > rc.height)
		{
			curAddress += align;
			Refresh();
		}
		else
			OnMouseDownL(event);
	}

	event.Skip(true);
}

void CMemoryView::OnMouseUpL(wxMouseEvent& event)
{
	if (event.m_x > 16)
	{
		curAddress = YToAddress(event.m_y);
		selecting = false;
		Refresh();
	}

	event.Skip(true);
}

void CMemoryView::OnPopupMenu(wxCommandEvent& event)
{
#if wxUSE_CLIPBOARD
	wxTheClipboard->Open();
#endif

	switch (event.GetId())
	{
#if wxUSE_CLIPBOARD
		case IDM_COPYADDRESS:
			wxTheClipboard->SetData(new wxTextDataObject(wxString::Format(_T("%08x"), selection)));
			break;

		case IDM_COPYHEX:
			{
			char temp[24];
			sprintf(temp, "%08x", debugger->readExtraMemory(memory, selection));
			wxTheClipboard->SetData(new wxTextDataObject(StrToWxStr(temp)));
			}
			break;
#endif

		case IDM_TOGGLEMEMORY:
			memory ^= 1;
			Refresh();
			break;

		case IDM_VIEWASFP:
			viewAsType = VIEWAS_FP;
			Refresh();
			break;

		case IDM_VIEWASASCII:
			viewAsType = VIEWAS_ASCII;
			Refresh();
			break;
		case IDM_VIEWASHEX:
			viewAsType = VIEWAS_HEX;
			Refresh();
			break;
	}

#if wxUSE_CLIPBOARD
	wxTheClipboard->Close();
#endif
	event.Skip(true);
}

void CMemoryView::OnMouseDownR(wxMouseEvent& event)
{
	// popup menu
	wxMenu* menu = new wxMenu;
	//menu.Append(IDM_GOTOINMEMVIEW, "&Goto in mem view");
#if wxUSE_CLIPBOARD
	menu->Append(IDM_COPYADDRESS, StrToWxStr("Copy &address"));
	menu->Append(IDM_COPYHEX, StrToWxStr("Copy &hex"));
#endif
	menu->Append(IDM_TOGGLEMEMORY, StrToWxStr("Toggle &memory"));

	wxMenu* viewAsSubMenu = new wxMenu;
	viewAsSubMenu->Append(IDM_VIEWASFP, StrToWxStr("FP value"));
	viewAsSubMenu->Append(IDM_VIEWASASCII, StrToWxStr("ASCII"));
	viewAsSubMenu->Append(IDM_VIEWASHEX, StrToWxStr("Hex"));
	menu->AppendSubMenu(viewAsSubMenu, StrToWxStr("View As:"));

	PopupMenu(menu);
}

void CMemoryView::OnPaint(wxPaintEvent& event)
{
	wxPaintDC dc(this);
	wxRect rc = GetClientRect();
	wxFont hFont(_T("Courier"));
	hFont.SetFamily(wxFONTFAMILY_TELETYPE);

	wxCoord w,h;
	dc.GetTextExtent(_T("0WJyq"),&w,&h,NULL,NULL,&hFont);
	if (h > rowHeight)
		rowHeight = h;
	dc.GetTextExtent(_T("0WJyq"),&w,&h,NULL,NULL,&DebuggerFont);
	if (h > rowHeight)
		rowHeight = h;

	if (viewAsType==VIEWAS_HEX)
		dc.SetFont(hFont);
	else
		dc.SetFont(DebuggerFont);

	dc.GetTextExtent(_T("W"),&w,&h);
	int fontSize = w;
	int textPlacement = 17 + 9 * fontSize;
	struct branch
	{
		int src, dst, srcAddr;
	};

	// TODO: Add any drawing code here...
	int width   = rc.width;
	int numRows = (rc.height / rowHeight) / 2 + 2;
	dc.SetBackgroundMode(wxTRANSPARENT);
	const wxChar* bgColor = _T("#ffffff");
	wxPen nullPen(bgColor);
	wxPen currentPen(_T("#000000"));
	wxPen selPen(_T("#808080")); // gray
	nullPen.SetStyle(wxTRANSPARENT);

	wxBrush currentBrush(_T("#FFEfE8")); // light gray
	wxBrush pcBrush(_T("#70FF70")); // green
	wxBrush mcBrush(_T("#1133FF")); // blue
	wxBrush bgBrush(bgColor);
	wxBrush nullBrush(bgColor);
	nullBrush.SetStyle(wxTRANSPARENT);

	dc.SetPen(nullPen);
	dc.SetBrush(bgBrush);
	dc.DrawRectangle(0, 0, 16, rc.height);
	dc.DrawRectangle(0, 0, rc.width, 5+8);

	// TODO - clean up this freaking mess!!!!!
	for (int row = -numRows; row <= numRows; row++)
	{
		unsigned int address = curAddress + row * align;

		int rowY1 = rc.height / 2 + rowHeight * row - rowHeight / 2;
		int rowY2 = rc.height / 2 + rowHeight * row + rowHeight / 2;

		wxString temp = wxString::Format(_T("%08x"), address);
		u32 col = debugger->getColor(address);
		wxBrush rowBrush(wxColor(col >> 16, col >> 8, col));
		dc.SetBrush(nullBrush);
		dc.SetPen(nullPen);
		dc.DrawRectangle(0, rowY1, 16, rowY2);

		if (selecting && (address == selection))
			dc.SetPen(selPen);
		else
			dc.SetPen(row == 0 ? currentPen : nullPen);

		if (address == debugger->getPC())
			dc.SetBrush(pcBrush);
		else
			dc.SetBrush(rowBrush);

		dc.DrawRectangle(16, rowY1, width, rowY2 - 1);
		dc.SetBrush(currentBrush);
		dc.SetTextForeground(_T("#600000"));
		dc.DrawText(temp, 17, rowY1);

		if (viewAsType != VIEWAS_HEX)
		{
			char mem[256];
			debugger->getRawMemoryString(memory, address, mem, 256);
			dc.SetTextForeground(_T("#000080"));
			dc.DrawText(StrToWxStr(mem), 17+fontSize*(8), rowY1);
			dc.SetTextForeground(_T("#000000"));
		}

		if (debugger->isAlive())
		{
			char dis[256] = {0};
			u32 mem_data = debugger->readExtraMemory(memory, address);

			if (viewAsType == VIEWAS_FP)
			{
				float flt = *(float *)(&mem_data);
				sprintf(dis, "f: %f", flt);
			}
			else if (viewAsType == VIEWAS_ASCII)
			{
				u32 a[4] = {(mem_data&0xff000000)>>24,
					(mem_data&0xff0000)>>16,
					(mem_data&0xff00)>>8,
					mem_data&0xff};
				for (size_t i = 0; i < 4; i++)
					if (a[i] == '\0')
						a[i] = ' ';
				sprintf(dis, "%c%c%c%c", a[0], a[1], a[2], a[3]);
			}
			else if (viewAsType == VIEWAS_HEX)
			{
				dis[0] = 0;
				dis[1] = 0;
				u32 mema[8] = {
					debugger->readExtraMemory(memory, address),
					debugger->readExtraMemory(memory, address+4),
					debugger->readExtraMemory(memory, address+8),
					debugger->readExtraMemory(memory, address+12),
					debugger->readExtraMemory(memory, address+16),
					debugger->readExtraMemory(memory, address+20),
					debugger->readExtraMemory(memory, address+24),
					debugger->readExtraMemory(memory, address+28)
				};

				for (int i = 0; i < 8; i++)
				{
					char buf[32] = "";
					switch (dataType)
					{
					case 0:
						sprintf(buf, " %02X %02X %02X %02X",
							((mema[i]&0xff000000)>>24)&0xFF,
							((mema[i]&0xff0000)>>16)&0xFF,
							((mema[i]&0xff00)>>8)&0xFF,
							mema[i]&0xff);
						break;
					case 1:
						sprintf(buf, " %02X%02X %02X%02X",
							((mema[i]&0xff000000)>>24)&0xFF,
							((mema[i]&0xff0000)>>16)&0xFF,
							((mema[i]&0xff00)>>8)&0xFF,
							mema[i]&0xff);
						break;
					case 2:
						sprintf(buf, " %02X%02X%02X%02X",
							((mema[i]&0xff000000)>>24)&0xFF,
							((mema[i]&0xff0000)>>16)&0xFF,
							((mema[i]&0xff00)>>8)&0xFF,
							mema[i]&0xff);
						break;
					}
					strcat(dis, buf);
				}
				curAddress += 32;
			}
			else
			{
				sprintf(dis, "INVALID VIEWAS TYPE");
			}

			char desc[256] = "";
			if (viewAsType != VIEWAS_HEX)
				dc.DrawText(StrToWxStr(dis), textPlacement + fontSize*(8 + 8), rowY1);
			else
				dc.DrawText(StrToWxStr(dis), textPlacement, rowY1);

			if (desc[0] == 0)
				strcpy(desc, debugger->getDescription(address).c_str());

			dc.SetTextForeground(_T("#0000FF"));

			if (strlen(desc))
				dc.DrawText(StrToWxStr(desc), 17+fontSize*((8+8+8+30)*2), rowY1);

			// Show blue memory check dot
			if (debugger->isMemCheck(address))
			{
				dc.SetBrush(mcBrush);
				dc.DrawRectangle(8, rowY1 + 1, 11, 11);
			}
		}
	}

	dc.SetPen(currentPen);
}

void CMemoryView::OnResize(wxSizeEvent& event)
{
	Refresh();
	event.Skip();
}
