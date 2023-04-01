// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <wx/dialog.h>
#include <wx/treebase.h>

#include "Common/IniFile.h"
#include "Core/ActionReplay.h"
#include "DiscIO/Filesystem.h"
#include "DiscIO/Volume.h"
#include "DiscIO/VolumeCreator.h"
#include "DolphinWX/ARCodeAddEdit.h"
#include "DolphinWX/ISOFile.h"
#include "DolphinWX/PatchAddEdit.h"

class GameListItem;
class wxButton;
class wxCheckBox;
class wxCheckListBox;
class wxChoice;
class wxSlider;
class wxSpinCtrl;
class wxStaticBitmap;
class wxTextCtrl;
class wxTreeCtrl;

namespace DiscIO { struct SFileInfo; }
namespace Gecko { class CodeConfigPanel; }

class WiiPartition final : public wxTreeItemData
{
public:
	WiiPartition(std::unique_ptr<DiscIO::IVolume> partition, std::unique_ptr<DiscIO::IFileSystem> file_system)
		: Partition(std::move(partition)), FileSystem(std::move(file_system))
	{
	}

	std::unique_ptr<DiscIO::IVolume> Partition;
	std::unique_ptr<DiscIO::IFileSystem> FileSystem;
};

struct PHackData
{
	bool PHackSZNear;
	bool PHackSZFar;
	std::string PHZNear;
	std::string PHZFar;
};

class CISOProperties : public wxDialog
{
public:
	CISOProperties(const GameListItem& game_list_item,
			wxWindow* parent,
			wxWindowID id = wxID_ANY,
			const wxString& title = _("Properties"),
			const wxPoint& pos = wxDefaultPosition,
			const wxSize& size = wxDefaultSize,
			long style = wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
	virtual ~CISOProperties();

private:
	DECLARE_EVENT_TABLE();

	std::unique_ptr<DiscIO::IVolume> m_open_iso;
	std::unique_ptr<DiscIO::IFileSystem> m_filesystem;

	std::vector<PatchEngine::Patch> onFrame;
	std::vector<ActionReplay::ARCode> arCodes;
	PHackData m_PHack_Data;

	// Core
	wxCheckBox *CPUThread, *SkipIdle, *MMU, *DCBZOFF, *FPRF;
	wxCheckBox *SyncGPU, *FastDiscSpeed, *DSPHLE;

	wxArrayString arrayStringFor_GPUDeterminism;
	wxChoice* GPUDeterminism;
	// Wii
	wxCheckBox* EnableWideScreen;

	// Stereoscopy
	wxSlider* DepthPercentage;
	wxSpinCtrl* Convergence;
	wxCheckBox* MonoDepth;

	wxArrayString arrayStringFor_EmuState;
	wxChoice* EmuState;
	wxTextCtrl* EmuIssues;

	wxCheckListBox* Patches;
	wxButton* EditPatch;
	wxButton* RemovePatch;

	wxCheckListBox* Cheats;
	wxButton* EditCheat;
	wxButton* RemoveCheat;

	wxTextCtrl* m_InternalName;
	wxTextCtrl* m_GameID;
	wxTextCtrl* m_Country;
	wxTextCtrl* m_MakerID;
	wxTextCtrl* m_Revision;
	wxTextCtrl* m_Date;
	wxTextCtrl* m_FST;
	wxTextCtrl* m_MD5Sum;
	wxButton*   m_MD5SumCompute;
	wxArrayString arrayStringFor_Lang;
	wxChoice*   m_Lang;
	wxTextCtrl* m_Name;
	wxTextCtrl* m_Maker;
	wxTextCtrl* m_Comment;
	wxStaticBitmap* m_Banner;

	wxTreeCtrl* m_Treectrl;
	wxTreeItemId RootId;

	Gecko::CodeConfigPanel* m_geckocode_panel;

	enum
	{
		ID_TREECTRL = 1000,

		ID_NOTEBOOK,
		ID_GAMECONFIG,
		ID_PATCH_PAGE,
		ID_ARCODE_PAGE,
		ID_SPEEDHACK_PAGE,
		ID_INFORMATION,
		ID_FILESYSTEM,

		ID_USEDUALCORE,
		ID_IDLESKIP,
		ID_MMU,
		ID_DCBZOFF,
		ID_FPRF,
		ID_SYNCGPU,
		ID_DISCSPEED,
		ID_AUDIO_DSP_HLE,
		ID_USE_BBOX,
		ID_ENABLEPROGRESSIVESCAN,
		ID_ENABLEWIDESCREEN,
		ID_EDITCONFIG,
		ID_SHOWDEFAULTCONFIG,
		ID_EMUSTATE,
		ID_EMU_ISSUES,
		ID_PATCHES_LIST,
		ID_EDITPATCH,
		ID_ADDPATCH,
		ID_REMOVEPATCH,
		ID_CHEATS_LIST,
		ID_EDITCHEAT,
		ID_ADDCHEAT,
		ID_REMOVECHEAT,
		ID_GPUDETERMINISM,
		ID_DEPTHPERCENTAGE,
		ID_CONVERGENCE,
		ID_MONODEPTH,

		ID_NAME,
		ID_GAMEID,
		ID_COUNTRY,
		ID_MAKERID,
		ID_REVISION,
		ID_DATE,
		ID_FST,
		ID_MD5SUM,
		ID_MD5SUMCOMPUTE,
		ID_VERSION,
		ID_LANG,
		ID_SHORTNAME,
		ID_LONGNAME,
		ID_MAKER,
		ID_COMMENT,
		ID_BANNER,
		IDM_EXTRACTDIR,
		IDM_EXTRACTALL,
		IDM_EXTRACTFILE,
		IDM_EXTRACTAPPLOADER,
		IDM_EXTRACTDOL,
		IDM_CHECKINTEGRITY,
		IDM_BNRSAVEAS
	};

	void LaunchExternalEditor(const std::string& filename, bool wait_until_closed);

	void CreateGUIControls();
	void OnClose(wxCloseEvent& event);
	void OnCloseClick(wxCommandEvent& event);
	void OnEditConfig(wxCommandEvent& event);
	void OnComputeMD5Sum(wxCommandEvent& event);
	void OnShowDefaultConfig(wxCommandEvent& event);
	void ListSelectionChanged(wxCommandEvent& event);
	void OnActionReplayCodeChecked(wxCommandEvent& event);
	void PatchButtonClicked(wxCommandEvent& event);
	void ActionReplayButtonClicked(wxCommandEvent& event);
	void RightClickOnBanner(wxMouseEvent& event);
	void OnBannerImageSave(wxCommandEvent& event);
	void OnRightClickOnTree(wxTreeEvent& event);
	void OnExtractFile(wxCommandEvent& event);
	void OnExtractDir(wxCommandEvent& event);
	void OnExtractDataFromHeader(wxCommandEvent& event);
	void CheckPartitionIntegrity(wxCommandEvent& event);
	void OnEmustateChanged(wxCommandEvent& event);
	void OnChangeBannerLang(wxCommandEvent& event);

	const GameListItem OpenGameListItem;

	typedef std::vector<const DiscIO::SFileInfo*>::iterator fileIter;

	size_t CreateDirectoryTree(wxTreeItemId& parent,
			const std::vector<DiscIO::SFileInfo>& fileInfos);
	size_t CreateDirectoryTree(wxTreeItemId& parent,
			const std::vector<DiscIO::SFileInfo>& fileInfos,
			const size_t _FirstIndex,
			const size_t _LastIndex);
	void ExportDir(const std::string& _rFullPath, const std::string& _rExportFilename,
			const WiiPartition* partition = nullptr);

	IniFile GameIniDefault;
	IniFile GameIniLocal;
	std::string GameIniFileLocal;
	std::string game_id;

	std::set<std::string> DefaultPatches;
	std::set<std::string> DefaultCheats;

	void LoadGameConfig();
	bool SaveGameConfig();
	void OnLocalIniModified(wxCommandEvent& ev);
	void GenerateLocalIniModified();
	void PatchList_Load();
	void PatchList_Save();
	void ActionReplayList_Load();
	void ActionReplayList_Save();
	void ChangeBannerDetails(DiscIO::IVolume::ELanguage language);

	long GetElementStyle(const char* section, const char* key);
	void SetCheckboxValueFromGameini(const char* section, const char* key, wxCheckBox* checkbox);
	void SaveGameIniValueFrom3StateCheckbox(const char* section, const char* key, wxCheckBox* checkbox);
};
