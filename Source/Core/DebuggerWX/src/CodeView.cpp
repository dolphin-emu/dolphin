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
#include "Common.h"

#include "CodeView.h"
#include "JitWindow.h"

#include <wx/event.h>
#include <wx/clipbrd.h>


enum
{
	IDM_GOTOINMEMVIEW = 12000,
	IDM_COPYADDRESS,
	IDM_COPYHEX,
	IDM_COPYCODE,
	IDM_INSERTBLR,
	IDM_RUNTOHERE,
	IDM_JITRESULTS,
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
	: wxControl(parent, Id, wxDefaultPosition, Size), debugger(debuginterface)
{
	rowHeight = 13;
	align = debuginterface->getInstructionSize(0);
	curAddress = debuginterface->getPC();
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


void CCodeView::OnMouseUpL(wxMouseEvent& event)
{
	if (event.m_x > 16)
	{
		curAddress = YToAddress(event.m_y);
		selecting = false;
		//ReleaseCapture();
		redraw();
	}

	event.Skip(true);
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
		    sprintf(temp, "%08x", debugger->readMemory(selection));
		    wxTheClipboard->SetData(new wxTextDataObject(wxString::FromAscii(temp)));
		    }
		    break;
#endif

	    case IDM_RUNTOHERE:
		    debugger->setBreakpoint(selection);
		    debugger->runToBreakpoint();
		    redraw();
		    break;

		case IDM_INSERTBLR:
			debugger->insertBLR(selection);
		    redraw();
			break;

	    case IDM_JITRESULTS:
			CJitWindow::ViewAddr(selection);
		    break;
	}

#if wxUSE_CLIPBOARD
	wxTheClipboard->Close();
#endif
	event.Skip(true);
}


void CCodeView::OnMouseUpR(wxMouseEvent& event)
{
	// popup menu
	wxMenu menu;
	//menu.Append(IDM_GOTOINMEMVIEW, "&Goto in mem view");
#if wxUSE_CLIPBOARD
	menu.Append(IDM_COPYADDRESS, wxString::FromAscii("Copy &address"));
	menu.Append(IDM_COPYCODE, wxString::FromAscii("Copy &code"));
	menu.Append(IDM_COPYHEX, wxString::FromAscii("Copy &hex"));
#endif
	menu.Append(IDM_RUNTOHERE, _T("&Run To Here"));
	menu.Append(IDM_INSERTBLR, wxString::FromAscii("Insert &blr"));
	menu.Append(IDM_JITRESULTS, wxString::FromAscii("PPC vs X86"));
	PopupMenu(&menu);
	event.Skip(true);
}


void CCodeView::OnErase(wxEraseEvent& event)
{}


void CCodeView::OnPaint(wxPaintEvent& event)
{
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
	dc.SetBackgroundMode(wxTRANSPARENT);
	const wxChar* bgColor = _T("#ffffff");
	wxPen nullPen(bgColor);
	wxPen currentPen(_T("#000000"));
	wxPen selPen(_T("#808080"));
	nullPen.SetStyle(wxTRANSPARENT);

	wxBrush currentBrush(_T("#FFEfE8"));
	wxBrush pcBrush(_T("#70FF70"));
	wxBrush bpBrush(_T("#FF3311"));
	wxBrush bgBrush(bgColor);
	wxBrush nullBrush(bgColor);
	nullBrush.SetStyle(wxTRANSPARENT);

	dc.SetPen(nullPen);
	dc.SetBrush(bgBrush);
	dc.DrawRectangle(0, 0, 16, rc.height);
	dc.DrawRectangle(0, 0, rc.width, 5);
	// TODO - clean up this freaking mess!!!!!
	int i;

	for (i = -numRows; i <= numRows; i++)
	{
		unsigned int address = curAddress + i * align;

		int rowY1 = rc.height / 2 + rowHeight * i - rowHeight / 2;
		int rowY2 = rc.height / 2 + rowHeight * i + rowHeight / 2;

		wxString temp = wxString::Format(_T("%08x"), address);
		u32 col = debugger->getColor(address);
		wxBrush rowBrush(wxColor(col >> 16, col >> 8, col));
		dc.SetBrush(nullBrush);
		dc.SetPen(nullPen);
		dc.DrawRectangle(0, rowY1, 16, rowY2);

		if (selecting && (address == selection))
		{
			dc.SetPen(selPen);
		}
		else
		{
			dc.SetPen(i == 0 ? currentPen : nullPen);
		}

		if (address == debugger->getPC())
		{
			dc.SetBrush(pcBrush);
		}
		else
		{
			dc.SetBrush(rowBrush);
		}

		dc.DrawRectangle(16, rowY1, width, rowY2 - 1);
		dc.SetBrush(currentBrush);
		dc.SetTextForeground(_T("#600000"));
		dc.DrawText(temp, 17, rowY1);
		dc.SetTextForeground(_T("#000000"));

		if (debugger->isAlive())
		{
			char dis[256] = {0};
			strcpy(dis, debugger->disasm(address));
			char* dis2 = strchr(dis, '\t');
			char desc[256] = "";

			if (dis2)
			{
				*dis2 = 0;
				dis2++;
				const char* mojs = strstr(dis2, "0x8");

				if (mojs)
				{
					for (int i = 0; i < 8; i++)
					{
						bool found = false;

						for (int j = 0; j < 22; j++)
						{
							if (mojs[i + 2] == "0123456789ABCDEFabcdef"[j])
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

				if (mojs)
				{
					int offs;
					sscanf(mojs + 2, "%08x", &offs);
					branches[numBranches].src = rowY1 + rowHeight / 2;
					branches[numBranches].srcAddr = address / align;
					branches[numBranches++].dst = (int)(rowY1 + ((s64)offs - (s64)address) * rowHeight / align + rowHeight / 2);
					sprintf(desc, "-->%s", debugger->getDescription(offs).c_str());
					dc.SetTextForeground(_T("#600060"));
				}
				else
				{
					dc.SetTextForeground(_T("#000000"));
				}

				dc.DrawText(wxString::FromAscii(dis2), 126, rowY1);
			}

			if (strcmp(dis, "blr"))
			{
				dc.SetTextForeground(_T("#007000"));
			}
			else
			{
				dc.SetTextForeground(_T("#8000FF"));
			}

			dc.DrawText(wxString::FromAscii(dis), 70, rowY1);

			if (desc[0] == 0)
			{
				strcpy(desc, debugger->getDescription(address).c_str());
			}

			dc.SetTextForeground(_T("#0000FF"));

			//char temp[256];
			//UnDecorateSymbolName(desc,temp,255,UNDNAME_COMPLETE);
			if (strlen(desc))
			{
				dc.DrawText(wxString::FromAscii(desc), 235, rowY1);
			}

			if (debugger->isBreakpoint(address))
			{
				dc.SetBrush(bpBrush);
				dc.DrawRectangle(2, rowY1, 7, 7);
//				DrawIconEx(hdc, 2, rowY1, breakPoint, 32, 32, 0, 0, DI_NORMAL);
			}
		}
	}

	dc.SetPen(currentPen);
	/*
	   for (i = 0; i < numBranches; i++)
	   {
	    int x = 250 + (branches[i].srcAddr % 9) * 8;
	    MoveToEx(hdc, x-2, branches[i].src, 0);

	    if (branches[i].dst < rect.bottom + 200 && branches[i].dst > -200)
	    {
	    LineTo(hdc, x+2, branches[i].src);
	    LineTo(hdc, x+2, branches[i].dst);
	    LineTo(hdc, x-4, branches[i].dst);

	    MoveToEx(hdc, x, branches[i].dst - 4,0);
	    LineTo(hdc, x-4, branches[i].dst);
	    LineTo(hdc, x+1, branches[i].dst+5);
	    }
	    else
	    {
	    LineTo(hdc, x+4, branches[i].src);
	    //MoveToEx(hdc,x+2,branches[i].dst-4,0);
	    //LineTo(hdc,x+6,branches[i].dst);
	    //LineTo(hdc,x+1,branches[i].dst+5);
	    }
	    //LineTo(hdc,x,branches[i].dst+4);

	    //LineTo(hdc,x-2,branches[i].dst);
	   }*/
}


