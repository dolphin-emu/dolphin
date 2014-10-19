// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cstddef>
#include <string>
#include <vector>
#include <wx/chartype.h>
#include <wx/defs.h>
#include <wx/dynarray.h>
#include <wx/event.h>
#include <wx/frame.h>
#include <wx/gdicmn.h>
#include <wx/list.h>
#include <wx/menu.h>
#include <wx/menuitem.h>
#include <wx/msgdlg.h>
#include <wx/object.h>
#include <wx/panel.h>
#include <wx/rtti.h>
#include <wx/sizer.h>
#include <wx/statusbr.h>
#include <wx/string.h>
#include <wx/textdlg.h>
#include <wx/toplevel.h>
#include <wx/translation.h>
#include <wx/window.h>
#include <wx/windowid.h>
#include <wx/aui/auibar.h>
#include <wx/aui/auibook.h>
#include <wx/aui/framemanager.h>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/MathUtil.h"
#include "Common/StringUtil.h"
#include "Common/Logging/ConsoleListener.h"
#include "Core/ConfigManager.h"
#include "DolphinWX/Frame.h"
#include "DolphinWX/Globals.h"
#include "DolphinWX/LogConfigWindow.h"
#include "DolphinWX/LogWindow.h"
#include "DolphinWX/WxUtils.h"
#include "DolphinWX/Debugger/CodeWindow.h"

// ------------
// Aui events

void CFrame::OnManagerResize(wxAuiManagerEvent& event)
{
	if (!g_pCodeWindow && m_LogWindow &&
			m_Mgr->GetPane("Pane 1").IsShown() &&
			!m_Mgr->GetPane("Pane 1").IsFloating())
	{
		m_LogWindow->x = m_Mgr->GetPane("Pane 1").rect.GetWidth();
		m_LogWindow->y = m_Mgr->GetPane("Pane 1").rect.GetHeight();
		m_LogWindow->winpos = m_Mgr->GetPane("Pane 1").dock_direction;
	}
	event.Skip();
}

void CFrame::OnPaneClose(wxAuiManagerEvent& event)
{
	event.Veto();

	wxAuiNotebook * nb = (wxAuiNotebook*)event.pane->window;
	if (!nb) return;

	if (!g_pCodeWindow)
	{
		if (nb->GetPage(0)->GetId() == IDM_LOGWINDOW ||
		    nb->GetPage(0)->GetId() == IDM_LOGCONFIGWINDOW)
		{

			SConfig::GetInstance().m_InterfaceLogWindow = false;
			SConfig::GetInstance().m_InterfaceLogConfigWindow = false;
			ToggleLogWindow(false);
			ToggleLogConfigWindow(false);
		}
	}
	else
	{
		if (GetNotebookCount() == 1)
		{
			wxMessageBox(_("At least one pane must remain open."),
					_("Notice"), wxOK, this);
		}
		else if (nb->GetPageCount() != 0 && !nb->GetPageText(0).IsSameAs("<>"))
		{
			wxMessageBox(_("You can't close panes that have pages in them."),
					_("Notice"), wxOK, this);
		}
		else
		{
			// Detach and delete the empty notebook
			event.pane->DestroyOnClose(true);
			m_Mgr->ClosePane(*event.pane);
		}
	}

	m_Mgr->Update();
}

void CFrame::ToggleLogWindow(bool bShow)
{
	if (!m_LogWindow)
		return;

	GetMenuBar()->FindItem(IDM_LOGWINDOW)->Check(bShow);

	if (bShow)
	{
		// Create a new log window if it doesn't exist.
		if (!m_LogWindow)
		{
			m_LogWindow = new CLogWindow(this, IDM_LOGWINDOW);
		}

		m_LogWindow->Enable();

		DoAddPage(m_LogWindow,
				g_pCodeWindow ? g_pCodeWindow->iNbAffiliation[0] : 0,
				g_pCodeWindow ? bFloatWindow[0] : false);
	}
	else
	{
		// Hiding the log window, so disable it and remove it.
		m_LogWindow->Disable();
		DoRemovePage(m_LogWindow, true);
	}

	// Hide or Show the pane
	if (!g_pCodeWindow)
		TogglePane();
}

void CFrame::ToggleLogConfigWindow(bool bShow)
{
	GetMenuBar()->FindItem(IDM_LOGCONFIGWINDOW)->Check(bShow);

	if (bShow)
	{
		if (!m_LogConfigWindow)
			m_LogConfigWindow = new LogConfigWindow(this, m_LogWindow, IDM_LOGCONFIGWINDOW);

		const int nbIndex = IDM_LOGCONFIGWINDOW - IDM_LOGWINDOW;
		DoAddPage(m_LogConfigWindow,
				g_pCodeWindow ? g_pCodeWindow->iNbAffiliation[nbIndex] : 0,
				g_pCodeWindow ? bFloatWindow[nbIndex] : false);
	}
	else
	{
		DoRemovePage(m_LogConfigWindow, false);
		m_LogConfigWindow = nullptr;
	}

	// Hide or Show the pane
	if (!g_pCodeWindow)
		TogglePane();
}

void CFrame::OnToggleWindow(wxCommandEvent& event)
{
	bool bShow = GetMenuBar()->IsChecked(event.GetId());

	switch (event.GetId())
	{
		case IDM_LOGWINDOW:
			if (!g_pCodeWindow)
				SConfig::GetInstance().m_InterfaceLogWindow = bShow;
			ToggleLogWindow(bShow);
			break;
		case IDM_LOGCONFIGWINDOW:
			if (!g_pCodeWindow)
				SConfig::GetInstance().m_InterfaceLogConfigWindow = bShow;
			ToggleLogConfigWindow(bShow);
			break;
		case IDM_REGISTERWINDOW:
			g_pCodeWindow->ToggleRegisterWindow(bShow);
			break;
		case IDM_WATCHWINDOW:
			g_pCodeWindow->ToggleWatchWindow(bShow);
			break;
		case IDM_BREAKPOINTWINDOW:
			g_pCodeWindow->ToggleBreakPointWindow(bShow);
			break;
		case IDM_MEMORYWINDOW:
			g_pCodeWindow->ToggleMemoryWindow(bShow);
			break;
		case IDM_JITWINDOW:
			g_pCodeWindow->ToggleJitWindow(bShow);
			break;
		case IDM_SOUNDWINDOW:
			g_pCodeWindow->ToggleSoundWindow(bShow);
			break;
		case IDM_VIDEOWINDOW:
			g_pCodeWindow->ToggleVideoWindow(bShow);
			break;
	}
}

// Notebooks
// ---------------------
void CFrame::ClosePages()
{
	ToggleLogWindow(false);
	ToggleLogConfigWindow(false);

	if (g_pCodeWindow)
	{
		g_pCodeWindow->ToggleCodeWindow(false);
		g_pCodeWindow->ToggleRegisterWindow(false);
		g_pCodeWindow->ToggleWatchWindow(false);
		g_pCodeWindow->ToggleBreakPointWindow(false);
		g_pCodeWindow->ToggleMemoryWindow(false);
		g_pCodeWindow->ToggleJitWindow(false);
		g_pCodeWindow->ToggleSoundWindow(false);
		g_pCodeWindow->ToggleVideoWindow(false);
	}
}

void CFrame::OnNotebookPageChanged(wxAuiNotebookEvent& event)
{
	event.Skip();

	if (!g_pCodeWindow)
		return;

	// Remove the blank page if any
	AddRemoveBlankPage();

	// Update the notebook affiliation
	for (int i = IDM_LOGWINDOW; i <= IDM_CODEWINDOW; i++)
	{
		if (GetNotebookAffiliation(i) >= 0)
			g_pCodeWindow->iNbAffiliation[i - IDM_LOGWINDOW] = GetNotebookAffiliation(i);
	}
}

void CFrame::OnNotebookPageClose(wxAuiNotebookEvent& event)
{
	// Override event
	event.Veto();

	wxAuiNotebook* Ctrl = (wxAuiNotebook*)event.GetEventObject();

	if (Ctrl->GetPage(event.GetSelection())->GetId() == IDM_LOGWINDOW)
		ToggleLogWindow(false);
	if (Ctrl->GetPage(event.GetSelection())->GetId() == IDM_LOGCONFIGWINDOW)
		ToggleLogConfigWindow(false);
	if (Ctrl->GetPage(event.GetSelection())->GetId() == IDM_REGISTERWINDOW)
		g_pCodeWindow->ToggleRegisterWindow(false);
	if (Ctrl->GetPage(event.GetSelection())->GetId() == IDM_WATCHWINDOW)
		g_pCodeWindow->ToggleWatchWindow(false);
	if (Ctrl->GetPage(event.GetSelection())->GetId() == IDM_BREAKPOINTWINDOW)
		g_pCodeWindow->ToggleBreakPointWindow(false);
	if (Ctrl->GetPage(event.GetSelection())->GetId() == IDM_JITWINDOW)
		g_pCodeWindow->ToggleJitWindow(false);
	if (Ctrl->GetPage(event.GetSelection())->GetId() == IDM_MEMORYWINDOW)
		g_pCodeWindow->ToggleMemoryWindow(false);
	if (Ctrl->GetPage(event.GetSelection())->GetId() == IDM_SOUNDWINDOW)
		g_pCodeWindow->ToggleSoundWindow(false);
	if (Ctrl->GetPage(event.GetSelection())->GetId() == IDM_VIDEOWINDOW)
		g_pCodeWindow->ToggleVideoWindow(false);
}

void CFrame::OnFloatingPageClosed(wxCloseEvent& event)
{
	ToggleFloatWindow(event.GetId() - IDM_LOGWINDOW_PARENT + IDM_FLOAT_LOGWINDOW);
}

void CFrame::OnFloatingPageSize(wxSizeEvent& event)
{
	event.Skip();
}

void CFrame::OnFloatWindow(wxCommandEvent& event)
{
	ToggleFloatWindow(event.GetId());
}

void CFrame::ToggleFloatWindow(int Id)
{
	wxWindowID WinId = Id - IDM_FLOAT_LOGWINDOW + IDM_LOGWINDOW;
	if (GetNotebookPageFromId(WinId))
	{
		DoFloatNotebookPage(WinId);
		bFloatWindow[WinId - IDM_LOGWINDOW] = true;
	}
	else
	{
		if (FindWindowById(WinId))
			DoUnfloatPage(WinId - IDM_LOGWINDOW + IDM_LOGWINDOW_PARENT);
		bFloatWindow[WinId - IDM_LOGWINDOW] = false;
	}
}

void CFrame::DoFloatNotebookPage(wxWindowID Id)
{
	wxPanel *Win = (wxPanel*)FindWindowById(Id);
	if (!Win) return;

	for (int i = 0; i < GetNotebookCount(); i++)
	{
		wxAuiNotebook *nb = GetNotebookFromId(i);
		if (nb->GetPageIndex(Win) != wxNOT_FOUND)
		{
			nb->RemovePage(nb->GetPageIndex(Win));
			// Create the parent frame and reparent the window
			CreateParentFrame(Win->GetId() + IDM_LOGWINDOW_PARENT - IDM_LOGWINDOW,
					Win->GetName(), Win);
			if (nb->GetPageCount() == 0)
				AddRemoveBlankPage();
		}
	}
}

void CFrame::DoUnfloatPage(int Id)
{
	wxFrame * Win = (wxFrame*)FindWindowById(Id);
	if (!Win) return;

	wxWindow * Child = Win->GetChildren().Item(0)->GetData();
	Child->Reparent(this);
	DoAddPage(Child, g_pCodeWindow->iNbAffiliation[Child->GetId() - IDM_LOGWINDOW], false);
	Win->Destroy();
}

void CFrame::OnTab(wxAuiNotebookEvent& event)
{
	event.Skip();
	if (!g_pCodeWindow) return;

	// Create the popup menu
	wxMenu* MenuPopup = new wxMenu;

	wxMenuItem* Item =  new wxMenuItem(MenuPopup, wxID_ANY, _("Select floating windows"));
	MenuPopup->Append(Item);
	Item->Enable(false);
	MenuPopup->Append(new wxMenuItem(MenuPopup));

	for (int i = IDM_LOGWINDOW; i <= IDM_CODEWINDOW; i++)
	{
		wxWindow *Win = FindWindowById(i);
		if (Win && Win->IsEnabled())
		{
			Item = new wxMenuItem(MenuPopup, i + IDM_FLOAT_LOGWINDOW - IDM_LOGWINDOW,
					Win->GetName(), "", wxITEM_CHECK);
			MenuPopup->Append(Item);
			Item->Check(!!FindWindowById(i + IDM_LOGWINDOW_PARENT - IDM_LOGWINDOW));
		}
	}

	// Line up our menu with the cursor
	wxPoint Pt = ::wxGetMousePosition();
	Pt = ScreenToClient(Pt);

	// Show
	PopupMenu(MenuPopup, Pt);
}

void CFrame::OnAllowNotebookDnD(wxAuiNotebookEvent& event)
{
	event.Skip();
	event.Allow();
}

void CFrame::ShowResizePane()
{
	if (!m_LogWindow) return;

	// Make sure the size is sane
	if (m_LogWindow->x > GetClientRect().GetWidth())
		m_LogWindow->x = GetClientRect().GetWidth() / 2;
	if (m_LogWindow->y > GetClientRect().GetHeight())
		m_LogWindow->y = GetClientRect().GetHeight() / 2;

	wxAuiPaneInfo &pane = m_Mgr->GetPane("Pane 1");

	// Hide first otherwise a resize doesn't work
	pane.Hide();
	m_Mgr->Update();

	pane.BestSize(m_LogWindow->x, m_LogWindow->y)
		.MinSize(m_LogWindow->x, m_LogWindow->y)
		.Direction(m_LogWindow->winpos).Show();
	m_Mgr->Update();

	// Reset the minimum size of the pane
	pane.MinSize(-1, -1);
	m_Mgr->Update();
}

void CFrame::TogglePane()
{
	// Get the first notebook
	wxAuiNotebook * NB = nullptr;
	for (u32 i = 0; i < m_Mgr->GetAllPanes().GetCount(); i++)
	{
		if (m_Mgr->GetAllPanes()[i].window->IsKindOf(CLASSINFO(wxAuiNotebook)))
			NB = (wxAuiNotebook*)m_Mgr->GetAllPanes()[i].window;
	}

	if (NB)
	{
		if (NB->GetPageCount() == 0)
		{
			m_Mgr->GetPane("Pane 1").Hide();
			m_Mgr->Update();
		}
		else
		{
			ShowResizePane();
		}
	}
}

void CFrame::DoRemovePage(wxWindow *Win, bool bHide)
{
	if (!Win) return;

	wxWindow *Parent = FindWindowById(Win->GetId() +
			IDM_LOGWINDOW_PARENT - IDM_LOGWINDOW);

	if (Parent)
	{
		if (bHide)
		{
			Win->Hide();
			Win->Reparent(this);
		}
		else
		{
			Win->Destroy();
		}

		Parent->Destroy();
	}
	else
	{
		for (int i = 0; i < GetNotebookCount(); i++)
		{
			int PageIndex = GetNotebookFromId(i)->GetPageIndex(Win);
			if (PageIndex != wxNOT_FOUND)
			{
				GetNotebookFromId(i)->RemovePage(PageIndex);
				if (bHide)
				{
					Win->Hide();
					Win->Reparent(this);
				}
				else
				{
					Win->Destroy();
				}
			}
		}
	}

	if (g_pCodeWindow)
		AddRemoveBlankPage();
}

void CFrame::DoAddPage(wxWindow *Win, int i, bool Float)
{
	if (!Win) return;

	// Ensure accessor remains within valid bounds.
	if (i < 0 || i > GetNotebookCount()-1)
		i = 0;

	// The page was already previously added, no need to add it again.
	if (Win && GetNotebookFromId(i)->GetPageIndex(Win) != wxNOT_FOUND)
		return;

	if (!Float)
		GetNotebookFromId(i)->AddPage(Win, Win->GetName(), true);
	else
		CreateParentFrame(Win->GetId() + IDM_LOGWINDOW_PARENT - IDM_LOGWINDOW,
				Win->GetName(), Win);
}

void CFrame::PopulateSavedPerspectives()
{
	// If the perspective submenu hasn't been created yet, return
	if (!m_SavedPerspectives) return;

	// Delete all saved perspective menu items
	while (m_SavedPerspectives->GetMenuItemCount() != 0)
	{
		// Delete the first menu item in the list (while there are menu items)
		m_SavedPerspectives->Delete(m_SavedPerspectives->FindItemByPosition(0));
	}

	if (Perspectives.size() > 0)
	{
		for (u32 i = 0; i < Perspectives.size(); i++)
		{
			wxMenuItem* mItem = new wxMenuItem(m_SavedPerspectives, IDM_PERSPECTIVES_0 + i,
					StrToWxStr(Perspectives[i].Name), "", wxITEM_CHECK);

			m_SavedPerspectives->Append(mItem);

			if (i == ActivePerspective)
			{
				mItem->Check(true);
			}
		}
	}
}

void CFrame::OnPerspectiveMenu(wxCommandEvent& event)
{
	ClearStatusBar();

	switch (event.GetId())
	{
		case IDM_SAVE_PERSPECTIVE:
			if (Perspectives.size() == 0)
			{
				wxMessageBox(_("Please create a perspective before saving"),
						_("Notice"), wxOK, this);
				return;
			}
			SaveIniPerspectives();
			GetStatusBar()->SetStatusText(StrToWxStr(std::string
						("Saved " + Perspectives[ActivePerspective].Name)), 0);
			break;
		case IDM_PERSPECTIVES_ADD_PANE:
			AddPane();
			break;
		case IDM_EDIT_PERSPECTIVES:
			m_bEdit = event.IsChecked();
			TogglePaneStyle(m_bEdit, IDM_EDIT_PERSPECTIVES);
			break;
		case IDM_ADD_PERSPECTIVE:
			{
				wxTextEntryDialog dlg(this,
						_("Enter a name for the new perspective:"),
						_("Create new perspective"));
				wxString DefaultValue = wxString::Format(_("Perspective %d"), (int)(Perspectives.size() + 1));
				dlg.SetValue(DefaultValue);

				int Return = 0;
				bool DlgOk = false;

				while (!DlgOk)
				{
					Return = dlg.ShowModal();
					if (Return == wxID_CANCEL)
					{
						return;
					}
					else if (dlg.GetValue().Find(",") != -1)
					{
						wxMessageBox(_("The name can not contain the character ','"),
								_("Notice"), wxOK, this);
						wxString Str = dlg.GetValue();
						Str.Replace(",", "", true);
						dlg.SetValue(Str);
					}
					else if (dlg.GetValue().IsSameAs(""))
					{
						wxMessageBox(_("The name can not be empty"),
								_("Notice"), wxOK, this);
						dlg.SetValue(DefaultValue);
					}
					else
					{
						DlgOk = true;
					}
				}

				SPerspectives Tmp;
				Tmp.Name = WxStrToStr(dlg.GetValue());
				Tmp.Perspective = m_Mgr->SavePerspective();

				ActivePerspective = (u32)Perspectives.size();
				Perspectives.push_back(Tmp);

				UpdateCurrentPerspective();
				PopulateSavedPerspectives();
			}
			break;
		case IDM_TAB_SPLIT:
			m_bTabSplit = event.IsChecked();
			ToggleNotebookStyle(m_bTabSplit, wxAUI_NB_TAB_SPLIT);
			break;
		case IDM_NO_DOCKING:
			m_bNoDocking = event.IsChecked();
			TogglePaneStyle(m_bNoDocking, IDM_NO_DOCKING);
			break;
	}
}

void CFrame::TogglePaneStyle(bool On, int EventId)
{
	wxAuiPaneInfoArray& AllPanes = m_Mgr->GetAllPanes();
	for (u32 i = 0; i < AllPanes.GetCount(); ++i)
	{
		wxAuiPaneInfo& Pane = AllPanes[i];
		if (Pane.window->IsKindOf(CLASSINFO(wxAuiNotebook)))
		{
			// Default
			Pane.CloseButton(true);
			Pane.MaximizeButton(true);
			Pane.MinimizeButton(true);
			Pane.PinButton(true);
			Pane.Show();

			switch (EventId)
			{
				case IDM_EDIT_PERSPECTIVES:
					Pane.CaptionVisible(On);
					Pane.Movable(On);
					Pane.Floatable(On);
					Pane.Dockable(On);
					break;
			}
			Pane.Dockable(!m_bNoDocking);
		}
	}
	m_Mgr->Update();
}

void CFrame::ToggleNotebookStyle(bool On, long Style)
{
	wxAuiPaneInfoArray& AllPanes = m_Mgr->GetAllPanes();
	for (int i = 0, Count = (int)AllPanes.GetCount(); i < Count; ++i)
	{
		wxAuiPaneInfo& Pane = AllPanes[i];
		if (Pane.window->IsKindOf(CLASSINFO(wxAuiNotebook)))
		{
			wxAuiNotebook* NB = (wxAuiNotebook*)Pane.window;

			if (On)
				NB->SetWindowStyleFlag(NB->GetWindowStyleFlag() | Style);
			else
				NB->SetWindowStyleFlag(NB->GetWindowStyleFlag() &~ Style);

			NB->Refresh();
		}
	}
}

void CFrame::OnSelectPerspective(wxCommandEvent& event)
{
	u32 _Selection = event.GetId() - IDM_PERSPECTIVES_0;
	if (Perspectives.size() <= _Selection) _Selection = 0;
	ActivePerspective = _Selection;
	DoLoadPerspective();
}

void CFrame::SetPaneSize()
{
	if (Perspectives.size() <= ActivePerspective)
		return;

	int iClientX = GetSize().GetX();
	int iClientY = GetSize().GetY();

	for (u32 i = 0, j = 0; i < m_Mgr->GetAllPanes().GetCount(); i++)
	{
		if (!m_Mgr->GetAllPanes()[i].window->IsKindOf(CLASSINFO(wxAuiToolBar)))
		{
			if (!m_Mgr->GetAllPanes()[i].IsOk())
				return;

			if (Perspectives[ActivePerspective].Width.size() <= j ||
					Perspectives[ActivePerspective].Height.size() <= j)
				continue;

			// Width and height of the active perspective
			u32 W = Perspectives[ActivePerspective].Width[j],
				H = Perspectives[ActivePerspective].Height[j];

			// Check limits
			MathUtil::Clamp<u32>(&W, 5, 95);
			MathUtil::Clamp<u32>(&H, 5, 95);

			// Convert percentages to pixel lengths
			W = (W * iClientX) / 100;
			H = (H * iClientY) / 100;
			m_Mgr->GetAllPanes()[i].BestSize(W,H).MinSize(W,H);

			j++;
		}
	}
	m_Mgr->Update();

	for (u32 i = 0; i < m_Mgr->GetAllPanes().GetCount(); i++)
	{
		if (!m_Mgr->GetAllPanes()[i].window->IsKindOf(CLASSINFO(wxAuiToolBar)))
		{
			m_Mgr->GetAllPanes()[i].MinSize(-1,-1);
		}
	}
}

void CFrame::ReloadPanes()
{
	// Close all pages
	ClosePages();

	CloseAllNotebooks();

	// Create new panes with notebooks
	for (u32 i = 0; i < Perspectives[ActivePerspective].Width.size() - 1; i++)
	{
		wxString PaneName = wxString::Format("Pane %i", i + 1);
		m_Mgr->AddPane(CreateEmptyNotebook(), wxAuiPaneInfo().Hide()
				.CaptionVisible(m_bEdit).Dockable(!m_bNoDocking).Position(i)
				.Name(PaneName).Caption(PaneName));
	}

	// Perspectives
	m_Mgr->LoadPerspective(Perspectives[ActivePerspective].Perspective, false);
	// Restore settings
	TogglePaneStyle(m_bNoDocking, IDM_NO_DOCKING);
	TogglePaneStyle(m_bEdit, IDM_EDIT_PERSPECTIVES);

	// Load GUI settings
	g_pCodeWindow->Load();
	// Open notebook pages
	AddRemoveBlankPage();
	g_pCodeWindow->OpenPages();

	// Repopulate perspectives
	PopulateSavedPerspectives();
}

void CFrame::DoLoadPerspective()
{
	ReloadPanes();
	// Restore the exact window sizes, which LoadPerspective doesn't always do
	SetPaneSize();

	m_Mgr->Update();
}

// Update the local perspectives array
void CFrame::LoadIniPerspectives()
{
	Perspectives.clear();
	std::vector<std::string> VPerspectives;
	std::string _Perspectives;

	IniFile ini;
	ini.Load(File::GetUserPath(F_DEBUGGERCONFIG_IDX));

	IniFile::Section* perspectives = ini.GetOrCreateSection("Perspectives");
	perspectives->Get("Perspectives", &_Perspectives, "Perspective 1");
	perspectives->Get("Active", &ActivePerspective, 0);
	SplitString(_Perspectives, ',', VPerspectives);

	for (auto& VPerspective : VPerspectives)
	{
		SPerspectives Tmp;
		std::string _Section, _Perspective, _Widths, _Heights;
		std::vector<std::string> _SWidth, _SHeight;
		Tmp.Name = VPerspective;

		// Don't save a blank perspective
		if (Tmp.Name.empty())
		{
			continue;
		}

		_Section = StringFromFormat("P - %s", Tmp.Name.c_str());

		IniFile::Section* perspec_section = ini.GetOrCreateSection(_Section);
		perspec_section->Get("Perspective", &_Perspective,
		                     "layout2|"
		                     "name=Pane 0;caption=Pane 0;state=768;dir=5;prop=100000;|"
		                     "name=Pane 1;caption=Pane 1;state=31458108;dir=4;prop=100000;|"
		                     "dock_size(5,0,0)=22|dock_size(4,0,0)=333|");
		perspec_section->Get("Width", &_Widths, "70,25");
		perspec_section->Get("Height", &_Heights, "80,80");

		Tmp.Perspective = StrToWxStr(_Perspective);

		SplitString(_Widths, ',', _SWidth);
		SplitString(_Heights, ',', _SHeight);
		for (auto& Width : _SWidth)
		{
			int _Tmp;
			if (TryParse(Width, &_Tmp))
				Tmp.Width.push_back(_Tmp);
		}
		for (auto& Height : _SHeight)
		{
			int _Tmp;
			if (TryParse(Height, &_Tmp))
				Tmp.Height.push_back(_Tmp);
		}
		Perspectives.push_back(Tmp);
	}
}

void CFrame::UpdateCurrentPerspective()
{
	SPerspectives *current = &Perspectives[ActivePerspective];
	current->Perspective = m_Mgr->SavePerspective();

	// Get client size
	int iClientX = GetSize().GetX(), iClientY = GetSize().GetY();
	current->Width.clear();
	current->Height.clear();
	for (u32 i = 0; i < m_Mgr->GetAllPanes().GetCount(); i++)
	{
		if (!m_Mgr->GetAllPanes()[i].window->
				IsKindOf(CLASSINFO(wxAuiToolBar)))
		{
			// Save width and height as a percentage of the client width and height
			current->Width.push_back(
					(m_Mgr->GetAllPanes()[i].window->GetClientSize().GetX() * 100) /
					iClientX);
			current->Height.push_back(
					(m_Mgr->GetAllPanes()[i].window->GetClientSize().GetY() * 100) /
					iClientY);
		}
	}
}

void CFrame::SaveIniPerspectives()
{
	if (Perspectives.size() == 0) return;
	if (ActivePerspective >= Perspectives.size()) ActivePerspective = 0;

	// Turn off edit before saving
	TogglePaneStyle(false, IDM_EDIT_PERSPECTIVES);

	UpdateCurrentPerspective();

	IniFile ini;
	ini.Load(File::GetUserPath(F_DEBUGGERCONFIG_IDX));

	// Save perspective names
	std::string STmp = "";
	for (auto& Perspective : Perspectives)
	{
		STmp += Perspective.Name + ",";
	}
	STmp = STmp.substr(0, STmp.length()-1);

	IniFile::Section* perspectives = ini.GetOrCreateSection("Perspectives");
	perspectives->Set("Perspectives", STmp);
	perspectives->Set("Active", ActivePerspective);

	// Save the perspectives
	for (auto& Perspective : Perspectives)
	{
		std::string _Section = "P - " + Perspective.Name;
		IniFile::Section* perspec_section = ini.GetOrCreateSection(_Section);
		perspec_section->Set("Perspective", WxStrToStr(Perspective.Perspective));

		std::string SWidth = "", SHeight = "";
		for (u32 j = 0; j < Perspective.Width.size(); j++)
		{
			SWidth += StringFromFormat("%i,", Perspective.Width[j]);
			SHeight += StringFromFormat("%i,", Perspective.Height[j]);
		}
		// Remove the ending ","
		SWidth = SWidth.substr(0, SWidth.length()-1);
		SHeight = SHeight.substr(0, SHeight.length()-1);

		perspec_section->Set("Width", SWidth);
		perspec_section->Set("Height", SHeight);
	}

	ini.Save(File::GetUserPath(F_DEBUGGERCONFIG_IDX));

	// Save notebook affiliations
	g_pCodeWindow->Save();

	TogglePaneStyle(m_bEdit, IDM_EDIT_PERSPECTIVES);
}

void CFrame::AddPane()
{
	int PaneNum = GetNotebookCount() + 1;
	wxString PaneName = wxString::Format("Pane %i", PaneNum);
	m_Mgr->AddPane(CreateEmptyNotebook(), wxAuiPaneInfo()
		.CaptionVisible(m_bEdit).Dockable(!m_bNoDocking)
		.Name(PaneName).Caption(PaneName)
		.Position(GetNotebookCount()));

	AddRemoveBlankPage();
	m_Mgr->Update();
}

wxWindow * CFrame::GetNotebookPageFromId(wxWindowID Id)
{
	for (u32 i = 0; i < m_Mgr->GetAllPanes().GetCount(); i++)
	{
		if (!m_Mgr->GetAllPanes()[i].window->IsKindOf(CLASSINFO(wxAuiNotebook)))
			continue;

		wxAuiNotebook * NB = (wxAuiNotebook*)m_Mgr->GetAllPanes()[i].window;
		for (u32 j = 0; j < NB->GetPageCount(); j++)
		{
			if (NB->GetPage(j)->GetId() == Id)
				return NB->GetPage(j);
		}
	}
	return nullptr;
}

wxFrame* CFrame::CreateParentFrame(wxWindowID Id, const wxString& Title, wxWindow* Child)
{
	wxFrame* Frame = new wxFrame(this, Id, Title);

	Child->Reparent(Frame);

	wxBoxSizer* m_MainSizer = new wxBoxSizer(wxHORIZONTAL);

	m_MainSizer->Add(Child, 1, wxEXPAND);

	Frame->Bind(wxEVT_CLOSE_WINDOW, &CFrame::OnFloatingPageClosed, this);

	// Main sizer
	Frame->SetSizer(m_MainSizer);
	// Minimum frame size
	Frame->SetMinSize(wxSize(200, 200));
	Frame->Show();
	return Frame;
}

wxAuiNotebook* CFrame::CreateEmptyNotebook()
{
	const long NOTEBOOK_STYLE = wxAUI_NB_TOP | wxAUI_NB_TAB_SPLIT |
		wxAUI_NB_TAB_EXTERNAL_MOVE | wxAUI_NB_SCROLL_BUTTONS |
		wxAUI_NB_WINDOWLIST_BUTTON | wxNO_BORDER;
	wxAuiNotebook* NB = new wxAuiNotebook(this, wxID_ANY,
			wxDefaultPosition, wxDefaultSize, NOTEBOOK_STYLE);
	return NB;
}

void CFrame::AddRemoveBlankPage()
{
	for (u32 i = 0; i < m_Mgr->GetAllPanes().GetCount(); i++)
	{
		if (!m_Mgr->GetAllPanes()[i].window->IsKindOf(CLASSINFO(wxAuiNotebook)))
			continue;

		wxAuiNotebook * NB = (wxAuiNotebook*)m_Mgr->GetAllPanes()[i].window;
		for (u32 j = 0; j < NB->GetPageCount(); j++)
		{
			if (NB->GetPageText(j).IsSameAs("<>") && NB->GetPageCount() > 1)
				NB->DeletePage(j);
		}

		if (NB->GetPageCount() == 0)
			NB->AddPage(new wxPanel(this, wxID_ANY), "<>", true);
	}
}

int CFrame::GetNotebookAffiliation(wxWindowID Id)
{
	for (u32 i = 0, j = 0; i < m_Mgr->GetAllPanes().GetCount(); i++)
	{
		if (!m_Mgr->GetAllPanes()[i].window->IsKindOf(CLASSINFO(wxAuiNotebook)))
			continue;

		wxAuiNotebook * NB = (wxAuiNotebook*)m_Mgr->GetAllPanes()[i].window;
		for (u32 k = 0; k < NB->GetPageCount(); k++)
		{
			if (NB->GetPage(k)->GetId() == Id)
				return j;
		}
		j++;
	}
	return -1;
}

// Close all panes with notebooks
void CFrame::CloseAllNotebooks()
{
	wxAuiPaneInfoArray AllPanes = m_Mgr->GetAllPanes();
	for (u32 i = 0; i < AllPanes.GetCount(); i++)
	{
		if (AllPanes[i].window->IsKindOf(CLASSINFO(wxAuiNotebook)))
		{
			AllPanes[i].DestroyOnClose(true);
			m_Mgr->ClosePane(AllPanes[i]);
		}
	}
}

int CFrame::GetNotebookCount()
{
	int Ret = 0;
	for (u32 i = 0; i < m_Mgr->GetAllPanes().GetCount(); i++)
	{
		if (m_Mgr->GetAllPanes()[i].window->IsKindOf(CLASSINFO(wxAuiNotebook)))
			Ret++;
	}
	return Ret;
}

wxAuiNotebook * CFrame::GetNotebookFromId(u32 NBId)
{
	for (u32 i = 0, j = 0; i < m_Mgr->GetAllPanes().GetCount(); i++)
	{
		if (!m_Mgr->GetAllPanes()[i].window->IsKindOf(CLASSINFO(wxAuiNotebook)))
			continue;
		if (j == NBId)
			return (wxAuiNotebook*)m_Mgr->GetAllPanes()[i].window;
		j++;
	}
	return nullptr;
}
