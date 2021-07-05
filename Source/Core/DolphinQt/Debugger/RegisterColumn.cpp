// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Debugger/RegisterColumn.h"

#include <cstring>
#include <utility>

#include "DolphinQt/QtUtils/ModalMessageBox.h"

RegisterColumn::RegisterColumn(RegisterType type, std::function<u64()> get,
                               std::function<void(u64)> set)
    : m_type(type), m_get_register(std::move(get)), m_set_register(std::move(set))
{
  RefreshValue();
  Update();

  setFlags(m_set_register == nullptr ? flags() ^ Qt::ItemIsEditable :
                                       Qt::ItemIsEditable | Qt::ItemIsEnabled);
  setData(DATA_TYPE, static_cast<quint32>(type));
}

RegisterDisplay RegisterColumn::GetDisplay() const
{
  return m_display;
}

void RegisterColumn::SetDisplay(RegisterDisplay display)
{
  m_display = display;
  Update();
}

void RegisterColumn::RefreshValue()
{
  QBrush brush = QPalette().brush(QPalette::Text);

  if (m_value != m_get_register())
  {
    m_value = m_get_register();
    brush.setColor(Qt::red);
  }

  setForeground(brush);

  Update();
}

u64 RegisterColumn::GetValue() const
{
  return m_value;
}

void RegisterColumn::SetValue()
{
  u64 value = 0;

  bool valid = false;

  switch (m_display)
  {
  case RegisterDisplay::Hex:
    value = text().toULongLong(&valid, 16);
    break;
  case RegisterDisplay::SInt32:
    value = text().toInt(&valid);
    break;
  case RegisterDisplay::UInt32:
    value = text().toUInt(&valid);
    break;
  case RegisterDisplay::Float:
  {
    float f = text().toFloat(&valid);
    std::memcpy(&value, &f, sizeof(u32));
    break;
  }
  case RegisterDisplay::Double:
  {
    double f = text().toDouble(&valid);
    std::memcpy(&value, &f, sizeof(u64));
    break;
  }
  }

  if (!valid)
    ModalMessageBox::critical(nullptr, QObject::tr("Error"), QObject::tr("Invalid input provided"));
  else
    m_set_register(value);

  RefreshValue();
}

void RegisterColumn::Update()
{
  QString text;

  switch (m_display)
  {
  case RegisterDisplay::Hex:
    text = QStringLiteral("%1").arg(m_value,
                                    (m_type == RegisterType::ibat || m_type == RegisterType::dbat ||
                                             m_type == RegisterType::fpr ||
                                             m_type == RegisterType::tb ?
                                         sizeof(u64) :
                                         sizeof(u32)) *
                                        2,
                                    16, QLatin1Char('0'));
    break;
  case RegisterDisplay::SInt32:
    text = QString::number(static_cast<qint32>(m_value));
    break;
  case RegisterDisplay::UInt32:
    text = QString::number(static_cast<quint32>(m_value));
    break;
  case RegisterDisplay::Float:
  {
    float tmp;
    std::memcpy(&tmp, &m_value, sizeof(float));
    text = QString::number(tmp);
    break;
  }
  case RegisterDisplay::Double:
  {
    double tmp;
    std::memcpy(&tmp, &m_value, sizeof(double));
    text = QString::number(tmp);
    break;
  }
  }

  setText(text);
}
