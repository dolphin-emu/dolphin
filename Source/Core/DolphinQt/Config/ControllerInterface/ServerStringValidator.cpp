// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/ControllerInterface/ServerStringValidator.h"

#include <string>

#include "DolphinQt/Config/ControllerInterface/DualShockUDPSettings.h"

ServerStringValidator::ServerStringValidator(QObject* parent) : QValidator(parent)
{
}

QValidator::State ServerStringValidator::validate(QString& input, int& pos) const
{
  if (input.isEmpty())
    return Invalid;

  if (input.contains(QString::fromStdString(std::string{DualShockUDPSettings::FIELD_SEPARATOR})))
    return Invalid;

  if (input.contains(QString::fromStdString(std::string{DualShockUDPSettings::SERVER_SEPARATOR})))
    return Invalid;

  return Acceptable;
}
