// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <array>
#include <string>

#include <QDialog>
#include <QVBoxLayout>
#include <QWidget>

#include "Core/Core.h"
#include "Core/IOS/USB/Emulated/Infinity.h"

class QCheckBox;
class QGroupBox;
class QLineEdit;

class InfinityBaseWindow : public QWidget
{
  Q_OBJECT
public:
  explicit InfinityBaseWindow(QWidget* parent = nullptr);
  ~InfinityBaseWindow() override;

protected:
  std::array<QLineEdit*, 7> m_edit_figures;

private:
  void CreateMainWindow();
  void AddFigureSlot(QVBoxLayout* vbox_group, QString name, u8 slot);
  void OnEmulationStateChanged(Core::State state);
  void EmulateBase(bool emulate);
  void ClearFigure(u8 slot);
  void LoadFigure(u8 slot);
  void CreateFigure(u8 slot);
  void LoadFigurePath(u8 slot, const QString& path);

  QCheckBox* m_checkbox;
  QGroupBox* m_group_figures;
};

class CreateFigureDialog : public QDialog
{
  Q_OBJECT

public:
  explicit CreateFigureDialog(QWidget* parent, u8 slot);
  QString GetFilePath() const;

protected:
  QString m_file_path;
};
