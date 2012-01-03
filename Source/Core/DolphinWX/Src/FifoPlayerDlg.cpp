// Copyright (C) 2003 Dolphin Project.

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

#include "Common.h"
#include "FifoPlayerDlg.h"
#include "FileUtil.h"
#include "Thread.h"
#include "FifoPlayer/FifoPlayer.h"
#include "FifoPlayer/FifoRecorder.h"
#include "OpcodeDecoding.h"
#include <wx/spinctrl.h>

DECLARE_EVENT_TYPE(RECORDING_FINISHED_EVENT, -1)
DEFINE_EVENT_TYPE(RECORDING_FINISHED_EVENT)

DECLARE_EVENT_TYPE(FRAME_WRITTEN_EVENT, -1)
DEFINE_EVENT_TYPE(FRAME_WRITTEN_EVENT)

using namespace std;

std::recursive_mutex sMutex;
wxEvtHandler *volatile FifoPlayerDlg::m_EvtHandler = NULL;

FifoPlayerDlg::FifoPlayerDlg(wxWindow * const parent) :
	wxDialog(parent, wxID_ANY, _("FIFO Player"), wxDefaultPosition, wxDefaultSize),
	m_FramesToRecord(1)
{
	CreateGUIControls();

	sMutex.lock();	
	m_EvtHandler = GetEventHandler();	
	sMutex.unlock();

	FifoPlayer::GetInstance().SetFileLoadedCallback(FileLoaded);
	FifoPlayer::GetInstance().SetFrameWrittenCallback(FrameWritten);
}

FifoPlayerDlg::~FifoPlayerDlg()
{
	Disconnect(RECORDING_FINISHED_EVENT, wxCommandEventHandler(FifoPlayerDlg::OnRecordingFinished), NULL, this);
	Disconnect(FRAME_WRITTEN_EVENT, wxCommandEventHandler(FifoPlayerDlg::OnFrameWritten), NULL, this);

	// Disconnect Events
	Disconnect(wxEVT_PAINT, wxPaintEventHandler(FifoPlayerDlg::OnPaint), NULL, this);
	m_FrameFromCtrl->Disconnect(wxEVT_COMMAND_SPINCTRL_UPDATED, wxSpinEventHandler(FifoPlayerDlg::OnFrameFrom), NULL, this);
	m_FrameToCtrl->Disconnect(wxEVT_COMMAND_SPINCTRL_UPDATED, wxSpinEventHandler(FifoPlayerDlg::OnFrameTo), NULL, this);
	m_ObjectFromCtrl->Disconnect(wxEVT_COMMAND_SPINCTRL_UPDATED, wxSpinEventHandler(FifoPlayerDlg::OnObjectFrom), NULL, this);
	m_ObjectToCtrl->Disconnect(wxEVT_COMMAND_SPINCTRL_UPDATED, wxSpinEventHandler(FifoPlayerDlg::OnObjectTo), NULL, this);
	m_EarlyMemoryUpdates->Disconnect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler(FifoPlayerDlg::OnCheckEarlyMemoryUpdates), NULL, this);
	m_RecordStop->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(FifoPlayerDlg::OnRecordStop), NULL, this);
	m_Save->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(FifoPlayerDlg::OnSaveFile), NULL, this);	
	m_FramesToRecordCtrl->Disconnect(wxEVT_COMMAND_SPINCTRL_UPDATED, wxSpinEventHandler(FifoPlayerDlg::OnNumFramesToRecord), NULL, this);
	m_Close->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(FifoPlayerDlg::OnCloseClick), NULL, this);

	m_framesList->Disconnect(wxEVT_COMMAND_LISTBOX_SELECTED, wxCommandEventHandler(FifoPlayerDlg::OnFrameListSelectionChanged), NULL, this);
	m_objectsList->Disconnect(wxEVT_COMMAND_LISTBOX_SELECTED, wxCommandEventHandler(FifoPlayerDlg::OnObjectListSelectionChanged), NULL, this);
	m_objectCmdList->Disconnect(wxEVT_COMMAND_LISTBOX_SELECTED, wxCommandEventHandler(FifoPlayerDlg::OnObjectCmdListSelectionChanged), NULL, this);

	FifoPlayer::GetInstance().SetFrameWrittenCallback(NULL);

	sMutex.lock();	
	m_EvtHandler = NULL;
	sMutex.unlock();
}

void FifoPlayerDlg::CreateGUIControls()
{
	SetSizeHints(wxDefaultSize, wxDefaultSize);
	
	wxBoxSizer* sMain;
	sMain = new wxBoxSizer(wxVERTICAL);
	
	m_Notebook = new wxNotebook(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0);

	{
	m_PlayPage = new wxPanel(m_Notebook, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
	wxBoxSizer* sPlayPage;
	sPlayPage = new wxBoxSizer(wxVERTICAL);
	
	wxStaticBoxSizer* sPlayInfo;
	sPlayInfo = new wxStaticBoxSizer(new wxStaticBox(m_PlayPage, wxID_ANY, _("File Info")), wxVERTICAL);
	
	m_NumFramesLabel = new wxStaticText(m_PlayPage, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0);
	m_NumFramesLabel->Wrap(-1);
	sPlayInfo->Add(m_NumFramesLabel, 0, wxALL, 5);
	
	m_CurrentFrameLabel = new wxStaticText(m_PlayPage, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0);
	m_CurrentFrameLabel->Wrap(-1);
	sPlayInfo->Add(m_CurrentFrameLabel, 0, wxALL, 5);
	
	m_NumObjectsLabel = new wxStaticText(m_PlayPage, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0);
	m_NumObjectsLabel->Wrap(-1);
	sPlayInfo->Add(m_NumObjectsLabel, 0, wxALL, 5);
	
	sPlayPage->Add(sPlayInfo, 1, wxEXPAND, 5);
	
	wxStaticBoxSizer* sFrameRange;
	sFrameRange = new wxStaticBoxSizer(new wxStaticBox(m_PlayPage, wxID_ANY, _("Frame Range")), wxHORIZONTAL);
	
	m_FrameFromLabel = new wxStaticText(m_PlayPage, wxID_ANY, _("From"), wxDefaultPosition, wxDefaultSize, 0);
	m_FrameFromLabel->Wrap(-1);
	sFrameRange->Add(m_FrameFromLabel, 0, wxALL, 5);
	
	m_FrameFromCtrl = new wxSpinCtrl(m_PlayPage, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 10, 0);
	sFrameRange->Add(m_FrameFromCtrl, 0, wxALL, 5);
	
	m_FrameToLabel = new wxStaticText(m_PlayPage, wxID_ANY, _("To"), wxDefaultPosition, wxDefaultSize, 0);
	m_FrameToLabel->Wrap(-1);
	sFrameRange->Add(m_FrameToLabel, 0, wxALL, 5);
	
	m_FrameToCtrl = new wxSpinCtrl(m_PlayPage, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(-1,-1), wxSP_ARROW_KEYS, 0, 10, 0);
	sFrameRange->Add(m_FrameToCtrl, 0, wxALL, 5);
	
	sPlayPage->Add(sFrameRange, 0, wxEXPAND, 5);
	
	wxStaticBoxSizer* sObjectRange;
	sObjectRange = new wxStaticBoxSizer(new wxStaticBox(m_PlayPage, wxID_ANY, _("Object Range")), wxHORIZONTAL);
	
	m_ObjectFromLabel = new wxStaticText(m_PlayPage, wxID_ANY, _("From"), wxDefaultPosition, wxDefaultSize, 0);
	m_ObjectFromLabel->Wrap(-1);
	sObjectRange->Add(m_ObjectFromLabel, 0, wxALL, 5);
	
	m_ObjectFromCtrl = new wxSpinCtrl(m_PlayPage, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 10000, 0);
	sObjectRange->Add(m_ObjectFromCtrl, 0, wxALL, 5);
	
	m_ObjectToLabel = new wxStaticText(m_PlayPage, wxID_ANY, _("To"), wxDefaultPosition, wxDefaultSize, 0);
	m_ObjectToLabel->Wrap(-1);
	sObjectRange->Add(m_ObjectToLabel, 0, wxALL, 5);
	
	m_ObjectToCtrl = new wxSpinCtrl(m_PlayPage, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 10000, 0);
	sObjectRange->Add(m_ObjectToCtrl, 0, wxALL, 5);
	
	sPlayPage->Add(sObjectRange, 0, wxEXPAND, 5);
	
	wxStaticBoxSizer* sPlayOptions;
	sPlayOptions = new wxStaticBoxSizer(new wxStaticBox(m_PlayPage, wxID_ANY, _("Playback Options")), wxVERTICAL);
	
	m_EarlyMemoryUpdates = new wxCheckBox(m_PlayPage, wxID_ANY, _("Early Memory Updates"), wxDefaultPosition, wxDefaultSize, 0);
	sPlayOptions->Add(m_EarlyMemoryUpdates, 0, wxALL, 5);
	
	sPlayPage->Add(sPlayOptions, 0, wxEXPAND, 5);
	
	m_PlayPage->SetSizer(sPlayPage);
	m_PlayPage->Layout();
	sPlayPage->Fit(m_PlayPage);
	m_Notebook->AddPage(m_PlayPage, _("Play"), true);
	}

	{
	m_RecordPage = new wxPanel(m_Notebook, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
	wxBoxSizer* sRecordPage;
	sRecordPage = new wxBoxSizer(wxVERTICAL);
	
	wxStaticBoxSizer* sRecordInfo;
	sRecordInfo = new wxStaticBoxSizer(new wxStaticBox(m_RecordPage, wxID_ANY, _("Recording Info")), wxVERTICAL);
	
	m_RecordingFifoSizeLabel = new wxStaticText(m_RecordPage, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0);
	m_RecordingFifoSizeLabel->Wrap(-1);
	sRecordInfo->Add(m_RecordingFifoSizeLabel, 0, wxALL, 5);
	
	m_RecordingMemSizeLabel = new wxStaticText(m_RecordPage, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0);
	m_RecordingMemSizeLabel->Wrap(-1);
	sRecordInfo->Add(m_RecordingMemSizeLabel, 0, wxALL, 5);
	
	m_RecordingFramesLabel = new wxStaticText(m_RecordPage, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0);
	m_RecordingFramesLabel->Wrap(-1);
	sRecordInfo->Add(m_RecordingFramesLabel, 0, wxALL, 5);
	
	sRecordPage->Add(sRecordInfo, 0, wxEXPAND, 5);
	
	wxBoxSizer* sRecordButtons;
	sRecordButtons = new wxBoxSizer(wxHORIZONTAL);
	
	m_RecordStop = new wxButton(m_RecordPage, wxID_ANY, _("Record"), wxDefaultPosition, wxDefaultSize, 0);
	sRecordButtons->Add(m_RecordStop, 0, wxALL, 5);
	
	m_Save = new wxButton(m_RecordPage, wxID_ANY, _("Save"), wxDefaultPosition, wxDefaultSize, 0);
	sRecordButtons->Add(m_Save, 0, wxALL, 5);
	
	sRecordPage->Add(sRecordButtons, 0, wxEXPAND, 5);
	
	wxStaticBoxSizer* sRecordingOptions;
	sRecordingOptions = new wxStaticBoxSizer(new wxStaticBox(m_RecordPage, wxID_ANY, _("Recording Options")), wxHORIZONTAL);
	
	m_FramesToRecordLabel = new wxStaticText(m_RecordPage, wxID_ANY, _("Frames To Record"), wxDefaultPosition, wxDefaultSize, 0);
	m_FramesToRecordLabel->Wrap(-1);
	sRecordingOptions->Add(m_FramesToRecordLabel, 0, wxALL, 5);
	
	m_FramesToRecordCtrl = new wxSpinCtrl(m_RecordPage, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 10000, 1);
	sRecordingOptions->Add(m_FramesToRecordCtrl, 0, wxALL, 5);
	
	sRecordPage->Add(sRecordingOptions, 0, wxEXPAND, 5);
	
	m_RecordPage->SetSizer(sRecordPage);
	m_RecordPage->Layout();
	sRecordPage->Fit(m_RecordPage);
	m_Notebook->AddPage(m_RecordPage, _("Record"), false);
	}

	// Analyze page
	{
	m_AnalyzePage = new wxPanel(m_Notebook, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
	wxBoxSizer* sAnalyzePage;
	sAnalyzePage = new wxBoxSizer(wxVERTICAL);

	wxStaticBoxSizer* sTestSizer;
	sTestSizer = new wxStaticBoxSizer(new wxStaticBox(m_AnalyzePage, wxID_ANY, _("Frame info")), wxHORIZONTAL);

	m_framesList = new wxListBox(m_AnalyzePage, wxID_ANY);
	m_framesList->SetMinSize(wxSize(100, 250));
	sTestSizer->Add(m_framesList, 0, wxALL, 5);

	m_objectsList = new wxListBox(m_AnalyzePage, wxID_ANY);
	m_objectsList->SetMinSize(wxSize(110, 250));
	sTestSizer->Add(m_objectsList, 0, wxALL, 5);

	m_objectCmdList = new wxListBox(m_AnalyzePage, wxID_ANY);
	m_objectCmdList->SetMinSize(wxSize(175, 250));
	sTestSizer->Add(m_objectCmdList, 0, wxALL, 5);

	sAnalyzePage->Add(sTestSizer, 0, wxEXPAND, 5);
	
	m_AnalyzePage->SetSizer(sAnalyzePage);
	m_AnalyzePage->Layout();
	sAnalyzePage->Fit(m_AnalyzePage);
	m_Notebook->AddPage(m_AnalyzePage, _("Analyze"), false);
	}

	sMain->Add(m_Notebook, 1, wxEXPAND | wxALL, 5);

	wxBoxSizer* sButtons;
	sButtons = new wxBoxSizer(wxHORIZONTAL);

	wxBoxSizer* sCloseButtonExpander;
	sCloseButtonExpander = new wxBoxSizer(wxHORIZONTAL);

	sButtons->Add(sCloseButtonExpander, 1, wxEXPAND, 5);

	m_Close = new wxButton(this, wxID_ANY, _("Close"), wxDefaultPosition, wxDefaultSize, 0);
	sButtons->Add(m_Close, 0, wxALL, 5);

	sMain->Add(sButtons, 0, wxEXPAND, 5);

	SetSizer(sMain);
	Layout();
	sMain->Fit(this);

	Center(wxBOTH);

	// Connect Events
	Connect(wxEVT_PAINT, wxPaintEventHandler(FifoPlayerDlg::OnPaint));
	m_FrameFromCtrl->Connect(wxEVT_COMMAND_SPINCTRL_UPDATED, wxSpinEventHandler(FifoPlayerDlg::OnFrameFrom), NULL, this);
	m_FrameToCtrl->Connect(wxEVT_COMMAND_SPINCTRL_UPDATED, wxSpinEventHandler(FifoPlayerDlg::OnFrameTo), NULL, this);
	m_ObjectFromCtrl->Connect(wxEVT_COMMAND_SPINCTRL_UPDATED, wxSpinEventHandler(FifoPlayerDlg::OnObjectFrom), NULL, this);
	m_ObjectToCtrl->Connect(wxEVT_COMMAND_SPINCTRL_UPDATED, wxSpinEventHandler(FifoPlayerDlg::OnObjectTo), NULL, this);
	m_EarlyMemoryUpdates->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler(FifoPlayerDlg::OnCheckEarlyMemoryUpdates), NULL, this);
	m_RecordStop->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(FifoPlayerDlg::OnRecordStop), NULL, this);
	m_Save->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(FifoPlayerDlg::OnSaveFile), NULL, this);
	m_FramesToRecordCtrl->Connect(wxEVT_COMMAND_SPINCTRL_UPDATED, wxSpinEventHandler(FifoPlayerDlg::OnNumFramesToRecord), NULL, this);
	Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(FifoPlayerDlg::OnCloseClick), NULL, this);

	m_framesList->Connect(wxEVT_COMMAND_LISTBOX_SELECTED, wxCommandEventHandler(FifoPlayerDlg::OnFrameListSelectionChanged), NULL, this);
	m_objectsList->Connect(wxEVT_COMMAND_LISTBOX_SELECTED, wxCommandEventHandler(FifoPlayerDlg::OnObjectListSelectionChanged), NULL, this);
	m_objectCmdList->Connect(wxEVT_COMMAND_LISTBOX_SELECTED, wxCommandEventHandler(FifoPlayerDlg::OnObjectCmdListSelectionChanged), NULL, this);

	Connect(RECORDING_FINISHED_EVENT, wxCommandEventHandler(FifoPlayerDlg::OnRecordingFinished), NULL, this);
	Connect(FRAME_WRITTEN_EVENT, wxCommandEventHandler(FifoPlayerDlg::OnFrameWritten), NULL, this);

	Show();
}

void FifoPlayerDlg::OnPaint(wxPaintEvent& event)
{
	UpdatePlayGui();
	UpdateRecorderGui();
	UpdateAnalyzerGui();

	event.Skip();
}

void FifoPlayerDlg::OnFrameFrom(wxSpinEvent& event)
{
	FifoPlayer &player = FifoPlayer::GetInstance();

	player.SetFrameRangeStart(event.GetPosition());

	m_FrameFromCtrl->SetValue(player.GetFrameRangeStart());
	m_FrameToCtrl->SetValue(player.GetFrameRangeEnd());
}

void FifoPlayerDlg::OnFrameTo(wxSpinEvent& event)
{
	FifoPlayer &player = FifoPlayer::GetInstance();
	player.SetFrameRangeEnd(event.GetPosition());

	m_FrameFromCtrl->SetValue(player.GetFrameRangeStart());
	m_FrameToCtrl->SetValue(player.GetFrameRangeEnd());	
}

void FifoPlayerDlg::OnObjectFrom(wxSpinEvent& event)
{
	FifoPlayer::GetInstance().SetObjectRangeStart(event.GetPosition());
}

void FifoPlayerDlg::OnObjectTo(wxSpinEvent& event)
{
	FifoPlayer::GetInstance().SetObjectRangeEnd(event.GetPosition());
}

void FifoPlayerDlg::OnCheckEarlyMemoryUpdates(wxCommandEvent& event)
{
	FifoPlayer::GetInstance().SetEarlyMemoryUpdates(event.IsChecked());
}

void FifoPlayerDlg::OnSaveFile(wxCommandEvent& WXUNUSED(event))
{
	FifoDataFile *file = FifoRecorder::GetInstance().GetRecordedFile();

	if (file)
		{
		wxString path = wxSaveFileSelector(_("Dolphin FIFO"), wxT("dff"), wxEmptyString, this);

		if (!path.empty())
		{
			wxBeginBusyCursor();
			bool result = file->Save(path.mb_str());
			wxEndBusyCursor();
			
			if (!result)
				PanicAlert("Error saving file");
		}
	}
}

void FifoPlayerDlg::OnRecordStop(wxCommandEvent& WXUNUSED(event))
{
	FifoRecorder& recorder = FifoRecorder::GetInstance();

	if (recorder.IsRecording())
	{
		recorder.StopRecording();
		m_RecordStop->Disable();
	}
	else
	{
		recorder.StartRecording(m_FramesToRecord, RecordingFinished);
		m_RecordStop->SetLabel(_("Stop"));
	}
}

void FifoPlayerDlg::OnNumFramesToRecord(wxSpinEvent& event)
{
	m_FramesToRecord = event.GetPosition();

	// Entering 0 frames in the control indicates infinite frames to record
	// The fifo recorder takes any value < 0 to be infinite frames
	if (m_FramesToRecord < 1)
		m_FramesToRecord = -1;
}

void FifoPlayerDlg::OnFrameListSelectionChanged(wxCommandEvent& event)
{
	FifoPlayer& player = FifoPlayer::GetInstance();

	m_objectsList->Clear();
	if (event.GetInt() != -1)
	{
		int num_objects = player.GetAnalyzedFrameInfo(event.GetInt()).objectStarts.size();
		for (int i = 0; i < num_objects; ++i)
			m_objectsList->Append(wxString::Format(wxT("Object %i"), i));
	}

	// Call OnObjectListSelectionChanged
	m_objectsList->SetSelection(-1);
}

void FifoPlayerDlg::OnObjectListSelectionChanged (wxCommandEvent& event)
{
	FifoPlayer& player = FifoPlayer::GetInstance();

	int frame_idx = m_framesList->GetSelection();
	int object_idx = event.GetInt();

	m_objectCmdList->Clear();
	if (frame_idx != -1 && object_idx != -1)
	{
		const AnalyzedFrameInfo& frame = player.GetAnalyzedFrameInfo(frame_idx);
		const FifoFrameInfo& fifo_frame = player.GetFile()->GetFrame(frame_idx);
		const u8* objectdata_start = &fifo_frame.fifoData[frame.objectStarts[object_idx]]; 
		const u8* objectdata_end = &fifo_frame.fifoData[frame.objectEnds[object_idx]]; 
		u8* objectdata = (u8*)objectdata_start;
		const int obj_offset = objectdata_start - &fifo_frame.fifoData[frame.objectStarts[0]];

		int cmd = *objectdata++;
		int stream_size = Common::swap16(objectdata);
		objectdata += 2;
		int vertex_size = (objectdata_end - objectdata) / stream_size;
		wxString newLabel = wxString::Format(wxT("%08X:  %02X %04X  "), obj_offset, cmd, stream_size);
		if ((objectdata_end - objectdata) % stream_size) newLabel += _("NOTE: Stream size doesn't match actual data length\n");
		while (objectdata < objectdata_end)
		{
			// Group bytes by vertex
			newLabel += wxString::Format(wxT("%02X"), *objectdata++);
			if (((objectdata - (objectdata_start+3)) % vertex_size) == 0) newLabel += wxT(" ");
		}
		m_objectCmdList->Append(newLabel);


		// Between objectdata_end and next_objdata_start, there are register setting commands
		if (object_idx + 1 < frame.objectStarts.size())
		{
			const u8* next_objdata_start = &fifo_frame.fifoData[frame.objectStarts[object_idx+1]]; 
			while (objectdata < next_objdata_start)
			{
				int cmd = *objectdata++;
				switch (cmd)
				{
				case GX_NOP:
					newLabel = _("NOP");
					break;

				case 0x44:
					newLabel = _("0x44");
					break;

				case GX_CMD_INVL_VC:
					newLabel = _("GX_CMD_INVL_VC");
					break;

				case GX_LOAD_CP_REG:
					{
						u32 cmd2 = *objectdata++;
						u32 value = Common::swap32(objectdata);
						objectdata += 4;

						newLabel = wxString::Format(wxT("CP  %02X  %08X"), cmd2, value);
					}
					break;

				case GX_LOAD_XF_REG:
					{
						u32 cmd2 = Common::swap32(objectdata);
						objectdata += 4;
						u8 streamSize = ((cmd2 >> 16) & 15) + 1;

						newLabel = wxString::Format(wxT("XF  %08X(%02X)  ..."), cmd2, streamSize);

						objectdata += streamSize * 4;
					}
					break;

				case GX_LOAD_INDX_A:
				case GX_LOAD_INDX_B:
				case GX_LOAD_INDX_C:
				case GX_LOAD_INDX_D:
					objectdata += 4;
					newLabel = wxString::Format(wxT("LOAD INDX %s"), (cmd == GX_LOAD_INDX_A) ? "A" :
																	(cmd == GX_LOAD_INDX_B) ? "B" :
																	(cmd == GX_LOAD_INDX_C) ? "C" : "D");
					break;

				case GX_CMD_CALL_DL:
					// The recorder should have expanded display lists into the fifo stream and skipped the call to start them
					// That is done to make it easier to track where memory is updated
					_assert_(false);
					objectdata += 8;
					newLabel = wxString::Format(wxT("CALL DL"));
					break;

				case GX_LOAD_BP_REG:
					{
						u32 cmd2 = Common::swap32(objectdata);
						objectdata += 4;
						newLabel = wxString::Format(wxT("BP  %02X %06X"), cmd2 >> 24, cmd2 & 0xFFFFFF);
					}
					break;

				default:
					newLabel = _("Unexpected 0x80 call? Aborting...");
					objectdata = (u8*)next_objdata_start;
					break;
				}
				m_objectCmdList->Append(newLabel);
			}
		}
	}
	// Call OnObjectCmdListSelectionChanged
	m_objectCmdList->SetSelection(-1);
}

void FifoPlayerDlg::OnObjectCmdListSelectionChanged (wxCommandEvent& event)
{

}

void FifoPlayerDlg::OnCloseClick(wxCommandEvent& WXUNUSED(event))
{
	Hide();
}

void FifoPlayerDlg::OnRecordingFinished(wxCommandEvent& WXUNUSED(event))
{
	m_RecordStop->SetLabel(_("Record"));
	m_RecordStop->Enable();

	UpdateRecorderGui();
}

void FifoPlayerDlg::OnFrameWritten(wxCommandEvent& WXUNUSED(event))
{
	m_CurrentFrameLabel->SetLabel(CreateCurrentFrameLabel());
	m_NumObjectsLabel->SetLabel(CreateFileObjectCountLabel());	
}

void FifoPlayerDlg::UpdatePlayGui()
{
	m_NumFramesLabel->SetLabel(CreateFileFrameCountLabel());
	m_CurrentFrameLabel->SetLabel(CreateCurrentFrameLabel());
	m_NumObjectsLabel->SetLabel(CreateFileObjectCountLabel());

	FifoPlayer &player = FifoPlayer::GetInstance();
	FifoDataFile *file = player.GetFile();
	u32 frameCount = 0;
	if (file)
		frameCount = file->GetFrameCount();

	m_FrameFromCtrl->SetRange(0, frameCount);
	m_FrameFromCtrl->SetValue(player.GetFrameRangeStart());

	m_FrameToCtrl->SetRange(0, frameCount);
	m_FrameToCtrl->SetValue(player.GetFrameRangeEnd());

	m_ObjectFromCtrl->SetValue(player.GetObjectRangeStart());
	m_ObjectToCtrl->SetValue(player.GetObjectRangeEnd());
}

void FifoPlayerDlg::UpdateRecorderGui()
{
	m_RecordingFifoSizeLabel->SetLabel(CreateRecordingFifoSizeLabel());
	m_RecordingMemSizeLabel->SetLabel(CreateRecordingMemSizeLabel());
	m_RecordingFramesLabel->SetLabel(CreateRecordingFrameCountLabel());	
	m_Save->Enable(GetSaveButtonEnabled());
}

void FifoPlayerDlg::UpdateAnalyzerGui()
{
	FifoPlayer &player = FifoPlayer::GetInstance();
	FifoDataFile* file = player.GetFile();

	int num_frames = (file) ? player.GetFile()->GetFrameCount() : 0;
	if (m_framesList->GetCount() != num_frames)
	{
		m_framesList->Clear();
		for (int i = 0; i < num_frames; ++i)
		{
			m_framesList->Append(wxString::Format(wxT("Frame %i"), i));
		}
		m_framesList->SetSelection(-1);
	}
}

wxString FifoPlayerDlg::CreateFileFrameCountLabel() const
{
	FifoDataFile *file = FifoPlayer::GetInstance().GetFile();

	if (file)
		return CreateIntegerLabel(file->GetFrameCount(), _("Frame"));
	
	return _("No file loaded");
}

wxString FifoPlayerDlg::CreateCurrentFrameLabel() const
{
	FifoDataFile *file = FifoPlayer::GetInstance().GetFile();

	if (file)
		return _("Frame ") + wxString::Format(wxT("%i"), FifoPlayer::GetInstance().GetCurrentFrameNum());
	
	return wxEmptyString;
}

wxString FifoPlayerDlg::CreateFileObjectCountLabel() const
{
	FifoDataFile *file = FifoPlayer::GetInstance().GetFile();

	if (file)
		return CreateIntegerLabel(FifoPlayer::GetInstance().GetFrameObjectCount(), _("Object"));
	
	return wxEmptyString;
}

wxString FifoPlayerDlg::CreateRecordingFifoSizeLabel() const
{
	FifoDataFile *file = FifoRecorder::GetInstance().GetRecordedFile();

	if (file)
	{
		int fifoBytes = 0;
		for (int i = 0; i < file->GetFrameCount(); ++i)
			fifoBytes += file->GetFrame(i).fifoDataSize;

		return CreateIntegerLabel(fifoBytes, _("FIFO Byte"));	
	}
	
	return _("No recorded file");
}

wxString FifoPlayerDlg::CreateRecordingMemSizeLabel() const
{
	FifoDataFile *file = FifoRecorder::GetInstance().GetRecordedFile();

	if (file)
	{
		int memBytes = 0;
		for (int frameNum = 0; frameNum < file->GetFrameCount(); ++frameNum)
		{
			const vector<MemoryUpdate>& memUpdates = file->GetFrame(frameNum).memoryUpdates;
			for (unsigned int i = 0; i < memUpdates.size(); ++i)
				memBytes += memUpdates[i].size;
		}

		return CreateIntegerLabel(memBytes, _("Memory Byte"));	
	}
	
	return wxEmptyString;
}

wxString FifoPlayerDlg::CreateRecordingFrameCountLabel() const
{
	FifoDataFile *file = FifoRecorder::GetInstance().GetRecordedFile();

	if (file)
	{
		int numFrames = file->GetFrameCount();
		return CreateIntegerLabel(numFrames, _("Frame"));
	}
	
	return wxEmptyString;
}

wxString FifoPlayerDlg::CreateIntegerLabel(int size, const wxString& label) const
{
	wxString postfix;
	if (size != 1)
		postfix = _("s");

	return wxString::Format(wxT("%i"), size) + wxT(" ") + label + postfix;
}

bool FifoPlayerDlg::GetSaveButtonEnabled() const
{
	return (FifoRecorder::GetInstance().GetRecordedFile() != NULL);
}

void FifoPlayerDlg::RecordingFinished()
{
	sMutex.lock();

	if (m_EvtHandler)
	{
		wxCommandEvent event(RECORDING_FINISHED_EVENT);
		m_EvtHandler->AddPendingEvent(event);
	}

	sMutex.unlock();
}

void FifoPlayerDlg::FileLoaded()
{
	sMutex.lock();

	if (m_EvtHandler)
	{
		wxPaintEvent event;
		m_EvtHandler->AddPendingEvent(event);
	}

	sMutex.unlock();
}

void FifoPlayerDlg::FrameWritten()
{
	sMutex.lock();

	if (m_EvtHandler)
	{
		wxCommandEvent event(FRAME_WRITTEN_EVENT);
		m_EvtHandler->AddPendingEvent(event);
	}

	sMutex.unlock();
}
