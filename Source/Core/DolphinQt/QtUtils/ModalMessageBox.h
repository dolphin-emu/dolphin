// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QMessageBox>

// Helper for making message boxes modal by default
class ModalMessageBox : public QMessageBox
{
public:
  explicit ModalMessageBox(QWidget* parent);

  static int critical(QWidget* parent, const QString& title, const QString& text,
                      StandardButtons buttons = Ok, StandardButton default_button = NoButton);
  static int information(QWidget* parent, const QString& title, const QString& text,
                         StandardButtons buttons = Ok, StandardButton default_button = NoButton);
  static int question(QWidget* parent, const QString& title, const QString& text,
                      StandardButtons buttons = Yes | No, StandardButton default_button = NoButton);
  static int warning(QWidget* parent, const QString& title, const QString& text,
                     StandardButtons buttons = Ok, StandardButton default_button = NoButton);
};
