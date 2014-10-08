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
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"
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
class wxListEvent;
class wxMenuItem;
class wxWindow;

class CRenderFrame : public wxFrame
{
	public:
		CRenderFrame(wxFrame* parent,
			wxWindowID id = wxID_ANY,
			const wxString& title = "Dolphin",
			const wxPoint& pos = wxDefaultPosition,
			const wxSize& size = wxDefaultSize,
			long style = wxDEFAULT_FRAME_STYLE | wxNO_FULL_REPAINT_ON_RESIZE);

		bool ShowFullScreen(bool show, long style = wxFULLSCREEN_ALL) override;

	private:
		void OnDropFiles(wxDropFilesEvent& event);
		static bool IsValidSavestateDropped(const std::string& filepath);
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
		#if defined(HAVE_X11) && HAVE_X11
			return reinterpret_cast<void*>(X11Utils::XWindowFromHandle(m_RenderParent->GetHandle()));
		#else
			return reinterpret_cast<void*>(m_RenderParent->GetHandle());
		#endif
	}

	// These have to be public
	CCodeWindow* g_pCodeWindow;
	NetPlaySetupDiag* g_NetPlaySetupDiag;
	wxCheatsWindow* g_CheatsWindow;
	TASInputDlg* g_TASInputDlg[8];

	void InitBitmaps();
	void DoPause();
	void DoStop();
	void OnStopped();
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
	bool UIHasFocus();
	void DoFullscreen(bool bF);
	void ToggleDisplayMode (bool bFullscreen);
	void UpdateWiiMenuChoice(wxMenuItem *WiiMenuItem=nullptr);
	void PopulateSavedPerspectives();
	static void ConnectWiimote(int wm_idx, bool connect);
	void UpdateTitle(const std::string &str);

	const CGameListCtrl *GetGameListCtrl() const;
	virtual wxMenuBar* GetMenuBar() const override;

#ifdef __WXGTK__
	Common::Event panic_event;
	bool bPanicResult;
	std::recursive_mutex keystate_lock;
#endif

#if defined(HAVE_XRANDR) && HAVE_XRANDR
	X11Utils::XRRConfiguration *m_XRRConfig;
#endif

	wxMenu* m_SavedPerspectives;

	wxToolBar *m_ToolBar;
	// AUI
	wxAuiManager *m_Mgr;
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
	wxWindow* m_RenderParent;
	CLogWindow* m_LogWindow;
	LogConfigWindow* m_LogConfigWindow;
	FifoPlayerDlg* m_FifoPlayerDlg;
	bool UseDebugger;
	bool m_bBatchMode;
	bool m_bEdit;
	bool m_bTabSplit;
	bool m_bNoDocking;
	bool m_bGameLoading;
	bool m_bClosing;
	bool m_confirmStop;

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

	wxMenuBar* m_menubar_shadow;

	void PopulateToolbar(wxToolBar* toolBar);
	void RecreateToolbar();
	wxMenuBar* CreateMenu();

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
	void OnPerspectiveMenu(wxCommandEvent& event);
	void OnSelectPerspective(wxCommandEvent& event);

#ifdef _WIN32
	// Override window proc for tricks like screensaver disabling
	WXLRESULT MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam);
#endif
	// Event functions
	void OnQuit(wxCommandEvent& event);
	void OnHelp(wxCommandEvent& event);

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
	void OnShowFrameCount(wxCommandEvent& event);
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

	void OnSelectSlot(wxCommandEvent& event);
	void OnSaveCurrentSlot(wxCommandEvent& event);
	void OnLoadCurrentSlot(wxCommandEvent& event);

	// Event table
	DECLARE_EVENT_TABLE();
};

int GetCmdForHotkey(unsigned int key);

void OnAfterLoadCallback();
void OnStoppedCallback();

// For TASInputDlg
void GCTASManipFunction(GCPadStatus* PadStatus, int controllerID);
void WiiTASManipFunction(u8* data, WiimoteEmu::ReportFeatures rptf, int controllerID);
bool TASInputHasFocus();
extern int g_saveSlot;
