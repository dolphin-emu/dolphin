// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <wx/app.h>
#include <wx/arrstr.h>
#include <wx/bitmap.h>
#include <wx/button.h>
#include <wx/chartype.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/combobox.h>
#include <wx/control.h>
#include <wx/dcmemory.h>
#include <wx/defs.h>
#include <wx/dialog.h>
#include <wx/event.h>
#include <wx/font.h>
#include <wx/gdicmn.h>
#include <wx/listbox.h>
#include <wx/msgdlg.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/slider.h>
#include <wx/spinctrl.h>
#include <wx/statbmp.h>
#include <wx/stattext.h>
#include <wx/string.h>
#include <wx/textctrl.h>
#include <wx/timer.h>
#include <wx/toplevel.h>
#include <wx/translation.h>
#include <wx/unichar.h>

#include "Common/FileSearch.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/MsgHandler.h"
#include "Core/HW/Wiimote.h"
#include "DolphinWX/InputConfigDiag.h"
#include "DolphinWX/WxUtils.h"
#include "InputCommon/ControllerEmu.h"
#include "InputCommon/InputConfig.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/ControllerInterface/Device.h"
#include "InputCommon/ControllerInterface/ExpressionParser.h"

using namespace ciface::ExpressionParser;

void GamepadPage::ConfigExtension(wxCommandEvent& event)
{
	ControllerEmu::Extension* const ex = ((ExtensionButton*)event.GetEventObject())->extension;

	// show config diag, if "none" isn't selected
	if (ex->switch_extension)
	{
		wxDialog dlg(this, -1,
			wxGetTranslation(StrToWxStr(ex->attachments[ex->switch_extension]->GetName())));

		wxBoxSizer* const main_szr = new wxBoxSizer(wxVERTICAL);
		const std::size_t orig_size = control_groups.size();

		ControlGroupsSizer* const szr = new ControlGroupsSizer(ex->attachments[ex->switch_extension].get(), &dlg, this, &control_groups);
		main_szr->Add(szr, 0, wxLEFT, 5);
		main_szr->Add(dlg.CreateButtonSizer(wxOK), 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
		dlg.SetSizerAndFit(main_szr);
		dlg.Center();

		dlg.ShowModal();

		// remove the new groups that were just added, now that the window closed
		control_groups.resize(orig_size);
	}
}

PadSettingExtension::PadSettingExtension(wxWindow* const parent, ControllerEmu::Extension* const ext)
	: PadSetting(new wxChoice(parent, -1))
	, extension(ext)
{
	for (auto& attachment : extension->attachments)
	{
		((wxChoice*)wxcontrol)->Append(wxGetTranslation(StrToWxStr(attachment->GetName())));
	}

	UpdateGUI();
}

void PadSettingExtension::UpdateGUI()
{
	((wxChoice*)wxcontrol)->Select(extension->switch_extension);
}

void PadSettingExtension::UpdateValue()
{
	extension->switch_extension = ((wxChoice*)wxcontrol)->GetSelection();
}

PadSettingCheckBox::PadSettingCheckBox(wxWindow* const parent, ControllerEmu::ControlGroup::Setting* const _setting)
	: PadSetting(new wxCheckBox(parent, -1, wxGetTranslation(StrToWxStr(_setting->name))))
	, setting(_setting)
{
	UpdateGUI();
}

void PadSettingCheckBox::UpdateGUI()
{
	((wxCheckBox*)wxcontrol)->SetValue(!!setting->GetValue());
}

void PadSettingCheckBox::UpdateValue()
{
	// 0.01 so its saved to the ini file as just 1. :(
	setting->SetValue(0.01 * ((wxCheckBox*)wxcontrol)->GetValue());
}

void PadSettingSpin::UpdateGUI()
{
	((wxSpinCtrl*)wxcontrol)->SetValue((int)(setting->GetValue() * 100));
}

void PadSettingSpin::UpdateValue()
{
	setting->SetValue(float(((wxSpinCtrl*)wxcontrol)->GetValue()) / 100);
}

ControlDialog::ControlDialog(GamepadPage* const parent, InputConfig& config, ControllerInterface::ControlReference* const ref)
	: wxDialog(parent, -1, _("Configure Control"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
	, control_reference(ref)
	, m_config(config)
	, m_parent(parent)
{
	m_devq = m_parent->controller->default_device;

	// GetStrings() sounds slow :/
	//device_cbox = new wxComboBox(this, -1, StrToWxStr(ref->device_qualifier.ToString()), wxDefaultPosition, wxSize(256,-1), parent->device_cbox->GetStrings(), wxTE_PROCESS_ENTER);
	device_cbox = new wxComboBox(this, -1, StrToWxStr(m_devq.ToString()), wxDefaultPosition, wxSize(256,-1), parent->device_cbox->GetStrings(), wxTE_PROCESS_ENTER);

	device_cbox->Bind(wxEVT_COMBOBOX, &ControlDialog::SetDevice, this);
	device_cbox->Bind(wxEVT_TEXT_ENTER, &ControlDialog::SetDevice, this);

	wxStaticBoxSizer* const control_chooser = CreateControlChooser(parent);

	wxStaticBoxSizer* const d_szr = new wxStaticBoxSizer(wxVERTICAL, this, _("Device"));
	d_szr->Add(device_cbox, 0, wxEXPAND|wxALL, 5);

	wxBoxSizer* const szr = new wxBoxSizer(wxVERTICAL);
	szr->Add(d_szr, 0, wxEXPAND|wxLEFT|wxRIGHT|wxTOP, 5);
	szr->Add(control_chooser, 1, wxEXPAND|wxALL, 5);

	SetSizerAndFit(szr); // needed

	UpdateGUI();
	SetFocus();
}

ControlButton::ControlButton(wxWindow* const parent, ControllerInterface::ControlReference* const _ref, const unsigned int width, const std::string& label)
: wxButton(parent, -1, "", wxDefaultPosition, wxSize(width,20))
, control_reference(_ref)
{
	if (label.empty())
		SetLabel(StrToWxStr(_ref->expression));
	else
		SetLabel(StrToWxStr(label));
}

void InputConfigDialog::UpdateProfileComboBox()
{
	std::string pname(File::GetUserPath(D_CONFIG_IDX));
	pname += PROFILES_PATH;
	pname += m_config.profile_name;

	CFileSearch::XStringVector exts;
	exts.push_back("*.ini");
	CFileSearch::XStringVector dirs;
	dirs.push_back(pname);
	CFileSearch cfs(exts, dirs);
	const CFileSearch::XStringVector& sv = cfs.GetFileNames();

	wxArrayString strs;
	for (auto si = sv.cbegin(); si != sv.cend(); ++si)
	{
		std::string str(si->begin() + si->find_last_of('/') + 1 , si->end() - 4) ;
		strs.push_back(StrToWxStr(str));
	}

	for (GamepadPage* page : m_padpages)
	{
		page->profile_cbox->Clear();
		page->profile_cbox->Append(strs);
	}
}

void InputConfigDialog::UpdateControlReferences()
{
	for (GamepadPage* page : m_padpages)
	{
		page->controller->UpdateReferences(g_controller_interface);
	}
}

void InputConfigDialog::ClickSave(wxCommandEvent& event)
{
	m_config.SaveConfig();
	event.Skip();
}

void ControlDialog::UpdateListContents()
{
	control_lbox->Clear();

	ciface::Core::Device* const dev = g_controller_interface.FindDevice(m_devq);
	if (dev)
	{
		if (control_reference->is_input)
		{
			for (ciface::Core::Device::Input* input : dev->Inputs())
			{
				control_lbox->Append(StrToWxStr(input->GetName()));
			}
		}
		else // It's an output
		{
			for (ciface::Core::Device::Output* output : dev->Outputs())
			{
				control_lbox->Append(StrToWxStr(output->GetName()));
			}
		}
	}
}

void ControlDialog::SelectControl(const std::string& name)
{
	//UpdateGUI();

	const int f = control_lbox->FindString(StrToWxStr(name));
	if (f >= 0)
		control_lbox->Select(f);
}

void ControlDialog::UpdateGUI()
{
	// update textbox
	textctrl->SetValue(StrToWxStr(control_reference->expression));

	// updates the "bound controls:" label
	m_bound_label->SetLabel(wxString::Format(_("Bound Controls: %lu"),
		(unsigned long)control_reference->BoundCount()));

	switch (control_reference->parse_error)
	{
	case EXPRESSION_PARSE_SYNTAX_ERROR:
		m_error_label->SetLabel(_("Syntax error"));
		break;
	case EXPRESSION_PARSE_NO_DEVICE:
		m_error_label->SetLabel(_("Device not found"));
		break;
	default:
		m_error_label->SetLabel("");
	}
};

void GamepadPage::UpdateGUI()
{
	device_cbox->SetValue(StrToWxStr(controller->default_device.ToString()));

	for (ControlGroupBox* cgBox : control_groups)
	{
		for (ControlButton* button : cgBox->control_buttons)
		{
			wxString expr = StrToWxStr(button->control_reference->expression);
			expr.Replace("&", "&&");
			button->SetLabel(expr);
		}

		for (PadSetting* padSetting : cgBox->options)
		{
			padSetting->UpdateGUI();
		}
	}
}

void GamepadPage::ClearAll(wxCommandEvent&)
{
	// just load an empty ini section to clear everything :P
	IniFile::Section section;
	controller->LoadConfig(&section);

	// no point in using the real ControllerInterface i guess
	ControllerInterface face;

	std::lock_guard<std::recursive_mutex> lk(m_config.controls_lock);
	controller->UpdateReferences(face);

	UpdateGUI();
}

void GamepadPage::LoadDefaults(wxCommandEvent&)
{
	controller->LoadDefaults(g_controller_interface);

	std::lock_guard<std::recursive_mutex> lk(m_config.controls_lock);
	controller->UpdateReferences(g_controller_interface);

	UpdateGUI();
}

bool ControlDialog::Validate()
{
	control_reference->expression = WxStrToStr(textctrl->GetValue());

	std::lock_guard<std::recursive_mutex> lk(m_config.controls_lock);
	g_controller_interface.UpdateReference(control_reference, m_parent->controller->default_device);

	UpdateGUI();

	return (control_reference->parse_error == EXPRESSION_PARSE_SUCCESS);
}

void GamepadPage::SetDevice(wxCommandEvent&)
{
	controller->default_device.FromString(WxStrToStr(device_cbox->GetValue()));

	// show user what it was validated as
	device_cbox->SetValue(StrToWxStr(controller->default_device.ToString()));

	// this will set all the controls to this default device
	controller->UpdateDefaultDevice();

	// update references
	std::lock_guard<std::recursive_mutex> lk(m_config.controls_lock);
	controller->UpdateReferences(g_controller_interface);
}

void ControlDialog::SetDevice(wxCommandEvent&)
{
	m_devq.FromString(WxStrToStr(device_cbox->GetValue()));

	// show user what it was validated as
	device_cbox->SetValue(StrToWxStr(m_devq.ToString()));

	// update gui
	UpdateListContents();
}

void ControlDialog::ClearControl(wxCommandEvent&)
{
	control_reference->expression.clear();

	std::lock_guard<std::recursive_mutex> lk(m_config.controls_lock);
	g_controller_interface.UpdateReference(control_reference, m_parent->controller->default_device);

	UpdateGUI();
}

inline bool IsAlphabetic(wxString &str)
{
	for (wxUniChar c : str)
		if (!isalpha(c))
			return false;

	return true;
}

inline void GetExpressionForControl(wxString &expr,
				    wxString &control_name,
				    ciface::Core::DeviceQualifier *control_device = nullptr,
				    ciface::Core::DeviceQualifier *default_device = nullptr)
{
	expr = "";

	// non-default device
	if (control_device && default_device && !(*control_device == *default_device))
	{
		expr += control_device->ToString();
		expr += ":";
	}

	// append the control name
	expr += control_name;

	if (!IsAlphabetic(expr))
		expr = wxString::Format("`%s`", expr);
}

bool ControlDialog::GetExpressionForSelectedControl(wxString &expr)
{
	const int num = control_lbox->GetSelection();

	if (num < 0)
		return false;

	wxString control_name = control_lbox->GetString(num);
	GetExpressionForControl(expr,
				control_name,
				&m_devq,
				&m_parent->controller->default_device);

	return true;
}

void ControlDialog::SetSelectedControl(wxCommandEvent&)
{
	wxString expr;

	if (!GetExpressionForSelectedControl(expr))
		return;

	textctrl->WriteText(expr);
	control_reference->expression = textctrl->GetValue();

	std::lock_guard<std::recursive_mutex> lk(m_config.controls_lock);
	g_controller_interface.UpdateReference(control_reference, m_parent->controller->default_device);

	UpdateGUI();
}

void ControlDialog::AppendControl(wxCommandEvent& event)
{
	wxString device_expr, expr;

	const wxString lbl = ((wxButton*)event.GetEventObject())->GetLabel();
	char op = lbl[0];

	if (!GetExpressionForSelectedControl(device_expr))
		return;

	// Unary ops (that is, '!') are a special case. When there's a selection,
	// put parens around it and prepend it with a '!', but when there's nothing,
	// just add a '!device'.
	if (op == '!')
	{
		wxString selection = textctrl->GetStringSelection();
		if (selection == "")
			expr = wxString::Format("%c%s", op, device_expr);
		else
			expr = wxString::Format("%c(%s)", op, selection);
	}
	else
	{
		expr = wxString::Format(" %c %s", op, device_expr);
	}

	textctrl->WriteText(expr);
	control_reference->expression = textctrl->GetValue();

	std::lock_guard<std::recursive_mutex> lk(m_config.controls_lock);
	g_controller_interface.UpdateReference(control_reference, m_parent->controller->default_device);

	UpdateGUI();
}

void GamepadPage::AdjustSetting(wxCommandEvent& event)
{
	std::lock_guard<std::recursive_mutex> lk(m_config.controls_lock);
	((PadSetting*)((wxControl*)event.GetEventObject())->GetClientData())->UpdateValue();
}

void GamepadPage::AdjustSettingUI(wxCommandEvent& event)
{
	m_iterate = !m_iterate;
	std::lock_guard<std::recursive_mutex> lk(m_config.controls_lock);
	((PadSetting*)((wxControl*)event.GetEventObject())->GetClientData())->UpdateValue();
}

void GamepadPage::AdjustControlOption(wxCommandEvent&)
{
	std::lock_guard<std::recursive_mutex> lk(m_config.controls_lock);
	m_control_dialog->control_reference->range = (ControlState)(m_control_dialog->range_slider->GetValue()) / SLIDER_TICK_COUNT;
}

void GamepadPage::ConfigControl(wxEvent& event)
{
	m_control_dialog = new ControlDialog(this, m_config, ((ControlButton*)event.GetEventObject())->control_reference);
	m_control_dialog->ShowModal();
	m_control_dialog->Destroy();

	// update changes that were made in the dialog
	UpdateGUI();
}

void GamepadPage::ClearControl(wxEvent& event)
{
	ControlButton* const btn = (ControlButton*)event.GetEventObject();
	btn->control_reference->expression.clear();
	btn->control_reference->range = 1.0f;

	std::lock_guard<std::recursive_mutex> lk(m_config.controls_lock);
	controller->UpdateReferences(g_controller_interface);

	// update changes
	UpdateGUI();
}

void ControlDialog::DetectControl(wxCommandEvent& event)
{
	wxButton* const btn = (wxButton*)event.GetEventObject();
	const wxString lbl = btn->GetLabel();

	ciface::Core::Device* const dev = g_controller_interface.FindDevice(m_devq);
	if (dev)
	{
		btn->SetLabel(_("[ waiting ]"));

		// This makes the "waiting" text work on Linux. true (only if needed) prevents crash on Windows
		wxTheApp->Yield(true);

		std::lock_guard<std::recursive_mutex> lk(m_config.controls_lock);
		ciface::Core::Device::Control* const ctrl = control_reference->Detect(DETECT_WAIT_TIME, dev);

		// if we got input, select it in the list
		if (ctrl)
			SelectControl(ctrl->GetName());

		btn->SetLabel(lbl);
	}
}

void GamepadPage::DetectControl(wxCommandEvent& event)
{
	ControlButton* btn = (ControlButton*)event.GetEventObject();
	if (DetectButton(btn) && m_iterate == true)
	{
		auto it = std::find(control_buttons.begin(), control_buttons.end(), btn);

		// std find will never return end since btn will always be found in control_buttons
		++it;
		for (; it != control_buttons.end(); ++it)
		{
			if (!DetectButton(*it))
				break;
		}
	}
}

bool GamepadPage::DetectButton(ControlButton* button)
{
	bool success = false;
	// find device :/
	ciface::Core::Device* const dev = g_controller_interface.FindDevice(controller->default_device);
	if (dev)
	{
		button->SetLabel(_("[ waiting ]"));

		// This makes the "waiting" text work on Linux. true (only if needed) prevents crash on Windows
		wxTheApp->Yield(true);

		std::lock_guard<std::recursive_mutex> lk(m_config.controls_lock);
		ciface::Core::Device::Control* const ctrl = button->control_reference->Detect(DETECT_WAIT_TIME, dev);

		// if we got input, update expression and reference
		if (ctrl)
		{
			wxString control_name = ctrl->GetName();
			wxString expr;
			GetExpressionForControl(expr, control_name);
			button->control_reference->expression = expr;
			g_controller_interface.UpdateReference(button->control_reference, controller->default_device);
			success = true;
		}
	}

	UpdateGUI();

	return success;
}

wxStaticBoxSizer* ControlDialog::CreateControlChooser(GamepadPage* const parent)
{
	wxStaticBoxSizer* const main_szr = new wxStaticBoxSizer(wxVERTICAL, this, control_reference->is_input ? _("Input") : _("Output"));

	textctrl = new wxTextCtrl(this, -1, wxEmptyString, wxDefaultPosition, wxSize(-1, 48), wxTE_MULTILINE | wxTE_RICH2);
	wxFont font = textctrl->GetFont();
	font.SetFamily(wxFONTFAMILY_MODERN);
	textctrl->SetFont(font);

	wxButton* const detect_button = new wxButton(this, -1, control_reference->is_input ? _("Detect") : _("Test"));

	wxButton* const clear_button = new  wxButton(this, -1, _("Clear"));

	wxButton* const select_button = new wxButton(this, -1, _("Select"));
	select_button->Bind(wxEVT_BUTTON, &ControlDialog::SetSelectedControl, this);

	wxButton* const or_button = new  wxButton(this, -1, _("| OR"));
	or_button->Bind(wxEVT_BUTTON, &ControlDialog::AppendControl, this);

	control_lbox = new wxListBox(this, -1, wxDefaultPosition, wxSize(-1, 64));

	wxBoxSizer* const button_sizer = new wxBoxSizer(wxVERTICAL);
	button_sizer->Add(detect_button, 1, 0, 5);
	button_sizer->Add(select_button, 1, 0, 5);
	button_sizer->Add(or_button, 1, 0, 5);

	if (control_reference->is_input)
	{
		// TODO: check if && is good on other OS
		wxButton* const and_button = new  wxButton(this, -1, _("&& AND"));
		wxButton* const not_button = new  wxButton(this, -1, _("! NOT"));
		wxButton* const add_button = new  wxButton(this, -1, _("+ ADD"));

		and_button->Bind(wxEVT_BUTTON, &ControlDialog::AppendControl, this);
		not_button->Bind(wxEVT_BUTTON, &ControlDialog::AppendControl, this);
		add_button->Bind(wxEVT_BUTTON, &ControlDialog::AppendControl, this);

		button_sizer->Add(and_button, 1, 0, 5);
		button_sizer->Add(not_button, 1, 0, 5);
		button_sizer->Add(add_button, 1, 0, 5);
	}

	range_slider = new wxSlider(this, -1, SLIDER_TICK_COUNT, -SLIDER_TICK_COUNT * 5, SLIDER_TICK_COUNT * 5, wxDefaultPosition, wxDefaultSize, wxSL_TOP | wxSL_LABELS /*| wxSL_AUTOTICKS*/);

	range_slider->SetValue((int)(control_reference->range * SLIDER_TICK_COUNT));

	detect_button->Bind(wxEVT_BUTTON, &ControlDialog::DetectControl, this);
	clear_button->Bind(wxEVT_BUTTON, &ControlDialog::ClearControl, this);

	range_slider->Bind(wxEVT_SCROLL_CHANGED, &GamepadPage::AdjustControlOption, parent);
	wxStaticText* const range_label = new wxStaticText(this, -1, _("Range"));

	m_bound_label = new wxStaticText(this, -1, "");
	m_error_label = new wxStaticText(this, -1, "");

	wxBoxSizer* const range_sizer = new wxBoxSizer(wxHORIZONTAL);
	range_sizer->Add(range_label, 0, wxCENTER|wxLEFT, 5);
	range_sizer->Add(range_slider, 1, wxEXPAND|wxLEFT, 5);

	wxBoxSizer* const ctrls_sizer = new wxBoxSizer(wxHORIZONTAL);
	ctrls_sizer->Add(control_lbox, 1, wxEXPAND, 0);
	ctrls_sizer->Add(button_sizer, 0, wxEXPAND, 0);

	wxSizer* const bottom_btns_sizer = CreateButtonSizer(wxOK|wxAPPLY);
	bottom_btns_sizer->Prepend(clear_button, 0, wxLEFT, 5);

	main_szr->Add(range_sizer, 0, wxEXPAND|wxLEFT|wxRIGHT, 5);
	main_szr->Add(ctrls_sizer, 0, wxEXPAND|wxLEFT|wxRIGHT|wxBOTTOM, 5);
	main_szr->Add(textctrl, 1, wxEXPAND|wxLEFT|wxRIGHT|wxBOTTOM, 5);
	main_szr->Add(bottom_btns_sizer, 0, wxEXPAND|wxBOTTOM|wxRIGHT, 5);
	main_szr->Add(m_bound_label, 0, wxCENTER, 0);
	main_szr->Add(m_error_label, 0, wxCENTER, 0);

	UpdateListContents();

	return main_szr;
}

void GamepadPage::GetProfilePath(std::string& path)
{
	const wxString& name = profile_cbox->GetValue();
	if (!name.empty())
	{
		// TODO: check for dumb characters maybe

		path = File::GetUserPath(D_CONFIG_IDX);
		path += PROFILES_PATH;
		path += m_config.profile_name;
		path += '/';
		path += WxStrToStr(profile_cbox->GetValue());
		path += ".ini";
	}
}

void GamepadPage::LoadProfile(wxCommandEvent&)
{
	std::string fname;
	GamepadPage::GetProfilePath(fname);

	if (!File::Exists(fname))
		return;

	IniFile inifile;
	inifile.Load(fname);

	std::lock_guard<std::recursive_mutex> lk(m_config.controls_lock);
	controller->LoadConfig(inifile.GetOrCreateSection("Profile"));
	controller->UpdateReferences(g_controller_interface);

	UpdateGUI();
}

void GamepadPage::SaveProfile(wxCommandEvent&)
{
	std::string fname;
	GamepadPage::GetProfilePath(fname);
	File::CreateFullPath(fname);

	if (!fname.empty())
	{
		IniFile inifile;
		controller->SaveConfig(inifile.GetOrCreateSection("Profile"));
		inifile.Save(fname);

		m_config_dialog->UpdateProfileComboBox();
	}
	else
	{
		WxUtils::ShowErrorDialog(_("You must enter a valid profile name."));
	}
}

void GamepadPage::DeleteProfile(wxCommandEvent&)
{
	std::string fname;
	GamepadPage::GetProfilePath(fname);

	const char* const fnamecstr = fname.c_str();

	if (File::Exists(fnamecstr) &&
			AskYesNoT("Are you sure you want to delete \"%s\"?",
			WxStrToStr(profile_cbox->GetValue()).c_str()))
	{
		File::Delete(fnamecstr);

		m_config_dialog->UpdateProfileComboBox();
	}
}

void InputConfigDialog::UpdateDeviceComboBox()
{
	ciface::Core::DeviceQualifier dq;
	for (GamepadPage* page : m_padpages)
	{
		page->device_cbox->Clear();

		for (ciface::Core::Device* d : g_controller_interface.Devices())
		{
			dq.FromDevice(d);
			page->device_cbox->Append(StrToWxStr(dq.ToString()));
		}

		page->device_cbox->SetValue(StrToWxStr(page->controller->default_device.ToString()));
	}
}

void GamepadPage::RefreshDevices(wxCommandEvent&)
{
	std::lock_guard<std::recursive_mutex> lk(m_config.controls_lock);

	// refresh devices
	g_controller_interface.Reinitialize();

	// update all control references
	m_config_dialog->UpdateControlReferences();

	// update device cbox
	m_config_dialog->UpdateDeviceComboBox();
}

ControlGroupBox::~ControlGroupBox()
{
	for (PadSetting* padSetting : options)
		delete padSetting;
}

ControlGroupBox::ControlGroupBox(ControllerEmu::ControlGroup* const group, wxWindow* const parent, GamepadPage* const eventsink)
	: wxBoxSizer(wxVERTICAL)
	, control_group(group)
{
	static_bitmap = nullptr;
	const std::vector<std::string> exclude_buttons = {"Mic", "Modifier"};
	const std::vector<std::string> exclude_groups = {"IR", "Swing", "Tilt", "Shake", "UDP Wiimote", "Extension", "Rumble"};

	wxFont m_SmallFont(7, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
	for (auto& control : group->controls)
	{
		wxStaticText* const label = new wxStaticText(parent, -1, wxGetTranslation(StrToWxStr(control->name)));

		ControlButton* const control_button = new ControlButton(parent, control->control_ref.get(), 80);
		control_button->SetFont(m_SmallFont);

		control_buttons.push_back(control_button);
		if (std::find(exclude_groups.begin(), exclude_groups.end(), control_group->name) == exclude_groups.end() &&
		    std::find(exclude_buttons.begin(), exclude_buttons.end(), control->name) == exclude_buttons.end())
			eventsink->control_buttons.push_back(control_button);


		if (control->control_ref->is_input)
		{
			control_button->SetToolTip(_("Left-click to detect input.\nMiddle-click to clear.\nRight-click for more options."));
			control_button->Bind(wxEVT_BUTTON, &GamepadPage::DetectControl, eventsink);
		}
		else
		{
			control_button->SetToolTip(_("Left/Right-click for more options.\nMiddle-click to clear."));
			control_button->Bind(wxEVT_BUTTON, &GamepadPage::ConfigControl, eventsink);
		}

		control_button->Bind(wxEVT_MIDDLE_DOWN, &GamepadPage::ClearControl, eventsink);
		control_button->Bind(wxEVT_RIGHT_UP, &GamepadPage::ConfigControl, eventsink);

		wxBoxSizer* const control_sizer = new wxBoxSizer(wxHORIZONTAL);
		control_sizer->AddStretchSpacer(1);
		control_sizer->Add(label, 0, wxCENTER | wxRIGHT, 3);
		control_sizer->Add(control_button, 0, 0, 0);

		Add(control_sizer, 0, wxEXPAND|wxLEFT|wxRIGHT, 3);

	}

	wxMemoryDC dc;

	switch (group->type)
	{
	case GROUP_TYPE_STICK:
	case GROUP_TYPE_TILT:
	case GROUP_TYPE_CURSOR:
	case GROUP_TYPE_FORCE:
		{
			wxBitmap bitmap(64, 64);
			dc.SelectObject(bitmap);
			dc.Clear();
			static_bitmap = new wxStaticBitmap(parent, -1, bitmap, wxDefaultPosition, wxDefaultSize, wxBITMAP_TYPE_BMP);

			wxBoxSizer* const szr = new wxBoxSizer(wxVERTICAL);
			for (auto& groupSetting : group->settings)
			{
				PadSettingSpin* setting = new PadSettingSpin(parent, groupSetting.get());
				setting->wxcontrol->Bind(wxEVT_SPINCTRL, &GamepadPage::AdjustSetting, eventsink);
				options.push_back(setting);
				szr->Add(new wxStaticText(parent, -1, wxGetTranslation(StrToWxStr(groupSetting->name))));
				szr->Add(setting->wxcontrol, 0, wxLEFT, 0);
			}

			wxBoxSizer* const h_szr = new wxBoxSizer(wxHORIZONTAL);
			h_szr->Add(szr, 1, 0, 5);
			h_szr->Add(static_bitmap, 0, wxALL|wxCENTER, 3);

			Add(h_szr, 0, wxEXPAND|wxLEFT|wxCENTER|wxTOP, 3);
		}
		break;
	case GROUP_TYPE_BUTTONS:
		{
			wxBitmap bitmap(int(12*group->controls.size()+1), 12);
			dc.SelectObject(bitmap);
			dc.Clear();
			static_bitmap = new wxStaticBitmap(parent, -1, bitmap, wxDefaultPosition, wxDefaultSize, wxBITMAP_TYPE_BMP);

			PadSettingSpin* const threshold_cbox = new PadSettingSpin(parent, group->settings[0].get());
			threshold_cbox->wxcontrol->Bind(wxEVT_SPINCTRL, &GamepadPage::AdjustSetting, eventsink);

			threshold_cbox->wxcontrol->SetToolTip(_("Adjust the analog control pressure required to activate buttons."));

			options.push_back(threshold_cbox);

			wxBoxSizer* const szr = new wxBoxSizer(wxHORIZONTAL);
			szr->Add(new wxStaticText(parent, -1, wxGetTranslation(StrToWxStr(group->settings[0]->name))),
					0, wxCENTER|wxRIGHT, 3);
			szr->Add(threshold_cbox->wxcontrol, 0, wxRIGHT, 3);

			Add(szr, 0, wxALL|wxCENTER, 3);
			Add(static_bitmap, 0, wxALL|wxCENTER, 3);
		}
		break;
	case GROUP_TYPE_MIXED_TRIGGERS:
	case GROUP_TYPE_TRIGGERS:
	case GROUP_TYPE_SLIDER:
		{
			int height = (int)(12 * group->controls.size());
			int width = 64;

			if (GROUP_TYPE_MIXED_TRIGGERS == group->type)
				width = 64+12+1;

			if (GROUP_TYPE_TRIGGERS != group->type)
				height /= 2;

			wxBitmap bitmap(width, height+1);
			dc.SelectObject(bitmap);
			dc.Clear();
			static_bitmap = new wxStaticBitmap(parent, -1, bitmap, wxDefaultPosition, wxDefaultSize, wxBITMAP_TYPE_BMP);

			for (auto& groupSetting : group->settings)
			{
				PadSettingSpin* setting = new PadSettingSpin(parent, groupSetting.get());
				setting->wxcontrol->Bind(wxEVT_SPINCTRL, &GamepadPage::AdjustSetting, eventsink);
				options.push_back(setting);
				wxBoxSizer* const szr = new wxBoxSizer(wxHORIZONTAL);
				szr->Add(new wxStaticText(parent, -1, wxGetTranslation(StrToWxStr(groupSetting->name))), 0, wxCENTER|wxRIGHT, 3);
				szr->Add(setting->wxcontrol, 0, wxRIGHT, 3);
				Add(szr, 0, wxALL|wxCENTER, 3);
			}

			Add(static_bitmap, 0, wxALL|wxCENTER, 3);
		}
		break;
	case GROUP_TYPE_EXTENSION:
		{
			PadSettingExtension* const attachments = new PadSettingExtension(parent, (ControllerEmu::Extension*)group);
			wxButton* const configure_btn = new ExtensionButton(parent, (ControllerEmu::Extension*)group);

			options.push_back(attachments);

			attachments->wxcontrol->Bind(wxEVT_CHOICE, &GamepadPage::AdjustSetting, eventsink);
			configure_btn->Bind(wxEVT_BUTTON, &GamepadPage::ConfigExtension, eventsink);

			Add(attachments->wxcontrol, 0, wxTOP|wxLEFT|wxRIGHT|wxEXPAND, 3);
			Add(configure_btn, 0, wxALL|wxEXPAND, 3);
		}
		break;
	default:
		{
			//options
			for (auto& groupSetting : group->settings)
			{
				if (groupSetting->high == DEFAULT_HIGH_VALUE)
				{
					PadSettingCheckBox* setting_cbox = new PadSettingCheckBox(parent, groupSetting.get());
					if (groupSetting->is_iterate == true)
					{
						setting_cbox->wxcontrol->Bind(wxEVT_CHECKBOX, &GamepadPage::AdjustSettingUI, eventsink);
						groupSetting->value = 0;
					}
					else
					{
						setting_cbox->wxcontrol->Bind(wxEVT_CHECKBOX, &GamepadPage::AdjustSetting, eventsink);
					}
					options.push_back(setting_cbox);
					Add(setting_cbox->wxcontrol, 0, wxALL | wxLEFT, 5);
				}
				else
				{
					PadSettingSpin* setting = new PadSettingSpin(parent, groupSetting.get());
					setting->wxcontrol->Bind(wxEVT_SPINCTRL, &GamepadPage::AdjustSetting, eventsink);
					options.push_back(setting);
					wxBoxSizer* const szr = new wxBoxSizer(wxHORIZONTAL);
					szr->Add(new wxStaticText(parent, -1, wxGetTranslation(StrToWxStr(groupSetting->name))), 0, wxCENTER | wxRIGHT, 3);
					szr->Add(setting->wxcontrol, 0, wxRIGHT, 3);
					Add(szr, 0, wxALL | wxCENTER, 3);
				}
			}
		}
		break;
	}

	dc.SelectObject(wxNullBitmap);

	//AddStretchSpacer(0);
}

ControlGroupsSizer::ControlGroupsSizer(ControllerEmu* const controller, wxWindow* const parent, GamepadPage* const eventsink, std::vector<ControlGroupBox*>* groups)
	: wxBoxSizer(wxHORIZONTAL)
{
	size_t col_size = 0;

	wxBoxSizer* stacked_groups = nullptr;
	for (auto& group : controller->groups)
	{
		ControlGroupBox* control_group_box = new ControlGroupBox(group.get(), parent, eventsink);
		wxStaticBoxSizer *control_group =
			new wxStaticBoxSizer(wxVERTICAL, parent, wxGetTranslation(StrToWxStr(group->name)));
		control_group->Add(control_group_box);

		const size_t grp_size = group->controls.size() + group->settings.size();
		col_size += grp_size;
		if (col_size > 8 || nullptr == stacked_groups)
		{
			if (stacked_groups)
				Add(stacked_groups, 0, /*wxEXPAND|*/wxBOTTOM|wxRIGHT, 5);

			stacked_groups = new wxBoxSizer(wxVERTICAL);
			stacked_groups->Add(control_group, 0, wxEXPAND);

			col_size = grp_size;
		}
		else
		{
			stacked_groups->Add(control_group, 0, wxEXPAND);
		}

		if (groups)
			groups->push_back(control_group_box);
	}

	if (stacked_groups)
		Add(stacked_groups, 0, /*wxEXPAND|*/wxBOTTOM|wxRIGHT, 5);
}

GamepadPage::GamepadPage(wxWindow* parent, InputConfig& config, const unsigned int pad_num, InputConfigDialog* const config_dialog)
	: wxPanel(parent, wxID_ANY)
	,controller(config.controllers[pad_num])
	, m_config_dialog(config_dialog)
	, m_config(config)
{

	wxBoxSizer* control_group_sizer = new ControlGroupsSizer(m_config.controllers[pad_num], this, this, &control_groups);

	wxStaticBoxSizer* profile_sbox = new wxStaticBoxSizer(wxHORIZONTAL, this, _("Profile"));

	// device chooser

	wxStaticBoxSizer* const device_sbox = new wxStaticBoxSizer(wxHORIZONTAL, this, _("Device"));

	device_cbox = new wxComboBox(this, -1, "", wxDefaultPosition, wxSize(64,-1));
	device_cbox->ToggleWindowStyle(wxTE_PROCESS_ENTER);

	wxButton* refresh_button = new wxButton(this, -1, _("Refresh"), wxDefaultPosition, wxSize(60,-1));

	device_cbox->Bind(wxEVT_COMBOBOX, &GamepadPage::SetDevice, this);
	device_cbox->Bind(wxEVT_TEXT_ENTER, &GamepadPage::SetDevice, this);
	refresh_button->Bind(wxEVT_BUTTON, &GamepadPage::RefreshDevices, this);

	device_sbox->Add(device_cbox, 1, wxLEFT|wxRIGHT, 3);
	device_sbox->Add(refresh_button, 0, wxRIGHT|wxBOTTOM, 3);

	wxButton* const default_button = new wxButton(this, -1, _("Default"), wxDefaultPosition, wxSize(48,-1));
	wxButton* const clearall_button = new wxButton(this, -1, _("Clear"), wxDefaultPosition, wxSize(58,-1));

	wxStaticBoxSizer* const clear_sbox = new wxStaticBoxSizer(wxHORIZONTAL, this, _("Reset"));
	clear_sbox->Add(default_button, 1, wxLEFT, 3);
	clear_sbox->Add(clearall_button, 1, wxRIGHT, 3);

	clearall_button->Bind(wxEVT_BUTTON, &GamepadPage::ClearAll, this);
	default_button->Bind(wxEVT_BUTTON, &GamepadPage::LoadDefaults, this);

	profile_cbox = new wxComboBox(this, -1, "", wxDefaultPosition, wxSize(64,-1));

	wxButton* const pload_btn = new wxButton(this, -1, _("Load"), wxDefaultPosition, wxSize(48,-1));
	wxButton* const psave_btn = new wxButton(this, -1, _("Save"), wxDefaultPosition, wxSize(48,-1));
	wxButton* const pdelete_btn = new wxButton(this, -1, _("Delete"), wxDefaultPosition, wxSize(60,-1));

	pload_btn->Bind(wxEVT_BUTTON, &GamepadPage::LoadProfile, this);
	psave_btn->Bind(wxEVT_BUTTON, &GamepadPage::SaveProfile, this);
	pdelete_btn->Bind(wxEVT_BUTTON, &GamepadPage::DeleteProfile, this);

	profile_sbox->Add(profile_cbox, 1, wxLEFT, 3);
	profile_sbox->Add(pload_btn, 0, wxLEFT, 3);
	profile_sbox->Add(psave_btn, 0, 0, 3);
	profile_sbox->Add(pdelete_btn, 0, wxRIGHT|wxBOTTOM, 3);

	wxBoxSizer* const dio = new wxBoxSizer(wxHORIZONTAL);
	dio->Add(device_sbox, 1, wxEXPAND|wxRIGHT, 5);
	dio->Add(clear_sbox, 0, wxEXPAND|wxRIGHT, 5);
	dio->Add(profile_sbox, 1, wxEXPAND|wxRIGHT, 5);

	wxBoxSizer* const mapping = new wxBoxSizer(wxVERTICAL);

	mapping->Add(dio, 1, wxEXPAND|wxLEFT|wxTOP|wxBOTTOM, 5);
	mapping->Add(control_group_sizer, 0, wxLEFT|wxEXPAND, 5);

	UpdateGUI();

	SetSizerAndFit(mapping); // needed
	Layout();
};


InputConfigDialog::InputConfigDialog(wxWindow* const parent, InputConfig& config, const std::string& name, const int tab_num)
	: wxDialog(parent, wxID_ANY, wxGetTranslation(StrToWxStr(name)), wxPoint(128,-1))
	, m_config(config)
{
	m_pad_notebook = new wxNotebook(this, -1, wxDefaultPosition, wxDefaultSize, wxNB_DEFAULT);
	for (unsigned int i = 0; i < std::min(config.controllers.size(), (size_t)MAX_WIIMOTES); ++i)
	{
		GamepadPage* gp = new GamepadPage(m_pad_notebook, m_config, i, this);
		m_padpages.push_back(gp);
		m_pad_notebook->AddPage(gp, wxString::Format("%s %u", wxGetTranslation(StrToWxStr(m_config.gui_name)), 1+i));
	}

	m_pad_notebook->SetSelection(tab_num);

	UpdateDeviceComboBox();
	UpdateProfileComboBox();

	Bind(wxEVT_BUTTON, &InputConfigDialog::ClickSave, this, wxID_OK);

	wxBoxSizer* const szr = new wxBoxSizer(wxVERTICAL);
	szr->Add(m_pad_notebook, 0, wxEXPAND|wxTOP|wxLEFT|wxRIGHT, 5);
	szr->Add(CreateButtonSizer(wxOK | wxCANCEL | wxNO_DEFAULT), 0, wxEXPAND|wxALL, 5);

	SetSizerAndFit(szr);
	Center();

	// live preview update timer
	m_update_timer = new wxTimer(this, -1);
	Bind(wxEVT_TIMER, &InputConfigDialog::UpdateBitmaps, this);
	m_update_timer->Start(PREVIEW_UPDATE_TIME, wxTIMER_CONTINUOUS);
}

bool InputConfigDialog::Destroy()
{
	m_update_timer->Stop();
	return true;
}
