// Copyright (C) 2003-2008 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include "resource.h"

#include "Config.h"
#include "ConfigDlg.h"

LRESULT
CConfigDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	// Load config settings
	g_Config.Load();

	// Center window
	//CenterWindow(this->GetParent());
	CenterWindow(GetParent());

	// Get button handles
	m_buttonEnableHLEAudio = GetDlgItem(IDC_ENABLE_HLE_AUDIO);
	m_buttonEnableDTKMusic = GetDlgItem(IDC_ENABLE_DTK_MUSIC);
	m_buttonEnableThrottle = GetDlgItem(IDC_ENABLE_THROTTLE);
	m_buttonAntiGap = GetDlgItem(IDC_ANTIGAP);
	m_buttonDumpSamples  = GetDlgItem(IDC_DUMPSAMPLES);
	m_editDumpSamplePath = GetDlgItem(IDC_SAMPLEDUMPPATH);
	m_comboSampleRate = GetDlgItem(IDC_SAMPLERATE);

	// Update checkboxes
	m_buttonEnableHLEAudio.SetCheck(g_Config.m_EnableHLEAudio ? BST_CHECKED : BST_UNCHECKED);
	m_buttonEnableDTKMusic.SetCheck(g_Config.m_EnableDTKMusic ? BST_CHECKED : BST_UNCHECKED);
	m_buttonEnableThrottle.SetCheck(g_Config.m_EnableThrottle ? BST_CHECKED : BST_UNCHECKED);
	m_buttonAntiGap.SetCheck(g_Config.m_AntiGap ? BST_CHECKED : BST_UNCHECKED);
	m_buttonDumpSamples.SetCheck(g_Config.m_DumpSamples ? BST_CHECKED : BST_UNCHECKED);
	m_editDumpSamplePath.SetWindowText(g_Config.m_szSamplePath.c_str());
	m_comboSampleRate.AddString("44100");
	m_comboSampleRate.AddString("48000");
	m_comboSampleRate.SetCurSel(g_Config.m_SampleRate == 44100 ? 0 : 1);

	// Add tooltips
	CToolTipCtrl ToolTips;
	ToolTips.Create(m_hWnd);
	ToolTips.Activate(true);
	ToolTips.SetMaxTipWidth(200); // limit the width
	ToolTips.SetDelayTime(TTDT_AUTOPOP, 20 * 1000); // give us time to read it
	CToolInfo tiHLE(TTF_SUBCLASS, m_buttonEnableHLEAudio, 0, NULL,
		"This is the most common sound type");
	CToolInfo tiDTK(TTF_SUBCLASS, m_buttonEnableDTKMusic, 0, NULL,
		"This is sometimes used to play music tracks from the disc");
	CToolInfo tiOther(TTF_SUBCLASS, m_buttonEnableThrottle, 0, NULL,
		"This is sometimes used together with pre-rendered movies. Disabling this"
		" also disables the speed throttle that is causes. Meaning that"
		" there will be no upper limit on your FPS.");
	ToolTips.AddTool(tiHLE);
	ToolTips.AddTool(tiDTK);
	ToolTips.AddTool(tiOther);

	return(TRUE);
}


LRESULT
CConfigDlg::OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	// Save settings
	if (wID == IDOK)
	{
		g_Config.m_EnableHLEAudio = (m_buttonEnableHLEAudio.GetCheck() == BST_CHECKED) ? true : false;
		g_Config.m_EnableDTKMusic = (m_buttonEnableDTKMusic.GetCheck() == BST_CHECKED) ? true : false;
		g_Config.m_EnableThrottle = (m_buttonEnableThrottle.GetCheck() == BST_CHECKED) ? true : false;
		g_Config.m_DumpSamples = (m_buttonDumpSamples.GetCheck() == BST_CHECKED) ? true : false;
		g_Config.m_AntiGap = (m_buttonAntiGap.GetCheck() == BST_CHECKED) ? true : false;
		char temp[MAX_PATH];
		m_editDumpSamplePath.GetWindowText(temp, MAX_PATH);
		g_Config.m_szSamplePath = temp;
		g_Config.m_SampleRate = (m_comboSampleRate.GetCurSel() == 0 ? 44100 : 48000);
		g_Config.Save();
	}

	EndDialog(wID);
	g_Config.Save();
	return(0);
}
