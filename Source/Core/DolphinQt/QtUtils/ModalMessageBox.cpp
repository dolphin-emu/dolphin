// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/QtUtils/ModalMessageBox.h"

#include <QApplication>

#include "DolphinQt/QtUtils/SetWindowDecorations.h"

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
                                 Qt::WindowModality modality, QString detailed_text)
{
  ModalMessageBox msg(parent, modality);
  msg.setIcon(icon);
  msg.setWindowTitle(title);
  msg.setText(text);
  msg.setStandardButtons(buttons);
  msg.setDefaultButton(default_button);
  msg.setDetailedText(detailed_text);

  SetQWidgetWindowDecorations(&msg);
  return msg.exec();
}

int ModalMessageBox::critical(QWidget* parent, const QString& title, const QString& text,
                              StandardButtons buttons, StandardButton default_button,
                              Qt::WindowModality modality, const QString& detailedText)
{
  return ExecMessageBox(QMessageBox::Critical, parent, title, text, buttons, default_button,
                        modality, detailedText);
}

int ModalMessageBox::information(QWidget* parent, const QString& title, const QString& text,
                                 StandardButtons buttons, StandardButton default_button,
                                 Qt::WindowModality modality, const QString& detailedText)
{
  return ExecMessageBox(QMessageBox::Information, parent, title, text, buttons, default_button,
                        modality, detailedText);
}

int ModalMessageBox::question(QWidget* parent, const QString& title, const QString& text,
                              StandardButtons buttons, StandardButton default_button,
                              Qt::WindowModality modality, const QString& detailedText)
{
  return ExecMessageBox(QMessageBox::Warning, parent, title, text, buttons, default_button,
                        modality, detailedText);
}

int ModalMessageBox::warning(QWidget* parent, const QString& title, const QString& text,
                             StandardButtons buttons, StandardButton default_button,
                             Qt::WindowModality modality, const QString& detailedText)
{
  return ExecMessageBox(QMessageBox::Warning, parent, title, text, buttons, default_button,
                        modality, detailedText);
}
