// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstddef>

#include <wx/bitmap.h>
#include <wx/panel.h>
#include <wx/aui/auibar.h>

#include "Common/FileUtil.h"
#include "Common/OnionConfig.h"

#include "Core/ConfigManager.h"
#include "Core/OnionCoreLoaders/GameConfigLoader.h"
#include "Core/PowerPC/PowerPC.h"

#include "DolphinWX/WxUtils.h"
#include "DolphinWX/Debugger/WatchView.h"
#include "DolphinWX/Debugger/WatchWindow.h"

class CWatchToolbar : public wxAuiToolBar
{
public:
CWatchToolbar(CWatchWindow* parent, const wxWindowID id)
	: wxAuiToolBar(parent, id, wxDefaultPosition, wxDefaultSize,
			wxAUI_TB_DEFAULT_STYLE | wxAUI_TB_TEXT)
{
	SetToolBitmapSize(wxSize(16, 16));

	m_Bitmaps[Toolbar_File] = WxUtils::LoadResourceBitmap("toolbar_debugger_delete");

	AddTool(ID_LOAD, _("Load"), m_Bitmaps[Toolbar_File]);
	Bind(wxEVT_TOOL, &CWatchWindow::Event_LoadAll, parent, ID_LOAD);

	AddTool(ID_SAVE, _("Save"), m_Bitmaps[Toolbar_File]);
	Bind(wxEVT_TOOL, &CWatchWindow::Event_SaveAll, parent, ID_SAVE);
}

private:

	enum
	{
		Toolbar_File,
		Num_Bitmaps
	};

	enum
	{
		ID_LOAD,
		ID_SAVE
	};

	wxBitmap m_Bitmaps[Num_Bitmaps];
};

CWatchWindow::CWatchWindow(wxWindow* parent, wxWindowID id,
		const wxPoint& position, const wxSize& size,
		long style, const wxString& name)
	: wxPanel(parent, id, position, size, style, name)
	, m_GPRGridView(nullptr)
{
	m_mgr.SetManagedWindow(this);
	m_mgr.SetFlags(wxAUI_MGR_DEFAULT | wxAUI_MGR_LIVE_RESIZE);

	m_GPRGridView = new CWatchView(this);

	m_mgr.AddPane(new CWatchToolbar(this, wxID_ANY), wxAuiPaneInfo().ToolbarPane().Top().
		LeftDockable(true).RightDockable(true).BottomDockable(false).Floatable(false));
	m_mgr.AddPane(m_GPRGridView, wxAuiPaneInfo().CenterPane());
	m_mgr.Update();
}

CWatchWindow::~CWatchWindow()
{
	m_mgr.UnInit();
}

void CWatchWindow::NotifyUpdate()
{
	if (m_GPRGridView != nullptr)
		m_GPRGridView->Update();
}

void CWatchWindow::Event_SaveAll(wxCommandEvent& WXUNUSED(event))
{
	SaveAll();
}

void CWatchWindow::SaveAll()
{
	std::unique_ptr<OnionConfig::BloomLayer> game_layer(new OnionConfig::BloomLayer(std::unique_ptr<OnionConfig::ConfigLayerLoader>(GenerateLocalGameConfigLoader(SConfig::GetInstance().GetUniqueID(), 0))));

	OnionConfig::OnionPetal* watches = game_layer->GetOrCreatePetal(OnionConfig::OnionSystem::SYSTEM_DEBUGGER, "Watches");

	watches->SetLines(PowerPC::watches.GetStrings());
	game_layer->Save();
}

void CWatchWindow::Event_LoadAll(wxCommandEvent& WXUNUSED(event))
{
	LoadAll();
}

void CWatchWindow::LoadAll()
{
	Watches::TWatchesStr watches;
	OnionConfig::OnionPetal* watches_config = OnionConfig::GetOrCreatePetal(OnionConfig::OnionSystem::SYSTEM_DEBUGGER, "Watches");

	if (watches_config->GetLines(&watches, false))
	{
		PowerPC::watches.Clear();
		PowerPC::watches.AddFromStrings(watches);
	}

	NotifyUpdate();
}
