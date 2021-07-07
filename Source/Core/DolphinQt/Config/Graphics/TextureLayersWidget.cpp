// Copyright 2021 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/Graphics/TextureLayersWidget.h"

#include <fmt/format.h>

#include <QBoxLayout>
#include <QGroupBox>
#include <QListWidget>
#include <QPushButton>

#include "Common/Config/Config.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "DolphinQt/Config/Graphics/NewTextureDialog.h"
#include "DolphinQt/Config/Graphics/NewTextureLayerDialog.h"

TextureLayersWidget::TextureLayersWidget(QWidget* parent, const Config::Info<std::string>& config)
    : QWidget(parent), m_config(config)
{
  CreateLayout();
  ConnectWidgets();
}

void TextureLayersWidget::CreateLayout()
{
  setLayout(CreateTextureLayersUI());

  BuildTextureLayersList();
}

QLayout* TextureLayersWidget::CreateTextureLayersUI()
{
  auto* texture_layers_vlayout = new QVBoxLayout;
  m_texture_layer_list = new QListWidget();
  m_texture_layer_list->setSelectionMode(QAbstractItemView::SelectionMode::SingleSelection);
  texture_layers_vlayout->addWidget(m_texture_layer_list);

  m_add_texture_layer = new QPushButton(tr("Add..."));
  m_remove_texture_layer = new QPushButton(tr("Remove"));

  QHBoxLayout* texture_layers_hlayout = new QHBoxLayout;
  texture_layers_hlayout->addStretch();
  texture_layers_hlayout->addWidget(m_add_texture_layer);
  texture_layers_hlayout->addWidget(m_remove_texture_layer);

  texture_layers_vlayout->addItem(texture_layers_hlayout);

  auto* texture_layer_textures_vlayout = new QVBoxLayout;
  m_texture_layer_textures_list = new QListWidget();
  m_texture_layer_textures_list->setSelectionMode(QAbstractItemView::SelectionMode::MultiSelection);

  texture_layer_textures_vlayout->addWidget(m_texture_layer_textures_list);

  m_add_texture_layer_texture = new QPushButton(tr("Add..."));
  m_add_texture_layer_texture->setEnabled(false);
  m_remove_texture_layer_texture = new QPushButton(tr("Remove"));
  m_remove_texture_layer_texture->setEnabled(false);

  QHBoxLayout* texture_layer_textures_hlayout = new QHBoxLayout;
  texture_layer_textures_hlayout->addStretch();
  texture_layer_textures_hlayout->addWidget(m_add_texture_layer_texture);
  texture_layer_textures_hlayout->addWidget(m_remove_texture_layer_texture);

  texture_layer_textures_vlayout->addItem(texture_layer_textures_hlayout);

  QHBoxLayout* hlayout = new QHBoxLayout;
  hlayout->addItem(texture_layers_vlayout);
  hlayout->addStretch();
  hlayout->addItem(texture_layer_textures_vlayout);

  return hlayout;
}

void TextureLayersWidget::ConnectWidgets()
{
  connect(m_add_texture_layer, &QPushButton::clicked, this,
          &TextureLayersWidget::OnTextureLayerAdded);
  connect(m_remove_texture_layer, &QPushButton::clicked, this,
          &TextureLayersWidget::OnTextureLayerRemoved);
  connect(m_texture_layer_list, &QListWidget::itemSelectionChanged, this,
          &TextureLayersWidget::OnTextureLayerChanged);

  connect(m_add_texture_layer_texture, &QPushButton::clicked, this,
          &TextureLayersWidget::OnTextureAddedToLayer);
  connect(m_remove_texture_layer_texture, &QPushButton::clicked, this,
          &TextureLayersWidget::OnTextureRemovedFromLayer);
}

void TextureLayersWidget::BuildTextureLayersList()
{
  m_texture_layer_list->clear();

  const auto& texture_layers_setting = Config::Get(m_config);
  const auto texture_layers = SplitString(texture_layers_setting, ';');
  for (const std::string& texture_layer : texture_layers)
  {
    const auto texture_layer_info = SplitString(texture_layer, ':');

    QListWidgetItem* list_item = new QListWidgetItem(QString::fromStdString(texture_layer_info[0]));

    if (texture_layer_info.size() > 1)
    {
      list_item->setData(Qt::UserRole, QString::fromStdString(texture_layer_info[1]));
    }
    else
    {
      list_item->setData(Qt::UserRole, QStringLiteral(""));
    }
    m_texture_layer_list->addItem(list_item);
  }
}

void TextureLayersWidget::OnTexturesChanged()
{
  std::vector<std::string> texture_layers;
  for (int i = 0; i < m_texture_layer_list->count(); i++)
  {
    const auto* item = m_texture_layer_list->item(i);
    const auto textures = item->data(Qt::UserRole).toString().toStdString();
    texture_layers.push_back(fmt::format("{}:{}", item->text().toStdString(), textures));
  }

  Config::SetBaseOrCurrent(m_config, fmt::to_string(fmt::join(texture_layers, ";")));
}

void TextureLayersWidget::OnTextureLayerAdded()
{
  if (m_add_texture_layer != QObject::sender())
    return;
  const int index = 0;
  NewTextureLayerDialog* window = new NewTextureLayerDialog(this);
  window->setModal(true);
  if (window->exec() == QDialog::Rejected)
    return;

  auto* item = new QListWidgetItem(window->GetTextureLayerName());
  item->setData(Qt::UserRole, QStringLiteral(""));
  m_texture_layer_list->addItem(item);

  OnTexturesChanged();
  BuildTextureLayersList();
}

void TextureLayersWidget::OnTextureLayerRemoved()
{
  m_texture_layer_list->takeItem(m_texture_layer_list->row(m_texture_layer_list->currentItem()));

  m_texture_layer_textures_list->clear();
  m_texture_layer_textures_list->clearSelection();

  m_add_texture_layer_texture->setEnabled(false);
  m_remove_texture_layer_texture->setEnabled(false);

  OnTexturesChanged();
  BuildTextureLayersList();
}

void TextureLayersWidget::OnTextureLayerChanged()
{
  m_current_texture_layer = m_texture_layer_list->currentItem();

  m_texture_layer_textures_list->clear();

  const QString textures_str = m_current_texture_layer->data(Qt::UserRole).toString();
  const auto textures = textures_str.split(QStringLiteral(","));

  for (const auto& texture : textures)
  {
    if (texture.isEmpty())
      continue;
    QListWidgetItem* list_item = new QListWidgetItem(texture);
    m_texture_layer_textures_list->addItem(list_item);
  }

  m_add_texture_layer_texture->setEnabled(true);
  m_remove_texture_layer_texture->setEnabled(m_texture_layer_textures_list->count() > 0);
}

void TextureLayersWidget::OnTextureAddedToLayer()
{
  if (m_add_texture_layer_texture != QObject::sender())
    return;
  const int index = 0;
  NewTextureDialog* window = new NewTextureDialog(this);
  window->setModal(true);
  if (window->exec() == QDialog::Rejected)
    return;

  auto* item = new QListWidgetItem(window->GetTextureName());
  m_texture_layer_textures_list->addItem(item);

  UpdateCurrentLayerTextures();
}

void TextureLayersWidget::OnTextureRemovedFromLayer()
{
  const auto items = m_texture_layer_textures_list->selectedItems();
  for (const auto item : items)
  {
    m_texture_layer_textures_list->takeItem(m_texture_layer_textures_list->row(item));
  }

  UpdateCurrentLayerTextures();
}

void TextureLayersWidget::UpdateCurrentLayerTextures()
{
  QString layer_textures;
  for (int i = 0; i < m_texture_layer_textures_list->count(); ++i)
  {
    QListWidgetItem* item = m_texture_layer_textures_list->item(i);

    if (i == m_texture_layer_textures_list->count() - 1)
    {
      layer_textures += item->text();
    }
    else
    {
      layer_textures += item->text() + QStringLiteral(",");
    }
  }

  m_current_texture_layer->setData(Qt::UserRole, layer_textures);

  OnTexturesChanged();
}
