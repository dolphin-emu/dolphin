// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <unordered_map>

#include "Common/OnionConfig.h"
#include "Common/StringUtil.h"

#include "DolphinWX/AdvancedConfigurationWindow.h"

class TreeItemData : public wxTreeItemData
{
public:
	enum class ItemType
	{
		TYPE_ROOT,
		TYPE_SYSTEM,
		TYPE_PETAL,
	};

	TreeItemData()
		: m_type(ItemType::TYPE_ROOT) {}

	TreeItemData(OnionConfig::OnionSystem system)
		: m_type(ItemType::TYPE_SYSTEM), m_system(system) {}

	TreeItemData(OnionConfig::OnionSystem system, const std::string& name)
		: m_type(ItemType::TYPE_PETAL), m_system(system), m_name(name) {}

	TreeItemData& operator=(const TreeItemData& b)
	{
		m_type = b.m_type;
		m_system = b.m_system;
		m_name = b.m_name;
		return *this;
	}

	bool ShouldFilter(OnionConfig::OnionSystem system)
	{
		if (m_type == ItemType::TYPE_ROOT)
			return false;
		return m_type == ItemType::TYPE_SYSTEM && m_system != system;
	}

	bool ShouldFilter(OnionConfig::OnionSystem system, const std::string& name)
	{
		return ShouldFilter(system) ||
		       (m_type == ItemType::TYPE_PETAL &&
		        !(m_system == system &&
			    m_name == name));
	}

	ItemType GetType() { return m_type; }
	OnionConfig::OnionSystem GetSystem() { return m_system; }
	const std::string& GetPetal() { return m_name; }

private:
	TreeItemData(ItemType type, OnionConfig::OnionSystem system, const std::string& name)
		: m_type(type), m_system(system), m_name(name) {}

	ItemType m_type;
	OnionConfig::OnionSystem m_system;
	std::string m_name;
};

class SystemTreeCtrl : public wxTreeCtrl
{
public:
	SystemTreeCtrl(wxWindow* parent, wxWindowID id)
		: wxTreeCtrl(parent, id)
	{ Clear(); }

	void AddPetal(OnionConfig::OnionSystem system, const std::string& petal);
	void Clear();

	void Select(OnionConfig::OnionSystem system, const std::string& petal);

private:
	std::map<OnionConfig::OnionSystem, std::pair<wxTreeItemId, std::list<wxTreeItemId>>> m_system_ids;
};

void SystemTreeCtrl::Clear()
{
	m_system_ids.clear();
	DeleteAllItems();

	static std::map<OnionConfig::OnionSystem, std::string> system_to_name =
	{
		{ OnionConfig::OnionSystem::SYSTEM_MAIN,     "Dolphin" },
		{ OnionConfig::OnionSystem::SYSTEM_GCPAD,    "GCPad" },
		{ OnionConfig::OnionSystem::SYSTEM_WIIPAD,   "Wiimote" },
		{ OnionConfig::OnionSystem::SYSTEM_GFX,      "Graphics" },
		{ OnionConfig::OnionSystem::SYSTEM_LOGGER,   "Logger" },
		{ OnionConfig::OnionSystem::SYSTEM_DEBUGGER, "Debugger" },
		{ OnionConfig::OnionSystem::SYSTEM_UI,       "UI" },
	};

	wxTreeItemId root = AddRoot("System", -1, -1,
		new TreeItemData());
	for (auto& system : system_to_name)
		m_system_ids[system.first] =
		{
			AppendItem(root, system.second, -1, -1,
				new TreeItemData(system.first)),
			{}
		};
	Expand(root);
}

void SystemTreeCtrl::AddPetal(OnionConfig::OnionSystem system, const std::string& petal)
{
	auto& system_pair = m_system_ids[system];
	wxTreeItemId system_id = system_pair.first;

	// First let's make sure that petal doesn't already exist
	for (const auto& petal_id : system_pair.second)
	{
		TreeItemData* const data = reinterpret_cast<TreeItemData* const>(GetItemData(petal_id));
		if (data->GetPetal() == petal)
			return;
	}

	wxTreeItemId key_id = AppendItem(system_id, petal, -1, -1,
		new TreeItemData(system, petal));
	Expand(system_id);

	system_pair.second.emplace_back(key_id);
}

class ConfigTable : public wxGridTableBase
{
public:
	ConfigTable(SystemTreeCtrl* tree_ctrl)
		: m_systems_ctrl(tree_ctrl)
	{}
	void RefreshConfig();

	int GetNumberCols() override;
	int GetNumberRows() override;
	wxString GetValue(int row, int col) override;
	void SetValue(int row, int col, const wxString &) override {};
	wxString GetRowLabelValue(int row);
	wxString GetColLabelValue(int col);

	// Filtering setting
	void SetFilter(TreeItemData* const filter) {m_filter = *filter; }

private:
	u32 m_cols = 0;
	u32 m_rows = 0;

	// Filtering
	TreeItemData m_filter;

	std::vector<OnionConfig::OnionLayerType> m_layers;
	std::map<const std::string, int> m_column_names;
	std::map<OnionConfig::OnionLayerType, std::map<const std::string, std::string>> m_config;

	SystemTreeCtrl* m_systems_ctrl;
};

void ConfigTable::RefreshConfig()
{
	m_layers.clear();
	m_column_names.clear();
	m_config.clear();
	Clear();

	OnionConfig::OnionBloom* full_bloom = OnionConfig::GetFullBloom();

	// The number of columns will always be the number of layers in the bloom
	m_rows = 0;
	m_cols = full_bloom->size();
	for (auto layer : *full_bloom)
	{
		OnionConfig::BloomLayerMap cur_layer_map = layer.second->GetBloomLayerMap();
		m_layers.emplace_back(layer.second->GetLayer());
		for (auto system : cur_layer_map)
		{
			OnionConfig::OnionSystem system_type = system.first;
			std::list<OnionConfig::OnionPetal*>& petals = system.second;

			for (const auto& petal : petals)
			{
				// We always want to add all petals to the tree list
				m_systems_ctrl->AddPetal(system.first, petal->GetName());

				// If we are filtering by petal and not in the right petal
				// then skip this configuration setting.
				if (m_filter.ShouldFilter(system_type, petal->GetName()))
					continue;

				const OnionConfig::PetalValueMap& petal_map = petal->GetValues();
				auto& config_layer = m_config[layer.second->GetLayer()];
				for (const auto& value : petal_map)
				{
					const std::string& value_key = value.first;
					config_layer[petal->GetName()] = value.first;

					const auto& value_loc = m_column_names.find(value_key);
					if (value_loc == m_column_names.end())
					{
						std::ostringstream col_name;
						switch (m_filter.GetType())
						{
						// Intentional fall-through
						case TreeItemData::ItemType::TYPE_ROOT:
							col_name << OnionConfig::GetSystemName(system.first) << "/";
						case TreeItemData::ItemType::TYPE_SYSTEM:
							col_name << petal->GetName() << "/";
						case TreeItemData::ItemType::TYPE_PETAL:
							break;
						};
						col_name << value_key;
						m_column_names[col_name.str()] = m_column_names.size();
					}
				}
			}
		}
	}

	m_rows = m_column_names.size();
}

int ConfigTable::GetNumberCols()
{
	return m_cols;
}

int ConfigTable::GetNumberRows()
{
	return m_rows;
}

wxString ConfigTable::GetValue(int row, int col)
{
	// Row = layer
	const std::string& col_name = std::next(m_column_names.begin(), row)->first;
	std::istringstream buffer(col_name);

	OnionConfig::OnionSystem system;
	std::string col_petal, col_key;

	if (m_filter.GetType() == TreeItemData::ItemType::TYPE_ROOT)
	{
		// Column = System/Petal/Key
		std::string col_system;
		std::getline(buffer, col_system, '/');
		std::getline(buffer, col_petal, '/');
		std::getline(buffer, col_key, '/');
		system = OnionConfig::GetSystemFromName(col_system);
	}
	else if (m_filter.GetType() == TreeItemData::ItemType::TYPE_SYSTEM)
	{
		// Column = Petal/Key
		system = m_filter.GetSystem();
		std::getline(buffer, col_petal, '/');
		std::getline(buffer, col_key, '/');
	}
	else if (m_filter.GetType() == TreeItemData::ItemType::TYPE_PETAL)
	{
		// Column = Key
		// If we are filtered by petal than the key is always just a petal key
		system = m_filter.GetSystem();
		col_petal = m_filter.GetPetal();
		col_key = col_name;
	}
	else
	{
		return "#ERR";
	}

	OnionConfig::BloomLayer* layer = OnionConfig::GetLayer(m_layers[col]);
	OnionConfig::OnionPetal* petal = layer->GetPetal(system, col_petal);

	std::string val = "";
	if (petal)
		petal->Get(col_key, &val);
	return val;
}

wxString ConfigTable::GetRowLabelValue(int row)
{
	return std::next(m_column_names.begin(), row)->first;
}

wxString ConfigTable::GetColLabelValue(int col)
{
	return OnionConfig::GetLayerName(m_layers[col]);
}

ConfigGrid::ConfigGrid(wxWindow* parent, SystemTreeCtrl* tree_ctrl, wxWindowID id)
	: wxGrid(parent, id),
	  m_config_table(new ConfigTable(tree_ctrl)),
	  m_systems_ctrl(tree_ctrl)
{
	TreeItemData root_filter;
	FilterTable(&root_filter);
}

void ConfigGrid::FilterTable(TreeItemData* const filter)
{
	m_config_table->SetFilter(filter);
	m_config_table->RefreshConfig();
	SetTable(GetGridTable(), false);

	DisableDragRowSize();
	SetRowLabelSize(wxGRID_AUTOSIZE);

	ClearGrid();
	ForceRefresh();
}

void AdvancedConfigWindow::SystemSelectionChanged(wxTreeEvent& event)
{
	wxTreeItemId new_item = event.GetItem();
	TreeItemData* const new_data = reinterpret_cast<TreeItemData* const>(m_systems_ctrl->GetItemData(new_item));

	m_config_grid->FilterTable(new_data);
}

AdvancedConfigWindow::AdvancedConfigWindow(wxWindow* const parent)
	: wxDialog(parent, wxID_ANY, _("Advanced Configuration"), wxPoint(128,-1))
{
	wxBoxSizer* const szr = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer* const horizontal_szr = new wxBoxSizer(wxHORIZONTAL);

	{
		wxBoxSizer* const tree_szr = new wxBoxSizer(wxVERTICAL);
		m_systems_ctrl = new SystemTreeCtrl(this, wxID_ANY);
		m_systems_ctrl->Bind(wxEVT_TREE_SEL_CHANGED, &AdvancedConfigWindow::SystemSelectionChanged, this);

		tree_szr->Add(m_systems_ctrl, 0, wxEXPAND, 0);
		tree_szr->SetMinSize(200, -1);
		horizontal_szr->Add(tree_szr, 0, wxEXPAND, 0);
	}

	{
		wxBoxSizer* const grid_szr = new wxBoxSizer(wxVERTICAL);
		m_config_grid = new ConfigGrid(this, m_systems_ctrl, wxID_ANY);

		grid_szr->Add(m_config_grid, 0, wxEXPAND, 0);
		grid_szr->SetMinSize(512, -1);
		horizontal_szr->Add(grid_szr, 0, wxEXPAND, 0);
	}

	szr->Add(horizontal_szr, 0, wxEXPAND, 0);
	szr->Add(CreateButtonSizer(wxOK), 0, wxEXPAND|wxALL, 0);

	SetMaxSize(wxSize(-1, 768));
	SetLayoutAdaptationMode(wxDIALOG_ADAPTATION_MODE_ENABLED);
	SetSizerAndFit(szr);
	Center();
}
