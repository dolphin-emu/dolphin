// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QWidget>

class QListWidget;
class QStackedWidget;

class ListTabWidget : public QWidget
{
public:
  ListTabWidget();
  int addTab(QWidget* page, const QString& label);
  void setTabIcon(int index, const QIcon& icon);
  void setCurrentIndex(int index);

private:
  QListWidget* m_labels;
  QStackedWidget* m_display;
};
