// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <vector>
#include <wx/dialog.h>

#include "Common/CommonTypes.h"

class wxButton;
class wxCheckBox;
class wxListBox;
class wxNotebook;
class wxPaintEvent;
class wxPanel;
class wxSpinCtrl;
class wxSpinEvent;
class wxStaticText;
class wxTextCtrl;

class FifoPlayerDlg : public wxDialog
{
public:
  FifoPlayerDlg(wxWindow* parent);
  ~FifoPlayerDlg();

private:
  void CreateGUIControls();

  void OnPaint(wxPaintEvent& event);
  void OnFrameFrom(wxSpinEvent& event);
  void OnFrameTo(wxSpinEvent& event);
  void OnObjectFrom(wxSpinEvent& event);
  void OnObjectTo(wxSpinEvent& event);
  void OnCheckEarlyMemoryUpdates(wxCommandEvent& event);
  void OnRecordStop(wxCommandEvent& event);
  void OnSaveFile(wxCommandEvent& event);
  void OnNumFramesToRecord(wxSpinEvent& event);

  void OnBeginSearch(wxCommandEvent& event);
  void OnFindNextClick(wxCommandEvent& event);
  void OnFindPreviousClick(wxCommandEvent& event);
  void OnSearchFieldTextChanged(wxCommandEvent& event);
  void ChangeSearchResult(unsigned int result_idx);
  void ResetSearch();

  void OnRecordingFinished(wxEvent& event);
  void OnFrameWritten(wxEvent& event);

  void OnFrameListSelectionChanged(wxCommandEvent& event);
  void OnObjectListSelectionChanged(wxCommandEvent& event);
  void OnObjectCmdListSelectionChanged(wxCommandEvent& event);
  void OnObjectCmdListSelectionCopy(wxCommandEvent& WXUNUSED(event));

  void UpdatePlayGui();
  void UpdateRecorderGui();
  void UpdateAnalyzerGui();

  wxString CreateFileFrameCountLabel() const;
  wxString CreateCurrentFrameLabel() const;
  wxString CreateFileObjectCountLabel() const;
  wxString CreateRecordingFifoSizeLabel() const;
  wxString CreateRecordingMemSizeLabel() const;
  wxString CreateRecordingFrameCountLabel() const;

  bool GetSaveButtonEnabled() const;

  // Called from a non-GUI thread
  static void RecordingFinished();
  static void FileLoaded();
  static void FrameWritten();

  static wxEvtHandler* volatile m_EvtHandler;

  wxNotebook* m_Notebook;
  wxPanel* m_PlayPage;
  wxStaticText* m_NumFramesLabel;
  wxStaticText* m_CurrentFrameLabel;
  wxStaticText* m_NumObjectsLabel;
  wxStaticText* m_FrameFromLabel;
  wxSpinCtrl* m_FrameFromCtrl;
  wxStaticText* m_FrameToLabel;
  wxSpinCtrl* m_FrameToCtrl;
  wxStaticText* m_ObjectFromLabel;
  wxSpinCtrl* m_ObjectFromCtrl;
  wxStaticText* m_ObjectToLabel;
  wxSpinCtrl* m_ObjectToCtrl;
  wxCheckBox* m_EarlyMemoryUpdates;
  wxPanel* m_RecordPage;
  wxStaticText* m_RecordingFifoSizeLabel;
  wxStaticText* m_RecordingMemSizeLabel;
  wxStaticText* m_RecordingFramesLabel;
  wxButton* m_RecordStop;
  wxButton* m_Save;
  wxStaticText* m_FramesToRecordLabel;
  wxSpinCtrl* m_FramesToRecordCtrl;

  wxPanel* m_AnalyzePage;
  wxListBox* m_framesList;
  wxListBox* m_objectsList;
  wxListBox* m_objectCmdList;
  std::vector<u32> m_objectCmdOffsets;
  wxStaticText* m_objectCmdInfo;

  wxTextCtrl* m_searchField;
  wxButton* m_beginSearch;
  wxButton* m_findNext;
  wxButton* m_findPrevious;
  wxStaticText* m_numResultsText;

  struct SearchResult
  {
    int frame_idx;
    int obj_idx;
    int cmd_idx;
  };
  std::vector<SearchResult> search_results;
  unsigned int m_search_result_idx;

  s32 m_FramesToRecord;
};
