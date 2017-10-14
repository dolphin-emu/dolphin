// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDialog>
#include <memory>

#include "DiscIO/Volume.h"
#include "DolphinQt2/GameList/GameFile.h"

class QStandardItem;
class QStandardItemModel;
class QTreeView;

namespace DiscIO
{
class FileInfo;
struct Partition;
};

class FilesystemWidget final : public QWidget
{
  Q_OBJECT
public:
  explicit FilesystemWidget(const GameFile& game);

private:
  void CreateWidgets();
  void ConnectWidgets();
  void PopulateView();
  void PopulateDirectory(int partition_id, QStandardItem* root, const DiscIO::Partition& partition);
  void PopulateDirectory(int partition_id, QStandardItem* root, const DiscIO::FileInfo& directory);

  void ShowContextMenu(const QPoint&);

  void ExtractPartition(const DiscIO::Partition& partition, const QString& out);
  void ExtractDirectory(const DiscIO::Partition& partition, const QString path, const QString& out);
  void ExtractFile(const DiscIO::Partition& pratition, const QString& path, const QString& out);
  void ExtractSystemData(const DiscIO::Partition& partition, const QString& out);
  void CheckIntegrity(const DiscIO::Partition& partition);

  DiscIO::Partition GetPartitionFromID(int id);

  QStandardItemModel* m_tree_model;
  QTreeView* m_tree_view;

  GameFile m_game;
  std::unique_ptr<DiscIO::Volume> m_volume;
};
