// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QWidget>

class QLineEdit;
class QPushButton;

class SearchBar : public QWidget
{
  Q_OBJECT
public:
  explicit SearchBar(QWidget* parent = nullptr);

  void Show();
  void Hide();

signals:
  void Search(const QString& serach);

private:
  void CreateWidgets();
  void ConnectWidgets();

  bool eventFilter(QObject* object, QEvent* event) final override;

  QLineEdit* m_search_edit;
  QPushButton* m_close_button;
};
