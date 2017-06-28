// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <wobjectdefs.h>
#include <string>

#include <QDialog>

class MainWindow;
class QGroupBox;
class QListWidget;
class QListWidgetItem;
class QStackedWidget;

class SettingsWindow final : public QDialog
{
  W_OBJECT(SettingsWindow)
public:
  explicit SettingsWindow(QWidget* parent = nullptr);
  void EmulationStarted() W_SIGNAL(EmulationStarted);
  void EmulationStopped() W_SIGNAL(EmulationStopped);

public:
  void changePage(QListWidgetItem* current, QListWidgetItem* previous); W_SLOT(changePage, (QListWidgetItem*, QListWidgetItem*));

private:
  void MakeCategoryList();
  void AddCategoryToList(const QString& title, const std::string& icon_name);
  void SetupSettingsWidget();
  QStackedWidget* m_settings_outer;
  QListWidget* m_categories;
};
