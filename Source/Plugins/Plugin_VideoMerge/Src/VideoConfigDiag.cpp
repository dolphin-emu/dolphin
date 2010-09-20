
#include "VideoConfigDiag.h"

#include "VideoConfig.h"
#include "Main.h"

#define _connect_macro_(b, f, c, s)	(b)->Connect(wxID_ANY, (c), wxCommandEventHandler( f ), (wxObject*)0, (wxEvtHandler*)s)

// template instantiation
template class BoolSetting<wxCheckBox>;
template class BoolSetting<wxRadioButton>;

typedef BoolSetting<wxCheckBox> SettingCheckBox;
typedef BoolSetting<wxRadioButton> SettingRadioButton;

SettingCheckBox::BoolSetting(wxWindow* parent, const wxString& label, bool &setting, bool reverse, long style)
	: wxCheckBox(parent, -1, label, wxDefaultPosition, wxDefaultSize, style)
	, m_setting(setting)
	, m_reverse(reverse)
{
	SetValue(m_setting ^ m_reverse);
	_connect_macro_(this, BoolSetting<W>::UpdateValue, wxEVT_COMMAND_CHECKBOX_CLICKED, this);
}

SettingRadioButton::BoolSetting(wxWindow* parent, const wxString& label, bool &setting, bool reverse, long style)
	: wxRadioButton(parent, -1, label, wxDefaultPosition, wxDefaultSize, style)
	, m_setting(setting)
	, m_reverse(reverse)
{
	SetValue(m_setting ^ m_reverse);
	_connect_macro_(this, BoolSetting<W>::UpdateValue, wxEVT_COMMAND_RADIOBUTTON_SELECTED, this);
}

template <typename W>
void BoolSetting<W>::UpdateValue(wxCommandEvent& ev)
{
	m_setting = (ev.GetInt() != 0) ^ m_reverse;
}

SettingChoice::SettingChoice(wxWindow* parent, int &setting, int num, const wxString choices[])
	: wxChoice(parent, -1, wxDefaultPosition, wxDefaultSize, num, choices)
	, m_setting(setting)
{
	Select(m_setting);
	_connect_macro_(this, SettingChoice::UpdateValue, wxEVT_COMMAND_CHOICE_SELECTED, this);
}

void SettingChoice::UpdateValue(wxCommandEvent& ev)
{
	m_setting = ev.GetInt();
}

void VideoConfigDiag::CloseDiag(wxCommandEvent&)
{
	Close();
}

VideoConfigDiag::VideoConfigDiag(wxWindow* parent)
	: wxDialog(parent, -1, wxT("Dolphin Graphics Configuration"), wxDefaultPosition, wxDefaultSize)
{
	VideoConfig &vconfig = g_Config;

	wxNotebook* const notebook = new wxNotebook(this, -1, wxDefaultPosition, wxDefaultSize);

	// -- GENERAL --
	{
	wxPanel* const page_general = new wxPanel(notebook, -1, wxDefaultPosition);
	notebook->AddPage(page_general, wxT("General"));
	wxBoxSizer* const szr_general = new wxBoxSizer(wxVERTICAL);

	// - basic
	{
	wxStaticBoxSizer* const group_basic = new wxStaticBoxSizer(wxVERTICAL, page_general, wxT("Basic"));
	szr_general->Add(group_basic, 0, wxEXPAND | wxALL, 5);
	wxFlexGridSizer* const szr_basic = new wxFlexGridSizer(2, 5, 5);
	group_basic->Add(szr_basic, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

	// graphics api
	{
	const wxString gfxapi_choices[] = { wxT("Software [not present]"),
		wxT("OpenGL [broken]"), wxT("Direct3D 9 [broken]"), wxT("Direct3D 11") };

	szr_basic->Add(new wxStaticText(page_general, -1, wxT("Graphics API:")), 1, wxALIGN_CENTER_VERTICAL, 0);
	wxChoice* const choice_gfxapi = new SettingChoice(page_general,
		g_gfxapi, sizeof(gfxapi_choices)/sizeof(*gfxapi_choices), gfxapi_choices);
	szr_basic->Add(choice_gfxapi, 1, 0, 0);
	}

	// for D3D only
	// adapter
	{
	//szr_basic->Add(new wxStaticText(page_general, -1, wxT("Adapter:")), 1, wxALIGN_CENTER_VERTICAL, 5);
	//wxChoice* const choice_adapter = new SettingChoice(page_general, vconfig.iAdapter);
	//szr_basic->Add(choice_adapter, 1, 0, 0);
	}
	
	// aspect-ratio
	{
	const wxString ar_choices[] = { wxT("Auto [recommended]"),
		wxT("Force 16:9"), wxT("Force 4:3"), wxT("Strech to Window") };

	szr_basic->Add(new wxStaticText(page_general, -1, wxT("Aspect ratio:")), 1, wxALIGN_CENTER_VERTICAL, 0);
	wxChoice* const choice_aspect = new SettingChoice(page_general,
		vconfig.iAspectRatio, sizeof(ar_choices)/sizeof(*ar_choices), ar_choices);
	szr_basic->Add(choice_aspect, 1, 0, 0);
	}

	// widescreen hack
	{
	szr_basic->AddStretchSpacer(1);
	szr_basic->Add(new SettingCheckBox(page_general, wxT("Widescreen Hack"), vconfig.bWidescreenHack), 1, 0, 0);
	szr_basic->AddStretchSpacer(1);
	szr_basic->Add(new SettingCheckBox(page_general, wxT("V-Sync"), vconfig.bVSync), 1, 0, 0);
	}

	// - EFB
	{
	wxStaticBoxSizer* const group_efb = new wxStaticBoxSizer(wxVERTICAL, page_general, wxT("EFB"));
	szr_general->Add(group_efb, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

	group_efb->Add(new SettingCheckBox(page_general, wxT("Enable CPU Access"), vconfig.bEFBAccessEnable), 0, wxBOTTOM | wxLEFT, 5);

	// EFB scale
	{
	// TODO: give this a label
	const wxString efbscale_choices[] = { wxT("Fractional"), wxT("Integral [recommended]"),
		wxT("1x"), wxT("2x"), wxT("3x")/*, wxT("4x")*/ };

	wxChoice *const choice_efbscale = new SettingChoice(page_general,
		vconfig.iEFBScale, sizeof(efbscale_choices)/sizeof(*efbscale_choices), efbscale_choices);
	group_efb->Add(choice_efbscale, 0, wxBOTTOM | wxLEFT, 5);
	}

	// EFB copy
	wxStaticBoxSizer* const group_efbcopy = new wxStaticBoxSizer(wxHORIZONTAL, page_general, wxT("Copy"));
	group_efb->Add(group_efbcopy, 0, wxEXPAND | wxBOTTOM, 5);

	group_efbcopy->Add(new SettingCheckBox(page_general, wxT("Enable"), vconfig.bEFBCopyDisable, true), 0, wxLEFT | wxRIGHT | wxBOTTOM, 5);
	group_efbcopy->AddStretchSpacer(1);
	group_efbcopy->Add(new SettingRadioButton(page_general, wxT("Texture"), vconfig.bCopyEFBToTexture, false, wxRB_GROUP), 0, wxRIGHT, 5);
	group_efbcopy->Add(new SettingRadioButton(page_general, wxT("RAM"), vconfig.bCopyEFBToTexture, true), 0, wxRIGHT, 5);
	}

	// - safe texture cache
	{
	wxStaticBoxSizer* const group_safetex = new wxStaticBoxSizer(wxHORIZONTAL, page_general, wxT("Safe Texture Cache"));
	szr_general->Add(group_safetex, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

	// safe texture cache
	group_safetex->Add(new SettingCheckBox(page_general, wxT("Enable"), vconfig.bSafeTextureCache), 0, wxLEFT | wxRIGHT | wxBOTTOM, 5);
	group_safetex->AddStretchSpacer(1);
	// TODO: make these radio buttons functional
	//group_safetex->Add(new wxRadioButton(page_general, -1, wxT("Safe"),
	//	wxDefaultPosition, wxDefaultSize, wxRB_GROUP), 0, wxRIGHT, 5);
	//group_safetex->Add(new wxRadioButton(page_general, -1, wxT("Normal")), 0, wxRIGHT, 5);
	//group_safetex->Add(new wxRadioButton(page_general, -1, wxT("Fast")), 0, wxRIGHT, 5);
	}

	}

	page_general->SetSizerAndFit(szr_general);
	}

	// -- ADVANCED --
	{
	wxPanel* const page_advanced = new wxPanel(notebook, -1, wxDefaultPosition);
	notebook->AddPage(page_advanced, wxT("Advanced"));
	wxBoxSizer* const szr_advanced = new wxBoxSizer(wxVERTICAL);

	// - rendering
	{
	wxStaticBoxSizer* const group_rendering = new wxStaticBoxSizer(wxVERTICAL, page_advanced, wxT("Rendering"));
	szr_advanced->Add(group_rendering, 0, wxEXPAND | wxALL, 5);
	wxGridSizer* const szr_rendering = new wxGridSizer(2, 5, 5);
	group_rendering->Add(szr_rendering, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

	szr_rendering->Add(new SettingCheckBox(page_advanced, wxT("Enable Wireframe"), vconfig.bWireFrame));
	szr_rendering->Add(new SettingCheckBox(page_advanced, wxT("Disable Lighting"), vconfig.bDisableLighting));
	szr_rendering->Add(new SettingCheckBox(page_advanced, wxT("Disable Textures"), vconfig.bDisableTexturing));
	szr_rendering->Add(new SettingCheckBox(page_advanced, wxT("Disable Fog"), vconfig.bDisableFog));
	szr_rendering->Add(new SettingCheckBox(page_advanced, wxT("Disable Dest. Alpha Pass"), vconfig.bDstAlphaPass));
	}

	// - info
	{
	wxStaticBoxSizer* const group_info = new wxStaticBoxSizer(wxVERTICAL, page_advanced, wxT("Overlay Information"));
	szr_advanced->Add(group_info, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
	wxGridSizer* const szr_info = new wxGridSizer(2, 5, 5);
	group_info->Add(szr_info, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

	szr_info->Add(new SettingCheckBox(page_advanced, wxT("Show FPS"), vconfig.bShowFPS));
	szr_info->Add(new SettingCheckBox(page_advanced, wxT("Various Statistics"), vconfig.bOverlayStats));
	szr_info->Add(new SettingCheckBox(page_advanced, wxT("Projection Stats"), vconfig.bOverlayProjStats));
	szr_info->Add(new SettingCheckBox(page_advanced, wxT("Texture Format"), vconfig.bTexFmtOverlayEnable));
	szr_info->Add(new SettingCheckBox(page_advanced, wxT("EFB Copy Regions"), vconfig.bShowEFBCopyRegions));
	}
	
	// - XFB
	{
	wxStaticBoxSizer* const group_xfb = new wxStaticBoxSizer(wxHORIZONTAL, page_advanced, wxT("XFB"));
	szr_advanced->Add(group_xfb, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

	group_xfb->Add(new SettingCheckBox(page_advanced, wxT("Enable"), vconfig.bUseXFB), 0, wxLEFT | wxRIGHT | wxBOTTOM, 5);
	group_xfb->AddStretchSpacer(1);
	group_xfb->Add(new SettingRadioButton(page_advanced, wxT("Virtual"), vconfig.bUseRealXFB, true, wxRB_GROUP), 0, wxRIGHT, 5);
	group_xfb->Add(new SettingRadioButton(page_advanced, wxT("Real"), vconfig.bUseRealXFB), 0, wxRIGHT, 5);
	}

	// - utility
	{
	wxStaticBoxSizer* const group_utility = new wxStaticBoxSizer(wxVERTICAL, page_advanced, wxT("Utility"));
	szr_advanced->Add(group_utility, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
	wxGridSizer* const szr_utility = new wxGridSizer(2, 5, 5);
	group_utility->Add(szr_utility, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

	szr_utility->Add(new SettingCheckBox(page_advanced, wxT("Dump Textures"), vconfig.bDumpTextures));
	szr_utility->Add(new SettingCheckBox(page_advanced, wxT("Load Hi-Res Textures"), vconfig.bHiresTextures));
	szr_utility->Add(new SettingCheckBox(page_advanced, wxT("Dump EFB Target"), vconfig.bDumpEFBTarget));
	szr_utility->Add(new SettingCheckBox(page_advanced, wxT("Dump Frames"), vconfig.bDumpFrames));
	szr_utility->Add(new SettingCheckBox(page_advanced, wxT("Free Look"), vconfig.bFreeLook));
	}

	// stuff to move/remove
	{
	szr_advanced->Add(new SettingCheckBox(page_advanced, wxT("Load Native Mipmaps"), vconfig.bUseNativeMips), 0, wxBOTTOM | wxLEFT, 5);
	szr_advanced->Add(new SettingCheckBox(page_advanced, wxT("EFB Scaled Copy"), vconfig.bCopyEFBScaled), 0, wxBOTTOM | wxLEFT, 5);
	szr_advanced->Add(new SettingCheckBox(page_advanced, wxT("Auto Scale"), vconfig.bAutoScale), 0, wxBOTTOM | wxLEFT, 5);
	szr_advanced->Add(new SettingCheckBox(page_advanced, wxT("Crop"), vconfig.bCrop), 0, wxBOTTOM | wxLEFT, 5);
	szr_advanced->Add(new SettingCheckBox(page_advanced, wxT("Enable OpenCL"), vconfig.bEnableOpenCL), 0, wxBOTTOM | wxLEFT, 5);
	szr_advanced->Add(new SettingCheckBox(page_advanced, wxT("Enable Display List Caching"), vconfig.bDlistCachingEnable), 0, wxBOTTOM | wxLEFT, 5);
	}

	page_advanced->SetSizerAndFit(szr_advanced);
	}

	wxButton* const btn_close = new wxButton(this, -1, wxT("Close"), wxDefaultPosition);
	_connect_macro_(btn_close, VideoConfigDiag::CloseDiag, wxEVT_COMMAND_BUTTON_CLICKED, this);

	wxBoxSizer* const szr_main = new wxBoxSizer(wxVERTICAL);
	szr_main->Add(notebook, 1, wxEXPAND | wxALL, 5);
	szr_main->Add(btn_close, 0, wxALIGN_RIGHT | wxRIGHT | wxBOTTOM, 5);

	SetSizerAndFit(szr_main);
	Center();
}
