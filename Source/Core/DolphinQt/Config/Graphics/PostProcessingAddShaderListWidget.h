// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDir>
#include <QString>
#include <QWidget>

#include <vector>

class QComboBox;
class QLineEdit;
class QListWidget;
class QVBoxLayout;

class PostProcessingAddShaderListWidget final : public QWidget
{
  Q_OBJECT
public:
  PostProcessingAddShaderListWidget(QWidget* parent, const QString& root_path,
                                    bool allows_drag_drop);

  std::vector<QString> ChosenShaderPathes() const { return m_chosen_shader_pathes; }

private:
  void CreateWidgets();
  void ConnectWidgets();

  void BuildShaderCategories();

  void OnShadersSelected();

  void OnShaderTypeChanged();
  void OnSearchTextChanged(QString search_text);
  static std::vector<std::string> GetAvailableShaders(const std::string& directory_path);

  void dragEnterEvent(QDragEnterEvent* event) override;
  void dropEvent(QDropEvent* event) override;

  QVBoxLayout* m_main_layout;
  QListWidget* m_shader_list;
  QComboBox* m_shader_type;
  QLineEdit* m_search_text;

  QDir m_root_path;
  bool m_allows_drag_drop;
  std::vector<QString> m_chosen_shader_pathes;
};
