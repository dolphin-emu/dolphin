// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>

#include <QWidget>

#include "Common/CommonTypes.h"

#include "VideoCommon/PEShaderSystem/Config/PEShaderConfig.h"
#include "VideoCommon/PEShaderSystem/Config/PEShaderConfigGroup.h"

class QGroupBox;
class QHBoxLayout;
class QLabel;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QVBoxLayout;

class PostProcessingShaderListWidget final : public QWidget
{
  Q_OBJECT
public:
  PostProcessingShaderListWidget(QWidget* parent, VideoCommon::PE::ShaderConfigGroup* group);

private:
  void CreateWidgets();
  void ConnectWidgets();

  void RefreshShaderList();
  void ShaderSelectionChanged();
  void ShaderItemChanged(QListWidgetItem* item);

  void OnShaderChanged(std::optional<u32> index);
  QLayout* BuildPassesLayout(u32 index);
  QLayout* BuildSnapshotsLayout(u32 index);
  QLayout* BuildOptionsLayout(u32 index);

  void SaveShaderList();

  void OnShaderAdded();
  void OnShaderRemoved();

  void ClearLayoutRecursively(QLayout* layout);

  VideoCommon::PE::ShaderConfigGroup* m_group;

  QListWidget* m_shader_list;

  QGroupBox* m_passes_box;
  QGroupBox* m_options_snapshots_box;
  QGroupBox* m_options_box;
  QLabel* m_selected_shader_name;
  QVBoxLayout* m_shader_meta_layout;
  QHBoxLayout* m_shader_buttons_layout;

  QPushButton* m_add_shader;
  QPushButton* m_remove_shader;
};
