// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include <QDialog>

class MainWindow;
class QGroupBox;
class QListWidget;
class QListWidgetItem;
class QStackedWidget;

class SettingsWindow final : public QDialog
{
  Q_OBJECT
public:
  explicit SettingsWindow(QWidget* parent = nullptr);
signals:
  void EmulationStarted();
  void EmulationStopped();

public slots:
  void changePage(QListWidgetItem* current, QListWidgetItem* previous);

private:
  void MakeCategoryList();
  void AddCategoryToList(const QString& title, const std::string& icon_name);
  void SetupSettingsWidget();
  QStackedWidget* m_settings_outer;
  QListWidget* m_categories;
};
