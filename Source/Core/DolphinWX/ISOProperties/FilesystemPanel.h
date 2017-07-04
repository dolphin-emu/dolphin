// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include <utility>
#include <wx/panel.h>

class GameListItem;
class wxTreeCtrl;
class wxTreeEvent;

namespace DiscIO
{
class FileSystem;
class Volume;
}

class FilesystemPanel final : public wxPanel
{
public:
  explicit FilesystemPanel(wxWindow* parent, wxWindowID id,
                           const std::unique_ptr<DiscIO::Volume>& opened_iso);
  ~FilesystemPanel();

private:
  enum
  {
    ID_EXTRACT_DIR = 20000,
    ID_EXTRACT_ALL,
    ID_EXTRACT_FILE,
    ID_EXTRACT_SYSTEM_DATA,
    ID_CHECK_INTEGRITY,
  };

  void CreateGUI();
  void BindEvents();

  bool PopulateFileSystemTree();

  void OnRightClickTree(wxTreeEvent&);
  void OnExtractFile(wxCommandEvent&);
  void OnExtractDirectories(wxCommandEvent&);
  void OnExtractSystemData(wxCommandEvent&);
  void OnExtractAll(wxCommandEvent&);
  void OnCheckPartitionIntegrity(wxCommandEvent&);

  void ExtractSingleFile(const wxString& output_file_path) const;
  void ExtractSingleDirectory(const wxString& output_folder);
  void ExtractDirectories(const std::string& full_path, const std::string& output_folder,
                          const DiscIO::FileSystem& filesystem);
  void ExtractPartition(const std::string& output_folder, const DiscIO::FileSystem& filesystem);

  std::pair<wxString, const DiscIO::FileSystem&> BuildFilePathFromSelection() const;
  std::pair<wxString, const DiscIO::FileSystem&> BuildDirectoryPathFromSelection() const;

  wxTreeCtrl* m_tree_ctrl;

  const std::unique_ptr<DiscIO::Volume>& m_opened_iso;

  std::unique_ptr<DiscIO::FileSystem> m_filesystem;
  bool m_has_partitions;
};
