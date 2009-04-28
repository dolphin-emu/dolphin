#include "MemcardManager.h"
#include "MCMdebug.h"

BEGIN_EVENT_TABLE(CMemcardManagerDebug, wxWindow)
	EVT_CLOSE(CMemcardManagerDebug::OnClose)
END_EVENT_TABLE()

CMemcardManagerDebug::CMemcardManagerDebug(wxFrame* parent, const wxPoint& pos, const wxSize& size) :
	wxFrame(parent, wxID_ANY, _T("Memcard Debug Window"), pos, size, MEMCARD_MANAGER_STYLE)
{
	// Create the GUI controls
	Init_ChildControls();

	// Setup Window
	SetBackgroundColour(m_Notebook_MCMD->GetBackgroundColour());
	SetSize(size);
	SetPosition(pos);

	Layout();
	Show();
}


void CMemcardManagerDebug::updatePanels(GCMemcard **memCard,int card)
{
	memoryCard = memCard;
	updateHDRtab(card);
	updateDIRtab(card);
	updateDIRBtab(card);
	updateBATtab(card);
	updateBATBtab(card);
}

void CMemcardManagerDebug::Init_ChildControls()
{
	// Main Notebook
	m_Notebook_MCMD = new wxNotebook(this, ID_NOTEBOOK_MAIN, wxDefaultPosition, wxDefaultSize);
	
	m_Tab_HDR = new wxPanel(m_Notebook_MCMD, ID_TAB_HDR, wxDefaultPosition, wxDefaultSize);
	m_Tab_DIR = new wxPanel(m_Notebook_MCMD, ID_TAB_DIR, wxDefaultPosition, wxDefaultSize);
	m_Tab_DIR_b = new wxPanel(m_Notebook_MCMD, ID_TAB_DIR_B, wxDefaultPosition, wxDefaultSize);
	m_Tab_BAT = new wxPanel(m_Notebook_MCMD, ID_TAB_BAT, wxDefaultPosition, wxDefaultSize);
	m_Tab_BAT_b = new wxPanel(m_Notebook_MCMD, ID_TAB_BAT_B, wxDefaultPosition, wxDefaultSize);

	Init_HDR();
	Init_DIR();
	Init_DIR_b();
	Init_BAT();
	Init_BAT_b();

	// Add Tabs to Notebook
	m_Notebook_MCMD->AddPage(m_Tab_HDR, _T("HDR"));
	m_Notebook_MCMD->AddPage(m_Tab_DIR, _T("DIR"));
	m_Notebook_MCMD->AddPage(m_Tab_DIR_b, _T("DIR_b"));
	m_Notebook_MCMD->AddPage(m_Tab_BAT, _T("Bat"));
	m_Notebook_MCMD->AddPage(m_Tab_BAT_b, _T("Bat_b"));

	wxBoxSizer* sMain = new wxBoxSizer(wxVERTICAL);
	sMain->Add(m_Notebook_MCMD, 1, wxEXPAND|wxALL, 5);
	SetSizer(sMain);
	Layout();
	Fit();
}
void CMemcardManagerDebug::OnClose(wxCloseEvent& WXUNUSED (event))
{
	Hide();
}

void CMemcardManagerDebug::Init_HDR()
{
	wxBoxSizer *sMain;
	wxStaticBoxSizer *sHDR[2];
	
	sMain = new wxBoxSizer(wxHORIZONTAL);
	sHDR[0]  = new wxStaticBoxSizer(wxVERTICAL, m_Tab_HDR, wxT("MEMCARD:A"));
	sHDR[1]  = new wxStaticBoxSizer(wxVERTICAL, m_Tab_HDR, wxT("MEMCARD:B"));
	
	wxGridBagSizer * sOtPaths[2];
	
	wxStaticText *st[2][11];
	
	for(int i = SLOT_A; i <=SLOT_B;i++)
	{
		sOtPaths[i] = new wxGridBagSizer(0, 0);
		sOtPaths[i]->AddGrowableCol(1);
		st[i][0]= new wxStaticText(m_Tab_HDR, 0, wxT("Ser\t\t"), wxDefaultPosition, wxDefaultSize, 0, wxEmptyString);
		st[i][1]= new wxStaticText(m_Tab_HDR, 0, wxT("fmtTime\t\t"), wxDefaultPosition, wxDefaultSize, 0, wxEmptyString);
		st[i][2]= new wxStaticText(m_Tab_HDR, 0, wxT("SRAMBIAS\t\t"), wxDefaultPosition, wxDefaultSize, 0, wxEmptyString);
		st[i][3]= new wxStaticText(m_Tab_HDR, 0, wxT("SRAMLANG\t\t"), wxDefaultPosition, wxDefaultSize, 0, wxEmptyString);
		st[i][4]= new wxStaticText(m_Tab_HDR, 0, wxT("Unk2\t\t"), wxDefaultPosition, wxDefaultSize, 0, wxEmptyString);
		st[i][5]= new wxStaticText(m_Tab_HDR, 0, wxT("deviceID\t\t"), wxDefaultPosition, wxDefaultSize, 0, wxEmptyString);
		st[i][6]= new wxStaticText(m_Tab_HDR, 0, wxT("Size\t\t"), wxDefaultPosition, wxDefaultSize, 0, wxEmptyString);
		st[i][7]= new wxStaticText(m_Tab_HDR, 0, wxT("Encoding\t\t"), wxDefaultPosition, wxDefaultSize, 0, wxEmptyString);
		st[i][8]= new wxStaticText(m_Tab_HDR, 0, wxT("UpdateCounter\t\t"), wxDefaultPosition, wxDefaultSize, 0, wxEmptyString);
		st[i][9]= new wxStaticText(m_Tab_HDR, 0, wxT("CheckSum1\t\t"), wxDefaultPosition, wxDefaultSize, 0, wxEmptyString);
		st[i][10]= new wxStaticText(m_Tab_HDR, 0, wxT("CheckSum2\t\t"), wxDefaultPosition, wxDefaultSize, 0, wxEmptyString);

		t_HDR_ser[i] = new wxStaticText(m_Tab_HDR, 0, wxT("XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX"),
			wxDefaultPosition, wxDefaultSize, 0, wxEmptyString);
		t_HDR_fmtTime[i] = new wxStaticText(m_Tab_HDR, 0, wxT("XXXXXXXX, XXXXXXXX"), wxDefaultPosition, wxDefaultSize, 0, wxEmptyString);
		t_HDR_SRAMBIAS[i] = new wxStaticText(m_Tab_HDR, 0, wxT("XX, XX, XX, XX"), wxDefaultPosition, wxDefaultSize, 0, wxEmptyString);
		t_HDR_SRAMLANG[i] = new wxStaticText(m_Tab_HDR, 0, wxT("XX, XX, XX, XX"), wxDefaultPosition, wxDefaultSize, 0, wxEmptyString);
		t_HDR_Unk2[i] = new wxStaticText(m_Tab_HDR, 0, wxT("XX, XX, XX, XX"), wxDefaultPosition, wxDefaultSize, 0, wxEmptyString);
		t_HDR_devID[i] = new wxStaticText(m_Tab_HDR, 0, wxT("XX, XX"), wxDefaultPosition, wxDefaultSize, 0, wxEmptyString);
		t_HDR_Size[i] = new wxStaticText(m_Tab_HDR, 0, wxT("XX, XX"), wxDefaultPosition, wxDefaultSize, 0, wxEmptyString);
		t_HDR_Encoding[i] = new wxStaticText(m_Tab_HDR, 0, wxT("XX, XX"), wxDefaultPosition, wxDefaultSize, 0, wxEmptyString);
		t_HDR_UpdateCounter[i] = new wxStaticText(m_Tab_HDR, 0, wxT("XX, XX"), wxDefaultPosition, wxDefaultSize, 0, wxEmptyString);
		t_HDR_CheckSum1[i] = new wxStaticText(m_Tab_HDR, 0,	wxT("XX, XX"), wxDefaultPosition, wxDefaultSize, 0, wxEmptyString);
		t_HDR_CheckSum2[i] = new wxStaticText(m_Tab_HDR, 0,	wxT("XX, XX"), wxDefaultPosition, wxDefaultSize, 0, wxEmptyString);

		sOtPaths[i]->Add(st[i][0], wxGBPosition(0, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
		sOtPaths[i]->Add(t_HDR_ser[i], wxGBPosition(0, 1), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);

		sOtPaths[i]->Add(st[i][1], wxGBPosition(1, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
		sOtPaths[i]->Add(t_HDR_fmtTime[i], wxGBPosition(1, 1), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);
	
		sOtPaths[i]->Add(st[i][2], wxGBPosition(2, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
		sOtPaths[i]->Add(t_HDR_SRAMBIAS[i], wxGBPosition(2, 1), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);

		sOtPaths[i]->Add(st[i][3], wxGBPosition(3, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
		sOtPaths[i]->Add(t_HDR_SRAMLANG[i], wxGBPosition(3, 1), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);
	
		sOtPaths[i]->Add(st[i][4], wxGBPosition(4, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
		sOtPaths[i]->Add(t_HDR_Unk2[i], wxGBPosition(4, 1), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);
	
		sOtPaths[i]->Add(st[i][5], wxGBPosition(5, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
		sOtPaths[i]->Add(t_HDR_devID[i], wxGBPosition(5, 1), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);
	
		sOtPaths[i]->Add(st[i][6], wxGBPosition(6, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
		sOtPaths[i]->Add(t_HDR_Size[i], wxGBPosition(6, 1), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);

		sOtPaths[i]->Add(st[i][7], wxGBPosition(7, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
		sOtPaths[i]->Add(t_HDR_Encoding[i], wxGBPosition(7, 1), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);
	
		sOtPaths[i]->Add(st[i][8], wxGBPosition(8, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
		sOtPaths[i]->Add(t_HDR_UpdateCounter[i], wxGBPosition(8, 1), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);

		sOtPaths[i]->Add(st[i][9], wxGBPosition(9, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
		sOtPaths[i]->Add(t_HDR_CheckSum1[i], wxGBPosition(9, 1), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);
	
		sOtPaths[i]->Add(st[i][10], wxGBPosition(10, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
		sOtPaths[i]->Add(t_HDR_CheckSum2[i], wxGBPosition(10, 1), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);

		sHDR[i]->Add(sOtPaths[i], 0, wxEXPAND|wxALL, 5);
		sMain->Add(sHDR[i], 0, wxEXPAND|wxALL, 1);
	}
	m_Tab_HDR->SetSizer(sMain);
	m_Tab_HDR->Layout();
}

void CMemcardManagerDebug::updateHDRtab(int card)
{	
	wxString wx_ser,
			 wx_fmtTime,
			 wx_SRAMBIAS,
			 wx_SRAMLANG,
			 wx_Unk2,
			 wx_devID,
			 wx_Size,
			 wx_Encoding,
			 wx_UpdateCounter,
			 wx_CheckSum1,
			 wx_CheckSum2;
	
	wx_ser.Printf(wxT("%02X, %02X, %02X, %02X, %02X, %02X, %02X, %02X, %02X, %02X, %02X, %02X"),
		memoryCard[card]->hdr.serial[0],memoryCard[card]->hdr.serial[1],
		memoryCard[card]->hdr.serial[2],memoryCard[card]->hdr.serial[3],
		memoryCard[card]->hdr.serial[4],memoryCard[card]->hdr.serial[5],
		memoryCard[card]->hdr.serial[6],memoryCard[card]->hdr.serial[7],
		memoryCard[card]->hdr.serial[8],memoryCard[card]->hdr.serial[9],
		memoryCard[card]->hdr.serial[10],memoryCard[card]->hdr.serial[11]);

	wx_fmtTime.Printf(wxT("%08X, %08X"), 
		Common::swap32(memoryCard[card]->hdr.fmtTime.high),Common::swap32(memoryCard[card]->hdr.fmtTime.low));
	
	wx_SRAMBIAS.Printf(wxT("%02X, %02X, %02X, %02X"),
		memoryCard[card]->hdr.SramBias[0],memoryCard[card]->hdr.SramBias[1],
		memoryCard[card]->hdr.SramBias[2],memoryCard[card]->hdr.SramBias[3]);
	
	wx_SRAMLANG.Printf(wxT("%02X, %02X, %02X, %02X"),
		memoryCard[card]->hdr.SramLang[0], memoryCard[card]->hdr.SramLang[1],
		memoryCard[card]->hdr.SramLang[2],memoryCard[card]->hdr.SramLang[3]);

	wx_Unk2.Printf(wxT("%02X, %02X, %02X, %02X"),
		memoryCard[card]->hdr.Unk2[0],memoryCard[card]->hdr.Unk2[1],
		memoryCard[card]->hdr.Unk2[2],memoryCard[card]->hdr.Unk2[3]);			

	wx_devID.Printf(wxT("%02X, %02X"),
		memoryCard[card]->hdr.deviceID[0],memoryCard[card]->hdr.deviceID[1]);

	wx_Size.Printf(wxT("%02X, %02X"),
		memoryCard[card]->hdr.Size[0],memoryCard[card]->hdr.Size[1]);

	wx_Encoding.Printf(	wxT("%02X, %02X"),
		memoryCard[card]->hdr.Encoding[0],memoryCard[card]->hdr.Encoding[1]);
	
	wx_UpdateCounter.Printf(wxT("%02X, %02X"),
		memoryCard[card]->hdr.UpdateCounter[0],memoryCard[card]->hdr.UpdateCounter[1]);

	wx_CheckSum1.Printf(wxT("%02X, %02X"),
		memoryCard[card]->hdr.CheckSum1[0],memoryCard[card]->hdr.CheckSum1[1]);

	wx_CheckSum2.Printf(wxT("%02X, %02X"),
			memoryCard[card]->hdr.CheckSum2[0],memoryCard[card]->hdr.CheckSum2[1]);


	t_HDR_ser[card]->SetLabel(wx_ser);
	t_HDR_fmtTime[card]->SetLabel(wx_fmtTime);
	t_HDR_SRAMBIAS[card]->SetLabel(wx_SRAMBIAS);
	t_HDR_SRAMLANG[card]->SetLabel(wx_SRAMLANG);
	t_HDR_Unk2[card]->SetLabel(wx_Unk2);
	t_HDR_devID[card]->SetLabel(wx_devID);
	t_HDR_Size[card]->SetLabel(wx_Size);
	t_HDR_Encoding[card]->SetLabel(wx_Encoding);
	t_HDR_UpdateCounter[card]->SetLabel(wx_UpdateCounter);
	t_HDR_CheckSum1[card]->SetLabel(wx_CheckSum1);
	t_HDR_CheckSum2[card]->SetLabel(wx_CheckSum2);
}



void CMemcardManagerDebug::Init_DIR()
{
	wxBoxSizer *sMain;
	wxStaticBoxSizer *sDIR[2];
	sMain = new wxBoxSizer(wxHORIZONTAL);
	sDIR[0]  = new wxStaticBoxSizer(wxVERTICAL, m_Tab_DIR, wxT("MEMCARD:A"));
	sDIR[1]  = new wxStaticBoxSizer(wxVERTICAL, m_Tab_DIR, wxT("MEMCARD:B"));

	wxGridBagSizer * sOtPaths[3];

	wxStaticText *st[2][9];
	for(int i = SLOT_A; i <=SLOT_B;i++)
	{
		sOtPaths[i] = new wxGridBagSizer(0, 0);
		sOtPaths[i]->AddGrowableCol(1);
		st[i][0]= new wxStaticText(m_Tab_DIR, 0, wxT("UpdateCounter\t\t"), wxDefaultPosition, wxDefaultSize, 0, wxEmptyString);
		st[i][1]= new wxStaticText(m_Tab_DIR, 0, wxT("CheckSum1\t\t"), wxDefaultPosition, wxDefaultSize, 0, wxEmptyString);
		st[i][2]= new wxStaticText(m_Tab_DIR, 0, wxT("CheckSum2\t\t"), wxDefaultPosition, wxDefaultSize, 0, wxEmptyString);

		t_DIR_UpdateCounter[i] = new wxStaticText(m_Tab_DIR, 0, wxT("XX, XX"), wxDefaultPosition, wxDefaultSize, 0, wxEmptyString);
		t_DIR_CheckSum1[i] = new wxStaticText(m_Tab_DIR, 0,	wxT("XX, XX"), wxDefaultPosition, wxDefaultSize, 0, wxEmptyString);
		t_DIR_CheckSum2[i] = new wxStaticText(m_Tab_DIR, 0, wxT("XX, XX"), wxDefaultPosition, wxDefaultSize, 0, wxEmptyString);

		sOtPaths[i]->Add(st[i][0], wxGBPosition(0, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
		sOtPaths[i]->Add(t_DIR_UpdateCounter[i], wxGBPosition(0, 1), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);

		sOtPaths[i]->Add(st[i][1], wxGBPosition(1, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
		sOtPaths[i]->Add(t_DIR_CheckSum1[i], wxGBPosition(1, 1), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);
	
		sOtPaths[i]->Add(st[i][2], wxGBPosition(2, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
		sOtPaths[i]->Add(t_DIR_CheckSum2[i], wxGBPosition(2, 1), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);

	
		sDIR[i]->Add(sOtPaths[i], 0, wxEXPAND|wxALL, 5);
		sMain->Add(sDIR[i], 0, wxEXPAND|wxALL, 1);
	}

	m_Tab_DIR->SetSizer(sMain);
	m_Tab_DIR->Layout();
}


void CMemcardManagerDebug::updateDIRtab(int card)
{	
	wxString wx_UpdateCounter,
			 wx_CheckSum1,
			 wx_CheckSum2;
	
	wx_UpdateCounter.Printf(wxT("%02X, %02X"),
		memoryCard[card]->dir.UpdateCounter[0],memoryCard[card]->dir.UpdateCounter[1]);
	
	wx_CheckSum1.Printf(wxT("%02X, %02X"),
		memoryCard[card]->dir.CheckSum1[0],memoryCard[card]->dir.CheckSum1[1]);
	
	wx_CheckSum2.Printf(wxT("%02X, %02X"),
		memoryCard[card]->dir.CheckSum2[0],memoryCard[card]->dir.CheckSum2[1]);

	t_DIR_UpdateCounter[card]->SetLabel(wx_UpdateCounter);
	t_DIR_CheckSum1[card]->SetLabel(wx_CheckSum1);
	t_DIR_CheckSum2[card]->SetLabel(wx_CheckSum2);
}


void CMemcardManagerDebug::Init_DIR_b()
{
	wxBoxSizer *sMain;
	wxStaticBoxSizer *sDIR_b[2];
	sMain = new wxBoxSizer(wxHORIZONTAL);
	sDIR_b[0]  = new wxStaticBoxSizer(wxVERTICAL, m_Tab_DIR_b, wxT("MEMCARD:A"));
	sDIR_b[1]  = new wxStaticBoxSizer(wxVERTICAL, m_Tab_DIR_b, wxT("MEMCARD:B"));
	
	wxGridBagSizer * sOtPaths[3];
	
	wxStaticText *st[2][9];
	for(int i = SLOT_A; i <=SLOT_B;i++)
	{
		sOtPaths[i] = new wxGridBagSizer(0, 0);
		sOtPaths[i]->AddGrowableCol(1);
		st[i][0]= new wxStaticText(m_Tab_DIR_b, 0, wxT("UpdateCounter\t\t"), wxDefaultPosition, wxDefaultSize, 0, wxEmptyString);
		st[i][1]= new wxStaticText(m_Tab_DIR_b, 0, wxT("CheckSum1\t\t"), wxDefaultPosition, wxDefaultSize, 0, wxEmptyString);
		st[i][2]= new wxStaticText(m_Tab_DIR_b, 0, wxT("CheckSum2\t\t"), wxDefaultPosition, wxDefaultSize, 0, wxEmptyString);

		t_DIR_b_UpdateCounter[i] = new wxStaticText(m_Tab_DIR_b, 0, wxT("XX, XX"), wxDefaultPosition, wxDefaultSize, 0, wxEmptyString);
		t_DIR_b_CheckSum1[i] = new wxStaticText(m_Tab_DIR_b, 0, wxT("XX, XX"), wxDefaultPosition, wxDefaultSize, 0, wxEmptyString);
		t_DIR_b_CheckSum2[i] = new wxStaticText(m_Tab_DIR_b, 0, wxT("XX, XX"), wxDefaultPosition, wxDefaultSize, 0, wxEmptyString);

		sOtPaths[i]->Add(st[i][0], wxGBPosition(0, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
		sOtPaths[i]->Add(t_DIR_b_UpdateCounter[i], wxGBPosition(0, 1), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);

		sOtPaths[i]->Add(st[i][1], wxGBPosition(1, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
		sOtPaths[i]->Add(t_DIR_b_CheckSum1[i], wxGBPosition(1, 1), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);

		sOtPaths[i]->Add(st[i][2], wxGBPosition(2, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
		sOtPaths[i]->Add(t_DIR_b_CheckSum2[i], wxGBPosition(2, 1), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);
		
		sDIR_b[i]->Add(sOtPaths[i], 0, wxEXPAND|wxALL, 5);
		sMain->Add(sDIR_b[i], 0, wxEXPAND|wxALL, 1);
	}
	m_Tab_DIR_b->SetSizer(sMain);
	m_Tab_DIR_b->Layout();
}

void CMemcardManagerDebug::updateDIRBtab(int card)
{	
	wxString wx_UpdateCounter,
			 wx_CheckSum1,
			 wx_CheckSum2;

	wx_UpdateCounter.Printf(wxT("%02X, %02X"),
		memoryCard[card]->dir_backup.UpdateCounter[0],memoryCard[card]->dir_backup.UpdateCounter[1]);
	wx_CheckSum1.Printf(wxT("%02X, %02X"),
		memoryCard[card]->dir_backup.CheckSum1[0],memoryCard[card]->dir_backup.CheckSum1[1]);
	wx_CheckSum2.Printf(wxT("%02X, %02X"),
		memoryCard[card]->dir_backup.CheckSum2[0],memoryCard[card]->dir_backup.CheckSum2[1]);

	t_DIR_b_UpdateCounter[card]->SetLabel(wx_UpdateCounter);
	t_DIR_b_CheckSum1[card]->SetLabel(wx_CheckSum1);
	t_DIR_b_CheckSum2[card]->SetLabel(wx_CheckSum2);
}

void CMemcardManagerDebug::Init_BAT()
{
	wxBoxSizer *sMain;
	wxStaticBoxSizer *sBAT[4];
	sMain    = new wxBoxSizer(wxHORIZONTAL);
	sBAT[0]  = new wxStaticBoxSizer(wxVERTICAL, m_Tab_BAT, wxT("MEMCARD:A"));
	sBAT[1]  = new wxStaticBoxSizer(wxVERTICAL, m_Tab_BAT, wxT("MEMCARD:B"));
	sBAT[2]  = new wxStaticBoxSizer(wxVERTICAL, m_Tab_BAT, wxT("MEMCARD:A MAP"));
	sBAT[3]  = new wxStaticBoxSizer(wxVERTICAL, m_Tab_BAT, wxT("MEMCARD:B MAP"));
	wxGridBagSizer * sOtPaths[4];

	wxStaticText *st[2][9];
	for(int i = SLOT_A; i <=SLOT_B;i++)
	{
		sOtPaths[i] = new wxGridBagSizer(0, 0);
		sOtPaths[i]->AddGrowableCol(1);
		st[i][0]= new wxStaticText(m_Tab_BAT, 0, wxT("CheckSum1\t\t"), wxDefaultPosition, wxDefaultSize, 0, wxEmptyString);
		st[i][1]= new wxStaticText(m_Tab_BAT, 0, wxT("CheckSum2\t\t"), wxDefaultPosition, wxDefaultSize, 0, wxEmptyString);
		st[i][2]= new wxStaticText(m_Tab_BAT, 0, wxT("UpdateCounter\t\t"), wxDefaultPosition, wxDefaultSize, 0, wxEmptyString);
		st[i][3]= new wxStaticText(m_Tab_BAT, 0, wxT("FreeBlocks\t\t"), wxDefaultPosition, wxDefaultSize, 0, wxEmptyString);
		st[i][4]= new wxStaticText(m_Tab_BAT, 0, wxT("LastAllocated\t\t"), wxDefaultPosition, wxDefaultSize, 0, wxEmptyString);

		t_BAT_CheckSum1[i] = new wxStaticText(m_Tab_BAT, 0,	wxT("XX, XX"), wxDefaultPosition, wxDefaultSize, 0, wxEmptyString);
		t_BAT_CheckSum2[i] = new wxStaticText(m_Tab_BAT, 0, wxT("XX, XX"), wxDefaultPosition, wxDefaultSize, 0, wxEmptyString);
		t_BAT_UpdateCounter[i] = new wxStaticText(m_Tab_BAT, 0, wxT("XX, XX"), wxDefaultPosition, wxDefaultSize, 0, wxEmptyString);
		t_BAT_FreeBlocks[i] = new wxStaticText(m_Tab_BAT, 0, wxT("XX, XX"), wxDefaultPosition, wxDefaultSize, 0, wxEmptyString);
		t_BAT_LastAllocated[i] = new wxStaticText(m_Tab_BAT, 0, wxT("XX, XX"), wxDefaultPosition, wxDefaultSize, 0, wxEmptyString);



		sOtPaths[i]->Add(st[i][0], wxGBPosition(0, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
		sOtPaths[i]->Add(t_BAT_CheckSum1[i], wxGBPosition(0, 1), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);
		
		sOtPaths[i]->Add(st[i][1], wxGBPosition(1, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
		sOtPaths[i]->Add(t_BAT_CheckSum2[i], wxGBPosition(1, 1), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);

		sOtPaths[i]->Add(st[i][2], wxGBPosition(2, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
		sOtPaths[i]->Add(t_BAT_UpdateCounter[i], wxGBPosition(2, 1), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);
		
		sOtPaths[i]->Add(st[i][3], wxGBPosition(3, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
		sOtPaths[i]->Add(t_BAT_FreeBlocks[i], wxGBPosition(3, 1), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);

		sOtPaths[i]->Add(st[i][4], wxGBPosition(4, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
		sOtPaths[i]->Add(t_BAT_LastAllocated[i], wxGBPosition(4, 1), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);

		
		sBAT[i]->Add(sOtPaths[i], 0, wxEXPAND|wxALL, 5);
		sMain->Add(sBAT[i], 0, wxEXPAND|wxALL, 1);
	}

	for (int k=2;k<=3;k++)	//256
	{
		sOtPaths[k] = new wxGridBagSizer(0, 0);
		sOtPaths[k]->AddGrowableCol(1);
	
		for(int j=0;j<256;j++)
		{
			t_BAT_map[j][k%2] = new wxStaticText(m_Tab_BAT, 0,	wxT("XXXX, XXXX, XXXX, XXXX, XXXX, XXXX, XXXX, XXXX"), wxDefaultPosition, wxDefaultSize, 0, wxEmptyString);
			sOtPaths[k]->Add(t_BAT_map[j][k%2], wxGBPosition(j, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
		
		}

		sBAT[k]->Add(sOtPaths[k], 0, wxEXPAND|wxALL, 5);
		sMain->Add(sBAT[k], 0, wxEXPAND|wxALL, 1);
	}
	
	m_Tab_BAT->SetSizer(sMain);
	m_Tab_BAT->Layout();
}

void CMemcardManagerDebug::updateBATtab(int card)
{	
	wxString wx_UpdateCounter,
			 wx_CheckSum1,
			 wx_CheckSum2,
			 wx_FreeBlocks,
			 wx_LastAllocated;

	wx_CheckSum1.Printf(wxT("%02X, %02X"), memoryCard[card]->bat.CheckSum1[0],memoryCard[card]->bat.CheckSum1[1]);
	wx_CheckSum2.Printf(wxT("%02X, %02X"), memoryCard[card]->bat.CheckSum2[0],memoryCard[card]->bat.CheckSum2[1]);
	wx_UpdateCounter.Printf(wxT("%02X, %02X"), memoryCard[card]->bat.UpdateCounter[0],memoryCard[card]->bat.UpdateCounter[1]);
	wx_FreeBlocks.Printf(wxT("%02X, %02X"), memoryCard[card]->bat.FreeBlocks[0],memoryCard[card]->bat.FreeBlocks[1]);
	wx_LastAllocated.Printf(wxT("%d"), memoryCard[card]->bat.LastAllocated[0] << 8 | memoryCard[card]->bat.LastAllocated[1]);

	t_BAT_CheckSum1[card]->SetLabel(wx_CheckSum1);
	t_BAT_CheckSum2[card]->SetLabel(wx_CheckSum2);
	t_BAT_UpdateCounter[card]->SetLabel(wx_UpdateCounter);
	t_BAT_FreeBlocks[card]->SetLabel(wx_FreeBlocks);
	t_BAT_LastAllocated[card]->SetLabel(wx_LastAllocated);

	wxString wx_map[256];
	int pagesMax = 2048;
	pagesMax = (1) * 13*8;
	for(int j=0;j<2048 && j < pagesMax ;j+=8)
	{
		wx_map[j/8].Printf(wxT("%d, %d, %d, %d, %d, %d, %d, %d"),
			(memoryCard[card]->bat.Map[j] == 0xFFFF)? -1 : Common::swap16(memoryCard[card]->bat.Map[j]),
			(memoryCard[card]->bat.Map[j+1] == 0xFFFF)? -1 : Common::swap16(memoryCard[card]->bat.Map[j+1]),
			(memoryCard[card]->bat.Map[j+2] == 0xFFFF)? -1 : Common::swap16(memoryCard[card]->bat.Map[j+2]),
			(memoryCard[card]->bat.Map[j+3] == 0xFFFF)? -1 : Common::swap16(memoryCard[card]->bat.Map[j+3]),
			(memoryCard[card]->bat.Map[j+4] == 0xFFFF)? -1 : Common::swap16(memoryCard[card]->bat.Map[j+4]),
			(memoryCard[card]->bat.Map[j+5] == 0xFFFF)? -1 : Common::swap16(memoryCard[card]->bat.Map[j+5]),
			(memoryCard[card]->bat.Map[j+6] == 0xFFFF)? -1 : Common::swap16(memoryCard[card]->bat.Map[j+6]),
			(memoryCard[card]->bat.Map[j+7] == 0xFFFF)? -1 : Common::swap16(memoryCard[card]->bat.Map[j+7]));
		t_BAT_map[j/8][card]->SetLabel(wx_map[j/8]);
	}
}

void CMemcardManagerDebug::Init_BAT_b()
{
	wxBoxSizer *sMain;
	wxStaticBoxSizer *sBAT_b[2];
	sMain		= new wxBoxSizer(wxHORIZONTAL);
	sBAT_b[0]  = new wxStaticBoxSizer(wxVERTICAL, m_Tab_BAT_b, wxT("MEMCARD:A"));
	sBAT_b[1]  = new wxStaticBoxSizer(wxVERTICAL, m_Tab_BAT_b, wxT("MEMCARD:B"));

	wxGridBagSizer * sOtPaths[3];
	wxStaticText *st[2][9];
	
	for(int i = SLOT_A; i <=SLOT_B;i++)
	{
		sOtPaths[i] = new wxGridBagSizer(0, 0);
		sOtPaths[i]->AddGrowableCol(1);
		st[i][0]= new wxStaticText(m_Tab_BAT_b, 0, wxT("CheckSum1\t\t"), wxDefaultPosition, wxDefaultSize, 0, wxEmptyString);
		st[i][1]= new wxStaticText(m_Tab_BAT_b, 0, wxT("CheckSum2\t\t"), wxDefaultPosition, wxDefaultSize, 0, wxEmptyString);
		st[i][2]= new wxStaticText(m_Tab_BAT_b, 0, wxT("UpdateCounter\t\t"), wxDefaultPosition, wxDefaultSize, 0, wxEmptyString);
		st[i][3]= new wxStaticText(m_Tab_BAT_b, 0, wxT("FreeBlocks\t\t"), wxDefaultPosition, wxDefaultSize, 0, wxEmptyString);
		st[i][4]= new wxStaticText(m_Tab_BAT_b, 0, wxT("LastAllocated\t\t"), wxDefaultPosition, wxDefaultSize, 0, wxEmptyString);

		t_BAT_b_CheckSum1[i] = new wxStaticText(m_Tab_BAT_b, 0,	wxT("XX, XX"), wxDefaultPosition, wxDefaultSize, 0, wxEmptyString);
		t_BAT_b_CheckSum2[i] = new wxStaticText(m_Tab_BAT_b, 0, wxT("XX, XX"), wxDefaultPosition, wxDefaultSize, 0, wxEmptyString);
		t_BAT_b_UpdateCounter[i] = new wxStaticText(m_Tab_BAT_b, 0, wxT("XX, XX"), wxDefaultPosition, wxDefaultSize, 0, wxEmptyString);
		t_BAT_b_FreeBlocks[i] = new wxStaticText(m_Tab_BAT_b, 0, wxT("XX, XX"), wxDefaultPosition, wxDefaultSize, 0, wxEmptyString);
		t_BAT_b_LastAllocated[i] = new wxStaticText(m_Tab_BAT_b, 0, wxT("XX, XX"), wxDefaultPosition, wxDefaultSize, 0, wxEmptyString);

		sOtPaths[i]->Add(st[i][0], wxGBPosition(0, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
		sOtPaths[i]->Add(t_BAT_b_CheckSum1[i], wxGBPosition(0, 1), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);

		sOtPaths[i]->Add(st[i][1], wxGBPosition(1, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
		sOtPaths[i]->Add(t_BAT_b_CheckSum2[i], wxGBPosition(1, 1), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);

		sOtPaths[i]->Add(st[i][2], wxGBPosition(2, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
		sOtPaths[i]->Add(t_BAT_b_UpdateCounter[i], wxGBPosition(2, 1), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);

		sOtPaths[i]->Add(st[i][3], wxGBPosition(3, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
		sOtPaths[i]->Add(t_BAT_b_FreeBlocks[i], wxGBPosition(3, 1), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);

		sOtPaths[i]->Add(st[i][4], wxGBPosition(4, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
		sOtPaths[i]->Add(t_BAT_b_LastAllocated[i], wxGBPosition(4, 1), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);

		sBAT_b[i]->Add(sOtPaths[i], 0, wxEXPAND|wxALL, 5);
		sMain->Add(sBAT_b[i], 0, wxEXPAND|wxALL, 1);
	}
	
	m_Tab_BAT_b->SetSizer(sMain);
	m_Tab_BAT_b->Layout();
}

void CMemcardManagerDebug::updateBATBtab(int card)
{	
	wxString wx_UpdateCounter,
			 wx_CheckSum1,
			 wx_CheckSum2,
			 wx_FreeBlocks,
			 wx_LastAllocated;

	wx_CheckSum1.Printf(wxT("%02X, %02X"), memoryCard[card]->bat_backup.CheckSum1[0],memoryCard[card]->bat_backup.CheckSum1[1]);
	wx_CheckSum2.Printf(wxT("%02X, %02X"), memoryCard[card]->bat_backup.CheckSum2[0],memoryCard[card]->bat_backup.CheckSum2[1]);
	wx_UpdateCounter.Printf(wxT("%02X, %02X"), memoryCard[card]->bat_backup.UpdateCounter[0],memoryCard[card]->bat_backup.UpdateCounter[1]);
	wx_FreeBlocks.Printf(wxT("%02X, %02X"), memoryCard[card]->bat_backup.FreeBlocks[0],memoryCard[card]->bat_backup.FreeBlocks[1]);
	wx_LastAllocated.Printf(wxT("%02X, %02X"), memoryCard[card]->bat_backup.LastAllocated[0],memoryCard[card]->bat_backup.LastAllocated[1]);

	t_BAT_b_CheckSum1[card]->SetLabel(wx_CheckSum1);
	t_BAT_b_CheckSum2[card]->SetLabel(wx_CheckSum2);
	t_BAT_b_UpdateCounter[card]->SetLabel(wx_UpdateCounter);
	t_BAT_b_FreeBlocks[card]->SetLabel(wx_FreeBlocks);
	t_BAT_b_LastAllocated[card]->SetLabel(wx_LastAllocated);
}
