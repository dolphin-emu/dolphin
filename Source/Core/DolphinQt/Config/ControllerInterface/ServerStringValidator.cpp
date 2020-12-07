// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/ControllerInterface/ServerStringValidator.h"

ServerStringValidator::ServerStringValidator(QObject* parent) : QValidator(parent)
{
}

QValidator::State ServerStringValidator::validate(QString& input, int& pos) const
{
  if (input.isEmpty())
    return Invalid;

  if (input.contains(QStringLiteral(":")))
    return Invalid;

  if (input.contains(QStringLiteral(";")))
    return Invalid;

  return Acceptable;
}
