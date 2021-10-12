// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/QtUtils/ModalMessageBox.h"

#include <QApplication>

ModalMessageBox::ModalMessageBox(QWidget* parent, Qt::WindowModality modality)
    : QMessageBox(parent != nullptr ? parent->window() : nullptr)
{
  setWindowModality(modality);
  setWindowFlags(Qt::Sheet | Qt::WindowTitleHint | Qt::CustomizeWindowHint);

  // No parent is still preferable to showing a hidden parent here.
  if (parent != nullptr && !parent->window()->isVisible())
    setParent(nullptr);
}

static inline int ExecMessageBox(ModalMessageBox::Icon icon, QWidget* parent, const QString& title,
                                 const QString& text, ModalMessageBox::StandardButtons buttons,
                                 ModalMessageBox::StandardButton default_button,
                                 Qt::WindowModality modality)
{
  ModalMessageBox msg(parent, modality);
  msg.setIcon(icon);
  msg.setWindowTitle(title);
  msg.setText(text);
  msg.setStandardButtons(buttons);
  msg.setDefaultButton(default_button);

  return msg.exec();
}

int ModalMessageBox::critical(QWidget* parent, const QString& title, const QString& text,
                              StandardButtons buttons, StandardButton default_button,
                              Qt::WindowModality modality)
{
  return ExecMessageBox(QMessageBox::Critical, parent, title, text, buttons, default_button,
                        modality);
}

int ModalMessageBox::information(QWidget* parent, const QString& title, const QString& text,
                                 StandardButtons buttons, StandardButton default_button,
                                 Qt::WindowModality modality)
{
  return ExecMessageBox(QMessageBox::Information, parent, title, text, buttons, default_button,
                        modality);
}

int ModalMessageBox::question(QWidget* parent, const QString& title, const QString& text,
                              StandardButtons buttons, StandardButton default_button,
                              Qt::WindowModality modality)
{
  return ExecMessageBox(QMessageBox::Warning, parent, title, text, buttons, default_button,
                        modality);
}

int ModalMessageBox::warning(QWidget* parent, const QString& title, const QString& text,
                             StandardButtons buttons, StandardButton default_button,
                             Qt::WindowModality modality)
{
  return ExecMessageBox(QMessageBox::Warning, parent, title, text, buttons, default_button,
                        modality);
}
