// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/FilesystemWidget.h"

#include <QApplication>
#include <QCoreApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QMenu>
#include <QMessageBox>
#include <QProgressDialog>
#include <QStandardItemModel>
#include <QStyleFactory>
#include <QTreeView>
#include <QVBoxLayout>

#include <future>

#include "DiscIO/DiscExtractor.h"
#include "DiscIO/Filesystem.h"
#include "DiscIO/Volume.h"

#include "DolphinQt/Resources.h"

#include "UICommon/UICommon.h"

constexpr int ENTRY_PARTITION = Qt::UserRole;
constexpr int ENTRY_NAME = Qt::UserRole + 1;
constexpr int ENTRY_TYPE = Qt::UserRole + 2;

enum class EntryType
{
  Disc = -2,
  Partition = -1,
  File = 0,
  Dir = 1
};
Q_DECLARE_METATYPE(EntryType);

FilesystemWidget::FilesystemWidget(const UICommon::GameFile& game)
    : m_game(game), m_volume(DiscIO::CreateVolumeFromFilename(game.GetFilePath()))
{
  CreateWidgets();
  ConnectWidgets();
  PopulateView();
}

FilesystemWidget::~FilesystemWidget() = default;

void FilesystemWidget::CreateWidgets()
{
  auto* layout = new QVBoxLayout;

  m_tree_model = new QStandardItemModel(0, 2);
  m_tree_model->setHorizontalHeaderLabels({tr("Name"), tr("Size")});

  m_tree_view = new QTreeView(this);
  m_tree_view->setModel(m_tree_model);
  m_tree_view->setContextMenuPolicy(Qt::CustomContextMenu);

  auto* header = m_tree_view->header();

  header->setSectionResizeMode(0, QHeaderView::Stretch);
  header->setSectionResizeMode(1, QHeaderView::ResizeToContents);
  header->setStretchLastSection(false);

// Windows: Set style to (old) windows, which draws branch lines
#ifdef _WIN32
  if (QApplication::style()->objectName() == QStringLiteral("windowsvista"))
    m_tree_view->setStyle(QStyleFactory::create(QStringLiteral("windows")));
#endif

  layout->addWidget(m_tree_view);

  setLayout(layout);
}

void FilesystemWidget::ConnectWidgets()
{
  connect(m_tree_view, &QTreeView::customContextMenuRequested, this,
          &FilesystemWidget::ShowContextMenu);
}

void FilesystemWidget::PopulateView()
{
  auto* disc = new QStandardItem(tr("Disc"));
  disc->setEditable(false);
  disc->setIcon(Resources::GetScaledIcon("isoproperties_disc"));
  disc->setData(QVariant::fromValue(EntryType::Disc), ENTRY_TYPE);
  m_tree_model->appendRow(disc);
  m_tree_view->expand(disc->index());

  const auto& partitions = m_volume->GetPartitions();

  for (size_t i = 0; i < partitions.size(); i++)
  {
    auto* item = new QStandardItem(tr("Partition %1").arg(i));
    item->setEditable(false);

    item->setIcon(Resources::GetScaledIcon("isoproperties_disc"));
    item->setData(static_cast<qlonglong>(i), ENTRY_PARTITION);
    item->setData(QVariant::fromValue(EntryType::Partition), ENTRY_TYPE);

    PopulateDirectory(static_cast<int>(i), item, partitions[i]);

    disc->appendRow(item);

    if (m_volume->GetGamePartition() == partitions[i])
      m_tree_view->expand(item->index());
  }

  if (partitions.empty())
    PopulateDirectory(-1, disc, DiscIO::PARTITION_NONE);
}

void FilesystemWidget::PopulateDirectory(int partition_id, QStandardItem* root,
                                         const DiscIO::Partition& partition)
{
  const DiscIO::FileSystem* const file_system = m_volume->GetFileSystem(partition);
  if (file_system)
    PopulateDirectory(partition_id, root, file_system->GetRoot());
}

void FilesystemWidget::PopulateDirectory(int partition_id, QStandardItem* root,
                                         const DiscIO::FileInfo& directory)
{
  for (const auto& info : directory)
  {
    auto* item = new QStandardItem(QString::fromStdString(info.GetName()));
    item->setEditable(false);
    item->setIcon(Resources::GetScaledIcon(info.IsDirectory() ? "isoproperties_folder" :
                                                                "isoproperties_file"));

    if (info.IsDirectory())
      PopulateDirectory(partition_id, item, info);

    item->setData(partition_id, ENTRY_PARTITION);
    item->setData(QString::fromStdString(info.GetPath()), ENTRY_NAME);
    item->setData(QVariant::fromValue(info.IsDirectory() ? EntryType::Dir : EntryType::File),
                  ENTRY_TYPE);

    const auto size = info.GetTotalSize();

    auto* size_item = new QStandardItem(QString::fromStdString(UICommon::FormatSize(size)));
    size_item->setTextAlignment(Qt::AlignRight);
    size_item->setEditable(false);

    root->appendRow({item, size_item});
  }
}

QString FilesystemWidget::SelectFolder()
{
  return QFileDialog::getExistingDirectory(this, QObject::tr("Choose the folder to extract to"));
}

void FilesystemWidget::ShowContextMenu(const QPoint&)
{
  auto* selection = m_tree_view->selectionModel();
  if (!selection->hasSelection())
    return;

  auto* item = m_tree_model->itemFromIndex(selection->selectedIndexes()[0]);

  QMenu* menu = new QMenu(this);

  EntryType type = item->data(ENTRY_TYPE).value<EntryType>();

  DiscIO::Partition partition = type == EntryType::Disc ?
                                    DiscIO::PARTITION_NONE :
                                    GetPartitionFromID(item->data(ENTRY_PARTITION).toInt());
  QString path = item->data(ENTRY_NAME).toString();

  const bool is_filesystem_root = (type == EntryType::Disc && m_volume->GetPartitions().empty()) ||
                                  type == EntryType::Partition;

  if (type == EntryType::Dir || is_filesystem_root)
  {
    menu->addAction(tr("Extract Files..."), this, [this, partition, path] {
      auto folder = SelectFolder();

      if (!folder.isEmpty())
        ExtractDirectory(partition, path, folder);
    });
  }

  if (is_filesystem_root)
  {
    menu->addAction(tr("Extract System Data..."), this, [this, partition] {
      auto folder = SelectFolder();

      if (folder.isEmpty())
        return;

      if (ExtractSystemData(partition, folder))
        QMessageBox::information(this, tr("Success"), tr("Successfully extracted system data."));
      else
        QMessageBox::critical(this, tr("Error"), tr("Failed to extract system data."));
    });
  }

  switch (type)
  {
  case EntryType::Disc:
    menu->addAction(tr("Extract Entire Disc..."), this, [this, path] {
      auto folder = SelectFolder();

      if (folder.isEmpty())
        return;

      if (m_volume->GetPartitions().empty())
      {
        ExtractPartition(DiscIO::PARTITION_NONE, folder);
      }
      else
      {
        for (DiscIO::Partition& p : m_volume->GetPartitions())
        {
          if (const std::optional<u32> partition_type = m_volume->GetPartitionType(p))
          {
            const std::string partition_name =
                DiscIO::DirectoryNameForPartitionType(*partition_type);
            ExtractPartition(p, folder + QChar(u'/') + QString::fromStdString(partition_name));
          }
        }
      }
    });
    break;
  case EntryType::Partition:
    menu->addAction(tr("Extract Entire Partition..."), this, [this, partition] {
      auto folder = SelectFolder();
      if (!folder.isEmpty())
        ExtractPartition(partition, folder);
    });
    if (m_volume->IsEncryptedAndHashed())
    {
      menu->addSeparator();
      menu->addAction(tr("Check Partition Integrity"), this,
                      [this, partition] { CheckIntegrity(partition); });
    }
    break;
  case EntryType::File:
    menu->addAction(tr("Extract File..."), this, [this, partition, path] {
      auto dest =
          QFileDialog::getSaveFileName(this, tr("Save File to"), QFileInfo(path).fileName());

      if (!dest.isEmpty())
        ExtractFile(partition, path, dest);
    });
    break;
  case EntryType::Dir:
    // Handled above the switch statement
    break;
  }

  menu->exec(QCursor::pos());
}

DiscIO::Partition FilesystemWidget::GetPartitionFromID(int id)
{
  return id == -1 ? DiscIO::PARTITION_NONE : m_volume->GetPartitions()[id];
}

void FilesystemWidget::ExtractPartition(const DiscIO::Partition& partition, const QString& out)
{
  ExtractDirectory(partition, QStringLiteral(""), out + QStringLiteral("/files"));
  ExtractSystemData(partition, out);
}

bool FilesystemWidget::ExtractSystemData(const DiscIO::Partition& partition, const QString& out)
{
  return DiscIO::ExportSystemData(*m_volume, partition, out.toStdString());
}

void FilesystemWidget::ExtractDirectory(const DiscIO::Partition& partition, const QString& path,
                                        const QString& out)
{
  const DiscIO::FileSystem* filesystem = m_volume->GetFileSystem(partition);
  if (!filesystem)
    return;

  std::unique_ptr<DiscIO::FileInfo> info = filesystem->FindFileInfo(path.toStdString());
  u32 size = info->GetTotalChildren();

  QProgressDialog* dialog = new QProgressDialog(this);
  dialog->setWindowFlags(dialog->windowFlags() & ~Qt::WindowContextHelpButtonHint);
  dialog->setMinimum(0);
  dialog->setMaximum(size);
  dialog->show();
  dialog->setWindowTitle(tr("Progress"));

  bool all = path.isEmpty();

  DiscIO::ExportDirectory(
      *m_volume, partition, *info, true, path.toStdString(), out.toStdString(),
      [all, dialog](const std::string& current) {
        dialog->setLabelText(
            (all ? QObject::tr("Extracting All Files...") : QObject::tr("Extracting Directory..."))
                .append(QStringLiteral(" %1").arg(QString::fromStdString(current))));
        dialog->setValue(dialog->value() + 1);

        QCoreApplication::processEvents();
        return dialog->wasCanceled();
      });

  dialog->close();
}

void FilesystemWidget::ExtractFile(const DiscIO::Partition& partition, const QString& path,
                                   const QString& out)
{
  const DiscIO::FileSystem* filesystem = m_volume->GetFileSystem(partition);
  if (!filesystem)
    return;

  bool success = DiscIO::ExportFile(
      *m_volume, partition, filesystem->FindFileInfo(path.toStdString()).get(), out.toStdString());

  if (success)
    QMessageBox::information(this, tr("Success"), tr("Successfully extracted file."));
  else
    QMessageBox::critical(this, tr("Error"), tr("Failed to extract file."));
}

void FilesystemWidget::CheckIntegrity(const DiscIO::Partition& partition)
{
  QProgressDialog* dialog = new QProgressDialog(this);
  std::future<bool> is_valid = std::async(
      std::launch::async, [this, partition] { return m_volume->CheckIntegrity(partition); });

  dialog->setLabelText(tr("Verifying integrity of partition..."));
  dialog->setWindowFlags(dialog->windowFlags() & ~Qt::WindowContextHelpButtonHint);
  dialog->setWindowTitle(tr("Verifying partition"));

  dialog->setMinimum(0);
  dialog->setMaximum(0);
  dialog->show();

  while (is_valid.wait_for(std::chrono::milliseconds(50)) != std::future_status::ready)
    QCoreApplication::processEvents();

  dialog->close();

  if (is_valid.get())
    QMessageBox::information(this, tr("Success"),
                             tr("Integrity check completed. No errors have been found."));
  else
    QMessageBox::critical(this, tr("Error"),
                          tr("Integrity check for partition failed. The disc image is most "
                             "likely corrupted or has been patched incorrectly."));
}
