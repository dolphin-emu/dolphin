// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "InputConfigDiag.h"
#include "UDPConfigDiag.h"
#include "WxUtils.h"
#include "HW/Wiimote.h"
#include "ControllerInterface/ExpressionParser.h"

using namespace ciface::ExpressionParser;

void GamepadPage::ConfigUDPWii(wxCommandEvent &event)
{
	UDPWrapper* const wrp = ((UDPConfigButton*)event.GetEventObject())->wrapper;
	UDPConfigDiag diag(this, wrp);
	diag.ShowModal();
}

void GamepadPage::ConfigExtension(wxCommandEvent& event)
{
	ControllerEmu::Extension* const ex = ((ExtensionButton*)event.GetEventObject())->extension;

	// show config diag, if "none" isn't selected
	if (ex->switch_extension)
	{
		wxDialog dlg(this, -1,
			wxGetTranslation(StrToWxStr(ex->attachments[ex->switch_extension]->GetName())),
			wxDefaultPosition, wxDefaultSize);

		wxBoxSizer* const main_szr = new wxBoxSizer(wxVERTICAL);
		const std::size_t orig_size = control_groups.size();

		ControlGroupsSizer* const szr =
			new ControlGroupsSizer(ex->attachments[ex->switch_extension], &dlg, this, &control_groups);
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

	std::vector<ControllerEmu*>::const_iterator
		i = extension->attachments.begin(),
		e = extension->attachments.end();

	for (; i!=e; ++i)
		((wxChoice*)wxcontrol)->Append(wxGetTranslation(StrToWxStr((*i)->GetName())));

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

PadSettingCheckBox::PadSettingCheckBox(wxWindow* const parent, ControlState& _value, const char* const label)
	: PadSetting(new wxCheckBox(parent, -1, wxGetTranslation(StrToWxStr(label)), wxDefaultPosition))
	, value(_value)
{
	UpdateGUI();
}

void PadSettingCheckBox::UpdateGUI()
{
	((wxCheckBox*)wxcontrol)->SetValue(value > 0);
}

void PadSettingCheckBox::UpdateValue()
{
	// 0.01 so its saved to the ini file as just 1. :(
	value = 0.01 * ((wxCheckBox*)wxcontrol)->GetValue();
}

void PadSettingSpin::UpdateGUI()
{
	((wxSpinCtrl*)wxcontrol)->SetValue((int)(value * 100));
}

void PadSettingSpin::UpdateValue()
{
	value = float(((wxSpinCtrl*)wxcontrol)->GetValue()) / 100;
}

ControlDialog::ControlDialog(GamepadPage* const parent, InputPlugin& plugin, ControllerInterface::ControlReference* const ref)
	: wxDialog(parent, -1, _("Configure Control"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
	, control_reference(ref)
	, m_plugin(plugin)
	, m_parent(parent)
{
	m_devq = m_parent->controller->default_device;

	// GetStrings() sounds slow :/
	//device_cbox = new wxComboBox(this, -1, StrToWxStr(ref->device_qualifier.ToString()), wxDefaultPosition, wxSize(256,-1), parent->device_cbox->GetStrings(), wxTE_PROCESS_ENTER);
	device_cbox = new wxComboBox(this, -1, StrToWxStr(m_devq.ToString()), wxDefaultPosition, wxSize(256,-1), parent->device_cbox->GetStrings(), wxTE_PROCESS_ENTER);

	device_cbox->Bind(wxEVT_COMMAND_COMBOBOX_SELECTED, &ControlDialog::SetDevice, this);
	device_cbox->Bind(wxEVT_COMMAND_TEXT_ENTER, &ControlDialog::SetDevice, this);

	wxStaticBoxSizer* const control_chooser = CreateControlChooser(parent);

	wxStaticBoxSizer* const d_szr = new wxStaticBoxSizer(wxVERTICAL, this, _("Device"));
	d_szr->Add(device_cbox, 0, wxEXPAND|wxALL, 5);

	wxBoxSizer* const szr = new wxBoxSizer(wxVERTICAL);
	szr->Add(d_szr, 0, wxEXPAND|wxLEFT|wxRIGHT|wxTOP, 5);
	szr->Add(control_chooser, 1, wxEXPAND|wxALL, 5);

	SetSizerAndFit(szr);	// needed

	UpdateGUI();
	SetFocus();
}

ControlButton::ControlButton(wxWindow* const parent, ControllerInterface::ControlReference* const _ref, const unsigned int width, const std::string& label)
: wxButton(parent, -1, wxT(""), wxDefaultPosition, wxSize(width,20))
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
	pname += m_plugin.profile_name;

	CFileSearch::XStringVector exts;
	exts.push_back("*.ini");
	CFileSearch::XStringVector dirs;
	dirs.push_back(pname);
	CFileSearch cfs(exts, dirs);
	const CFileSearch::XStringVector& sv = cfs.GetFileNames();

	wxArrayString strs;
	CFileSearch::XStringVector::const_iterator si = sv.begin(),
		se = sv.end();
	for (; si!=se; ++si)
	{
		std::string str(si->begin() + si->find_last_of('/') + 1 , si->end() - 4) ;
		strs.push_back(StrToWxStr(str));
	}

	std::vector< GamepadPage* >::iterator i = m_padpages.begin(),
		e = m_padpages.end();
	for (; i != e; ++i)
	{
		(*i)->profile_cbox->Clear();
		(*i)->profile_cbox->Append(strs);
	}
}

void InputConfigDialog::UpdateControlReferences()
{
	std::vector< GamepadPage* >::iterator i = m_padpages.begin(),
		e = m_padpages.end();
	for (; i != e; ++i)
		(*i)->controller->UpdateReferences(g_controller_interface);
}

void InputConfigDialog::ClickSave(wxCommandEvent& event)
{
	m_plugin.SaveConfig();
	event.Skip();
}

void ControlDialog::UpdateListContents()
{
	control_lbox->Clear();

	Device* const dev = g_controller_interface.FindDevice(m_devq);
	if (dev)
	{
		if (control_reference->is_input)
		{
			// for inputs
			std::vector<Device::Input*>::const_iterator
				i = dev->Inputs().begin(),
				e = dev->Inputs().end();
			for (; i!=e; ++i)
				control_lbox->Append(StrToWxStr((*i)->GetName()));
		}
		else
		{
			// for outputs
			std::vector<Device::Output*>::const_iterator
				i = dev->Outputs().begin(),
				e = dev->Outputs().end();
			for (; i!=e; ++i)
				control_lbox->Append(StrToWxStr((*i)->GetName()));
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
		m_error_label->SetLabel("Syntax error");
		break;
	case EXPRESSION_PARSE_NO_DEVICE:
		m_error_label->SetLabel("Device not found");
		break;
	default:
		m_error_label->SetLabel("");
	}
};

void GamepadPage::UpdateGUI()
{
	device_cbox->SetValue(StrToWxStr(controller->default_device.ToString()));

	std::vector< ControlGroupBox* >::const_iterator g = control_groups.begin(),
		ge = control_groups.end();
	for (; g!=ge; ++g)
	{
		// buttons
		std::vector<ControlButton*>::const_iterator i = (*g)->control_buttons.begin()
			, e = (*g)->control_buttons.end();
		for (; i!=e; ++i) {
			wxString expr = StrToWxStr((*i)->control_reference->expression);
			expr.Replace("&", "&&");
			(*i)->SetLabel(expr);
		}

		// cboxes
		std::vector<PadSetting*>::const_iterator si = (*g)->options.begin()
			, se = (*g)->options.end();
		for (; si!=se; ++si)
			(*si)->UpdateGUI();
	}
}

void GamepadPage::ClearAll(wxCommandEvent&)
{
	// just load an empty ini section to clear everything :P
	IniFile::Section section;
	controller->LoadConfig(&section);

	// no point in using the real ControllerInterface i guess
	ControllerInterface face;

	std::lock_guard<std::recursive_mutex> lk(m_plugin.controls_lock);
	controller->UpdateReferences(face);

	UpdateGUI();
}

void GamepadPage::LoadDefaults(wxCommandEvent&)
{
	controller->LoadDefaults(g_controller_interface);

	std::lock_guard<std::recursive_mutex> lk(m_plugin.controls_lock);
	controller->UpdateReferences(g_controller_interface);

	UpdateGUI();
}

bool ControlDialog::Validate()
{
	control_reference->expression = WxStrToStr(textctrl->GetValue());

	std::lock_guard<std::recursive_mutex> lk(m_plugin.controls_lock);
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
	std::lock_guard<std::recursive_mutex> lk(m_plugin.controls_lock);
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

	std::lock_guard<std::recursive_mutex> lk(m_plugin.controls_lock);
	g_controller_interface.UpdateReference(control_reference, m_parent->controller->default_device);

	UpdateGUI();
}

inline bool IsAlphabetic(wxString &str)
{
	for (wxString::const_iterator it = str.begin(); it != str.end(); ++it)
		if (!isalpha(*it))
			return false;
	return true;
}

inline void GetExpressionForControl(wxString &expr,
				    wxString &control_name,
				    DeviceQualifier *control_device = NULL,
				    DeviceQualifier *default_device = NULL)
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

	std::lock_guard<std::recursive_mutex> lk(m_plugin.controls_lock);
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
		expr = wxString::Format(" %c %s", op, device_expr);

	textctrl->WriteText(expr);
	control_reference->expression = textctrl->GetValue();

	std::lock_guard<std::recursive_mutex> lk(m_plugin.controls_lock);
	g_controller_interface.UpdateReference(control_reference, m_parent->controller->default_device);

	UpdateGUI();
}

void GamepadPage::AdjustSetting(wxCommandEvent& event)
{
	std::lock_guard<std::recursive_mutex> lk(m_plugin.controls_lock);
	((PadSetting*)((wxControl*)event.GetEventObject())->GetClientData())->UpdateValue();
}

void GamepadPage::AdjustControlOption(wxCommandEvent&)
{
	std::lock_guard<std::recursive_mutex> lk(m_plugin.controls_lock);
	m_control_dialog->control_reference->range = (ControlState)(m_control_dialog->range_slider->GetValue()) / SLIDER_TICK_COUNT;
}

void GamepadPage::ConfigControl(wxEvent& event)
{
	m_control_dialog = new ControlDialog(this, m_plugin, ((ControlButton*)event.GetEventObject())->control_reference);
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

	std::lock_guard<std::recursive_mutex> lk(m_plugin.controls_lock);
	controller->UpdateReferences(g_controller_interface);

	// update changes
	UpdateGUI();
}

void ControlDialog::DetectControl(wxCommandEvent& event)
{
	wxButton* const btn = (wxButton*)event.GetEventObject();
	const wxString lbl = btn->GetLabel();

	Device* const dev = g_controller_interface.FindDevice(m_devq);
	if (dev)
	{
		btn->SetLabel(_("[ waiting ]"));

		// apparently, this makes the "waiting" text work on Linux
		wxTheApp->Yield();

		std::lock_guard<std::recursive_mutex> lk(m_plugin.controls_lock);
		Device::Control* const ctrl = control_reference->Detect(DETECT_WAIT_TIME, dev);

		// if we got input, select it in the list
		if (ctrl)
			SelectControl(ctrl->GetName());

		btn->SetLabel(lbl);
	}
}

void GamepadPage::DetectControl(wxCommandEvent& event)
{
	ControlButton* btn = (ControlButton*)event.GetEventObject();

	// find device :/
	Device* const dev = g_controller_interface.FindDevice(controller->default_device);
	if (dev)
	{
		btn->SetLabel(_("[ waiting ]"));

		// apparently, this makes the "waiting" text work on Linux
		wxTheApp->Yield();

		std::lock_guard<std::recursive_mutex> lk(m_plugin.controls_lock);
		Device::Control* const ctrl = btn->control_reference->Detect(DETECT_WAIT_TIME, dev);

		// if we got input, update expression and reference
		if (ctrl)
		{
			wxString control_name = ctrl->GetName();
			wxString expr;
			GetExpressionForControl(expr, control_name);
			btn->control_reference->expression = expr;
			g_controller_interface.UpdateReference(btn->control_reference, controller->default_device);
		}
	}

	UpdateGUI();
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
	select_button->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &ControlDialog::SetSelectedControl, this);

	wxButton* const not_button = new  wxButton(this, -1, _("! NOT"), wxDefaultPosition);
	not_button->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &ControlDialog::AppendControl, this);

	wxButton* const or_button = new  wxButton(this, -1, _("| OR"), wxDefaultPosition);
	or_button->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &ControlDialog::AppendControl, this);

	control_lbox = new wxListBox(this, -1, wxDefaultPosition, wxSize(-1, 64));

	wxBoxSizer* const button_sizer = new wxBoxSizer(wxVERTICAL);
	button_sizer->Add(detect_button, 1, 0, 5);
	button_sizer->Add(select_button, 1, 0, 5);
	button_sizer->Add(or_button, 1, 0, 5);

	if (control_reference->is_input)
	{
		// TODO: check if && is good on other OS
		wxButton* const and_button = new  wxButton(this, -1, _("&& AND"), wxDefaultPosition);
		wxButton* const add_button = new  wxButton(this, -1, _("+ ADD"), wxDefaultPosition);

		and_button->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &ControlDialog::AppendControl, this);
		add_button->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &ControlDialog::AppendControl, this);

		button_sizer->Add(and_button, 1, 0, 5);
		button_sizer->Add(not_button, 1, 0, 5);
		button_sizer->Add(add_button, 1, 0, 5);
	}

	range_slider = new wxSlider(this, -1, SLIDER_TICK_COUNT, -SLIDER_TICK_COUNT * 5, SLIDER_TICK_COUNT * 5, wxDefaultPosition, wxDefaultSize, wxSL_TOP | wxSL_LABELS /*| wxSL_AUTOTICKS*/);

	range_slider->SetValue((int)(control_reference->range * SLIDER_TICK_COUNT));

	detect_button->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &ControlDialog::DetectControl, this);
	clear_button->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &ControlDialog::ClearControl, this);

	range_slider->Bind(wxEVT_SCROLL_CHANGED, &GamepadPage::AdjustControlOption, parent);
	wxStaticText* const range_label = new wxStaticText(this, -1, _("Range"));

	m_bound_label = new wxStaticText(this, -1, wxT(""));
	m_error_label = new wxStaticText(this, -1, wxT(""));

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
	if (false == name.empty())
	{
		// TODO: check for dumb characters maybe

		path = File::GetUserPath(D_CONFIG_IDX);
		path += PROFILES_PATH;
		path += m_plugin.profile_name;
		path += '/';
		path += WxStrToStr(profile_cbox->GetValue());
		path += ".ini";
	}
}

void GamepadPage::LoadProfile(wxCommandEvent&)
{
	std::string fname;
	GamepadPage::GetProfilePath(fname);

	if (false == File::Exists(fname))
		return;

	IniFile inifile;
	inifile.Load(fname);

	std::lock_guard<std::recursive_mutex> lk(m_plugin.controls_lock);
	controller->LoadConfig(inifile.GetOrCreateSection("Profile"));
	controller->UpdateReferences(g_controller_interface);

	UpdateGUI();
}

void GamepadPage::SaveProfile(wxCommandEvent&)
{
	std::string fname;
	GamepadPage::GetProfilePath(fname);
	File::CreateFullPath(fname);

	if (false == fname.empty())
	{
		IniFile inifile;
		controller->SaveConfig(inifile.GetOrCreateSection("Profile"));
		inifile.Save(fname);
		
		m_config_dialog->UpdateProfileComboBox();
	}
	else
	{
		PanicAlertT("You must enter a valid profile name.");
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
	std::vector< GamepadPage* >::iterator i = m_padpages.begin(),
		e = m_padpages.end();
	DeviceQualifier dq;
	for (; i != e; ++i)
	{
		(*i)->device_cbox->Clear();
		std::vector<Device*>::const_iterator
			di = g_controller_interface.Devices().begin(),
			de = g_controller_interface.Devices().end();
		for (; di!=de; ++di)
		{
			dq.FromDevice(*di);
			(*i)->device_cbox->Append(StrToWxStr(dq.ToString()));
		}
		(*i)->device_cbox->SetValue(StrToWxStr((*i)->controller->default_device.ToString()));
	}
}

void GamepadPage::RefreshDevices(wxCommandEvent&)
{
	std::lock_guard<std::recursive_mutex> lk(m_plugin.controls_lock);

	// refresh devices
	g_controller_interface.Shutdown();
	g_controller_interface.Initialize();

	// update all control references
	m_config_dialog->UpdateControlReferences();

	// update device cbox
	m_config_dialog->UpdateDeviceComboBox();
}

ControlGroupBox::~ControlGroupBox()
{
	std::vector<PadSetting*>::const_iterator
		i = options.begin(),
		e = options.end();
	for (; i!=e; ++i)
		delete *i;
}

ControlGroupBox::ControlGroupBox(ControllerEmu::ControlGroup* const group, wxWindow* const parent, GamepadPage* const eventsink)
	: wxBoxSizer(wxVERTICAL)
	, control_group(group)
{
	static_bitmap = NULL;

	wxFont m_SmallFont(7, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
	std::vector<ControllerEmu::ControlGroup::Control*>::iterator
		ci = group->controls.begin(),
		ce = group->controls.end();
	for (; ci != ce; ++ci)
	{

		wxStaticText* const label = new wxStaticText(parent, -1, wxGetTranslation(StrToWxStr((*ci)->name)));
		
		ControlButton* const control_button = new ControlButton(parent, (*ci)->control_ref, 80);
		control_button->SetFont(m_SmallFont);

		control_buttons.push_back(control_button);

		if ((*ci)->control_ref->is_input)
		{
			control_button->SetToolTip(_("Left-click to detect input.\nMiddle-click to clear.\nRight-click for more options."));
			control_button->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &GamepadPage::DetectControl, eventsink);
		}
		else
		{
			control_button->SetToolTip(_("Left/Right-click for more options.\nMiddle-click to clear."));
			control_button->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &GamepadPage::ConfigControl, eventsink);
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

			std::vector< ControllerEmu::ControlGroup::Setting* >::const_iterator
				i = group->settings.begin(),
				e = group->settings.end();

			wxBoxSizer* const szr = new wxBoxSizer(wxVERTICAL);
			for (; i!=e; ++i)
			{
				PadSettingSpin* setting = new PadSettingSpin(parent, *i);
				setting->wxcontrol->Bind(wxEVT_COMMAND_SPINCTRL_UPDATED, &GamepadPage::AdjustSetting, eventsink);
				options.push_back(setting);
				szr->Add(new wxStaticText(parent, -1, wxGetTranslation(StrToWxStr((*i)->name))));
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

			PadSettingSpin* const threshold_cbox = new PadSettingSpin(parent, group->settings[0]);
			threshold_cbox->wxcontrol->Bind(wxEVT_COMMAND_SPINCTRL_UPDATED, &GamepadPage::AdjustSetting, eventsink);

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

			std::vector<ControllerEmu::ControlGroup::Setting*>::const_iterator
				i = group->settings.begin(),
				e = group->settings.end();
			for (; i!=e; ++i)
			{
				PadSettingSpin* setting = new PadSettingSpin(parent, *i);
				setting->wxcontrol->Bind(wxEVT_COMMAND_SPINCTRL_UPDATED, &GamepadPage::AdjustSetting, eventsink);
				options.push_back(setting);
				wxBoxSizer* const szr = new wxBoxSizer(wxHORIZONTAL);
				szr->Add(new wxStaticText(parent, -1, wxGetTranslation(StrToWxStr((*i)->name))), 0, wxCENTER|wxRIGHT, 3);
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

			attachments->wxcontrol->Bind(wxEVT_COMMAND_CHOICE_SELECTED, &GamepadPage::AdjustSetting, eventsink);
			configure_btn->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &GamepadPage::ConfigExtension, eventsink);

			Add(attachments->wxcontrol, 0, wxTOP|wxLEFT|wxRIGHT|wxEXPAND, 3);
			Add(configure_btn, 0, wxALL|wxEXPAND, 3);
		}
		break;
	case GROUP_TYPE_UDPWII:
		{
			wxButton* const btn = new UDPConfigButton(parent, (UDPWrapper*)group);
			btn->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &GamepadPage::ConfigUDPWii, eventsink);
			Add(btn, 0, wxALL|wxEXPAND, 3);
		}
		break;
	default:
		{
			//options

			std::vector<ControllerEmu::ControlGroup::Setting*>::const_iterator
				i = group->settings.begin(),
				e = group->settings.end();
			for (; i!=e; ++i)
			{
				PadSettingCheckBox* setting_cbox = new PadSettingCheckBox(parent, (*i)->value, (*i)->name);
				setting_cbox->wxcontrol->Bind(wxEVT_COMMAND_CHECKBOX_CLICKED, &GamepadPage::AdjustSetting, eventsink);
				options.push_back(setting_cbox);

				Add(setting_cbox->wxcontrol, 0, wxALL|wxLEFT, 5);

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

	wxBoxSizer* stacked_groups = NULL;
	for (unsigned int i = 0; i < controller->groups.size(); ++i)
	{
		ControlGroupBox* control_group_box = new ControlGroupBox(controller->groups[i], parent, eventsink);
		wxStaticBoxSizer *control_group =
			new wxStaticBoxSizer(wxVERTICAL, parent, wxGetTranslation(StrToWxStr(controller->groups[i]->name)));
		control_group->Add(control_group_box);

		const size_t grp_size = controller->groups[i]->controls.size() + controller->groups[i]->settings.size();
		col_size += grp_size;
		if (col_size > 8 || NULL == stacked_groups)
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

GamepadPage::GamepadPage(wxWindow* parent, InputPlugin& plugin, const unsigned int pad_num, InputConfigDialog* const config_dialog)
	: wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize)
	,controller(plugin.controllers[pad_num])
	, m_config_dialog(config_dialog)
	, m_plugin(plugin)
{

	wxBoxSizer* control_group_sizer = new ControlGroupsSizer(m_plugin.controllers[pad_num], this, this, &control_groups);

	wxStaticBoxSizer* profile_sbox = new wxStaticBoxSizer(wxHORIZONTAL, this, _("Profile"));

	// device chooser

	wxStaticBoxSizer* const device_sbox = new wxStaticBoxSizer(wxHORIZONTAL, this, _("Device"));

	device_cbox = new wxComboBox(this, -1, wxT(""), wxDefaultPosition, wxSize(64,-1));
	device_cbox->ToggleWindowStyle(wxTE_PROCESS_ENTER);

	wxButton* refresh_button = new wxButton(this, -1, _("Refresh"), wxDefaultPosition, wxSize(60,-1));

	device_cbox->Bind(wxEVT_COMMAND_COMBOBOX_SELECTED, &GamepadPage::SetDevice, this);
	device_cbox->Bind(wxEVT_COMMAND_TEXT_ENTER, &GamepadPage::SetDevice, this);
	refresh_button->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &GamepadPage::RefreshDevices, this);

	device_sbox->Add(device_cbox, 1, wxLEFT|wxRIGHT, 3);
	device_sbox->Add(refresh_button, 0, wxRIGHT|wxBOTTOM, 3);

	wxButton* const default_button = new wxButton(this, -1, _("Default"), wxDefaultPosition, wxSize(48,-1));
	wxButton* const clearall_button = new wxButton(this, -1, _("Clear"), wxDefaultPosition, wxSize(58,-1));

	wxStaticBoxSizer* const clear_sbox = new wxStaticBoxSizer(wxHORIZONTAL, this, _("Reset"));
	clear_sbox->Add(default_button, 1, wxLEFT, 3);
	clear_sbox->Add(clearall_button, 1, wxRIGHT, 3);

	clearall_button->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &GamepadPage::ClearAll, this);
	default_button->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &GamepadPage::LoadDefaults, this);

	profile_cbox = new wxComboBox(this, -1, wxT(""), wxDefaultPosition, wxSize(64,-1));

	wxButton* const pload_btn = new wxButton(this, -1, _("Load"), wxDefaultPosition, wxSize(48,-1));
	wxButton* const psave_btn = new wxButton(this, -1, _("Save"), wxDefaultPosition, wxSize(48,-1));
	wxButton* const pdelete_btn = new wxButton(this, -1, _("Delete"), wxDefaultPosition, wxSize(60,-1));

	pload_btn->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &GamepadPage::LoadProfile, this);
	psave_btn->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &GamepadPage::SaveProfile, this);
	pdelete_btn->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &GamepadPage::DeleteProfile, this);

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

	SetSizerAndFit(mapping);	// needed
	Layout();
};


InputConfigDialog::InputConfigDialog(wxWindow* const parent, InputPlugin& plugin, const std::string& name, const int tab_num)
	: wxDialog(parent, wxID_ANY, wxGetTranslation(StrToWxStr(name)), wxPoint(128,-1), wxDefaultSize)
	, m_plugin(plugin)
{
	m_pad_notebook = new wxNotebook(this, -1, wxDefaultPosition, wxDefaultSize, wxNB_DEFAULT);
	for (unsigned int i = 0; i < std::min(plugin.controllers.size(), (size_t)MAX_WIIMOTES); ++i)
	{
		GamepadPage* gp = new GamepadPage(m_pad_notebook, m_plugin, i, this);
		m_padpages.push_back(gp);
		m_pad_notebook->AddPage(gp, wxString::Format(wxT("%s %u"), wxGetTranslation(StrToWxStr(m_plugin.gui_name)), 1+i));
	}

	m_pad_notebook->SetSelection(tab_num);

	UpdateDeviceComboBox();
	UpdateProfileComboBox();

	Bind(wxEVT_COMMAND_BUTTON_CLICKED, &InputConfigDialog::ClickSave, this, wxID_OK);

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
