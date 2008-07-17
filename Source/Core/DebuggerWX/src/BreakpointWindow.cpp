//__________________________________________________________________________________________________
// F|RES and ector 2003-2008
//

#include "Debugger.h"
#include "BreakPointWindow.h"
#include "BreakpointView.h"
#include "CodeWindow.h"

#include "wx/mstream.h"

extern "C" {
#include "../resources/toolbar_add_breakpoint.c"
#include "../resources/toolbar_add_memorycheck.c"
#include "../resources/toolbar_delete.c"
}

static const long TOOLBAR_STYLE = wxTB_FLAT | wxTB_DOCKABLE | wxTB_TEXT;

BEGIN_EVENT_TABLE(CBreakPointWindow, wxFrame)
	EVT_CLOSE(CBreakPointWindow::OnClose)
	EVT_MENU(IDM_DELETE, CBreakPointWindow::OnDelete)
	EVT_MENU(IDM_ADD_BREAKPOINT, CBreakPointWindow::OnAddBreakPoint)
	EVT_MENU(IDM_ADD_MEMORYCHECK, CBreakPointWindow::OnAddMemoryCheck)
    EVT_LIST_ITEM_ACTIVATED(ID_BPS, CBreakPointWindow::OnActivated)
END_EVENT_TABLE()


#define wxGetBitmapFromMemory(name) _wxGetBitmapFromMemory(name, sizeof(name))
inline wxBitmap _wxGetBitmapFromMemory(const unsigned char* data, int length)
{
	wxMemoryInputStream is(data, length);
	return(wxBitmap(wxImage(is, wxBITMAP_TYPE_ANY, -1), -1));
}


CBreakPointWindow::CBreakPointWindow(CCodeWindow* _pCodeWindow, wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& position, const wxSize& size, long style)
	: wxFrame(parent, id, title, position, size, style)
	, m_BreakPointListView(NULL)
    , m_pCodeWindow(_pCodeWindow)
{
	InitBitmaps();

	CreateGUIControls();

	// Create the toolbar
	RecreateToolbar();

}


CBreakPointWindow::~CBreakPointWindow()
{}


void 
CBreakPointWindow::CreateGUIControls()
{
	SetTitle(wxT("Breakpoints"));
	SetIcon(wxNullIcon);
	SetSize(8, 8, 400, 370);
	Center();

	m_BreakPointListView = new CBreakPointView(this, ID_BPS, wxDefaultPosition, GetSize(),
			wxLC_REPORT | wxSUNKEN_BORDER | wxLC_ALIGN_LEFT | wxLC_SINGLE_SEL | wxLC_SORT_ASCENDING);

	NotifyUpdate();
}


void
CBreakPointWindow::PopulateToolbar(wxToolBar* toolBar)
{
	int w = m_Bitmaps[Toolbar_Delete].GetWidth(),
		h = m_Bitmaps[Toolbar_Delete].GetHeight();

	toolBar->SetToolBitmapSize(wxSize(w, h));
	toolBar->AddTool(IDM_DELETE, _T("Delete"), m_Bitmaps[Toolbar_Delete], _T("Refresh"));
	toolBar->AddSeparator();
	toolBar->AddTool(IDM_ADD_BREAKPOINT,    _T("BP"),    m_Bitmaps[Toolbar_Add_BreakPoint], _T("Open file..."));
	toolBar->AddTool(IDM_ADD_MEMORYCHECK, _T("MemCheck"), m_Bitmaps[Toolbar_Add_Memcheck], _T("Refresh"));

	// after adding the buttons to the toolbar, must call Realize() to reflect
	// the changes
	toolBar->Realize();
}


void
CBreakPointWindow::RecreateToolbar()
{
	// delete and recreate the toolbar
	wxToolBarBase* toolBar = GetToolBar();
	long style = toolBar ? toolBar->GetWindowStyle() : TOOLBAR_STYLE;

	delete toolBar;
	SetToolBar(NULL);

	style &= ~(wxTB_HORIZONTAL | wxTB_VERTICAL | wxTB_BOTTOM | wxTB_RIGHT | wxTB_HORZ_LAYOUT | wxTB_TOP);
	wxToolBar* theToolBar = CreateToolBar(style, ID_TOOLBAR);

	PopulateToolbar(theToolBar);
	SetToolBar(theToolBar);
}


void
CBreakPointWindow::InitBitmaps()
{
	// load orignal size 48x48
	m_Bitmaps[Toolbar_Delete] = wxGetBitmapFromMemory(toolbar_delete_png);
	m_Bitmaps[Toolbar_Add_BreakPoint] = wxGetBitmapFromMemory(toolbar_add_breakpoint_png);
	m_Bitmaps[Toolbar_Add_Memcheck] = wxGetBitmapFromMemory(toolbar_add_memcheck_png);

	// scale to 24x24 for toolbar
	for (size_t n = Toolbar_Delete; n < WXSIZEOF(m_Bitmaps); n++)
	{
		m_Bitmaps[n] = wxBitmap(m_Bitmaps[n].ConvertToImage().Scale(16, 16));
	}
}


void 
CBreakPointWindow::OnClose(wxCloseEvent& /*event*/)
{
	Hide();
}


void CBreakPointWindow::NotifyUpdate()
{
	if (m_BreakPointListView != NULL)
	{
		m_BreakPointListView->Update();
	}
}


void 
CBreakPointWindow::OnDelete(wxCommandEvent& event)
{
	if (m_BreakPointListView)
	{
		m_BreakPointListView->DeleteCurrentSelection();
	}
}


void 
CBreakPointWindow::OnAddBreakPoint(wxCommandEvent& event)
{

}


void 
CBreakPointWindow::OnAddMemoryCheck(wxCommandEvent& event)
{

}


void
CBreakPointWindow::OnActivated(wxListEvent& event)
{
    long Index = event.GetIndex();
    if (Index >= 0)
    {
        u32 Address = (u32)m_BreakPointListView->GetItemData(Index);
        if (m_pCodeWindow != NULL)
        {
            m_pCodeWindow->JumpToAddress(Address);
        }
    }
}

