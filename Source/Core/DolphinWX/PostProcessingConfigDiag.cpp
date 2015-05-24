// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <math.h>
#include <unordered_map>

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/slider.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include "Common/StringUtil.h"

#include "DolphinWX/PostProcessingConfigDiag.h"

#include "VideoCommon/RenderBase.h"

PostProcessingConfigDiag::PostProcessingConfigDiag(wxWindow* parent, const std::string& shader)
	: wxDialog(parent, wxID_ANY,
		wxString::Format(_("Post Processing Shader Configuration"))),
	  m_shader(shader)
{
	// Depending on if we are running already, either use the one from the videobackend
	// or generate our own.
	if (g_renderer && g_renderer->GetPostProcessor())
	{
		m_post_processor = g_renderer->GetPostProcessor()->GetConfig();
	}
	else
	{
		m_post_processor = new PostProcessingShaderConfiguration();
		m_post_processor->LoadShader(m_shader);
	}

	// Create our UI classes
	const PostProcessingShaderConfiguration::ConfigMap& config_map = m_post_processor->GetOptions();
	for (const auto& it : config_map)
	{
		if (it.second.m_type == PostProcessingShaderConfiguration::ConfigurationOption::OptionType::OPTION_BOOL)
		{
			ConfigGrouping* group = new ConfigGrouping(ConfigGrouping::WidgetType::TYPE_TOGGLE,
				it.second.m_gui_name, it.first, it.second.m_dependent_option,
				&it.second);
			m_config_map[it.first] = group;
		}
		else
		{
			ConfigGrouping* group = new ConfigGrouping(ConfigGrouping::WidgetType::TYPE_SLIDER,
				it.second.m_gui_name, it.first, it.second.m_dependent_option,
				&it.second);
			m_config_map[it.first] = group;
		}
	}

	// Arrange our vectors based on dependency
	for (const auto& it : m_config_map)
	{
		const std::string parent_name = it.second->GetParent();
		if (parent_name.size())
		{
			// Since it depends on a different object, push it to a parent's object
			m_config_map[parent_name]->AddChild(m_config_map[it.first]);
		}
		else
		{
			// It doesn't have a child, just push it to the vector
			m_config_groups.push_back(m_config_map[it.first]);
		}
	}

	// Generate our UI
	wxNotebook* const notebook = new wxNotebook(this, wxID_ANY);
	wxPanel* const page_general = new wxPanel(notebook);
	wxFlexGridSizer* const szr_general = new wxFlexGridSizer(2, 5, 5);

	// Now let's actually populate our window with our information
	bool add_general_page = false;
	for (const auto& it : m_config_groups)
	{
		if (it->HasChildren())
		{
			// Options with children get their own tab
			wxPanel* const page_option = new wxPanel(notebook);
			wxFlexGridSizer* const szr_option = new wxFlexGridSizer(2, 10, 5);
			it->GenerateUI(this, page_option, szr_option);

			// Add all the children
			for (const auto& child : it->GetChildren())
			{
				child->GenerateUI(this, page_option, szr_option);
			}
			page_option->SetSizerAndFit(szr_option);
			notebook->AddPage(page_option, _(it->GetGUIName()));
		}
		else
		{
			// Options with no children go in to the general tab
			if (!add_general_page)
			{
				// Make it so it doesn't show up if there aren't any options without children.
				add_general_page = true;
			}
			it->GenerateUI(this, page_general, szr_general);
		}
	}

	if (add_general_page)
	{
		page_general->SetSizerAndFit(szr_general);
		notebook->InsertPage(0, page_general, _("General"));
	}

	// Close Button
	wxButton* const btn_close = new wxButton(this, wxID_OK, _("Close"));
	btn_close->Bind(wxEVT_BUTTON, &PostProcessingConfigDiag::Event_ClickClose, this);

	Bind(wxEVT_CLOSE_WINDOW, &PostProcessingConfigDiag::Event_Close, this);

	wxBoxSizer* const szr_main = new wxBoxSizer(wxVERTICAL);
	szr_main->Add(notebook, 1, wxEXPAND | wxALL, 5);
	szr_main->Add(btn_close, 0, wxALIGN_RIGHT | wxRIGHT | wxBOTTOM, 5);

	SetSizerAndFit(szr_main);
	Center();
	SetFocus();

	UpdateWindowUI();
}

PostProcessingConfigDiag::~PostProcessingConfigDiag()
{
	m_post_processor->SaveOptionsConfiguration();
	if (g_renderer && g_renderer->GetPostProcessor())
		m_post_processor = nullptr;
	else
		delete m_post_processor;
}

void PostProcessingConfigDiag::ConfigGrouping::GenerateUI(PostProcessingConfigDiag* dialog, wxWindow* parent, wxFlexGridSizer* sizer)
{
	if (m_type == WidgetType::TYPE_TOGGLE)
	{
		m_option_checkbox = new wxCheckBox(parent, wxID_ANY, _(m_gui_name));
		m_option_checkbox->SetValue(m_config_option->m_bool_value);
		m_option_checkbox->Bind(wxEVT_CHECKBOX, &PostProcessingConfigDiag::Event_CheckBox,
		                        dialog, wxID_ANY, wxID_ANY, new UserEventData(m_option));

		sizer->Add(m_option_checkbox);
		sizer->AddStretchSpacer();
	}
	else
	{
		size_t vector_size = 0;
		if (m_config_option->m_type == PostProcessingShaderConfiguration::ConfigurationOption::OptionType::OPTION_INTEGER)
			vector_size = m_config_option->m_integer_values.size();
		else
			vector_size = m_config_option->m_float_values.size();

		wxFlexGridSizer* const szr_values = new wxFlexGridSizer(vector_size + 1, 0, 0);
		wxStaticText* const option_static_text = new wxStaticText(parent, wxID_ANY, _(m_gui_name));
		sizer->Add(option_static_text);

		for (size_t i = 0; i < vector_size; ++i)
		{
			// wxSlider uses a signed integer for it's minimum and maximum values
			// This won't work for floats.
			// Let's determine how many steps we can take and use that instead.
			int steps = 0;
			int current_value = 0;
			std::string string_value;
			if (m_config_option->m_type == PostProcessingShaderConfiguration::ConfigurationOption::OptionType::OPTION_INTEGER)
			{
				// Find out our range by taking the max subtracting the minimum.
				double range = m_config_option->m_integer_max_values[i] - m_config_option->m_integer_min_values[i];

				// How many steps we have is the range divided by the step interval configured.
				// This may not be 100% spot on accurate since developers can have odd stepping intervals set.
				// Round up so if it is outside our range, then set it to the minimum or maximum
				steps = ceil(range / (double)m_config_option->m_integer_step_values[i]);

				// Default value is just the currently set value here
				current_value = m_config_option->m_integer_values[i];
				string_value = std::to_string(m_config_option->m_integer_values[i]);
			}
			else
			{
				// Same as above but with floats
				float range = m_config_option->m_float_max_values[i] - m_config_option->m_float_min_values[i];
				steps = ceil(range / m_config_option->m_float_step_values[i]);

				// We need to convert our default float value from a float to the nearest step value range
				current_value = (m_config_option->m_float_values[i] / m_config_option->m_float_step_values[i]);
				string_value = std::to_string(m_config_option->m_float_values[i]);
			}

			wxSlider* slider = new wxSlider(parent, wxID_ANY, current_value, 0, steps,
			                                wxDefaultPosition, wxSize(200, -1), wxSL_HORIZONTAL | wxSL_BOTTOM);
			wxTextCtrl* text_ctrl = new wxTextCtrl(parent, wxID_ANY, string_value);

			// Disable the textctrl, it's only there to show the absolute value from the slider
			text_ctrl->Disable();

			// wxWidget takes over the pointer provided to it in the event handler.
			// This won't be a memory leak, it'll be destroyed on dialog close.
			slider->Bind(wxEVT_SLIDER, &PostProcessingConfigDiag::Event_Slider,
					 dialog, wxID_ANY, wxID_ANY, new UserEventData(m_option));

			m_option_sliders.push_back(slider);
			m_option_text_ctrls.push_back(text_ctrl);
		}

		if (vector_size == 1)
		{
			szr_values->Add(m_option_sliders[0], wxSizerFlags().Align(wxALIGN_CENTER_VERTICAL));
			szr_values->Add(m_option_text_ctrls[0]);

			sizer->Add(szr_values);
		}
		else
		{
			wxFlexGridSizer* const szr_inside = new wxFlexGridSizer(2, 0, 0);
			for (size_t i = 0; i < vector_size; ++i)
			{
				szr_inside->Add(m_option_sliders[i], wxSizerFlags().Align(wxALIGN_CENTER_VERTICAL));
				szr_inside->Add(m_option_text_ctrls[i]);
			}

			szr_values->Add(szr_inside);
			sizer->Add(szr_values);
		}
	}
}

void PostProcessingConfigDiag::ConfigGrouping::EnableDependentChildren(bool enable)
{
	// Enable or disable all the children
	for (auto& it : m_children)
	{
		if (it->m_type == ConfigGrouping::WidgetType::TYPE_TOGGLE)
		{
			it->m_option_checkbox->Enable(enable);
		}
		else
		{
			for (auto& slider : it->m_option_sliders)
				slider->Enable(enable);
		}
		// Set this objects children as well
		it->EnableDependentChildren(enable);
	}
}

void PostProcessingConfigDiag::Event_CheckBox(wxCommandEvent &ev)
{
	UserEventData* config_option = (UserEventData*)ev.GetEventUserData();
	ConfigGrouping* config = m_config_map[config_option->GetData()];

	m_post_processor->SetOptionb(config->GetOption(), ev.IsChecked());

	config->EnableDependentChildren(ev.IsChecked());

	ev.Skip();
}

void PostProcessingConfigDiag::Event_Slider(wxCommandEvent &ev)
{
	UserEventData* config_option = (UserEventData*)ev.GetEventUserData();
	ConfigGrouping* config = m_config_map[config_option->GetData()];

	const auto& option_data = m_post_processor->GetOption(config->GetOption());

	size_t vector_size = 0;
	if (option_data.m_type == PostProcessingShaderConfiguration::ConfigurationOption::OptionType::OPTION_INTEGER)
		vector_size = option_data.m_integer_values.size();
	else
		vector_size = option_data.m_float_values.size();

	for (size_t i = 0; i < vector_size; ++i)
	{
		// Got to do this garbage again.
		// Convert current step in to the real range value
		int current_step = config->GetSliderValue(i);
		std::string string_value;
		if (option_data.m_type == PostProcessingShaderConfiguration::ConfigurationOption::OptionType::OPTION_INTEGER)
		{
			s32 value = option_data.m_integer_step_values[i] * current_step + option_data.m_integer_min_values[i];
			m_post_processor->SetOptioni(config->GetOption(), i, value);
			string_value = std::to_string(value);
		}
		else
		{
			float value = option_data.m_float_step_values[i] * current_step + option_data.m_float_min_values[i];
			m_post_processor->SetOptionf(config->GetOption(), i, value);
			string_value = std::to_string(value);
		}
		// Update the text box to include the new value
		config->SetSliderText(i, string_value);
	}
	ev.Skip();
}

void PostProcessingConfigDiag::Event_ClickClose(wxCommandEvent&)
{
	Close();
}

void PostProcessingConfigDiag::Event_Close(wxCloseEvent& ev)
{
	EndModal(wxID_OK);
}

