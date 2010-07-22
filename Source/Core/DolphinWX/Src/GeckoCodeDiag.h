
#ifndef __GECKOCODEDIAG_h__
#define __GECKOCODEDIAG_h__

#include "GeckoCode.h"
#include "GeckoCodeConfig.h"

#include "wx/wx.h"

namespace Gecko
{


class CodeConfigPanel : public wxPanel
{
public:
	CodeConfigPanel(wxWindow* const parent);


	void LoadCodes(const IniFile& inifile);
	const std::vector<GeckoCode>& GetCodes() const { return m_gcodes; }

protected:
	void UpdateInfoBox(wxCommandEvent&);
	void ToggleCode(wxCommandEvent& evt);
	void ApplyChanges(wxCommandEvent&);

private:
	std::vector<GeckoCode> m_gcodes;

	// wxwidgets stuff
	wxCheckListBox	*m_listbox_gcodes;
	struct
	{
		wxStaticText	*label_name, *label_description, *label_creator;
		wxListBox	*listbox_codes;
	} m_infobox;

};



}

#endif
