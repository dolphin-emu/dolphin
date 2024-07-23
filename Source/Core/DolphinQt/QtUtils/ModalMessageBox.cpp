// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/QtUtils/ModalMessageBox.h"

#include <QApplication>

#include "DolphinQt/QtUtils/SetWindowDecorations.h"

ModalMessageBox::ModalMessageBox(const QWidget* parent, const Qt::WindowModality modality)
    : QMessageBox(parent != nullptr ? parent->window() : nullptr)
{
  setWindowModality(modality);
  setWindowFlags(Qt::Sheet | Qt::WindowTitleHint | Qt::CustomizeWindowHint);

  // No parent is still preferable to showing a hidden parent here.
  if (parent != nullptr && !parent->window()->isVisible())
    setParent(nullptr);
}

static inline int ExecMessageBox(const ModalMessageBox::Icon icon, const QWidget* parent, const QString& title,
                                 const QString& text, const ModalMessageBox::StandardButtons buttons,
                                 const ModalMessageBox::StandardButton default_button,
                                 const Qt::WindowModality modality)
{
  ModalMessageBox msg(parent, modality);
  msg.setIcon(icon);
  msg.setWindowTitle(title);
  msg.setText(text);
  msg.setStandardButtons(buttons);
  msg.setDefaultButton(default_button);

  SetQWidgetWindowDecorations(&msg);
  return msg.exec();
}

int ModalMessageBox::critical(const QWidget* parent, const QString& title, const QString& text,
                              const StandardButtons buttons, const StandardButton default_button,
                              const Qt::WindowModality modality)
{
  return ExecMessageBox(Critical, parent, title, text, buttons, default_button,
                        modality);
}

int ModalMessageBox::information(const QWidget* parent, const QString& title, const QString& text,
                                 const StandardButtons buttons, const StandardButton default_button,
                                 const Qt::WindowModality modality)
{
  return ExecMessageBox(Information, parent, title, text, buttons, default_button,
                        modality);
}

int ModalMessageBox::question(const QWidget* parent, const QString& title, const QString& text,
                              const StandardButtons buttons, const StandardButton default_button,
                              const Qt::WindowModality modality)
{
  return ExecMessageBox(Warning, parent, title, text, buttons, default_button,
                        modality);
}

int ModalMessageBox::warning(const QWidget* parent, const QString& title, const QString& text,
                             const StandardButtons buttons, const StandardButton default_button,
                             const Qt::WindowModality modality)
{
  return ExecMessageBox(Warning, parent, title, text, buttons, default_button,
                        modality);
}
