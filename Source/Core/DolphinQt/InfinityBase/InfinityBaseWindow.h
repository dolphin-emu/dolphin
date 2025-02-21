// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>

#include <QDialog>
#include <QWidget>

#include "Common/CommonTypes.h"

class QCheckBox;
class QGroupBox;
class QLineEdit;
class QVBoxLayout;

namespace Core
{
enum class State;
}

namespace IOS::HLE::USB
{
enum class FigureUIPosition : u8;
}

class InfinityBaseWindow : public QWidget
{
  Q_OBJECT
public:
  explicit InfinityBaseWindow(QWidget* parent = nullptr);
  ~InfinityBaseWindow() override;

protected:
  std::array<QLineEdit*, 9> m_edit_figures;

private:
  void CreateMainWindow();
  void AddFigureSlot(QVBoxLayout* vbox_group, QString name, IOS::HLE::USB::FigureUIPosition slot);
  void OnEmulationStateChanged(Core::State state);
  void EmulateBase(bool emulate);
  void ClearFigure(IOS::HLE::USB::FigureUIPosition slot);
  void LoadFigure(IOS::HLE::USB::FigureUIPosition slot);
  void CreateFigure(IOS::HLE::USB::FigureUIPosition slot);
  void LoadFigurePath(IOS::HLE::USB::FigureUIPosition slot, const QString& path);

  QCheckBox* m_checkbox;
  QGroupBox* m_group_figures;
};

class CreateFigureDialog : public QDialog
{
  Q_OBJECT

public:
  explicit CreateFigureDialog(QWidget* parent, IOS::HLE::USB::FigureUIPosition slot);
  QString GetFilePath() const;

protected:
  QString m_file_path;
};
