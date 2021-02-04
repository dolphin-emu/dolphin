// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDialog>

class QDialogButtonBox;

class FreeLookWindow final : public QDialog
{
  Q_OBJECT
public:
  explicit FreeLookWindow(QWidget* parent);

private:
  void CreateMainLayout();

  QDialogButtonBox* m_button_box;
};
