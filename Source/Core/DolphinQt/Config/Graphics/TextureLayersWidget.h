// Copyright 2021 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QWidget>

#include "Common/Config/ConfigInfo.h"

class QLayout;
class QListWidget;
class QListWidgetItem;
class QPushButton;

class TextureLayersWidget final : public QWidget
{
  Q_OBJECT
public:
  TextureLayersWidget(QWidget* parent, const Config::Info<std::string>& config);

private:
  void CreateLayout();
  void ConnectWidgets();

  QLayout* CreateTextureLayersUI();

  void BuildTextureLayersList();
  void OnTexturesChanged();

  void OnTextureLayerAdded();
  void OnTextureLayerRemoved();
  void OnTextureLayerChanged();

  void OnTextureAddedToLayer();
  void OnTextureRemovedFromLayer();
  void UpdateCurrentLayerTextures();

  Config::Info<std::string> m_config;

  QListWidget* m_texture_layer_list;
  QPushButton* m_add_texture_layer;
  QPushButton* m_remove_texture_layer;
  QListWidgetItem* m_current_texture_layer;

  QListWidget* m_texture_layer_textures_list;
  QPushButton* m_add_texture_layer_texture;
  QPushButton* m_remove_texture_layer_texture;
};
