// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>
#include <wx/dialog.h>
#include <wx/grid.h>
#include <wx/sizer.h>
#include <wx/treectrl.h>

#include "Common/OnionConfig.h"

class ConfigTable;
class SystemTreeCtrl;
class TreeItemData;

class ConfigGrid : public wxGrid
{
public:
	ConfigGrid(wxWindow* parent, SystemTreeCtrl* tree_ctrl, wxWindowID id);

	ConfigTable* GetGridTable() { return m_config_table.get(); }
	void FilterTable(TreeItemData* const filter);

private:
	std::unique_ptr<ConfigTable> m_config_table;
	SystemTreeCtrl* m_systems_ctrl;
};

class AdvancedConfigWindow : public wxDialog
{
public:
	AdvancedConfigWindow(wxWindow* const parent);

private:
	ConfigGrid* m_config_grid;
	SystemTreeCtrl* m_systems_ctrl;

	void SystemSelectionChanged(wxTreeEvent& event);
};
