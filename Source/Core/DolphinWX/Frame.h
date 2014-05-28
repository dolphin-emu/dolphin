// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <mutex>
#include <string>
#include <vector>
#include <wx/bitmap.h>
#include <wx/chartype.h>
#include <wx/defs.h>
#include <wx/event.h>
#include <wx/frame.h>
#include <wx/gdicmn.h>
#include <wx/image.h>
#include <wx/mstream.h>
#include <wx/panel.h>
#include <wx/string.h>
#include <wx/toplevel.h>
#include <wx/windowid.h>

#include "Common/CommonTypes.h"
#include "Common/Event.h"
#include "DolphinWX/Globals.h"
#include "InputCommon/GCPadStatus.h"

#if defined(HAVE_X11) && HAVE_X11
#include "DolphinWX/X11Utils.h"
#endif

// Class declarations
class CGameListCtrl;
class CCodeWindow;
class CLogWindow;
class FifoPlayerDlg;
class LogConfigWindow;
class NetPlaySetupDiag;
class TASInputDlg;
class wxCheatsWindow;

class wxAuiManager;
class wxAuiManagerEvent;
class wxAuiNotebook;
class wxAuiNotebookEvent;
class wxAuiToolBar;
class wxAuiToolBarEvent;
class wxListEvent;
class wxMenuItem;
class wxWindow;

// The CPanel class to receive MSWWindowProc messages from the video backend.
class CPanel : public wxPanel
{
	public:
		CPanel(
			wxWindow* parent,
			wxWindowID id = wxID_ANY
			);

	private:
		DECLARE_EVENT_TABLE();

		#ifdef _WIN32
			// Receive WndProc messages
			WXLRESULT MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam);
		#endif
};

class CRenderFrame : public wxFrame
{
	public:
		CRenderFrame(wxFrame* parent,
			wxWindowID id = wxID_ANY,
			const wxString& title = "Dolphin",
			const wxPoint& pos = wxDefaultPosition,
			const wxSize& size = wxDefaultSize,
			long style = wxDEFAULT_FRAME_STYLE | wxNO_FULL_REPAINT_ON_RESIZE);

	private:
		void OnDropFiles(wxDropFilesEvent& event);
		#ifdef _WIN32
			// Receive WndProc messages
			WXLRESULT MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam);
		#endif

};

class CFrame : public CRenderFrame
{
public:
	CFrame(wxFrame* parent,
		wxWindowID id = wxID_ANY,
		const wxString& title = "Dolphin",
		const wxPoint& pos = wxDefaultPosition,
		const wxSize& size = wxDefaultSize,
		bool _UseDebugger = false,
		bool _BatchMode = false,
		bool ShowLogWindow = false,
		long style = wxDEFAULT_FRAME_STYLE | wxNO_FULL_REPAINT_ON_RESIZE);

	virtual ~CFrame();

	void* GetRenderHandle()
	{
		#ifdef _WIN32
			return (void *)m_RenderParent->GetHandle();
		#elif defined(HAVE_X11) && HAVE_X11
			return (void *)X11Utils::XWindowFromHandle(m_RenderParent->GetHandle());
		#else
			return m_RenderParent;
		#endif
	}

	// These have to be public
	CCodeWindow* g_pCodeWindow;
	NetPlaySetupDiag* g_NetPlaySetupDiag;
	wxCheatsWindow* g_CheatsWindow;
	TASInputDlg* g_TASInputDlg[4];

	void InitBitmaps();
	void DoPause();
	void DoStop();
	void DoRecordingSave();
	void UpdateGUI();
	void UpdateGameList();
	void ToggleLogWindow(bool bShow);
	void ToggleLogConfigWindow(bool bShow);
	void PostEvent(wxCommandEvent& event);
	void StatusBarMessage(const char * Text, ...);
	void ClearStatusBar();
	void GetRenderWindowSize(int& x, int& y, int& width, int& height);
	void OnRenderWindowSizeRequest(int width, int height);
	void BootGame(const std::string& filename);
	void OnRenderParentClose(wxCloseEvent& event);
	void OnRenderParentMove(wxMoveEvent& event);
	bool RendererHasFocus();
	void DoFullscreen(bool bF);
	void ToggleDisplayMode (bool bFullscreen);
	void UpdateWiiMenuChoice(wxMenuItem *WiiMenuItem=nullptr);
	static void ConnectWiimote(int wm_idx, bool connect);

	const CGameListCtrl *GetGameListCtrl() const;

#ifdef __WXGTK__
	Common::Event panic_event;
	bool bPanicResult;
	std::recursive_mutex keystate_lock;
#endif

#if defined(HAVE_XRANDR) && HAVE_XRANDR
	X11Utils::XRRConfiguration *m_XRRConfig;
#endif

	// AUI
	wxAuiManager *m_Mgr;
	wxAuiToolBar *m_ToolBar, *m_ToolBarDebug, *m_ToolBarAui;
	bool bFloatWindow[IDM_CODEWINDOW - IDM_LOGWINDOW + 1];

	// Perspectives (Should find a way to make all of this private)
	void DoAddPage(wxWindow *Win, int i, bool Float);
	void DoRemovePage(wxWindow *, bool bHide = true);
	struct SPerspectives
	{
		std::string Name;
		wxString Perspective;
		std::vector<int> Width, Height;
	};
	std::vector<SPerspectives> Perspectives;
	u32 ActivePerspective;

private:
	CGameListCtrl* m_GameListCtrl;
	wxPanel* m_Panel;
	CRenderFrame* m_RenderFrame;
	wxPanel* m_RenderParent;
	CLogWindow* m_LogWindow;
	LogConfigWindow* m_LogConfigWindow;
	FifoPlayerDlg* m_FifoPlayerDlg;
	bool UseDebugger;
	bool m_bBatchMode;
	bool m_bEdit;
	bool m_bTabSplit;
	bool m_bNoDocking;
	bool m_bGameLoading;

	std::vector<std::string> drives;

	enum EToolbar
	{
		Toolbar_FileOpen,
		Toolbar_Refresh,
		Toolbar_Browse,
		Toolbar_Play,
		Toolbar_Stop,
		Toolbar_Pause,
		Toolbar_Screenshot,
		Toolbar_FullScreen,
		Toolbar_ConfigMain,
		Toolbar_ConfigGFX,
		Toolbar_ConfigDSP,
		Toolbar_ConfigPAD,
		Toolbar_Wiimote,
		EToolbar_Max
	};

	wxBitmap m_Bitmaps[EToolbar_Max];
	wxBitmap m_BitmapsMenu[EToolbar_Max];

	void PopulateToolbar(wxAuiToolBar* toolBar);
	void PopulateToolbarAui(wxAuiToolBar* toolBar);
	void RecreateToolbar();
	void CreateMenu();

	// Utility
	wxString GetMenuLabel(int Id);
	wxWindow * GetNotebookPageFromId(wxWindowID Id);
	wxAuiNotebook * GetNotebookFromId(u32 NBId);
	int GetNotebookCount();
	wxAuiNotebook *CreateEmptyNotebook();

	// Perspectives
	void AddRemoveBlankPage();
	void OnNotebookPageClose(wxAuiNotebookEvent& event);
	void OnAllowNotebookDnD(wxAuiNotebookEvent& event);
	void OnNotebookPageChanged(wxAuiNotebookEvent& event);
	void OnFloatWindow(wxCommandEvent& event);
	void ToggleFloatWindow(int Id);
	void OnTab(wxAuiNotebookEvent& event);
	int GetNotebookAffiliation(wxWindowID Id);
	void ClosePages();
	void CloseAllNotebooks();
	void ShowResizePane();
	void TogglePane();
	void SetPaneSize();
	void ResetToolbarStyle();
	void TogglePaneStyle(bool On, int EventId);
	void ToggleNotebookStyle(bool On, long Style);
	// Float window
	void DoUnfloatPage(int Id);
	void OnFloatingPageClosed(wxCloseEvent& event);
	void OnFloatingPageSize(wxSizeEvent& event);
	void DoFloatNotebookPage(wxWindowID Id);
	wxFrame * CreateParentFrame(wxWindowID Id = wxID_ANY,
			const wxString& title = "",
			wxWindow * = nullptr);
	wxString AuiFullscreen, AuiCurrent;
	void AddPane();
	void UpdateCurrentPerspective();
	void SaveIniPerspectives();
	void LoadIniPerspectives();
	void OnPaneClose(wxAuiManagerEvent& evt);
	void ReloadPanes();
	void DoLoadPerspective();
	void OnDropDownToolbarSelect(wxCommandEvent& event);
	void OnDropDownSettingsToolbar(wxAuiToolBarEvent& event);
	void OnDropDownToolbarItem(wxAuiToolBarEvent& event);
	void OnSelectPerspective(wxCommandEvent& event);

#ifdef _WIN32
	// Override window proc for tricks like screensaver disabling
	WXLRESULT MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam);
#endif
	// Event functions
	void OnQuit(wxCommandEvent& event);
	void OnHelp(wxCommandEvent& event);
	void OnToolBar(wxCommandEvent& event);
	void OnAuiToolBar(wxAuiToolBarEvent& event);

	void OnOpen(wxCommandEvent& event); // File menu
	void DoOpen(bool Boot);
	void OnRefresh(wxCommandEvent& event);
	void OnBrowse(wxCommandEvent& event);
	void OnBootDrive(wxCommandEvent& event);

	void OnPlay(wxCommandEvent& event); // Emulation
	void OnStop(wxCommandEvent& event);
	void OnReset(wxCommandEvent& event);
	void OnRecord(wxCommandEvent& event);
	void OnPlayRecording(wxCommandEvent& event);
	void OnRecordExport(wxCommandEvent& event);
	void OnRecordReadOnly(wxCommandEvent& event);
	void OnTASInput(wxCommandEvent& event);
	void OnTogglePauseMovie(wxCommandEvent& event);
	void OnShowLag(wxCommandEvent& event);
	void OnChangeDisc(wxCommandEvent& event);
	void OnScreenshot(wxCommandEvent& event);
	void OnActive(wxActivateEvent& event);
	void OnClose(wxCloseEvent &event);
	void OnLoadState(wxCommandEvent& event);
	void OnSaveState(wxCommandEvent& event);
	void OnLoadStateFromFile(wxCommandEvent& event);
	void OnSaveStateToFile(wxCommandEvent& event);
	void OnLoadLastState(wxCommandEvent& event);
	void OnSaveFirstState(wxCommandEvent& event);
	void OnUndoLoadState(wxCommandEvent& event);
	void OnUndoSaveState(wxCommandEvent& event);

	void OnFrameSkip(wxCommandEvent& event);
	void OnFrameStep(wxCommandEvent& event);

	void OnConfigMain(wxCommandEvent& event); // Options
	void OnConfigGFX(wxCommandEvent& event);
	void OnConfigDSP(wxCommandEvent& event);
	void OnConfigPAD(wxCommandEvent& event);
	void OnConfigWiimote(wxCommandEvent& event);
	void OnConfigHotkey(wxCommandEvent& event);

	void OnToggleFullscreen(wxCommandEvent& event);
	void OnToggleDualCore(wxCommandEvent& event);
	void OnToggleSkipIdle(wxCommandEvent& event);
	void OnToggleThrottle(wxCommandEvent& event);
	void OnManagerResize(wxAuiManagerEvent& event);
	void OnMove(wxMoveEvent& event);
	void OnResize(wxSizeEvent& event);
	void OnToggleToolbar(wxCommandEvent& event);
	void DoToggleToolbar(bool);
	void OnToggleStatusbar(wxCommandEvent& event);
	void OnToggleWindow(wxCommandEvent& event);

	void OnKeyDown(wxKeyEvent& event); // Keyboard
	void OnKeyUp(wxKeyEvent& event);

	void OnMouse(wxMouseEvent& event); // Mouse

	void OnHostMessage(wxCommandEvent& event);

	void OnMemcard(wxCommandEvent& event); // Misc
	void OnImportSave(wxCommandEvent& event);
	void OnExportAllSaves(wxCommandEvent& event);

	void OnNetPlay(wxCommandEvent& event);

	void OnShow_CheatsWindow(wxCommandEvent& event);
	void OnLoadWiiMenu(wxCommandEvent& event);
	void OnInstallWAD(wxCommandEvent& event);
	void OnFifoPlayer(wxCommandEvent& event);
	void OnConnectWiimote(wxCommandEvent& event);
	void GameListChanged(wxCommandEvent& event);

	void OnGameListCtrl_ItemActivated(wxListEvent& event);
	void OnRenderParentResize(wxSizeEvent& event);
	bool RendererIsFullscreen();
	void StartGame(const std::string& filename);
	void OnChangeColumnsVisible(wxCommandEvent& event);

	// Event table
	DECLARE_EVENT_TABLE();
};

int GetCmdForHotkey(unsigned int key);

void OnAfterLoadCallback();

// For TASInputDlg
void TASManipFunction(SPADStatus *PadStatus, int controllerID);
bool TASInputHasFocus();
