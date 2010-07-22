
#include "GeckoCodeDiag.h"

#define _connect_macro_(b, f, c, s)	(b)->Connect(wxID_ANY, (c), wxCommandEventHandler(f), (wxObject*)0, (wxEvtHandler*)s)

namespace Gecko
{

static const wxString wxstr_name(wxT("Name: ")),
	wxstr_description(wxT("Description: ")),
	wxstr_creator(wxT("Creator: "));

CodeConfigPanel::CodeConfigPanel(wxWindow* const parent)
	: wxPanel(parent, -1, wxDefaultPosition, wxDefaultSize)
{
	m_listbox_gcodes = new wxCheckListBox(this, -1, wxDefaultPosition, wxDefaultSize);
	_connect_macro_(m_listbox_gcodes, CodeConfigPanel::UpdateInfoBox, wxEVT_COMMAND_LISTBOX_SELECTED, this);
	_connect_macro_(m_listbox_gcodes, CodeConfigPanel::ToggleCode, wxEVT_COMMAND_CHECKLISTBOX_TOGGLED, this);

	m_infobox.label_name = new wxStaticText(this, -1, wxstr_name);
	m_infobox.label_creator = new wxStaticText(this, -1, wxstr_creator);
	m_infobox.label_description = new wxStaticText(this, -1, wxstr_description);
	m_infobox.listbox_codes = new wxListBox(this, -1, wxDefaultPosition, wxSize(-1, 64));

	// TODO: buttons to add/edit codes

	// sizers
	wxBoxSizer* const sizer_infobox = new wxBoxSizer(wxVERTICAL);
	sizer_infobox->Add(m_infobox.label_name, 0, wxLEFT | wxBOTTOM, 5);
	sizer_infobox->Add(m_infobox.label_creator, 0, wxLEFT | wxBOTTOM, 5);
	sizer_infobox->Add(m_infobox.label_description, 0, wxLEFT | wxBOTTOM, 5);
	sizer_infobox->Add(m_infobox.listbox_codes, 0, wxLEFT | wxBOTTOM, 5);

	//wxBoxSizer* const sizer_horz = new wxBoxSizer(wxHORIZONTAL);
	//sizer_horz->Add(sizer_infobox, 1, 0);

	// silly
	//if (show_apply_button)
	//{
	//	wxButton* const btn_apply = new wxButton(this, -1, wxT("Apply Changes"), wxDefaultPosition, wxSize(128, -1));
	//	_connect_macro_(btn_apply, CodeConfigPanel::ApplyChanges, wxEVT_COMMAND_BUTTON_CLICKED, this);
	//	sizer_horz->Add(btn_apply, 0, wxALIGN_RIGHT | wxALIGN_BOTTOM);
	//}
	
	wxBoxSizer* const sizer_main = new wxBoxSizer(wxVERTICAL);
	sizer_main->Add(m_listbox_gcodes, 1, wxALL | wxEXPAND, 5);
	sizer_main->Add(sizer_infobox, 0, wxALL | wxEXPAND, 5);

	SetSizerAndFit(sizer_main);
}

void CodeConfigPanel::LoadCodes(const IniFile& inifile)
{
	m_gcodes.clear();
	Gecko::LoadCodes(inifile, m_gcodes);

	m_listbox_gcodes->Clear();
	// add the codes to the listbox
	std::vector<GeckoCode>::const_iterator
		gcodes_iter = m_gcodes.begin(),
		gcodes_end = m_gcodes.end();
	for (; gcodes_iter!=gcodes_end; ++gcodes_iter)
	{
		m_listbox_gcodes->Append(wxString::FromAscii(gcodes_iter->name.c_str()));
		if (gcodes_iter->enabled)
			m_listbox_gcodes->Check(m_listbox_gcodes->GetCount()-1, true);
	}
	
	wxCommandEvent evt;
	UpdateInfoBox(evt);
}

void CodeConfigPanel::ToggleCode(wxCommandEvent& evt)
{
	const int sel = evt.GetInt();	// this right?
	if (sel > -1)
		m_gcodes[sel].enabled = m_listbox_gcodes->IsChecked(sel);
}

void CodeConfigPanel::UpdateInfoBox(wxCommandEvent&)
{
	m_infobox.listbox_codes->Clear();
	const int sel = m_listbox_gcodes->GetSelection();

	if (sel > -1)
	{
		m_infobox.label_name->SetLabel(wxstr_name + wxString::FromAscii(m_gcodes[sel].name.c_str()));
		m_infobox.label_description->SetLabel(wxstr_description + wxString::FromAscii(m_gcodes[sel].description.c_str()));
		m_infobox.label_creator->SetLabel(wxstr_creator + wxString::FromAscii(m_gcodes[sel].creator.c_str()));

		// add codes to info listbox
		std::vector<GeckoCode::Code>::const_iterator
		codes_iter = m_gcodes[sel].codes.begin(),
		codes_end = m_gcodes[sel].codes.end();
		for (; codes_iter!=codes_end; ++codes_iter)
			m_infobox.listbox_codes->Append(wxString::Format(wxT("%08X %08X"), codes_iter->address, codes_iter->data));
	}
	else
	{
		m_infobox.label_name->SetLabel(wxstr_name);
		m_infobox.label_description->SetLabel(wxstr_description);
		m_infobox.label_creator->SetLabel(wxstr_creator);
	}
}

void CodeConfigPanel::ApplyChanges(wxCommandEvent&)
{
	Gecko::SetActiveCodes(m_gcodes);
}


}
