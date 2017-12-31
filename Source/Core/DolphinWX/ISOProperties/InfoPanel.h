// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <wx/panel.h>

class wxButton;
class wxChoice;
class wxStaticBitmap;
class wxStaticBoxSizer;
class wxTextCtrl;

namespace DiscIO
{
enum class Language;
class Volume;
}

namespace UICommon
{
class GameFile;
}

class InfoPanel final : public wxPanel
{
public:
  InfoPanel(wxWindow* parent, wxWindowID id, const UICommon::GameFile& item,
            const std::unique_ptr<DiscIO::Volume>& opened_iso);

private:
  enum
  {
    IDM_SAVE_BANNER
  };

  void CreateGUI();
  void BindEvents();
  void LoadGUIData();
  void LoadISODetails();
  void LoadBannerDetails();
  wxImage ConvertBannerImage();
  void LoadBannerImage();

  wxStaticBoxSizer* CreateISODetailsSizer();
  wxStaticBoxSizer* CreateBannerDetailsSizer();
  wxChoice* CreateCommentLanguageChoice();

  void OnComputeMD5(wxCommandEvent&);
  void OnChangeBannerLanguage(wxCommandEvent&);
  void OnRightClickBanner(wxMouseEvent&);
  void OnSaveBannerImage(wxCommandEvent&);

  void ChangeBannerDetails(DiscIO::Language language);

  void EmitTitleChangeEvent(const wxString& new_title);

  const UICommon::GameFile& m_game_list_item;
  const std::unique_ptr<DiscIO::Volume>& m_opened_iso;

  wxTextCtrl* m_internal_name;
  wxTextCtrl* m_game_id;
  wxTextCtrl* m_country;
  wxTextCtrl* m_maker_id;
  wxTextCtrl* m_revision;
  wxTextCtrl* m_date;
  wxTextCtrl* m_ios_version = nullptr;
  wxTextCtrl* m_md5_sum;
  wxButton* m_md5_sum_compute;
  wxChoice* m_languages;
  wxTextCtrl* m_name;
  wxTextCtrl* m_maker;
  wxTextCtrl* m_comment;
  wxStaticBitmap* m_banner;
};
