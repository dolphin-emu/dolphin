// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDialog>
#include <QIcon>
#include <memory>

class QStandardItem;
class QStandardItemModel;
class QTreeView;

namespace DiscIO
{
class FileInfo;
class Volume;

struct Partition;
}  // namespace DiscIO

class FilesystemWidget final : public QWidget
{
  Q_OBJECT
public:
  explicit FilesystemWidget(std::shared_ptr<DiscIO::Volume> volume);
  ~FilesystemWidget() override;

private:
  void CreateWidgets();
  void ConnectWidgets();
  void PopulateView();
  void PopulateDirectory(int partition_id, QStandardItem* root, const DiscIO::Partition& partition);
  void PopulateDirectory(int partition_id, QStandardItem* root, const DiscIO::FileInfo& directory);

  QString SelectFolder();

  void ShowContextMenu(const QPoint&);

  void ExtractPartition(const DiscIO::Partition& partition, const QString& out);
  void ExtractDirectory(const DiscIO::Partition& partition, const QString& path,
                        const QString& out);
  void ExtractFile(const DiscIO::Partition& partition, const QString& path, const QString& out);
  bool ExtractSystemData(const DiscIO::Partition& partition, const QString& out);

  DiscIO::Partition GetPartitionFromID(int id);

  QStandardItemModel* m_tree_model;
  QTreeView* m_tree_view;

  std::shared_ptr<DiscIO::Volume> m_volume;

  QIcon m_folder_icon;
  QIcon m_file_icon;
};
