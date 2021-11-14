// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QMessageBox>

// Helper for making message boxes modal by default
class ModalMessageBox : public QMessageBox
{
public:
  explicit ModalMessageBox(QWidget* parent, Qt::WindowModality modality = Qt::WindowModal);

  static int critical(QWidget* parent, const QString& title, const QString& text,
                      StandardButtons buttons = Ok, StandardButton default_button = NoButton,
                      Qt::WindowModality modality = Qt::WindowModal);
  static void primehack_initialrun(QWidget* parent);
  static bool primehack_wiitab(QWidget* parent);
  static bool primehack_gctab(QWidget* parent);
  static int information(QWidget* parent, const QString& title, const QString& text,
                         StandardButtons buttons = Ok, StandardButton default_button = NoButton,
                         Qt::WindowModality modality = Qt::WindowModal);
  static int primehackInitial();
  static int question(QWidget* parent, const QString& title, const QString& text,
                      StandardButtons buttons = Yes | No, StandardButton default_button = NoButton,
                      Qt::WindowModality modality = Qt::WindowModal);
  static int warning(QWidget* parent, const QString& title, const QString& text,
                     StandardButtons buttons = Ok, StandardButton default_button = NoButton,
                     Qt::WindowModality modality = Qt::WindowModal);
};
