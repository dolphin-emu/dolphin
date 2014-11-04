// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <wx/checkbox.h>
#include <wx/combobox.h>
#include <wx/gbsizer.h>
#include <wx/notebook.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

#include "DolphinWX/WXInputBase.h"

#include "Core/Core.h"

#include "DolphinWX/ConfigVR.h"
#include "DolphinWX/Main.h"
#include "DolphinWX/WxUtils.h"

#include "InputCommon/HotkeysXInput.h"

#include <wx/msgdlg.h>


BEGIN_EVENT_TABLE(CConfigVR, wxDialog)

EVT_CLOSE(CConfigVR::OnClose)

END_EVENT_TABLE()

CConfigVR::CConfigVR(wxWindow* parent, wxWindowID id, const wxString& title,
		const wxPoint& position, const wxSize& size, long style)
	: wxDialog(parent, id, title, position, size, style)
{
	CreateGUIControls();

	UpdateDeviceComboBox();
}

CConfigVR::~CConfigVR()
{
}

namespace
{
	const float INPUT_DETECT_THRESHOLD = 0.55f;
}

// Used to restrict changing of some options while emulator is running
void CConfigVR::UpdateGUI()
{
	//if (Core::IsRunning())
	//{
		// Disable the Core stuff on GeneralPage
		//CPUThread->Disable();
		//SkipIdle->Disable();
		//EnableCheats->Disable();

		//CPUEngine->Disable();
		//_NTSCJ->Disable();
	//}

	device_cbox->SetValue(StrToWxStr(default_device.ToString())); //Update ComboBox
	
	int i = 0;

	//Update Buttons
	for (wxButton* button : m_Button_VRSettings)
	{
		SetButtonText(i,
			SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettingsKBM[i],
			WxUtils::WXKeyToString(SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettings[i]),
			WxUtils::WXKeymodToString(SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettingsModifier[i]),
			HotkeysXInput::GetwxStringfromXInputIni(SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettingsXInputMapping[i]));
		i++;
	}
}

#define VR_NUM_COLUMNS 2

void CConfigVR::CreateGUIControls()
{

	const wxString pageNames[] =
	{
		_("VR Freelook"),
		_("VR Options")
	};

	const wxString VRText[] =
	{
		_("Reset Camera"),
		_("Camera Forward"),
		_("Camera Backward"),
		_("Camera Left"),
		_("Camera Right"),
		_("Camera Up"),
		_("Camera Down"),

		_("Permanent Camera Forward"),
		_("Permanent Camera Backward"),
		_("Larger Scale"),
		_("Smaller Scale"),
		_("Tilt Camera Up"),
		_("Tilt Camera Down"),

		_("HUD Forward"),
		_("HUD Backward"),
		_("HUD Thicker"),
		_("HUD Thinner"),
		_("HUD 3D Items Closer"),
		_("HUD 3D Items Further"),

		_("2D Screen Larger"),
		_("2D Screen Smaller"),
		_("2D Camera Forward"),
		_("2D Camera Backward"),
		//_("2D Screen Left"), //Doesn't exist right now?
		//_("2D Screen Right"), //Doesn't exist right now?
		_("2D Camera Up"),
		_("2D Camera Down"),
		_("2D Camera Tilt Up"),
		_("2D Camera Tilt Down"),
		_("2D Screen Thicker"),
		_("2D Screen Thinner"),
		


	};

	const int page_breaks[3] = {VR_POSITION_RESET, NUM_VR_OPTIONS, NUM_VR_OPTIONS};

	// Configuration controls sizes
	wxSize size(100,20);
	// A small type font
	wxFont m_SmallFont(7, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);

	wxNotebook *Notebook = new wxNotebook(this, wxID_ANY);

	button_already_clicked = false; //Used to determine whether a button has already been clicked.  If it has, don't allow more buttons to be clicked.

	for (int j = 0; j < 2; j++)
	{
		wxPanel *Page = new wxPanel(Notebook, wxID_ANY);
		Notebook->AddPage(Page, pageNames[j]);

		wxGridBagSizer *sVRKeys = new wxGridBagSizer();

		// Header line
		if (j == 0){
			for (int i = 0; i < VR_NUM_COLUMNS; i++)
			{
				wxBoxSizer *HeaderSizer = new wxBoxSizer(wxHORIZONTAL);
				wxStaticText *StaticTextHeader = new wxStaticText(Page, wxID_ANY, _("Action"));
				HeaderSizer->Add(StaticTextHeader, 1, wxALL, 2);
				StaticTextHeader = new wxStaticText(Page, wxID_ANY, _("Key"), wxDefaultPosition, size);
				HeaderSizer->Add(StaticTextHeader, 0, wxALL, 2);
				sVRKeys->Add(HeaderSizer, wxGBPosition(0, i), wxDefaultSpan, wxEXPAND | wxLEFT, (i > 0) ? 30 : 1);
			}
		}

		int column_break = (page_breaks[j+1] + page_breaks[j] + 1) / 2;

		for (int i = page_breaks[j]; i < page_breaks[j+1]; i++)
		{
			// Text for the action
			wxStaticText *stHotkeys = new wxStaticText(Page, wxID_ANY, VRText[i]);

			// Key selection button
			m_Button_VRSettings[i] = new wxButton(Page, i, wxEmptyString, wxDefaultPosition, size);

			m_Button_VRSettings[i]->SetFont(m_SmallFont);
			m_Button_VRSettings[i]->SetToolTip(_("Left click to change the controlling key (assign space to clear).\nMiddle click to clear.\nRight click to choose XInput Combinations (if a gamepad is detected)"));
			SetButtonText(i, 
				SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettingsKBM[i],
				WxUtils::WXKeyToString(SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettings[i]),
				WxUtils::WXKeymodToString(SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettingsModifier[i]),
				HotkeysXInput::GetwxStringfromXInputIni(SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettingsXInputMapping[i]));

			m_Button_VRSettings[i]->Bind(wxEVT_BUTTON, &CConfigVR::DetectControl, this);
			m_Button_VRSettings[i]->Bind(wxEVT_MIDDLE_DOWN, &CConfigVR::ClearControl, this);
			m_Button_VRSettings[i]->Bind(wxEVT_RIGHT_UP, &CConfigVR::ConfigControl, this);

			wxBoxSizer *sVRKey = new wxBoxSizer(wxHORIZONTAL);
			sVRKey->Add(stHotkeys, 1, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxALL, 2);
			sVRKey->Add(m_Button_VRSettings[i], 0, wxALL, 2);
			sVRKeys->Add(sVRKey,
				wxGBPosition((i < column_break) ? i - page_breaks[j] + 1 : i - column_break + 1,
				(i < column_break) ? 0 : 1),
				wxDefaultSpan, wxEXPAND | wxLEFT, (i < column_break) ? 1 : 30);
		}

		if (j == 0) {

			wxStaticBoxSizer *sVRKeyBox = new wxStaticBoxSizer(wxVERTICAL, Page, _("VR Camera Controls"));
			sVRKeyBox->Add(sVRKeys);

			wxStaticBoxSizer* const device_sbox = new wxStaticBoxSizer(wxHORIZONTAL, Page, _("Device"));

			device_cbox = new wxComboBox(Page, -1, "", wxDefaultPosition, wxSize(64, -1));
			device_cbox->ToggleWindowStyle(wxTE_PROCESS_ENTER);

			wxButton* refresh_button = new wxButton(Page, -1, _("Refresh"), wxDefaultPosition, wxSize(60, -1));

			device_cbox->Bind(wxEVT_COMBOBOX, &CConfigVR::SetDevice, this);
			device_cbox->Bind(wxEVT_TEXT_ENTER, &CConfigVR::SetDevice, this);
			refresh_button->Bind(wxEVT_BUTTON, &CConfigVR::RefreshDevices, this);

			device_sbox->Add(device_cbox, 4, wxLEFT | wxRIGHT, 3);
			device_sbox->Add(refresh_button, 1, wxLEFT | wxRIGHT, 3);

			wxBoxSizer* const sPage = new wxBoxSizer(wxVERTICAL);
			sPage->Add(device_sbox, 0, wxEXPAND | wxALL, 5);
			sPage->Add(sVRKeyBox, 0, wxEXPAND | wxALL, 5);
			Page->SetSizer(sPage);

		}
	}

	wxBoxSizer *sMainSizer = new wxBoxSizer(wxVERTICAL);
	sMainSizer->Add(Notebook, 0, wxEXPAND | wxALL, 5);
	sMainSizer->Add(CreateButtonSizer(wxOK), 0, wxEXPAND | wxLEFT | wxRIGHT | wxDOWN, 5);
	SetSizerAndFit(sMainSizer);
	SetFocus();

}

//Poll devices available and put them in the device combo box.
void CConfigVR::UpdateDeviceComboBox()
{
	ciface::Core::DeviceQualifier dq;
	device_cbox->Clear();

	int i = 0;
	for (ciface::Core::Device* d : g_controller_interface.Devices()) //For Every Device Attached
	{
		dq.FromDevice(d);
		device_cbox->Append(StrToWxStr(dq.ToString())); //Put Name of Device into Combo Box
		if (i == 0){
			device_cbox->SetValue(StrToWxStr(dq.ToString())); //Show first device detected in Combo Box by default
		}
		i++;
	}

	default_device.FromString(WxStrToStr(device_cbox->GetValue())); //Set device to be used to match what's selected in the Combo Box
}

void CConfigVR::OnClose(wxCloseEvent& WXUNUSED (event))
{
	EndModal(wxID_OK);
}

void CConfigVR::OnOk(wxCommandEvent& WXUNUSED (event))
{
	Close();

	// Save the config. Dolphin crashes too often to only save the settings on closing
	//SConfig::GetInstance().SaveSettings();
}

//On Combo Box Selection
void CConfigVR::SetDevice(wxCommandEvent&)
{
	default_device.FromString(WxStrToStr(device_cbox->GetValue()));

	// show user what it was validated as
	device_cbox->SetValue(StrToWxStr(default_device.ToString()));

	// this will set all the controls to this default device
	//vr_hotkey_controller->UpdateDefaultDevice();

	// update references
	//std::lock_guard<std::recursive_mutex> lk(m_config.controls_lock);
	//vr_hotkey_controller->UpdateReferences(g_controller_interface);
}

//On Refresh Button Click
void CConfigVR::RefreshDevices(wxCommandEvent&)
{
	//std::lock_guard<std::recursive_mutex> lk(m_config.controls_lock);

	// refresh devices
	g_controller_interface.Reinitialize();

	// update all control references
	//UpdateControlReferences();

	// update device cbox
	UpdateDeviceComboBox();
}

// Input button clicked
void CConfigVR::OnButtonClick(wxCommandEvent& event)
{
	event.Skip();

	//if (m_ButtonMappingTimer.IsRunning())
	//	return;

	wxTheApp->Bind(wxEVT_KEY_DOWN, &CConfigVR::OnKeyDown, this);

	// Get the button
	ClickedButton = (wxButton *)event.GetEventObject();
	SetEscapeId(wxID_CANCEL);  //This stops escape from exiting the whole ConfigVR box.
	// Save old label so we can revert back
	OldLabel = ClickedButton->GetLabel();
	ClickedButton->SetWindowStyle(wxWANTS_CHARS);
	ClickedButton->SetLabel(_("<Press Key>"));
	//DoGetButtons(ClickedButton->GetId());
}

void CConfigVR::OnKeyDown(wxKeyEvent& event)
{
	if (ClickedButton != nullptr)
	{
		// Save the key
		g_Pressed = event.GetKeyCode();
		g_Modkey = event.GetModifiers();

		// Don't allow modifier keys
		if (g_Pressed == WXK_CONTROL || g_Pressed == WXK_ALT ||
			g_Pressed == WXK_SHIFT || g_Pressed == WXK_COMMAND)
			return;

		// Use the space key to set a blank key
		if (g_Pressed == WXK_SPACE)
		{
			SaveButtonMapping(ClickedButton->GetId(), true, -1, 0);
			SaveXInputBinary(ClickedButton->GetId(), true, 0);
			SetButtonText(ClickedButton->GetId(), true, wxString());
		}
		// Cancel and restore the old label if escape is hit.
		else if (g_Pressed == WXK_ESCAPE){
			ClickedButton->SetLabel(OldLabel);
			button_already_clicked = false;
			return;
		}
		else
		{
			// Check if the hotkey combination was already applied to another button
			// and unapply it if necessary.
			for (wxButton* btn : m_Button_VRSettings)
			{
				// We compare against this to see if we have a duplicate bind attempt.
				wxString existingHotkey = btn->GetLabel();

				wxString tentativeModKey = WxUtils::WXKeymodToString(g_Modkey);
				wxString tentativePressedKey = WxUtils::WXKeyToString(g_Pressed);
				wxString tentativeHotkey(tentativeModKey + tentativePressedKey);

				// Found a button that already has this binding. Unbind it.
				if (tentativeHotkey == existingHotkey)
				{
					SaveButtonMapping(btn->GetId(), true, -1, 0); //TO DO: Should this set to true? Probably should be an if statement before this...
					SetButtonText(btn->GetId(), true, wxString());
				}
			}

			// Proceed to apply the binding to the selected button.
			SetButtonText(ClickedButton->GetId(),
				true,
				WxUtils::WXKeyToString(g_Pressed),
				WxUtils::WXKeymodToString(g_Modkey));
			SaveButtonMapping(ClickedButton->GetId(), true, g_Pressed, g_Modkey);
		}
	}

	EndGetButtons();
}

// Update the textbox for the buttons
void CConfigVR::SetButtonText(int id, bool KBM, const wxString &keystr, const wxString &modkeystr, const wxString &XInputMapping)
{
	if (KBM == true){
		m_Button_VRSettings[id]->SetLabel(modkeystr + keystr);
	}
	else {
		m_Button_VRSettings[id]->SetLabel(XInputMapping);
	}
}

// Save keyboard key mapping
void CConfigVR::SaveButtonMapping(int Id, bool KBM, int Key, int Modkey)
{
	SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettingsKBM[Id] = KBM;
	SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettings[Id] = Key;
	SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettingsModifier[Id] = Modkey;
	SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettingsXInputMapping[Id] = 0;
}

void CConfigVR::SaveXInputBinary(int Id, bool KBM, u32 Key)
{
	SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettingsKBM[Id] = KBM;
	SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettingsXInputMapping[Id] = Key;
}

void CConfigVR::EndGetButtons()
{
	wxTheApp->Unbind(wxEVT_KEY_DOWN, &CConfigVR::OnKeyDown, this);
	button_already_clicked = false;
	ClickedButton = nullptr;
	SetEscapeId(wxID_ANY);
}

void CConfigVR::EndGetButtonsXInput()
{
	button_already_clicked = false;
	ClickedButton = nullptr;
	SetEscapeId(wxID_ANY);
}

void CConfigVR::ConfigControl(wxEvent& event)
{
	ClickedButton = (wxButton *)event.GetEventObject();
	
	ciface::Core::DeviceQualifier dq;

	int count = 0;

	for (ciface::Core::Device* d : g_controller_interface.Devices()) //For Every Device Attached
	{
		dq.FromDevice(d);
		if (dq.source == "XInput"){
			m_vr_dialog = new VRDialog(this, ClickedButton->GetId());
			m_vr_dialog->ShowModal();
			m_vr_dialog->Destroy();
			count++;
			break;
		}
	}

	if (count == 0){
		wxMessageDialog m_no_xinput(
			this,
			_("No XInput Device Detected.\nAttach an XInput Device to use this Feature."),
			_("No XInput Device"),
			wxOK | wxSTAY_ON_TOP,
			wxDefaultPosition);

		m_no_xinput.ShowModal();
	}

	// update changes that were made in the dialog
	UpdateGUI();
}

void CConfigVR::ClearControl(wxEvent& event)
{

	ClickedButton = (wxButton *)event.GetEventObject();
	SaveButtonMapping(ClickedButton->GetId(), true, -1, 0);
	SaveXInputBinary(ClickedButton->GetId(), true, 0);
	SetButtonText(ClickedButton->GetId(), true, wxString());
   
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
		expr = wxString::Format("%s", expr);
}

void CConfigVR::DetectControl(wxCommandEvent& event)
{
	if (button_already_clicked){ //Stop the user from being able to select multiple buttons at once
		return;
	}
	else {
		button_already_clicked = true;
		// find devices
		ciface::Core::Device* const dev = g_controller_interface.FindDevice(default_device);
		if (dev)
		{
			if (default_device.name == "Keyboard Mouse") {
				OnButtonClick(event);
			}
			else if (default_device.source == "XInput") {
				// Get the button
				ClickedButton = (wxButton *)event.GetEventObject();
				SetEscapeId(wxID_CANCEL);  //This stops escape from exiting the whole ConfigVR box.
				// Save old label so we can revert back
				OldLabel = ClickedButton->GetLabel();

				ClickedButton->SetLabel(_("<Press Button>"));

				// This makes the "Press Button" text work on Linux. true (only if needed) prevents crash on Windows
				wxTheApp->Yield(true);

				//std::lock_guard<std::recursive_mutex> lk(m_config.controls_lock);
				ciface::Core::Device::Control* const ctrl = InputDetect(DETECT_WAIT_TIME, dev);

				// if we got input, update expression and reference
				if (ctrl)
				{
					wxString control_name = ctrl->GetName();
					wxString expr;
					GetExpressionForControl(expr, control_name);
					ClickedButton->SetLabel(expr);
					u32 xinput_binary = HotkeysXInput::GetBinaryfromXInputIniStr(expr);
					SaveXInputBinary(ClickedButton->GetId(), false, xinput_binary);
				}
				else {
					ClickedButton->SetLabel(OldLabel);
				}

				EndGetButtonsXInput();
			}
		}
		else {
			button_already_clicked = false;
		}
	}

}


ciface::Core::Device::Control* CConfigVR::InputDetect(const unsigned int ms, ciface::Core::Device* const device)
{
	unsigned int time = 0;
	std::vector<bool> states(device->Inputs().size());

	if (device->Inputs().size() == 0)
		return nullptr;

	// get starting state of all inputs,
	// so we can ignore those that were activated at time of Detect start
	std::vector<ciface::Core::Device::Input*>::const_iterator
		i = device->Inputs().begin(),
		e = device->Inputs().end();
	for (std::vector<bool>::iterator state = states.begin(); i != e; ++i)
		*state++ = ((*i)->GetState() > (1 - INPUT_DETECT_THRESHOLD));

	while (time < ms)
	{
		device->UpdateInput();
		i = device->Inputs().begin();
		for (std::vector<bool>::iterator state = states.begin(); i != e; ++i, ++state)
		{
			// detected an input
			if ((*i)->IsDetectable() && (*i)->GetState() > INPUT_DETECT_THRESHOLD)
			{
				// input was released at some point during Detect call
				// return the detected input
				if (false == *state)
					return *i;
			}
			else if ((*i)->GetState() < (1 - INPUT_DETECT_THRESHOLD))
			{
				*state = false;
			}
		}
		Common::SleepCurrentThread(10); time += 10;
	}

	// no input was detected
	return nullptr;
}

VRDialog::VRDialog(CConfigVR* const parent, int from_button)
	: wxDialog(parent, -1, _("Configure Control"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
	wxStaticBoxSizer* const main_szr = new wxStaticBoxSizer(wxVERTICAL, this, "Input");

	button_id = from_button;

	ciface::Core::DeviceQualifier dq;
	for (ciface::Core::Device* d : g_controller_interface.Devices()) //For Every Device Attached
	{
		dq.FromDevice(d);
		if (dq.source == "XInput"){
			for (ciface::Core::Device::Input* input : d->Inputs())
			{
				wxCheckBox  *XInputCheckboxes = new wxCheckBox(this, -1, wxGetTranslation(StrToWxStr(input->GetName())), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
				XInputCheckboxes->Bind(wxEVT_CHECKBOX, &VRDialog::OnCheckBox, this);
				if (HotkeysXInput::IsXInputButtonSet(input->GetName(), button_id)){
					XInputCheckboxes->SetValue(true);
				}
				main_szr->Add(XInputCheckboxes, 1, wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL | wxALL, 2);
			}
		}

	}

	wxBoxSizer* const szr = new wxBoxSizer(wxVERTICAL);
	szr->Add(main_szr, 1, wxEXPAND | wxALL, 5);
	szr->Add(CreateButtonSizer(wxOK), 0, wxEXPAND | wxLEFT | wxRIGHT | wxDOWN, 5);

	SetSizerAndFit(szr); // needed
	SetFocus();
}

void VRDialog::OnCheckBox(wxCommandEvent& event)
{
	wxCheckBox* checkbox = (wxCheckBox*)event.GetEventObject();
	u32 single_button_mask = HotkeysXInput::GetBinaryfromXInputIniStr(checkbox->GetLabel());
	u32 value = SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettingsXInputMapping[button_id];
	if (checkbox->IsChecked()){
		value |= single_button_mask;
	}
	else {
		value &= ~single_button_mask;
	}

	SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettingsKBM[button_id] = FALSE;
	SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettingsXInputMapping[button_id] = value;

	event.Skip();
}