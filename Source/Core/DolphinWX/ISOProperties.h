// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <set>
#include <string>
#include <vector>
#include <wx/arrstr.h>
#include <wx/dialog.h>
#include <wx/event.h>
#include <wx/gdicmn.h>
#include <wx/string.h>
#include <wx/toplevel.h>
#include <wx/translation.h>
#include <wx/treebase.h>
#include <wx/windowid.h>

#include "Common/IniFile.h"
#include "Core/ActionReplay.h"
#include "DiscIO/Filesystem.h"
#include "DiscIO/Volume.h"
#include "DiscIO/VolumeCreator.h"
#include "DolphinWX/ARCodeAddEdit.h"
#include "DolphinWX/PatchAddEdit.h"

class GameListItem;
class wxButton;
class wxCheckBox;
class wxCheckListBox;
class wxChoice;
class wxSlider;
class wxSpinCtrl;
class wxSpinCtrlDouble;
class wxStaticBitmap;
class wxTextCtrl;
class wxTreeCtrl;
class wxWindow;
namespace DiscIO { struct SFileInfo; }
namespace Gecko { class CodeConfigPanel; }

class WiiPartition final : public wxTreeItemData
{
public:
	DiscIO::IVolume *Partition;
	DiscIO::IFileSystem *FileSystem;
	std::vector<const DiscIO::SFileInfo *> Files;
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
	CISOProperties(const std::string fileName,
			wxWindow* parent,
			wxWindowID id = wxID_ANY,
			const wxString& title = _("Properties"),
			const wxPoint& pos = wxDefaultPosition,
			const wxSize& size = wxDefaultSize,
			long style = wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
	virtual ~CISOProperties();

	bool bRefreshList;

	void ActionReplayList_Load();
	bool SaveGameConfig();

private:
	DECLARE_EVENT_TABLE();

	DiscIO::IVolume *OpenISO;
	DiscIO::IFileSystem *pFileSystem;

	std::vector<PatchEngine::Patch> onFrame;
	std::vector<ActionReplay::ARCode> arCodes;
	PHackData m_PHack_Data;

	// Core
	wxCheckBox *CPUThread, *SkipIdle, *MMU, *DCBZOFF, *FPRF;
	wxCheckBox *SyncGPU, *FastDiscSpeed, *DSPHLE;

	wxArrayString arrayStringFor_GPUDeterminism;
	wxChoice* GPUDeterminism;
	wxSpinCtrlDouble* AudioSlowDown;

	// Wii
	wxCheckBox* EnableWideScreen;

	// Stereoscopy
	wxSlider* DepthPercentage;
	wxSpinCtrl* ConvergenceMinimum;
	wxCheckBox* MonoDepth;

	wxArrayString arrayStringFor_EmuState;
	wxArrayString arrayStringFor_VRState;
	wxChoice* EmuState;
	wxTextCtrl* EmuIssues;
	wxCheckListBox* HideObjects;
	wxButton* EditHideObject;
	wxButton* RemoveHideObject;

	wxCheckListBox* Patches;
	wxButton* EditPatch;
	wxButton* RemovePatch;

	wxCheckListBox* Cheats;
	wxButton* EditCheat;
	wxButton* RemoveCheat;


	// VR
	wxCheckBox *Disable3D;
	wxCheckBox *HudFullscreen;
	wxCheckBox *HudOnTop;
	wxSpinCtrlDouble* UnitsPerMetre;
	wxSpinCtrlDouble* HudDistance;
	wxSpinCtrlDouble* HudThickness;
	wxSpinCtrlDouble* Hud3DCloser;
	wxSpinCtrlDouble* CameraForward;
	wxSpinCtrlDouble* CameraPitch;
	wxSpinCtrlDouble* AimDistance;
	wxSpinCtrlDouble* ScreenHeight;
	wxSpinCtrlDouble* ScreenDistance;
	wxSpinCtrlDouble* ScreenThickness;
	wxSpinCtrlDouble* ScreenUp;
	wxSpinCtrlDouble* ScreenRight;
	wxSpinCtrlDouble* ScreenPitch;
	wxSpinCtrlDouble* MinFOV;
	wxChoice *VRState;
	wxTextCtrl *VRIssues;


	wxTextCtrl* m_Name;
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
	wxTextCtrl* m_ShortName;
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
		ID_VR,
		ID_DISABLE_3D,
		ID_HUD_FULLSCREEN,
		ID_HUD_ON_TOP,
		ID_UNITS_PER_METRE,
		ID_HUD_DISTANCE,
		ID_HUD_THICKNESS,
		ID_HUD_3D_CLOSER,
		ID_CAMERA_FORWARD,
		ID_CAMERA_PITCH,
		ID_AIM_DISTANCE,
		ID_MIN_FOV,
		ID_SCREEN_HEIGHT,
		ID_SCREEN_DISTANCE,
		ID_SCREEN_THICKNESS,
		ID_SCREEN_RIGHT,
		ID_SCREEN_UP,
		ID_SCREEN_PITCH,
		ID_HIDEOBJECT_PAGE,
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
		ID_HIDEOBJECTS_LIST,
		ID_HIDEOBJECTS_CHECKBOX,
		ID_EDITHIDEOBJECT,
		ID_ADDHideObject,
		ID_REMOVEHIDEOBJECT,
		ID_PATCHES_LIST,
		ID_EDITPATCH,
		ID_ADDPATCH,
		ID_REMOVEPATCH,
		ID_CHEATS_LIST,
		ID_EDITCHEAT,
		ID_ADDCHEAT,
		ID_REMOVECHEAT,
		ID_GPUDETERMINISM,
		ID_AUDIOSLOWDOWN,
		ID_DEPTHPERCENTAGE,
		ID_CONVERGENCEMINIMUM,
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

	void LaunchExternalEditor(const std::string& filename);

	void CreateGUIControls(bool);
	void OnClose(wxCloseEvent& event);
	void OnCloseClick(wxCommandEvent& event);
	void OnEditConfig(wxCommandEvent& event);
	void OnComputeMD5Sum(wxCommandEvent& event);
	void OnShowDefaultConfig(wxCommandEvent& event);
	void HideObjectButtonClicked(wxCommandEvent& event);
	void ListSelectionChanged(wxCommandEvent& event);
	void CheckboxSelectionChanged(wxCommandEvent& event);
	void PatchButtonClicked(wxCommandEvent& event);
	void ActionReplayButtonClicked(wxCommandEvent& event);
	void RightClickOnBanner(wxMouseEvent& event);
	void OnBannerImageSave(wxCommandEvent& event);
	void OnRightClickOnTree(wxTreeEvent& event);
	void OnExtractFile(wxCommandEvent& event);
	void OnExtractDir(wxCommandEvent& event);
	void OnExtractDataFromHeader(wxCommandEvent& event);
	void CheckPartitionIntegrity(wxCommandEvent& event);
	void SetRefresh(wxCommandEvent& event);
	void OnChangeBannerLang(wxCommandEvent& event);

	GameListItem* OpenGameListItem;

	std::vector<const DiscIO::SFileInfo*> GCFiles;
	typedef std::vector<const DiscIO::SFileInfo*>::iterator fileIter;

	size_t CreateDirectoryTree(wxTreeItemId& parent,
			std::vector<const DiscIO::SFileInfo*> fileInfos,
			const size_t _FirstIndex,
			const size_t _LastIndex);
	void ExportDir(const std::string& _rFullPath, const std::string& _rExportFilename,
			const WiiPartition* partition = nullptr);

	IniFile GameIniDefault;
	IniFile GameIniLocal;
	std::string GameIniFileLocal;
	std::string game_id;

	std::set<std::string> DefaultHideObjects;
	std::set<std::string> DefaultPatches;
	std::set<std::string> DefaultCheats;

	void LoadGameConfig();
	void HideObjectList_Load();
	void HideObjectList_Save();
	void PatchList_Load();
	void PatchList_Save();
	void ActionReplayList_Save();
	void ChangeBannerDetails(int lang);

	long GetElementStyle(const char* section, const char* key);
	void SetCheckboxValueFromGameini(const char* section, const char* key, wxCheckBox* checkbox);
	void SaveGameIniValueFrom3StateCheckbox(const char* section, const char* key, wxCheckBox* checkbox);
};
