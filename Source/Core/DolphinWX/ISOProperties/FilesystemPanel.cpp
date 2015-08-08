// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinWX/ISOProperties/FilesystemPanel.h"

#include <array>
#include <functional>
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
#include "DiscIO/Enums.h"
#include "DiscIO/Filesystem.h"
// TODO: eww
#include "DiscIO/FileSystemGCWii.h"
#include "DiscIO/Volume.h"
#include "DiscIO/VolumeCreator.h"
#include "DolphinWX/ISOFile.h"
#include "DolphinWX/WxUtils.h"

namespace
{
class WiiPartition final : public wxTreeItemData
{
public:
  WiiPartition(std::unique_ptr<DiscIO::IVolume> volume_,
               std::unique_ptr<DiscIO::IFileSystem> filesystem_)
      : volume{std::move(volume_)}, filesystem{std::move(filesystem_)}
  {
  }

  std::unique_ptr<DiscIO::IVolume> volume;
  std::unique_ptr<DiscIO::IFileSystem> filesystem;
};

class IntegrityCheckThread final : public wxThread
{
public:
  explicit IntegrityCheckThread(const WiiPartition& partition)
      : wxThread{wxTHREAD_JOINABLE}, m_partition{partition}
  {
    Create();
  }

  ExitCode Entry() override
  {
    return reinterpret_cast<ExitCode>(m_partition.volume->CheckIntegrity());
  }

private:
  const WiiPartition& m_partition;
};

enum : int
{
  ICON_DISC,
  ICON_FOLDER,
  ICON_FILE
};

wxImageList* LoadIconBitmaps(const wxWindow* context)
{
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
                         const DiscIO::IFileInfo& directory)
{
  for (const DiscIO::IFileInfo& file_info : directory)
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

FilesystemPanel::FilesystemPanel(wxWindow* parent, wxWindowID id, const GameListItem& item,
                                 const std::unique_ptr<DiscIO::IVolume>& opened_iso)
    : wxPanel{parent, id}, m_game_list_item{item}, m_opened_iso{opened_iso}
{
  CreateGUI();
  BindEvents();
  PopulateFileSystemTree();

  m_tree_ctrl->Expand(m_tree_ctrl->GetRootItem());
}

FilesystemPanel::~FilesystemPanel() = default;

void FilesystemPanel::BindEvents()
{
  m_tree_ctrl->Bind(wxEVT_TREE_ITEM_RIGHT_CLICK, &FilesystemPanel::OnRightClickTree, this);

  Bind(wxEVT_MENU, &FilesystemPanel::OnExtractFile, this, ID_EXTRACT_FILE);
  Bind(wxEVT_MENU, &FilesystemPanel::OnExtractDirectories, this, ID_EXTRACT_ALL);
  Bind(wxEVT_MENU, &FilesystemPanel::OnExtractDirectories, this, ID_EXTRACT_DIR);
  Bind(wxEVT_MENU, &FilesystemPanel::OnExtractHeaderData, this, ID_EXTRACT_APPLOADER);
  Bind(wxEVT_MENU, &FilesystemPanel::OnExtractHeaderData, this, ID_EXTRACT_DOL);
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

void FilesystemPanel::PopulateFileSystemTree()
{
  switch (m_opened_iso->GetVolumeType())
  {
  case DiscIO::Platform::GAMECUBE_DISC:
    PopulateFileSystemTreeGC();
    break;

  case DiscIO::Platform::WII_DISC:
    PopulateFileSystemTreeWii();
    break;

  case DiscIO::Platform::ELF_DOL:
  case DiscIO::Platform::NUMBER_OF_PLATFORMS:
  case DiscIO::Platform::WII_WAD:
    break;
  }
}

void FilesystemPanel::PopulateFileSystemTreeGC()
{
  m_filesystem = DiscIO::CreateFileSystem(m_opened_iso.get());
  if (!m_filesystem)
    return;

  CreateDirectoryTree(m_tree_ctrl, m_tree_ctrl->GetRootItem(), m_filesystem->GetRoot());
}

void FilesystemPanel::PopulateFileSystemTreeWii() const
{
  u32 partition_count = 0;

  for (u32 group = 0; group < 4; group++)
  {
    // yes, technically there can be OVER NINE THOUSAND partitions...
    for (u32 i = 0; i < 0xFFFFFFFF; i++)
    {
      auto volume = DiscIO::CreateVolumeFromFilename(m_game_list_item.GetFileName(), group, i);
      if (volume == nullptr)
        break;

      auto file_system = DiscIO::CreateFileSystem(volume.get());
      if (file_system != nullptr)
      {
        auto* const partition = new WiiPartition(std::move(volume), std::move(file_system));

        const wxTreeItemId partition_root = m_tree_ctrl->AppendItem(
            m_tree_ctrl->GetRootItem(), wxString::Format(_("Partition %u"), partition_count),
            ICON_DISC);

        m_tree_ctrl->SetItemData(partition_root, partition);
        CreateDirectoryTree(m_tree_ctrl, partition_root, partition->filesystem->GetRoot());

        if (partition_count == 1)
          m_tree_ctrl->Expand(partition_root);

        partition_count++;
      }
    }
  }
}

void FilesystemPanel::OnRightClickTree(wxTreeEvent& event)
{
  m_tree_ctrl->SelectItem(event.GetItem());

  wxMenu menu;

  const auto selection = m_tree_ctrl->GetSelection();
  const auto first_visible_item = m_tree_ctrl->GetFirstVisibleItem();
  const int image_type = m_tree_ctrl->GetItemImage(selection);

  if (image_type == ICON_DISC && first_visible_item != selection)
  {
    menu.Append(ID_EXTRACT_DIR, _("Extract Partition..."));
  }
  else if (image_type == ICON_FOLDER)
  {
    menu.Append(ID_EXTRACT_DIR, _("Extract Directory..."));
  }
  else if (image_type == ICON_FILE)
  {
    menu.Append(ID_EXTRACT_FILE, _("Extract File..."));
  }

  menu.Append(ID_EXTRACT_ALL, _("Extract All Files..."));

  if (m_opened_iso->GetVolumeType() != DiscIO::Platform::WII_DISC ||
      (image_type == ICON_DISC && first_visible_item != selection))
  {
    menu.AppendSeparator();
    menu.Append(ID_EXTRACT_APPLOADER, _("Extract Apploader..."));
    menu.Append(ID_EXTRACT_DOL, _("Extract DOL..."));
  }

  if (image_type == ICON_DISC && first_visible_item != selection)
  {
    menu.AppendSeparator();
    menu.Append(ID_CHECK_INTEGRITY, _("Check Partition Integrity"));
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

  if (extract_path.empty() || selected_directory_label.empty())
    return;

  switch (event.GetId())
  {
  case ID_EXTRACT_ALL:
    ExtractAllFiles(extract_path);
    break;
  case ID_EXTRACT_DIR:
    ExtractSingleDirectory(extract_path);
    break;
  }
}

void FilesystemPanel::OnExtractHeaderData(wxCommandEvent& event)
{
  DiscIO::IFileSystem* file_system = nullptr;
  const wxString path = wxDirSelector(_("Choose the folder to extract to"));

  if (path.empty())
    return;

  if (m_opened_iso->GetVolumeType() == DiscIO::Platform::WII_DISC)
  {
    const auto* const selection_data = m_tree_ctrl->GetItemData(m_tree_ctrl->GetSelection());
    const auto* const partition = static_cast<const WiiPartition*>(selection_data);

    file_system = partition->filesystem.get();
  }
  else
  {
    file_system = m_filesystem.get();
  }

  bool ret = false;
  if (event.GetId() == ID_EXTRACT_APPLOADER)
  {
    ret = file_system->ExportApploader(WxStrToStr(path));
  }
  else if (event.GetId() == ID_EXTRACT_DOL)
  {
    ret = file_system->ExportDOL(WxStrToStr(path));
  }

  if (!ret)
  {
    WxUtils::ShowErrorDialog(
        wxString::Format(_("Failed to extract to %s!"), WxStrToStr(path).c_str()));
  }
}

void FilesystemPanel::OnCheckPartitionIntegrity(wxCommandEvent& WXUNUSED(event))
{
  // Normally we can't enter this function if we aren't analyzing a Wii disc
  // anyway, but let's still check to be sure.
  if (m_opened_iso->GetVolumeType() != DiscIO::Platform::WII_DISC)
    return;

  wxProgressDialog dialog(_("Checking integrity..."), _("Working..."), 1000, this,
                          wxPD_APP_MODAL | wxPD_ELAPSED_TIME | wxPD_SMOOTH);

  const auto selection = m_tree_ctrl->GetSelection();

  IntegrityCheckThread thread(*static_cast<WiiPartition*>(m_tree_ctrl->GetItemData(selection)));
  thread.Run();

  while (thread.IsAlive())
  {
    dialog.Pulse();
    wxThread::Sleep(50);
  }

  dialog.Destroy();

  if (thread.Wait())
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

void FilesystemPanel::ExtractAllFiles(const wxString& output_folder)
{
  switch (m_opened_iso->GetVolumeType())
  {
  case DiscIO::Platform::GAMECUBE_DISC:
    ExtractAllFilesGC(output_folder);
    break;

  case DiscIO::Platform::WII_DISC:
    ExtractAllFilesWii(output_folder);
    break;

  case DiscIO::Platform::ELF_DOL:
  case DiscIO::Platform::NUMBER_OF_PLATFORMS:
  case DiscIO::Platform::WII_WAD:
    break;
  }
}

void FilesystemPanel::ExtractAllFilesGC(const wxString& output_folder)
{
  ExtractDirectories("", WxStrToStr(output_folder), *m_filesystem);
}

void FilesystemPanel::ExtractAllFilesWii(const wxString& output_folder)
{
  const wxTreeItemId root = m_tree_ctrl->GetRootItem();

  wxTreeItemIdValue cookie;
  wxTreeItemId item = m_tree_ctrl->GetFirstChild(root, cookie);

  while (item.IsOk())
  {
    const auto* const partition = static_cast<WiiPartition*>(m_tree_ctrl->GetItemData(item));
    ExtractDirectories("", WxStrToStr(output_folder), *partition->filesystem);
    item = m_tree_ctrl->GetNextChild(root, cookie);
  }
}

void FilesystemPanel::ExtractSingleFile(const wxString& output_file_path) const
{
  const auto selection_file_path = BuildFilePathFromSelection();

  switch (m_opened_iso->GetVolumeType())
  {
  case DiscIO::Platform::GAMECUBE_DISC:
    ExtractSingleFileGC(selection_file_path, output_file_path);
    break;

  case DiscIO::Platform::WII_DISC:
    ExtractSingleFileWii(selection_file_path, output_file_path);
    break;

  case DiscIO::Platform::ELF_DOL:
  case DiscIO::Platform::NUMBER_OF_PLATFORMS:
  case DiscIO::Platform::WII_WAD:
    break;
  }
}

void FilesystemPanel::ExtractSingleFileGC(const wxString& file_path,
                                          const wxString& output_file_path) const
{
  m_filesystem->ExportFile(m_filesystem->FindFileInfo(WxStrToStr(file_path)).get(),
                           WxStrToStr(output_file_path));
}

void FilesystemPanel::ExtractSingleFileWii(wxString file_path,
                                           const wxString& output_file_path) const
{
  const size_t slash_index = file_path.find('/');
  const wxString partition_label = file_path.substr(0, slash_index);
  const auto* const partition = FindWiiPartition(m_tree_ctrl, partition_label);

  // Remove "Partition x/"
  file_path.erase(0, slash_index + 1);

  partition->filesystem->ExportFile(
      partition->filesystem->FindFileInfo(WxStrToStr(file_path)).get(),
      WxStrToStr(output_file_path));
}

void FilesystemPanel::ExtractSingleDirectory(const wxString& output_folder)
{
  const wxString directory_path = BuildDirectoryPathFromSelection();

  switch (m_opened_iso->GetVolumeType())
  {
  case DiscIO::Platform::GAMECUBE_DISC:
    ExtractSingleDirectoryGC(directory_path, output_folder);
    break;

  case DiscIO::Platform::WII_DISC:
    ExtractSingleDirectoryWii(directory_path, output_folder);
    break;

  case DiscIO::Platform::ELF_DOL:
  case DiscIO::Platform::NUMBER_OF_PLATFORMS:
  case DiscIO::Platform::WII_WAD:
    break;
  }
}

void FilesystemPanel::ExtractSingleDirectoryGC(const wxString& directory_path,
                                               const wxString& output_folder)
{
  ExtractDirectories(WxStrToStr(directory_path), WxStrToStr(output_folder), *m_filesystem);
}

void FilesystemPanel::ExtractSingleDirectoryWii(wxString directory_path,
                                                const wxString& output_folder)
{
  const size_t slash_index = directory_path.find('/');
  const wxString partition_label = directory_path.substr(0, slash_index);
  const auto* const partition = FindWiiPartition(m_tree_ctrl, partition_label);

  // Remove "Partition x/"
  directory_path.erase(0, slash_index + 1);

  ExtractDirectories(WxStrToStr(directory_path), WxStrToStr(output_folder), *partition->filesystem);
}

void ExtractDir(const std::string& full_path, const std::string& output_folder,
                const DiscIO::IFileSystem& file_system, const DiscIO::IFileInfo& directory,
                const std::function<bool(const std::string& path)>& update_progress)
{
  for (const DiscIO::IFileInfo& file_info : directory)
  {
    const std::string path = full_path + file_info.GetName() + (file_info.IsDirectory() ? "/" : "");
    const std::string output_path = output_folder + DIR_SEP_CHR + path;

    if (update_progress(path))
      return;

    DEBUG_LOG(DISCIO, output_path.c_str());

    if (file_info.IsDirectory())
    {
      File::CreateFullPath(output_path);
      ExtractDir(path, output_folder, file_system, file_info, update_progress);
    }
    else
    {
      if (File::Exists(output_path))
        NOTICE_LOG(DISCIO, "%s already exists", output_path.c_str());
      else if (!file_system.ExportFile(&file_info, output_path))
        ERROR_LOG(DISCIO, "Could not export %s", output_path.c_str());
    }
  }
}

void FilesystemPanel::ExtractDirectories(const std::string& full_path,
                                         const std::string& output_folder,
                                         const DiscIO::IFileSystem& filesystem)
{
  if (full_path.empty())  // Root
  {
    filesystem.ExportApploader(output_folder);
    filesystem.ExportDOL(output_folder);
  }

  std::unique_ptr<DiscIO::IFileInfo> file_info = filesystem.FindFileInfo(full_path);
  u32 size = file_info->GetTotalChildren();
  u32 progress = 0;

  wxString dialog_title = full_path.empty() ? _("Extracting All Files") : _("Extracting Directory");
  wxProgressDialog dialog(dialog_title, _("Extracting..."), size, this,
                          wxPD_APP_MODAL | wxPD_AUTO_HIDE | wxPD_CAN_ABORT | wxPD_ELAPSED_TIME |
                              wxPD_ESTIMATED_TIME | wxPD_REMAINING_TIME | wxPD_SMOOTH);

  File::CreateFullPath(output_folder + "/" + full_path);
  ExtractDir(full_path, output_folder, filesystem, *file_info, [&](const std::string& path) {
    dialog.SetTitle(wxString::Format(
        "%s : %d%%", dialog_title.c_str(),
        static_cast<u32>((static_cast<float>(progress) / static_cast<float>(size)) * 100)));
    dialog.Update(progress, wxString::Format(_("Extracting %s"), StrToWxStr(path)));
    ++progress;
    return dialog.WasCancelled();
  });
}

wxString FilesystemPanel::BuildFilePathFromSelection() const
{
  wxString file_path = m_tree_ctrl->GetItemText(m_tree_ctrl->GetSelection());

  const auto root_node = m_tree_ctrl->GetRootItem();
  auto node = m_tree_ctrl->GetItemParent(m_tree_ctrl->GetSelection());

  while (node != root_node)
  {
    file_path = m_tree_ctrl->GetItemText(node) + DIR_SEP_CHR + file_path;
    node = m_tree_ctrl->GetItemParent(node);
  }

  return file_path;
}

wxString FilesystemPanel::BuildDirectoryPathFromSelection() const
{
  wxString directory_path = BuildFilePathFromSelection();
  directory_path += DIR_SEP_CHR;
  return directory_path;
}
