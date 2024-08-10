// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QMessageBox>

// Helper for making message boxes modal by default
class ModalMessageBox : public QMessageBox
{
public:
  explicit ModalMessageBox(const QWidget* parent, Qt::WindowModality modality = Qt::WindowModal);

  static int critical(const QWidget* parent, const QString& title, const QString& text,
                      StandardButtons buttons = Ok, StandardButton default_button = NoButton,
                      Qt::WindowModality modality = Qt::WindowModal);
  static int information(const QWidget* parent, const QString& title, const QString& text,
                         StandardButtons buttons = Ok, StandardButton default_button = NoButton,
                         Qt::WindowModality modality = Qt::WindowModal);
  static int question(const QWidget* parent, const QString& title, const QString& text,
                      StandardButtons buttons = Yes | No, StandardButton default_button = NoButton,
                      Qt::WindowModality modality = Qt::WindowModal);
  static int warning(const QWidget* parent, const QString& title, const QString& text,
                     StandardButtons buttons = Ok, StandardButton default_button = NoButton,
                     Qt::WindowModality modality = Qt::WindowModal);
};
