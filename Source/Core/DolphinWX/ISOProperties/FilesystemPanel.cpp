// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinWX/ISOProperties/FilesystemPanel.h"

#include <array>
#include <chrono>
#include <future>
#include <memory>
#include <vector>

#include <wx/bitmap.h>
#include <wx/button.h>
#include <wx/filepicker.h>
#include <wx/imaglist.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/progdlg.h>
#include <wx/sizer.h>
#include <wx/textctrl.h>
#include <wx/treectrl.h>

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "DiscIO/DiscExtractor.h"
#include "DiscIO/Enums.h"
#include "DiscIO/Filesystem.h"
#include "DiscIO/Volume.h"
#include "DolphinWX/ISOFile.h"
#include "DolphinWX/WxUtils.h"

namespace
{
class WiiPartition final : public wxTreeItemData
{
public:
  WiiPartition(const DiscIO::Partition& partition_) : partition(partition_) {}
  DiscIO::Partition partition;
};

enum : int
{
  ICON_DISC,
  ICON_FOLDER,
  ICON_FILE
};

wxImageList* LoadIconBitmaps(const wxWindow* context)
{
#if defined(_MSC_VER) && _MSC_VER <= 1800
#define constexpr const
#endif
  static constexpr std::array<const char*, 3> icon_names{
      {"isoproperties_disc", "isoproperties_folder", "isoproperties_file"}};

  const wxSize icon_size = context->FromDIP(wxSize(16, 16));
  auto* const icon_list = new wxImageList(icon_size.GetWidth(), icon_size.GetHeight());

  for (const auto& name : icon_names)
  {
    icon_list->Add(
        WxUtils::LoadScaledResourceBitmap(name, context, icon_size, wxDefaultSize,
                                          WxUtils::LSI_SCALE_DOWN | WxUtils::LSI_ALIGN_CENTER));
  }

  return icon_list;
}

void CreateDirectoryTree(wxTreeCtrl* tree_ctrl, wxTreeItemId parent,
                         const DiscIO::FileInfo& directory)
{
  for (const DiscIO::FileInfo& file_info : directory)
  {
    const wxString name = StrToWxStr(file_info.GetName());
    if (file_info.IsDirectory())
    {
      wxTreeItemId item = tree_ctrl->AppendItem(parent, name, ICON_FOLDER);
      CreateDirectoryTree(tree_ctrl, item, file_info);
    }
    else
    {
      tree_ctrl->AppendItem(parent, name, ICON_FILE);
    }
  }
}

void CreateDirectoryTree(wxTreeCtrl* tree_ctrl, wxTreeItemId parent,
                         const DiscIO::FileSystem* file_system)
{
  if (file_system)
    CreateDirectoryTree(tree_ctrl, parent, file_system->GetRoot());
}

WiiPartition* FindWiiPartition(wxTreeCtrl* tree_ctrl, const wxString& label)
{
  wxTreeItemIdValue cookie;
  auto partition = tree_ctrl->GetFirstChild(tree_ctrl->GetRootItem(), cookie);

  while (partition.IsOk())
  {
    const wxString partition_label = tree_ctrl->GetItemText(partition);

    if (partition_label == label)
      return static_cast<WiiPartition*>(tree_ctrl->GetItemData(partition));

    partition = tree_ctrl->GetNextSibling(partition);
  }

  return nullptr;
}
}  // Anonymous namespace

FilesystemPanel::FilesystemPanel(wxWindow* parent, wxWindowID id,
                                 const std::unique_ptr<DiscIO::Volume>& opened_iso)
    : wxPanel{parent, id}, m_opened_iso{opened_iso}
{
  CreateGUI();
  if (PopulateFileSystemTree())
  {
    BindEvents();
    m_tree_ctrl->Expand(m_tree_ctrl->GetRootItem());
  }
}

FilesystemPanel::~FilesystemPanel() = default;

void FilesystemPanel::BindEvents()
{
  m_tree_ctrl->Bind(wxEVT_TREE_ITEM_RIGHT_CLICK, &FilesystemPanel::OnRightClickTree, this);

  Bind(wxEVT_MENU, &FilesystemPanel::OnExtractFile, this, ID_EXTRACT_FILE);
  Bind(wxEVT_MENU, &FilesystemPanel::OnExtractAll, this, ID_EXTRACT_ALL);
  Bind(wxEVT_MENU, &FilesystemPanel::OnExtractDirectories, this, ID_EXTRACT_DIR);
  Bind(wxEVT_MENU, &FilesystemPanel::OnExtractSystemData, this, ID_EXTRACT_SYSTEM_DATA);
  Bind(wxEVT_MENU, &FilesystemPanel::OnCheckPartitionIntegrity, this, ID_CHECK_INTEGRITY);
}

void FilesystemPanel::CreateGUI()
{
  m_tree_ctrl = new wxTreeCtrl(this);
  m_tree_ctrl->AssignImageList(LoadIconBitmaps(this));
  m_tree_ctrl->AddRoot(_("Disc"), ICON_DISC);

  const auto space_5 = FromDIP(5);
  auto* const main_sizer = new wxBoxSizer(wxVERTICAL);
  main_sizer->AddSpacer(space_5);
  main_sizer->Add(m_tree_ctrl, 1, wxEXPAND | wxLEFT | wxRIGHT, space_5);
  main_sizer->AddSpacer(space_5);

  SetSizer(main_sizer);
}

bool FilesystemPanel::PopulateFileSystemTree()
{
  const std::vector<DiscIO::Partition> partitions = m_opened_iso->GetPartitions();
  m_has_partitions = !partitions.empty();

  if (m_has_partitions)
  {
    for (size_t i = 0; i < partitions.size(); ++i)
    {
      wxTreeItemId partition_root = m_tree_ctrl->AppendItem(
          m_tree_ctrl->GetRootItem(), wxString::Format(_("Partition %zu"), i), ICON_DISC);

      WiiPartition* const partition = new WiiPartition(partitions[i]);

      m_tree_ctrl->SetItemData(partition_root, partition);
      CreateDirectoryTree(m_tree_ctrl, partition_root, m_opened_iso->GetFileSystem(partitions[i]));

      if (partitions[i] == m_opened_iso->GetGamePartition())
        m_tree_ctrl->Expand(partition_root);
    }
  }
  else
  {
    CreateDirectoryTree(m_tree_ctrl, m_tree_ctrl->GetRootItem(),
                        m_opened_iso->GetFileSystem(DiscIO::PARTITION_NONE));
  }

  return true;
}

void FilesystemPanel::OnRightClickTree(wxTreeEvent& event)
{
  m_tree_ctrl->SelectItem(event.GetItem());

  wxMenu menu;

  const wxTreeItemId selection = m_tree_ctrl->GetSelection();
  const wxTreeItemId first_visible_item = m_tree_ctrl->GetFirstVisibleItem();
  const int image_type = m_tree_ctrl->GetItemImage(selection);
  const bool is_parent_of_partitions = m_has_partitions && first_visible_item == selection;

  if (image_type == ICON_FILE)
    menu.Append(ID_EXTRACT_FILE, _("Extract File..."));
  else if (!is_parent_of_partitions)
    menu.Append(ID_EXTRACT_DIR, _("Extract Files..."));

  if (image_type == ICON_DISC)
  {
    if (!is_parent_of_partitions)
      menu.Append(ID_EXTRACT_SYSTEM_DATA, _("Extract System Data..."));

    if (first_visible_item == selection)
      menu.Append(ID_EXTRACT_ALL, _("Extract Entire Disc..."));
    else
      menu.Append(ID_EXTRACT_ALL, _("Extract Entire Partition..."));

    if (first_visible_item != selection)
    {
      menu.AppendSeparator();
      menu.Append(ID_CHECK_INTEGRITY, _("Check Partition Integrity"));
    }
  }

  PopupMenu(&menu);
  event.Skip();
}

void FilesystemPanel::OnExtractFile(wxCommandEvent& WXUNUSED(event))
{
  const wxString selection_label = m_tree_ctrl->GetItemText(m_tree_ctrl->GetSelection());

  const wxString output_file_path =
      wxFileSelector(_("Extract File"), wxEmptyString, selection_label, wxEmptyString,
                     wxGetTranslation(wxALL_FILES), wxFD_SAVE, this);

  if (output_file_path.empty() || selection_label.empty())
    return;

  ExtractSingleFile(output_file_path);
}

void FilesystemPanel::OnExtractDirectories(wxCommandEvent& event)
{
  const wxString selected_directory_label = m_tree_ctrl->GetItemText(m_tree_ctrl->GetSelection());
  const wxString extract_path = wxDirSelector(_("Choose the folder to extract to"));

  if (!extract_path.empty() && !selected_directory_label.empty())
    ExtractSingleDirectory(extract_path);
}

void FilesystemPanel::OnExtractSystemData(wxCommandEvent& event)
{
  const wxString path = wxDirSelector(_("Choose the folder to extract to"));

  if (path.empty())
    return;

  DiscIO::Partition partition;
  if (m_has_partitions)
  {
    const auto* const selection_data = m_tree_ctrl->GetItemData(m_tree_ctrl->GetSelection());
    const auto* const wii_partition = static_cast<const WiiPartition*>(selection_data);

    partition = wii_partition->partition;
  }
  else
  {
    partition = DiscIO::PARTITION_NONE;
  }

  if (!DiscIO::ExportSystemData(*m_opened_iso, partition, WxStrToStr(path)))
  {
    WxUtils::ShowErrorDialog(
        wxString::Format(_("Failed to extract to %s!"), WxStrToStr(path).c_str()));
  }
}

void FilesystemPanel::OnExtractAll(wxCommandEvent& event)
{
  const wxString extract_path = wxDirSelector(_("Choose the folder to extract to"));

  if (extract_path.empty())
    return;

  const std::string std_extract_path = WxStrToStr(extract_path);

  const wxTreeItemId selection = m_tree_ctrl->GetSelection();
  const bool first_item_selected = m_tree_ctrl->GetFirstVisibleItem() == selection;

  if (m_has_partitions && first_item_selected)
  {
    const wxTreeItemId root = m_tree_ctrl->GetRootItem();

    wxTreeItemIdValue cookie;
    wxTreeItemId item = m_tree_ctrl->GetFirstChild(root, cookie);

    while (item.IsOk())
    {
      const auto* const partition = static_cast<WiiPartition*>(m_tree_ctrl->GetItemData(item));
      const std::optional<u32> partition_type =
          *m_opened_iso->GetPartitionType(partition->partition);
      if (partition_type)
      {
        const std::string partition_name = DiscIO::DirectoryNameForPartitionType(*partition_type);
        ExtractPartition(std_extract_path + '/' + partition_name, partition->partition);
      }
      item = m_tree_ctrl->GetNextChild(root, cookie);
    }
  }
  else if (m_has_partitions && !first_item_selected)
  {
    const auto* const partition = static_cast<WiiPartition*>(m_tree_ctrl->GetItemData(selection));
    ExtractPartition(std_extract_path, partition->partition);
  }
  else
  {
    ExtractPartition(std_extract_path, DiscIO::PARTITION_NONE);
  }
}

void FilesystemPanel::OnCheckPartitionIntegrity(wxCommandEvent& WXUNUSED(event))
{
  // Normally we can't enter this function if we're analyzing a volume that
  // doesn't have partitions anyway, but let's still check to be sure.
  if (!m_has_partitions)
    return;

  wxProgressDialog dialog(_("Checking integrity..."), _("Working..."), 1000, this,
                          wxPD_APP_MODAL | wxPD_ELAPSED_TIME | wxPD_SMOOTH);

  const auto selection = m_tree_ctrl->GetSelection();
  WiiPartition* partition =
      static_cast<WiiPartition*>(m_tree_ctrl->GetItemData(m_tree_ctrl->GetSelection()));
  std::future<bool> is_valid = std::async(
      std::launch::async, [&] { return m_opened_iso->CheckIntegrity(partition->partition); });

  while (is_valid.wait_for(std::chrono::milliseconds(50)) != std::future_status::ready)
    dialog.Pulse();
  dialog.Hide();

  if (is_valid.get())
  {
    wxMessageBox(_("Integrity check completed. No errors have been found."),
                 _("Integrity check completed"), wxOK | wxICON_INFORMATION, this);
  }
  else
  {
    wxMessageBox(wxString::Format(_("Integrity check for %s failed. The disc image is most "
                                    "likely corrupted or has been patched incorrectly."),
                                  m_tree_ctrl->GetItemText(selection)),
                 _("Integrity Check Error"), wxOK | wxICON_ERROR, this);
  }
}

void FilesystemPanel::ExtractSingleFile(const wxString& output_file_path) const
{
  const std::pair<wxString, DiscIO::Partition> path = BuildFilePathFromSelection();
  DiscIO::ExportFile(*m_opened_iso, path.second, WxStrToStr(path.first),
                     WxStrToStr(output_file_path));
}

void FilesystemPanel::ExtractSingleDirectory(const wxString& output_folder)
{
  const std::pair<wxString, DiscIO::Partition> path = BuildDirectoryPathFromSelection();
  ExtractDirectories(WxStrToStr(path.first), WxStrToStr(output_folder), path.second);
}

void FilesystemPanel::ExtractDirectories(const std::string& full_path,
                                         const std::string& output_folder,
                                         const DiscIO::Partition& partition)
{
  const DiscIO::FileSystem* file_system = m_opened_iso->GetFileSystem(partition);
  if (!file_system)
    return;

  std::unique_ptr<DiscIO::FileInfo> file_info = file_system->FindFileInfo(full_path);
  u32 size = file_info->GetTotalChildren();
  u32 progress = 0;

  wxString dialog_title = full_path.empty() ? _("Extracting All Files") : _("Extracting Directory");
  wxProgressDialog dialog(dialog_title, _("Extracting..."), size, this,
                          wxPD_APP_MODAL | wxPD_AUTO_HIDE | wxPD_CAN_ABORT | wxPD_ELAPSED_TIME |
                              wxPD_ESTIMATED_TIME | wxPD_REMAINING_TIME | wxPD_SMOOTH);

  DiscIO::ExportDirectory(
      *m_opened_iso, partition, *file_info, true, full_path, output_folder,
      [&](const std::string& path) {
        dialog.SetTitle(wxString::Format(
            "%s : %d%%", dialog_title.c_str(),
            static_cast<u32>((static_cast<float>(progress) / static_cast<float>(size)) * 100)));
        dialog.Update(progress, wxString::Format(_("Extracting %s"), StrToWxStr(path)));
        ++progress;
        return dialog.WasCancelled();
      });
}

void FilesystemPanel::ExtractPartition(const std::string& output_folder,
                                       const DiscIO::Partition& partition)
{
  ExtractDirectories("", output_folder + "/files", partition);
  DiscIO::ExportSystemData(*m_opened_iso, partition, output_folder);
}

std::pair<wxString, DiscIO::Partition> FilesystemPanel::BuildFilePathFromSelection() const
{
  const wxTreeItemId root_node = m_tree_ctrl->GetRootItem();
  wxTreeItemId node = m_tree_ctrl->GetSelection();

  wxString file_path;

  if (node != root_node)
  {
    file_path = m_tree_ctrl->GetItemText(node);
    node = m_tree_ctrl->GetItemParent(node);

    while (node != root_node)
    {
      file_path = m_tree_ctrl->GetItemText(node) + DIR_SEP_CHR + file_path;
      node = m_tree_ctrl->GetItemParent(node);
    }
  }

  if (m_has_partitions)
  {
    const size_t slash_index = file_path.find('/');
    const wxString partition_label = file_path.substr(0, slash_index);
    const WiiPartition* const partition = FindWiiPartition(m_tree_ctrl, partition_label);

    // Remove "Partition x/"
    file_path.erase(0, slash_index + 1);

    return {file_path, partition->partition};
  }
  else
  {
    return {file_path, DiscIO::PARTITION_NONE};
  }
}

std::pair<wxString, DiscIO::Partition> FilesystemPanel::BuildDirectoryPathFromSelection() const
{
  const std::pair<wxString, DiscIO::Partition> result = BuildFilePathFromSelection();
  return {result.first + DIR_SEP_CHR, result.second};
}
