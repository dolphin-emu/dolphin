// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDialog>

class QComboBox;
class QRadioButton;

class GCMemcardCreateNewDialog : public QDialog
{
  Q_OBJECT
public:
  explicit GCMemcardCreateNewDialog(QWidget* parent = nullptr);
  ~GCMemcardCreateNewDialog();

  std::string GetMemoryCardPath() const;

private:
  bool CreateCard();

  QComboBox* m_combobox_size;
  QRadioButton* m_radio_western;
  QRadioButton* m_radio_shiftjis;
  std::string m_card_path;
};
