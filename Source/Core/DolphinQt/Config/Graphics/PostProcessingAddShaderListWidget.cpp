// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Graphics/PostProcessingAddShaderListWidget.h"

#include <string>

#include <QComboBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QLineEdit>
#include <QListWidget>
#include <QMimeData>
#include <QString>
#include <QVBoxLayout>

#include "Common/CommonFuncs.h"
#include "Common/CommonPaths.h"
#include "Common/FileSearch.h"
#include "Common/FileUtil.h"
#include "Common/StringUtil.h"

#include "DolphinQt/QtUtils/ModalMessageBox.h"

PostProcessingAddShaderListWidget::PostProcessingAddShaderListWidget(QWidget* parent,
                                                                     const QString& root_path,
                                                                     bool allows_drag_drop)
    : QWidget(parent), m_root_path(root_path), m_allows_drag_drop(allows_drag_drop)
{
  CreateWidgets();
  ConnectWidgets();
  setLayout(m_main_layout);

  if (m_allows_drag_drop)
  {
    setAcceptDrops(true);
  }

  // Update with initial selection
  OnShaderTypeChanged();
}

void PostProcessingAddShaderListWidget::CreateWidgets()
{
  m_main_layout = new QVBoxLayout;

  m_shader_type = new QComboBox();
  BuildShaderCategories();
  m_main_layout->addWidget(m_shader_type);

  m_search_text = new QLineEdit();
  m_search_text->setPlaceholderText(tr("Filter shaders..."));
  m_main_layout->addWidget(m_search_text);

  m_shader_list = new QListWidget();
  m_shader_list->setSortingEnabled(false);
  m_shader_list->setSelectionBehavior(QAbstractItemView::SelectionBehavior::SelectItems);
  m_shader_list->setSelectionMode(QAbstractItemView::SelectionMode::SingleSelection);
  m_shader_list->setSelectionRectVisible(true);
  m_main_layout->addWidget(m_shader_list);
}

void PostProcessingAddShaderListWidget::ConnectWidgets()
{
  connect(m_shader_list, &QListWidget::itemSelectionChanged, this, [this] { OnShadersSelected(); });
  connect(m_shader_type, &QComboBox::currentTextChanged, this, [this] { OnShaderTypeChanged(); });
  connect(m_search_text, &QLineEdit::textEdited, this,
          [this](QString text) { OnSearchTextChanged(text); });
}

void PostProcessingAddShaderListWidget::BuildShaderCategories()
{
  m_shader_type->clear();
  for (QFileInfo info : m_root_path.entryInfoList(QDir::AllDirs | QDir::NoDotAndDotDot))
  {
    m_shader_type->addItem(info.fileName(), info.absoluteFilePath());
  }
}

void PostProcessingAddShaderListWidget::OnShadersSelected()
{
  m_chosen_shader_pathes.clear();
  if (m_shader_list->selectedItems().empty())
    return;
  for (auto item : m_shader_list->selectedItems())
  {
    m_chosen_shader_pathes.push_back(item->data(Qt::UserRole).toString());
  }
}

void PostProcessingAddShaderListWidget::OnShaderTypeChanged()
{
  const int current_index = m_shader_type->currentIndex();
  if (current_index == -1)
    return;
  const auto available_shaders =
      GetAvailableShaders(m_shader_type->itemData(current_index).toString().toStdString());
  m_shader_list->clear();
  for (const std::string& shader : available_shaders)
  {
    std::string name;
    SplitPath(shader, nullptr, &name, nullptr);
    QListWidgetItem* item = new QListWidgetItem(QString::fromStdString(name));
    item->setData(Qt::UserRole, QString::fromStdString(shader));
    m_shader_list->addItem(item);
  }
}

void PostProcessingAddShaderListWidget::OnSearchTextChanged(QString search_text)
{
  for (int i = 0; i < m_shader_list->count(); i++)
    m_shader_list->item(i)->setHidden(true);

  const QList<QListWidgetItem*> matched_items =
      m_shader_list->findItems(search_text, Qt::MatchFlag::MatchContains);
  for (auto* item : matched_items)
    item->setHidden(false);
}

std::vector<std::string>
PostProcessingAddShaderListWidget::GetAvailableShaders(const std::string& directory_path)
{
  const std::vector<std::string> search_dirs = {directory_path};
  const std::vector<std::string> search_extensions = {".glsl"};
  std::vector<std::string> result;
  std::vector<std::string> paths;

  // main folder
  paths = Common::DoFileSearch(search_dirs, search_extensions, false);
  for (const std::string& path : paths)
  {
    if (std::find(result.begin(), result.end(), path) == result.end())
      result.push_back(path);
  }

  // folders/sub-shaders
  // paths = Common::DoFileSearch(search_dirs, false);
  for (const std::string& dirname : paths)
  {
    // find sub-shaders in this folder
    size_t pos = dirname.find_last_of(DIR_SEP_CHR);
    if (pos != std::string::npos && (pos != dirname.length() - 1))
    {
      std::string shader_dirname = dirname.substr(pos + 1);
      std::vector<std::string> sub_paths =
          Common::DoFileSearch(search_extensions, {dirname}, false);
      for (const std::string& sub_path : sub_paths)
      {
        if (std::find(result.begin(), result.end(), sub_path) == result.end())
          result.push_back(sub_path);
      }
    }
  }

  // sort by path
  std::sort(result.begin(), result.end());
  return result;
}

void PostProcessingAddShaderListWidget::dragEnterEvent(QDragEnterEvent* event)
{
  if (event->mimeData()->hasUrls() && event->mimeData()->urls().size() == 1)
    event->acceptProposedAction();
}

void PostProcessingAddShaderListWidget::dropEvent(QDropEvent* event)
{
  const auto& urls = event->mimeData()->urls();
  if (urls.empty())
    return;

  const auto& url = urls[0];
  QFileInfo file_info(url.toLocalFile());

  if (!file_info.exists() || !file_info.isReadable())
  {
    ModalMessageBox::critical(this, tr("Error"),
                              tr("Path '%1' is not readable").arg(file_info.filePath()));
    return;
  }

  if (file_info.isDir())
  {
    const auto file_path = file_info.absoluteFilePath().toStdString();
    const std::vector<std::string> search_dirs = {file_path};
    const std::vector<std::string> search_extensions = {".glsl"};
    const auto pathes = Common::DoFileSearch(search_dirs, search_extensions, false);
    if (pathes.empty())
    {
      ModalMessageBox::critical(
          this, tr("Error"),
          tr("Folder '%1' must container a shader that ends in glsl").arg(file_info.filePath()));
      return;
    }

    const auto root_path = m_root_path.filePath(file_info.fileName()).toStdString();
    File::CopyDir(file_path, root_path + DIR_SEP_CHR, false);
  }
  else if (file_info.isFile())
  {
    if (file_info.completeSuffix().toLower() != QStringLiteral("glsl"))
    {
      ModalMessageBox::critical(
          this, tr("Error"),
          tr("File '%1' must be a shader that ends in glsl").arg(file_info.filePath()));
      return;
    }

    const auto file_path = file_info.absoluteFilePath().toStdString();
    const auto root_file = m_root_path.filePath(file_info.fileName()).toStdString();
    File::Copy(file_path, root_file);
  }

  BuildShaderCategories();
  OnShaderTypeChanged();
}
