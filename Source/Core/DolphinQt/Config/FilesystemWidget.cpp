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
#include <QProgressDialog>
#include <QStandardItemModel>
#include <QStyleFactory>
#include <QTreeView>
#include <QVBoxLayout>

#include <future>

#include "DiscIO/DiscExtractor.h"
#include "DiscIO/Filesystem.h"
#include "DiscIO/Volume.h"

#include "DolphinQt/QtUtils/ModalMessageBox.h"
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

FilesystemWidget::FilesystemWidget(std::shared_ptr<DiscIO::Volume> volume)
    : m_volume(std::move(volume))
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
  // Cache these two icons, the tree will use them a lot.
  m_folder_icon = Resources::GetScaledIcon("isoproperties_folder");
  m_file_icon = Resources::GetScaledIcon("isoproperties_file");

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
    item->setIcon(info.IsDirectory() ? m_folder_icon : m_file_icon);

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
        ModalMessageBox::information(this, tr("Success"),
                                     tr("Successfully extracted system data."));
      else
        ModalMessageBox::critical(this, tr("Error"), tr("Failed to extract system data."));
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
            const std::string partition_name = DiscIO::NameForPartitionType(*partition_type, true);
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
    ModalMessageBox::information(this, tr("Success"), tr("Successfully extracted file."));
  else
    ModalMessageBox::critical(this, tr("Error"), tr("Failed to extract file."));
}
