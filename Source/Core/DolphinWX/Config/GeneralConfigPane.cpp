// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/event.h>
#include <wx/menu.h>
#include <wx/radiobox.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/PowerPC/PowerPC.h"
#include "DolphinWX/Frame.h"
#include "DolphinWX/Main.h"
#include "DolphinWX/Config/GeneralConfigPane.h"
#include "DolphinWX/Debugger/CodeWindow.h"


struct CPUCore
{
	int CPUid;
	wxString name;
};
static const CPUCore cpu_cores[] = {
		{ 0, _("Interpreter (VERY slow)") },
#ifdef _M_X86_64
		{ 1, _("JIT Recompiler (recommended)") },
		{ 2, _("JITIL Recompiler (slower, experimental)") },
#elif defined(_M_ARM_32)
		{ 3, _("Arm JIT (experimental)") },
#elif defined(_M_ARM_64)
		{ 4, _("Arm64 JIT (experimental)") },
#endif
};

GeneralConfigPane::GeneralConfigPane(wxWindow* parent, wxWindowID id)
	: wxPanel(parent, id)
{
	InitializeGUI();
	LoadGUIValues();
	RefreshGUI();
}

void GeneralConfigPane::InitializeGUI()
{
	m_frame_limit_array_string.Add(_("Off"));
	m_frame_limit_array_string.Add(_("Auto"));
	for (int i = 5; i <= 150; i += 5) // from 5 to 150
		m_frame_limit_array_string.Add(wxString::Format("%i", i));

	m_gpu_determinism_string.Add(_("auto"));
	m_gpu_determinism_string.Add(_("none"));
	m_gpu_determinism_string.Add(_("fake-completion"));

	for (const CPUCore& cpu_core : cpu_cores)
		m_cpu_engine_array_string.Add(cpu_core.name);

	m_dual_core_checkbox   = new wxCheckBox(this, wxID_ANY, _("Enable Dual Core (speedup)"));
	m_gpu_determinism      = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_gpu_determinism_string);
	m_idle_skip_checkbox   = new wxCheckBox(this, wxID_ANY, _("Enable Idle Skipping (speedup)"));
	m_cheats_checkbox      = new wxCheckBox(this, wxID_ANY, _("Enable Cheats"));
	m_force_ntscj_checkbox = new wxCheckBox(this, wxID_ANY, _("Force Console as NTSC-J"));
	m_frame_limit_choice   = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_frame_limit_array_string);
	m_cpu_engine_radiobox  = new wxRadioBox(this, wxID_ANY, _("CPU Emulator Engine"), wxDefaultPosition, wxDefaultSize, m_cpu_engine_array_string, 0, wxRA_SPECIFY_ROWS);

	m_dual_core_checkbox->SetToolTip(_("Splits the CPU and GPU threads so they can be run on separate cores.\nProvides major speed improvements on most modern PCs, but can cause occasional crashes/glitches."));
	m_gpu_determinism->SetToolTip(_("")); // Add me
	m_idle_skip_checkbox->SetToolTip(_("Attempt to detect and skip wait-loops.\nIf unsure, leave this checked."));
	m_cheats_checkbox->SetToolTip(_("Enables the use of Action Replay and Gecko cheats."));
	m_force_ntscj_checkbox->SetToolTip(_("Forces NTSC-J mode for using the Japanese ROM font.\nIf left unchecked, Dolphin defaults to NTSC-U and automatically enables this setting when playing Japanese games."));
	m_frame_limit_choice->SetToolTip(_("Limits the game speed to the specified number of frames per second (full speed is 60 for NTSC and 50 for PAL)."));

	m_dual_core_checkbox->Bind(wxEVT_CHECKBOX, &GeneralConfigPane::OnDualCoreCheckBoxChanged, this);
	m_gpu_determinism->Bind(wxEVT_CHOICE, &GeneralConfigPane::OnGPUDeterminsmChanged, this);
	m_idle_skip_checkbox->Bind(wxEVT_CHECKBOX, &GeneralConfigPane::OnIdleSkipCheckBoxChanged, this);
	m_cheats_checkbox->Bind(wxEVT_CHECKBOX, &GeneralConfigPane::OnCheatCheckBoxChanged, this);
	m_force_ntscj_checkbox->Bind(wxEVT_CHECKBOX, &GeneralConfigPane::OnForceNTSCJCheckBoxChanged, this);
	m_frame_limit_choice->Bind(wxEVT_CHOICE, &GeneralConfigPane::OnFrameLimitChoiceChanged, this);
	m_cpu_engine_radiobox->Bind(wxEVT_RADIOBOX, &GeneralConfigPane::OnCPUEngineRadioBoxChanged, this);

	wxBoxSizer* const frame_limit_sizer = new wxBoxSizer(wxHORIZONTAL);
	frame_limit_sizer->Add(new wxStaticText(this, wxID_ANY, _("Framelimit:")), 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT | wxBOTTOM, 5);
	frame_limit_sizer->Add(m_frame_limit_choice, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 5);

	wxBoxSizer* const gpu_determinism_sizer = new wxBoxSizer(wxHORIZONTAL);
	gpu_determinism_sizer->Add(new wxStaticText(this, wxID_ANY, _("Deterministic Dual Core:")), 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT | wxBOTTOM, 5);
	gpu_determinism_sizer->Add(m_gpu_determinism, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 5);

	wxStaticBoxSizer* const basic_settings_sizer = new wxStaticBoxSizer(wxVERTICAL, this, _("Basic Settings"));
	basic_settings_sizer->Add(m_dual_core_checkbox, 0, wxALL, 5);
	basic_settings_sizer->Add(gpu_determinism_sizer);
	basic_settings_sizer->Add(m_idle_skip_checkbox, 0, wxALL, 5);
	basic_settings_sizer->Add(m_cheats_checkbox, 0, wxALL, 5);
	basic_settings_sizer->Add(frame_limit_sizer);

	wxStaticBoxSizer* const advanced_settings_sizer = new wxStaticBoxSizer(wxVERTICAL, this, _("Advanced Settings"));
	advanced_settings_sizer->Add(m_cpu_engine_radiobox, 0, wxALL, 5);
	advanced_settings_sizer->Add(m_force_ntscj_checkbox, 0, wxALL, 5);

	wxBoxSizer* const main_sizer = new wxBoxSizer(wxVERTICAL);
	main_sizer->Add(basic_settings_sizer, 0, wxEXPAND | wxALL, 5);
	main_sizer->Add(advanced_settings_sizer, 0, wxEXPAND | wxALL, 5);

	SetSizer(main_sizer);
}

void GeneralConfigPane::LoadGUIValues()
{
	const SCoreStartupParameter& startup_params = SConfig::GetInstance().m_LocalCoreStartupParameter;

	m_dual_core_checkbox->SetValue(startup_params.bCPUThread);
	m_gpu_determinism->Select(startup_params.m_GPUDeterminismMode);
	m_idle_skip_checkbox->SetValue(startup_params.bSkipIdle);
	m_cheats_checkbox->SetValue(startup_params.bEnableCheats);
	m_force_ntscj_checkbox->SetValue(startup_params.bForceNTSCJ);
	m_frame_limit_choice->SetSelection(SConfig::GetInstance().m_Framelimit);

	for (size_t i = 0; i < (sizeof(cpu_cores) / sizeof(CPUCore)); ++i)
	{
		if (cpu_cores[i].CPUid == startup_params.iCPUCore)
			m_cpu_engine_radiobox->SetSelection(i);
	}
}

void GeneralConfigPane::RefreshGUI()
{
	if (Core::IsRunning())
	{
		m_dual_core_checkbox->Disable();
		m_gpu_determinism->Disable();
		m_idle_skip_checkbox->Disable();
		m_cheats_checkbox->Disable();
		m_force_ntscj_checkbox->Disable();
		m_cpu_engine_radiobox->Disable();
	}
}

void GeneralConfigPane::OnDualCoreCheckBoxChanged(wxCommandEvent& event)
{
	if (Core::IsRunning())
		return;

	SConfig::GetInstance().m_LocalCoreStartupParameter.bCPUThread = m_dual_core_checkbox->IsChecked();
}

void GeneralConfigPane::OnGPUDeterminsmChanged(wxCommandEvent& event)
{
	SConfig::GetInstance().m_LocalCoreStartupParameter.m_GPUDeterminismMode = (GPUDeterminismMode)m_gpu_determinism->GetSelection();
	SConfig::GetInstance().m_LocalCoreStartupParameter.m_strGPUDeterminismMode = m_gpu_determinism_string[SConfig::GetInstance().m_LocalCoreStartupParameter.m_GPUDeterminismMode];
}

void GeneralConfigPane::OnIdleSkipCheckBoxChanged(wxCommandEvent& event)
{
	SConfig::GetInstance().m_LocalCoreStartupParameter.bSkipIdle = m_idle_skip_checkbox->IsChecked();
}

void GeneralConfigPane::OnCheatCheckBoxChanged(wxCommandEvent& event)
{
	SConfig::GetInstance().m_LocalCoreStartupParameter.bEnableCheats = m_cheats_checkbox->IsChecked();
}

void GeneralConfigPane::OnForceNTSCJCheckBoxChanged(wxCommandEvent& event)
{
	SConfig::GetInstance().m_LocalCoreStartupParameter.bForceNTSCJ = m_force_ntscj_checkbox->IsChecked();
}

void GeneralConfigPane::OnFrameLimitChoiceChanged(wxCommandEvent& event)
{
	SConfig::GetInstance().m_Framelimit = m_frame_limit_choice->GetSelection();
}

void GeneralConfigPane::OnCPUEngineRadioBoxChanged(wxCommandEvent& event)
{
	const int selection = m_cpu_engine_radiobox->GetSelection();

	if (main_frame->g_pCodeWindow)
	{

		bool using_interp = (SConfig::GetInstance().m_LocalCoreStartupParameter.iCPUCore == PowerPC::CORE_INTERPRETER);
		main_frame->g_pCodeWindow->GetMenuBar()->Check(IDM_INTERPRETER, using_interp);
	}

	SConfig::GetInstance().m_LocalCoreStartupParameter.iCPUCore = cpu_cores[selection].CPUid;
}
