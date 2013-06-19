// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common.h"
#include "FifoPlayerDlg.h"
#include "FileUtil.h"
#include "Thread.h"
#include "FifoPlayer/FifoPlayer.h"
#include "FifoPlayer/FifoRecorder.h"
#include "OpcodeDecoding.h"
#include "WxUtils.h"

#include <wx/spinctrl.h>
#include <wx/clipbrd.h>

#include <algorithm>
#include <vector>

DECLARE_EVENT_TYPE(RECORDING_FINISHED_EVENT, -1)
DEFINE_EVENT_TYPE(RECORDING_FINISHED_EVENT)

DECLARE_EVENT_TYPE(FRAME_WRITTEN_EVENT, -1)
DEFINE_EVENT_TYPE(FRAME_WRITTEN_EVENT)

using namespace std;

std::recursive_mutex sMutex;
wxEvtHandler *volatile FifoPlayerDlg::m_EvtHandler = NULL;

FifoPlayerDlg::FifoPlayerDlg(wxWindow * const parent) :
	wxDialog(parent, wxID_ANY, _("FIFO Player"), wxDefaultPosition, wxDefaultSize),
	m_search_result_idx(0), m_FramesToRecord(1)
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
	Unbind(RECORDING_FINISHED_EVENT, &FifoPlayerDlg::OnRecordingFinished, this);
	Unbind(FRAME_WRITTEN_EVENT, &FifoPlayerDlg::OnFrameWritten, this);

	// Disconnect Events
	Unbind(wxEVT_PAINT, &FifoPlayerDlg::OnPaint, this);
	m_FrameFromCtrl->Unbind(wxEVT_COMMAND_SPINCTRL_UPDATED, &FifoPlayerDlg::OnFrameFrom, this);
	m_FrameToCtrl->Unbind(wxEVT_COMMAND_SPINCTRL_UPDATED, &FifoPlayerDlg::OnFrameTo, this);
	m_ObjectFromCtrl->Unbind(wxEVT_COMMAND_SPINCTRL_UPDATED, &FifoPlayerDlg::OnObjectFrom, this);
	m_ObjectToCtrl->Unbind(wxEVT_COMMAND_SPINCTRL_UPDATED, &FifoPlayerDlg::OnObjectTo, this);
	m_EarlyMemoryUpdates->Unbind(wxEVT_COMMAND_CHECKBOX_CLICKED, &FifoPlayerDlg::OnCheckEarlyMemoryUpdates, this);
	m_RecordStop->Unbind(wxEVT_COMMAND_BUTTON_CLICKED, &FifoPlayerDlg::OnRecordStop, this);
	m_Save->Unbind(wxEVT_COMMAND_BUTTON_CLICKED, &FifoPlayerDlg::OnSaveFile, this);
	m_FramesToRecordCtrl->Unbind(wxEVT_COMMAND_SPINCTRL_UPDATED, &FifoPlayerDlg::OnNumFramesToRecord, this);
	m_Close->Unbind(wxEVT_COMMAND_BUTTON_CLICKED, &FifoPlayerDlg::OnCloseClick, this);

	m_framesList->Unbind(wxEVT_COMMAND_LISTBOX_SELECTED, &FifoPlayerDlg::OnFrameListSelectionChanged, this);
	m_objectsList->Unbind(wxEVT_COMMAND_LISTBOX_SELECTED, &FifoPlayerDlg::OnObjectListSelectionChanged, this);
	m_objectCmdList->Unbind(wxEVT_COMMAND_LISTBOX_SELECTED, &FifoPlayerDlg::OnObjectCmdListSelectionChanged, this);

	FifoPlayer::GetInstance().SetFrameWrittenCallback(NULL);

	sMutex.lock();
	m_EvtHandler = NULL;
	sMutex.unlock();
}

void FifoPlayerDlg::CreateGUIControls()
{
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
	sPlayPage->AddStretchSpacer();

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
	
	wxString initialNum = wxString::Format(_T("%d"), m_FramesToRecord);
	m_FramesToRecordCtrl = new wxSpinCtrl(m_RecordPage, wxID_ANY, initialNum, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 10000, 1);
	sRecordingOptions->Add(m_FramesToRecordCtrl, 0, wxALL, 5);
	
	sRecordPage->Add(sRecordingOptions, 0, wxEXPAND, 5);
	sRecordPage->AddStretchSpacer();

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

	wxStaticBoxSizer* sFrameInfoSizer;
	sFrameInfoSizer = new wxStaticBoxSizer(new wxStaticBox(m_AnalyzePage, wxID_ANY, _("Frame Info")), wxVERTICAL);

	wxBoxSizer* sListsSizer = new wxBoxSizer(wxHORIZONTAL);

	m_framesList = new wxListBox(m_AnalyzePage, wxID_ANY);
	m_framesList->SetMinSize(wxSize(100, 250));
	sListsSizer->Add(m_framesList, 0, wxALL, 5);

	m_objectsList = new wxListBox(m_AnalyzePage, wxID_ANY);
	m_objectsList->SetMinSize(wxSize(110, 250));
	sListsSizer->Add(m_objectsList, 0, wxALL, 5);

	m_objectCmdList = new wxListBox(m_AnalyzePage, wxID_ANY);
	m_objectCmdList->SetMinSize(wxSize(175, 250));
	sListsSizer->Add(m_objectCmdList, 0, wxALL, 5);

	sFrameInfoSizer->Add(sListsSizer, 0, wxALL, 5);

	m_objectCmdInfo = new wxStaticText(m_AnalyzePage, wxID_ANY, wxString());
	sFrameInfoSizer->Add(m_objectCmdInfo, 0, wxALL, 5);

	sAnalyzePage->Add(sFrameInfoSizer, 0, wxEXPAND, 5);

	wxStaticBoxSizer* sSearchSizer = new wxStaticBoxSizer(new wxStaticBox(m_AnalyzePage, wxID_ANY, _("Search current Object")), wxVERTICAL);

	wxBoxSizer* sSearchField = new wxBoxSizer(wxHORIZONTAL);

	sSearchField->Add(new wxStaticText(m_AnalyzePage, wxID_ANY, _("Search for hex Value:")), 0, wxALIGN_CENTER_VERTICAL, 5);
	// TODO: ugh, wxValidator sucks - but we should use it anyway.
	m_searchField = new wxTextCtrl(m_AnalyzePage, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
	m_numResultsText = new wxStaticText(m_AnalyzePage, wxID_ANY, wxEmptyString);

	sSearchField->Add(m_searchField, 0, wxALL, 5);
	sSearchField->Add(m_numResultsText, 0, wxALIGN_CENTER_VERTICAL, 5);

	wxBoxSizer* sSearchButtons = new wxBoxSizer(wxHORIZONTAL);

	m_beginSearch = new wxButton(m_AnalyzePage, wxID_ANY, _("Search"));
	m_findNext = new wxButton(m_AnalyzePage, wxID_ANY, _("Find next"));
	m_findPrevious = new wxButton(m_AnalyzePage, wxID_ANY, _("Find previous"));

	ResetSearch();

	sSearchButtons->Add(m_beginSearch, 0, wxALL, 5);
	sSearchButtons->Add(m_findNext, 0, wxALL, 5);
	sSearchButtons->Add(m_findPrevious, 0, wxALL, 5);

	sSearchSizer->Add(sSearchField, 0, wxEXPAND, 5);
	sSearchSizer->Add(sSearchButtons, 0, wxEXPAND, 5);

	sAnalyzePage->Add(sSearchSizer, 0, wxEXPAND, 5);
	sAnalyzePage->AddStretchSpacer();

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
	Bind(wxEVT_PAINT, &FifoPlayerDlg::OnPaint, this);
	m_FrameFromCtrl->Bind(wxEVT_COMMAND_SPINCTRL_UPDATED, &FifoPlayerDlg::OnFrameFrom, this);
	m_FrameToCtrl->Bind(wxEVT_COMMAND_SPINCTRL_UPDATED, &FifoPlayerDlg::OnFrameTo, this);
	m_ObjectFromCtrl->Bind(wxEVT_COMMAND_SPINCTRL_UPDATED, &FifoPlayerDlg::OnObjectFrom, this);
	m_ObjectToCtrl->Bind(wxEVT_COMMAND_SPINCTRL_UPDATED, &FifoPlayerDlg::OnObjectTo, this);
	m_EarlyMemoryUpdates->Bind(wxEVT_COMMAND_CHECKBOX_CLICKED, &FifoPlayerDlg::OnCheckEarlyMemoryUpdates, this);
	m_RecordStop->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &FifoPlayerDlg::OnRecordStop, this);
	m_Save->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &FifoPlayerDlg::OnSaveFile, this);
	m_FramesToRecordCtrl->Bind(wxEVT_COMMAND_SPINCTRL_UPDATED, &FifoPlayerDlg::OnNumFramesToRecord, this);
	Bind(wxEVT_COMMAND_BUTTON_CLICKED, &FifoPlayerDlg::OnCloseClick, this);

	m_framesList->Bind(wxEVT_COMMAND_LISTBOX_SELECTED, &FifoPlayerDlg::OnFrameListSelectionChanged, this);
	m_objectsList->Bind(wxEVT_COMMAND_LISTBOX_SELECTED, &FifoPlayerDlg::OnObjectListSelectionChanged, this);
	m_objectCmdList->Bind(wxEVT_COMMAND_LISTBOX_SELECTED, &FifoPlayerDlg::OnObjectCmdListSelectionChanged, this);

	m_beginSearch->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &FifoPlayerDlg::OnBeginSearch, this);
	m_findNext->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &FifoPlayerDlg::OnFindNextClick, this);
	m_findPrevious->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &FifoPlayerDlg::OnFindPreviousClick, this);

	m_searchField->Bind(wxEVT_COMMAND_TEXT_ENTER, &FifoPlayerDlg::OnBeginSearch, this);
	m_searchField->Bind(wxEVT_COMMAND_TEXT_UPDATED, &FifoPlayerDlg::OnSearchFieldTextChanged, this);

	// Setup command copying
	wxAcceleratorEntry entry;
	entry.Set(wxACCEL_CTRL, (int)'C', wxID_COPY);
	wxAcceleratorTable accel(1, &entry);
	m_objectCmdList->SetAcceleratorTable(accel);
	m_objectCmdList->Bind(wxEVT_COMMAND_MENU_SELECTED, &FifoPlayerDlg::OnObjectCmdListSelectionCopy, this, wxID_COPY);

	Bind(RECORDING_FINISHED_EVENT, &FifoPlayerDlg::OnRecordingFinished, this);
	Bind(FRAME_WRITTEN_EVENT, &FifoPlayerDlg::OnFrameWritten, this);

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
	// Pointer to the file data that was created as a result of recording.
	FifoDataFile *file = FifoRecorder::GetInstance().GetRecordedFile();

	if (file)
	{
		// Bring up a save file dialog. The location the user chooses will be assigned to this variable.
		wxString path = wxSaveFileSelector(_("Dolphin FIFO"), wxT("dff"), wxEmptyString, this);

		// Has a valid file path
		if (!path.empty())
		{
			// Attempt to save the file to the path the user chose
			wxBeginBusyCursor();
			bool result = file->Save(WxStrToStr(path).c_str());
			wxEndBusyCursor();
			
			// Wasn't able to save the file, shit's whack, yo.
			if (!result)
				PanicAlert("Error saving file");
		}
	}
}

void FifoPlayerDlg::OnRecordStop(wxCommandEvent& WXUNUSED(event))
{
	FifoRecorder& recorder = FifoRecorder::GetInstance();

	// Recorder is still recording
	if (recorder.IsRecording())
	{
		// Then stop recording
		recorder.StopRecording();

		// and change the button label accordingly.
		m_RecordStop->SetLabel(_("Record"));
	}
	else // Recorder is actually about to start recording
	{
		// So start recording
		recorder.StartRecording(m_FramesToRecord, RecordingFinished);

		// and change the button label accordingly.
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

void FifoPlayerDlg::OnBeginSearch(wxCommandEvent& event)
{
	wxString str_search_val = m_searchField->GetValue();

	if (m_framesList->GetSelection() == -1)
		return;

	// TODO: Limited to even str lengths...
	if (str_search_val.Length() && str_search_val.Length() % 2)
	{
		m_numResultsText->SetLabel(_("Invalid search string (only even string lengths supported)"));
		return;
	}

	unsigned int const val_length = str_search_val.Length() / 2;
	std::vector<u8> search_val(val_length);
	for (unsigned int i = 0; i < val_length; ++i)
	{
		wxString char_str = str_search_val.Mid(2*i, 2);
		unsigned long val = 0;
		if (!char_str.ToULong(&val, 16))
		{
			m_numResultsText->SetLabel(_("Invalid search string (couldn't convert to number)"));
			return;
		}
		search_val[i] = (u8)val;
	}
	search_results.clear();

	int const frame_idx = m_framesList->GetSelection();
	FifoPlayer& player = FifoPlayer::GetInstance();
	const AnalyzedFrameInfo& frame = player.GetAnalyzedFrameInfo(frame_idx);
	const FifoFrameInfo& fifo_frame = player.GetFile()->GetFrame(frame_idx);

	// TODO: Support searching through the last object... How do we know were the cmd data ends?
	// TODO: Support searching for bit patterns
	int obj_idx = m_objectsList->GetSelection();
	if (obj_idx == -1)
	{
		m_numResultsText->SetLabel(_("Invalid search parameters (no object selected)"));
		return;
	}

	const u8* const start_ptr = &fifo_frame.fifoData[frame.objectStarts[obj_idx]];
	const u8* const end_ptr = &fifo_frame.fifoData[frame.objectStarts[obj_idx+1]];

	for (const u8* ptr = start_ptr; ptr < end_ptr-val_length+1; ++ptr)
	{
		if (std::equal(search_val.begin(), search_val.end(), ptr))
		{
			SearchResult result;
			result.frame_idx = frame_idx;

			result.obj_idx = m_objectsList->GetSelection();
			result.cmd_idx = 0;
			for (unsigned int cmd_idx = 1; cmd_idx < m_objectCmdOffsets.size(); ++cmd_idx)
			{
				if (ptr < start_ptr + m_objectCmdOffsets[cmd_idx])
				{
					result.cmd_idx = cmd_idx-1;
					break;
				}
			}
			search_results.push_back(result);
		}
	}

	ChangeSearchResult(0);
	m_beginSearch->Disable();
	m_numResultsText->SetLabel(wxString::Format(_("Found %d results for \'"), search_results.size()) + m_searchField->GetValue() + _("\'"));
}

void FifoPlayerDlg::OnSearchFieldTextChanged(wxCommandEvent& event)
{
	ResetSearch();
}

void FifoPlayerDlg::OnFindNextClick(wxCommandEvent& event)
{
	int cur_cmd_index = m_objectCmdList->GetSelection();
	if (cur_cmd_index == -1)
	{
		ChangeSearchResult(0);
		return;
	}

	for (std::vector<SearchResult>::iterator it = search_results.begin(); it != search_results.end(); ++it)
	{
		if (it->cmd_idx > cur_cmd_index)
		{
			ChangeSearchResult(it - search_results.begin());
			return;
		}
	}
}

void FifoPlayerDlg::OnFindPreviousClick(wxCommandEvent& event)
{
	int cur_cmd_index = m_objectCmdList->GetSelection();
	if (cur_cmd_index == -1)
	{
		ChangeSearchResult(search_results.size() - 1);
		return;
	}

	for (std::vector<SearchResult>::reverse_iterator it = search_results.rbegin(); it != search_results.rend(); ++it)
	{
		if (it->cmd_idx < cur_cmd_index)
		{
			ChangeSearchResult(search_results.size()-1 - (it - search_results.rbegin()));
			return;
		}
	}
}

void FifoPlayerDlg::ChangeSearchResult(unsigned int result_idx)
{
	if (result_idx < search_results.size()) // if index is valid
	{
		m_search_result_idx = result_idx;
		int prev_frame = m_framesList->GetSelection();
		int prev_obj = m_objectsList->GetSelection();
		int prev_cmd = m_objectCmdList->GetSelection();
		m_framesList->SetSelection(search_results[result_idx].frame_idx);
		m_objectsList->SetSelection(search_results[result_idx].obj_idx);
		m_objectCmdList->SetSelection(search_results[result_idx].cmd_idx);

		wxCommandEvent ev(wxEVT_COMMAND_LISTBOX_SELECTED);
		if (prev_frame != m_framesList->GetSelection())
		{
			ev.SetInt(m_framesList->GetSelection());
			OnFrameListSelectionChanged(ev);
		}
		if (prev_obj != m_objectsList->GetSelection())
		{
			ev.SetInt(m_objectsList->GetSelection());
			OnObjectListSelectionChanged(ev);
		}
		if (prev_cmd != m_objectCmdList->GetSelection())
		{
			ev.SetInt(m_objectCmdList->GetSelection());
			OnObjectCmdListSelectionChanged(ev);
		}

		m_findNext->Enable(result_idx+1 < search_results.size());
		m_findPrevious->Enable(result_idx != 0);
	}
	else if (search_results.size())
	{
		ChangeSearchResult(search_results.size() - 1);
	}
}

void FifoPlayerDlg::ResetSearch()
{
	m_beginSearch->Enable(m_searchField->GetLineLength(0) > 0);
	m_findNext->Disable();
	m_findPrevious->Disable();

	search_results.clear();
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

	// Update object list
	wxCommandEvent ev = wxCommandEvent(wxEVT_COMMAND_LISTBOX_SELECTED);
	ev.SetInt(-1);
	OnObjectListSelectionChanged(ev);

	ResetSearch();
}

void FifoPlayerDlg::OnObjectListSelectionChanged(wxCommandEvent& event)
{
	FifoPlayer& player = FifoPlayer::GetInstance();

	int frame_idx = m_framesList->GetSelection();
	int object_idx = event.GetInt();

	m_objectCmdList->Clear();
	m_objectCmdOffsets.clear();
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
		wxString newLabel = wxString::Format(wxT("%08X:  %02X %04X  "), obj_offset, cmd, stream_size);
		if (stream_size && ((objectdata_end - objectdata) % stream_size))
			newLabel += _("NOTE: Stream size doesn't match actual data length\n");

		while (objectdata < objectdata_end)
		{
			newLabel += wxString::Format(wxT("%02X"), *objectdata++);
		}
		m_objectCmdList->Append(newLabel);
		m_objectCmdOffsets.push_back(0);


		// Between objectdata_end and next_objdata_start, there are register setting commands
		if (object_idx + 1 < (int)frame.objectStarts.size())
		{
			const u8* next_objdata_start = &fifo_frame.fifoData[frame.objectStarts[object_idx+1]]; 
			while (objectdata < next_objdata_start)
			{
				m_objectCmdOffsets.push_back(objectdata - objectdata_start);
				int new_offset = objectdata - &fifo_frame.fifoData[frame.objectStarts[0]];
				int command = *objectdata++;
				switch (command)
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

						const u8* stream_start = objectdata;
						const u8* stream_end = stream_start + streamSize * 4;

						newLabel = wxString::Format(wxT("XF  %08X  "), cmd2);
						while (objectdata < stream_end)
						{
							newLabel += wxString::Format(wxT("%02X"), *objectdata++);
							if (((objectdata - stream_start) % 4) == 0) newLabel += wxT(" ");
						}
					}
					break;

				case GX_LOAD_INDX_A:
				case GX_LOAD_INDX_B:
				case GX_LOAD_INDX_C:
				case GX_LOAD_INDX_D:
					objectdata += 4;
					newLabel = wxString::Format(wxT("LOAD INDX %s"), (command == GX_LOAD_INDX_A) ? _("A") :
																	(command == GX_LOAD_INDX_B) ? _("B") :
																	(command == GX_LOAD_INDX_C) ? _("C") : _("D"));
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
				newLabel = wxString::Format(_("%08X:  "), new_offset) + newLabel;
				m_objectCmdList->Append(newLabel);
			}
		}
	}
	// Update command list
	wxCommandEvent ev = wxCommandEvent(wxEVT_COMMAND_LISTBOX_SELECTED);
	ev.SetInt(-1);
	OnObjectCmdListSelectionChanged(ev);

	ResetSearch();
}

void FifoPlayerDlg::OnObjectCmdListSelectionChanged(wxCommandEvent& event)
{
	const int frame_idx = m_framesList->GetSelection();
	const int object_idx =  m_objectsList->GetSelection();

	if (event.GetInt() == -1 || frame_idx == -1 || object_idx == -1)
	{
		m_objectCmdInfo->SetLabel(wxEmptyString);
		return;
	}

	FifoPlayer& player = FifoPlayer::GetInstance();
	const AnalyzedFrameInfo& frame = player.GetAnalyzedFrameInfo(frame_idx);
	const FifoFrameInfo& fifo_frame = player.GetFile()->GetFrame(frame_idx);
	const u8* cmddata = &fifo_frame.fifoData[frame.objectStarts[object_idx]] + m_objectCmdOffsets[event.GetInt()];

	// TODO: Not sure whether we should bother translating the descriptions
	wxString newLabel;
	if (*cmddata == GX_LOAD_BP_REG)
	{
		char name[64]="\0", desc[512]="\0";
		GetBPRegInfo(cmddata+1, name, sizeof(name), desc, sizeof(desc));
		newLabel = _("BP register ");
		newLabel += (name[0] != '\0') ? StrToWxStr(name) : wxString::Format(_("UNKNOWN_%02X"), *(cmddata+1));
		newLabel += wxT(":\n");
		if (desc[0] != '\0')
			newLabel += StrToWxStr(desc);
		else
			newLabel += _("No description available");
	}
	else if (*cmddata == GX_LOAD_CP_REG)
		newLabel = _("CP reg");
	else if (*cmddata == GX_LOAD_XF_REG)
		newLabel = _("XF reg");
	else
		newLabel = _("No description available");

	m_objectCmdInfo->SetLabel(newLabel);
	Layout();
	Fit();
}

void FifoPlayerDlg::OnObjectCmdListSelectionCopy(wxCommandEvent& WXUNUSED(event))
{
	if (wxTheClipboard->Open())
	{
		wxTheClipboard->SetData(new wxTextDataObject(m_objectCmdList->GetStringSelection()));
		wxTheClipboard->Close();
	}
}

void FifoPlayerDlg::OnCloseClick(wxCommandEvent& WXUNUSED(event))
{
	Hide();
}

void FifoPlayerDlg::OnRecordingFinished(wxEvent&)
{
	m_RecordStop->SetLabel(_("Record"));
	m_RecordStop->Enable();

	UpdateRecorderGui();
}

void FifoPlayerDlg::OnFrameWritten(wxEvent&)
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
	if ((int)m_framesList->GetCount() != num_frames)
	{
		m_framesList->Clear();

		for (int i = 0; i < num_frames; ++i)
		{
			m_framesList->Append(wxString::Format(wxT("Frame %i"), i));
		}

		wxCommandEvent ev = wxCommandEvent(wxEVT_COMMAND_LISTBOX_SELECTED);
		ev.SetInt(-1);
		OnFrameListSelectionChanged(ev);
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
