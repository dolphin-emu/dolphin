// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDialog>

class QLineEdit;
class QSpinBox;

class BBAConfigWidget : public QDialog
{
  Q_OBJECT

public:
  explicit BBAConfigWidget(QWidget* parent = nullptr);

  QString MacAddr() const;
  void SetMacAddr(const QString& mac_addr);

private slots:
  void Submit();
  void GenerateMac();
  void TextChanged(const QString& text);

private:
  QLineEdit* m_mac_addr;
};
