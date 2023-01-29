// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <vector>

#include <QDialog>
#include <QString>

class PostProcessingAddShaderListWidget;
class QDialogButtonBox;
class QTabWidget;
class QVBoxLayout;

class PostProcessingAddShaderDialog final : public QDialog
{
  Q_OBJECT
public:
  explicit PostProcessingAddShaderDialog(QWidget* parent);

  std::vector<QString> ChosenUserShaderPathes() const;
  std::vector<QString> ChosenSystemShaderPathes() const;

private:
  void CreateWidgets();

  QDialogButtonBox* m_buttonbox;
  QTabWidget* m_shader_tabs;
  QVBoxLayout* m_main_layout;
  PostProcessingAddShaderListWidget* m_system_shader_list;
  PostProcessingAddShaderListWidget* m_user_shader_list;
};
