// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/QtUtils/UTF8CodePointCountValidator.h"

#include "Common/StringUtil.h"

UTF8CodePointCountValidator::UTF8CodePointCountValidator(int max_count, QObject* parent)
    : QValidator(parent), m_max_count(max_count)
{
}

QValidator::State UTF8CodePointCountValidator::validate(QString& input, int& pos) const
{
  if (StringUTF8CodePointCount(input.toStdString()) > m_max_count)
    return QValidator::Invalid;

  return QValidator::Acceptable;
}
