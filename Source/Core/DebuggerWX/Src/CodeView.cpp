// Copyright (C) 2003-2008 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include "Debugger.h"
#include "Debugger/PPCDebugInterface.h"
#include "PowerPC/SymbolDB.h"
#include "HW/Memmap.h" // for Write_U32
#include "Common.h"
#include "StringUtil.h"

#include "Host.h"
#include "CodeView.h"
#include "JitWindow.h"

#include <wx/event.h>
#include <wx/clipbrd.h>
#include <wx/textdlg.h>

DEFINE_EVENT_TYPE(wxEVT_CODEVIEW_CHANGE);

enum
{
	IDM_GOTOINMEMVIEW = 12000,
	IDM_COPYADDRESS,
	IDM_COPYHEX,
	IDM_COPYCODE,
	IDM_INSERTBLR,
	IDM_RUNTOHERE,
	IDM_JITRESULTS,
	IDM_FOLLOWBRANCH,
	IDM_RENAMESYMBOL,
	IDM_PATCHALERT,
	IDM_COPYFUNCTION,
};


BEGIN_EVENT_TABLE(CCodeView, wxControl)
	EVT_ERASE_BACKGROUND(CCodeView::OnErase)
	EVT_PAINT(CCodeView::OnPaint)
	EVT_LEFT_DOWN(CCodeView::OnMouseDown)
	EVT_LEFT_UP(CCodeView::OnMouseUpL)
	EVT_MOTION(CCodeView::OnMouseMove)
	EVT_RIGHT_DOWN(CCodeView::OnMouseDown)
	EVT_RIGHT_UP(CCodeView::OnMouseUpR)
	EVT_MENU(-1, CCodeView::OnPopupMenu)
END_EVENT_TABLE()

CCodeView::CCodeView(DebugInterface* debuginterface, wxWindow* parent, wxWindowID Id, const wxSize& Size)
	: wxControl(parent, Id, wxDefaultPosition, Size),
      debugger(debuginterface),
      rowHeight(13),
      selection(0),
      oldSelection(0),
      selectionChanged(false),
      selecting(false),
      hasFocus(false),
      showHex(false),
      lx(-1),
      ly(-1)
{
	rowHeight = 13;
	align = debuginterface->getInstructionSize(0);
	curAddress = debuginterface->getPC();
	selection = 0;
}


wxSize CCodeView::DoGetBestSize() const
{
	wxSize bestSize;
	bestSize.x = 400;
	bestSize.y = 800;
	return(bestSize);
}


int CCodeView::YToAddress(int y)
{
	wxRect rc = GetClientRect();
	int ydiff = y - rc.height / 2 - rowHeight / 2;
	ydiff = (int)(floorf((float)ydiff / (float)rowHeight)) + 1;
	return(curAddress + ydiff * align);
}


void CCodeView::OnMouseDown(wxMouseEvent& event)
{
	int x = event.m_x;
	int y = event.m_y;

	if (x > 16)
	{
		oldSelection = selection;
		selection = YToAddress(y);
		// SetCapture(wnd);
		bool oldselecting = selecting;
		selecting = true;

		if (!oldselecting || (selection != oldSelection))
		{
			redraw();
		}
	}
	else
	{
		debugger->toggleBreakpoint(YToAddress(y));
		redraw();
	}

	event.Skip(true);
}


void CCodeView::OnMouseMove(wxMouseEvent& event)
{
	wxRect rc = GetClientRect();

	if (event.m_leftDown)
	{
		if (event.m_x > 16)
		{
			if (event.m_y < 0)
			{
				curAddress -= align;
				redraw();
			}
			else if (event.m_y > rc.height)
			{
				curAddress += align;
				redraw();
			}
			else
			{
				OnMouseDown(event);
			}
		}
	}

	event.Skip(true);
}

void CCodeView::RaiseEvent()
{
	wxCommandEvent ev(wxEVT_CODEVIEW_CHANGE, GetId());
	ev.SetEventObject(this);
	ev.SetInt(selection);
	GetEventHandler()->ProcessEvent(ev);
}

void CCodeView::OnMouseUpL(wxMouseEvent& event)
{
	if (event.m_x > 16)
	{
		curAddress = YToAddress(event.m_y);
		selecting = false;
		//ReleaseCapture();
		redraw();
	}
	RaiseEvent();
	event.Skip(true);
}

u32 CCodeView::AddrToBranch(u32 addr)
{
	const char *temp = debugger->disasm(addr);
	const char *mojs = strstr(temp, "->0x");
	if (mojs)
	{
		u32 dest;
		sscanf(mojs+4,"%08x", &dest);
		return dest;
	}
	return 0;
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
		    wxTheClipboard->SetData(new wxTextDataObject(wxString::Format(_T("%08x"), selection)));
		    break;

	    case IDM_COPYCODE:
		    wxTheClipboard->SetData(new wxTextDataObject(wxString::FromAscii(debugger->disasm(selection)))); //Have to manually convert from char* to wxString, don't have to in Windows?
		    break;

	    case IDM_COPYHEX:
		    {
		    char temp[24];
		    sprintf(temp, "%08x", debugger->readInstruction(selection));
		    wxTheClipboard->SetData(new wxTextDataObject(wxString::FromAscii(temp)));
		    }
		    break;
		case IDM_COPYFUNCTION:
			{
			Symbol *symbol = g_symbolDB.GetSymbolFromAddr(selection);
			if (symbol) {
				std::string text;
				text = text + symbol->name + "\r\n";
				// we got a function
				u32 start = symbol->address;
				u32 end = start + symbol->size;
				for (u32 addr = start; addr != end; addr += 4) {
					text = text + StringFromFormat("%08x: ", addr) + debugger->disasm(addr) + "\r\n";
				}
				wxTheClipboard->SetData(new wxTextDataObject(wxString::FromAscii(text.c_str())));
			}
			}
			break;
#endif

	    case IDM_RUNTOHERE:
		    debugger->setBreakpoint(selection);
		    debugger->runToBreakpoint();
		    redraw();
		    break;

		// Insert blr or restore old value
		case IDM_INSERTBLR:
			{
				// Check if this address has been modified
				int find = -1;				
				for(u32 i = 0; i < BlrList.size(); i++)
				{
					if(BlrList.at(i).Address == selection)
					{ find = i; break;  }					
				}

				// Save the old value				
				if(find >= 0)
				{
					Memory::Write_U32(BlrList.at(find).OldValue, selection);
					BlrList.erase(BlrList.begin() + find);
				}
				else
				{
					BlrStruct Temp;
					Temp.Address = selection;
					Temp.OldValue = debugger->readMemory(selection);
					BlrList.push_back(Temp);
					debugger->insertBLR(selection);					
				}
				redraw();
			}
			break;

	    case IDM_JITRESULTS:
			CJitWindow::ViewAddr(selection);
		    break;
			
		case IDM_FOLLOWBRANCH:
			{
			u32 dest = AddrToBranch(selection);
			if (dest)
				Center(dest);
				RaiseEvent();
			}
			break;
	
		case IDM_RENAMESYMBOL:
			{
			Symbol *symbol = g_symbolDB.GetSymbolFromAddr(selection);
			if (symbol) {
				wxTextEntryDialog input_symbol(this, wxString::FromAscii("Rename symbol:"), wxGetTextFromUserPromptStr,
					wxString::FromAscii(symbol->name.c_str()));
				if (input_symbol.ShowModal() == wxID_OK) {
					symbol->name = input_symbol.GetValue().mb_str();
				}
//			    redraw();
				Host_NotifyMapLoaded();
			}
			}
			break;

		case IDM_PATCHALERT:
			{

			}
			break;
	}

#if wxUSE_CLIPBOARD
	wxTheClipboard->Close();
#endif
	event.Skip(true);
}


void CCodeView::OnMouseUpR(wxMouseEvent& event)
{
	bool isSymbol = g_symbolDB.GetSymbolFromAddr(selection) != 0;
	// popup menu
	wxMenu menu;
	//menu.Append(IDM_GOTOINMEMVIEW, "&Goto in mem view");
	menu.Append(IDM_FOLLOWBRANCH, wxString::FromAscii("&Follow branch"))->Enable(AddrToBranch(selection) ? true : false);
	menu.AppendSeparator();
#if wxUSE_CLIPBOARD
	menu.Append(IDM_COPYADDRESS, wxString::FromAscii("Copy &address"));
	menu.Append(IDM_COPYFUNCTION, wxString::FromAscii("Copy &function"))->Enable(isSymbol);
	menu.Append(IDM_COPYCODE, wxString::FromAscii("Copy &code line"));
	menu.Append(IDM_COPYHEX, wxString::FromAscii("Copy &hex"));
	menu.AppendSeparator();
#endif
	menu.Append(IDM_RENAMESYMBOL, wxString::FromAscii("Rename &symbol"))->Enable(isSymbol);
	menu.AppendSeparator();
	menu.Append(IDM_RUNTOHERE, _T("&Run To Here"));
	menu.Append(IDM_JITRESULTS, wxString::FromAscii("PPC vs X86"));
	menu.Append(IDM_INSERTBLR, wxString::FromAscii("Insert &blr"));
	menu.Append(IDM_PATCHALERT, wxString::FromAscii("Patch alert"));
	PopupMenu(&menu);
	event.Skip(true);
}


void CCodeView::OnErase(wxEraseEvent& event)
{}


void CCodeView::OnPaint(wxPaintEvent& event)
{
	// --------------------------------------------------------------------
	// General settings
	// -------------------------
	wxPaintDC dc(this);
	wxRect rc = GetClientRect();
	wxFont font(7, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_LIGHT);
	dc.SetFont(font);
	struct branch
	{
		int src, dst, srcAddr;
	};

	branch branches[256];
	int numBranches = 0;
	// TODO: Add any drawing code here...
	int width   = rc.width;
	int numRows = (rc.height / rowHeight) / 2 + 2;
	//numRows=(numRows&(~1)) + 1;
	// ------------


	// --------------------------------------------------------------------
	// Colors and brushes
	// -------------------------	
	dc.SetBackgroundMode(wxTRANSPARENT); // the text background
	const wxChar* bgColor = _T("#ffffff");
	wxPen nullPen(bgColor);
	wxPen currentPen(_T("#000000"));
	wxPen selPen(_T("#808080")); // gray
	nullPen.SetStyle(wxTRANSPARENT);
	currentPen.SetStyle(wxSOLID);
	wxBrush currentBrush(_T("#FFEfE8")); // the ... ? ... is light gray
	wxBrush pcBrush(_T("#70FF70")); // the selected code line is green	
	wxBrush bpBrush(_T("#FF3311")); // red

	wxBrush bgBrush(bgColor);
	wxBrush nullBrush(bgColor);
	nullBrush.SetStyle(wxTRANSPARENT);

	dc.SetPen(nullPen);
	dc.SetBrush(bgBrush);
	dc.DrawRectangle(0, 0, 16, rc.height);
	dc.DrawRectangle(0, 0, rc.width, 5);
	// ------------


	// --------------------------------------------------------------------
	// Walk through all visible rows
	// -------------------------
	for (int i = -numRows; i <= numRows; i++)
	{
		unsigned int address = curAddress + i * align;

		int rowY1 = rc.height / 2 + rowHeight * i - rowHeight / 2;
		int rowY2 = rc.height / 2 + rowHeight * i + rowHeight / 2;

		wxString temp = wxString::Format(_T("%08x"), address);
		u32 col = debugger->getColor(address);
		wxBrush rowBrush(wxColor(col >> 16, col >> 8, col));
		dc.SetBrush(nullBrush);
		dc.SetPen(nullPen);
		dc.DrawRectangle(0, rowY1, 16, rowY2 - rowY1 + 2);

		if (selecting && (address == selection))
			dc.SetPen(selPen);
		else
			dc.SetPen(i == 0 ? currentPen : nullPen);

		if (address == debugger->getPC())
			dc.SetBrush(pcBrush);
		else
			dc.SetBrush(rowBrush);

		dc.DrawRectangle(16, rowY1, width, rowY2 - rowY1 + 1);
		dc.SetBrush(currentBrush);
		dc.SetTextForeground(_T("#600000")); // the address text is dark red
		dc.DrawText(temp, 17, rowY1);
		dc.SetTextForeground(_T("#000000"));

		if (debugger->isAlive())
		{
			char dis[256] = {0};
			strcpy(dis, debugger->disasm(address));
			char* dis2 = strchr(dis, '\t');
			char desc[256] = "";

			// If we have a code
			if (dis2)
			{
				*dis2 = 0;
				dis2++;
				const char* mojs = strstr(dis2, "0x8");

				// --------------------------------------------------------------------
				// Colors and brushes
				// -------------------------
				if (mojs)
				{
					for (int k = 0; k < 8; k++)
					{
						bool found = false;

						for (int j = 0; j < 22; j++)
						{
							if (mojs[k + 2] == "0123456789ABCDEFabcdef"[j])
							{
								found = true;
							}
						}
						if (!found)
						{
							mojs = 0;
							break;
						}
					}
				}
				// ------------

			
				// --------------------------------------------------------------------
				// Colors and brushes
				// -------------------------
				if (mojs)
				{
					int offs;
					sscanf(mojs + 2, "%08x", &offs);
					branches[numBranches].src = rowY1 + rowHeight / 2;
					branches[numBranches].srcAddr = address / align;
					branches[numBranches++].dst = (int)(rowY1 + ((s64)(u32)offs - (s64)(u32)address) * rowHeight / align + rowHeight / 2);
					sprintf(desc, "-->%s", debugger->getDescription(offs).c_str());
					dc.SetTextForeground(_T("#600060")); // the -> arrow illustrations are purple
				}
				else
				{
					dc.SetTextForeground(_T("#000000"));
				}

				dc.DrawText(wxString::FromAscii(dis2), 126, rowY1);
				// ------------
			}

			// Show blr as its' own color
			if (strcmp(dis, "blr"))
				dc.SetTextForeground(_T("#007000")); // dark green
			else
				dc.SetTextForeground(_T("#8000FF")); // purple

			dc.DrawText(wxString::FromAscii(dis), 70, rowY1);

			if (desc[0] == 0)
			{
				strcpy(desc, debugger->getDescription(address).c_str());
			}

			dc.SetTextForeground(_T("#0000FF")); // blue

			//char temp[256];
			//UnDecorateSymbolName(desc,temp,255,UNDNAME_COMPLETE);
			if (strlen(desc))
			{
				dc.DrawText(wxString::FromAscii(desc), 235, rowY1);
			}

			// Show red breakpoint dot
			if (debugger->isBreakpoint(address))
			{
				dc.SetBrush(bpBrush);
				dc.DrawRectangle(2, rowY1, 7, 7);
				//DrawIconEx(hdc, 2, rowY1, breakPoint, 32, 32, 0, 0, DI_NORMAL);
			}
		}
	} // end of for
	// ------------


	// --------------------------------------------------------------------
	// Colors and brushes
	// -------------------------
	dc.SetPen(currentPen);
	
	for (int i = 0; i < numBranches; i++)
	{
	    int x = 300 + (branches[i].srcAddr % 9) * 8;
	    _MoveTo(x-2, branches[i].src);

		if (branches[i].dst < rc.height + 400 && branches[i].dst > -400)
	    {
			_LineTo(dc, x+2, branches[i].src);
			_LineTo(dc, x+2, branches[i].dst);
			_LineTo(dc, x-4, branches[i].dst);

			_MoveTo(x, branches[i].dst - 4);
			_LineTo(dc, x-4, branches[i].dst);
			_LineTo(dc, x+1, branches[i].dst+5);
	    }
	    else
	    {
			_LineTo(dc, x+4, branches[i].src);
			//MoveToEx(hdc,x+2,branches[i].dst-4,0);
			//LineTo(hdc,x+6,branches[i].dst);
			//LineTo(hdc,x+1,branches[i].dst+5);
	    }

	    //LineTo(hdc,x,branches[i].dst+4);
	    //LineTo(hdc,x-2,branches[i].dst);
	}
	// ------------
}


void CCodeView::_LineTo(wxPaintDC &dc, int x, int y)
{
	dc.DrawLine(lx, ly, x, y);
	lx = x;
	ly = y;
}

