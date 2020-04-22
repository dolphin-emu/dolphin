// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/BBAConfigWidget.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

#include "Common/Network.h"

BBAConfigWidget::BBAConfigWidget(QWidget* parent) : QDialog(parent)
{
  setWindowTitle(tr("Broadband Adapter Configuration"));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  auto* vbox_layout = new QVBoxLayout(this);

  {
    auto* form_layout = new QFormLayout;

    {
      auto* hbox_layout = new QHBoxLayout;

      m_mac_addr = new QLineEdit(this);
      m_mac_addr->setPlaceholderText(tr("Leave empty for random"));
      connect(m_mac_addr, &QLineEdit::textChanged, this, &BBAConfigWidget::TextChanged);
      hbox_layout->addWidget(m_mac_addr);

      {
        auto* button = new QToolButton(this);
        button->setText(tr("Randomize"));
        connect(button, &QAbstractButton::pressed, this, &BBAConfigWidget::GenerateMac);
        hbox_layout->addWidget(button);
      }

      form_layout->addRow(tr("MAC address:"), hbox_layout);
    }

    vbox_layout->addLayout(form_layout, 1);
  }

  {
    auto* button_box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Close, this);
    connect(button_box, &QDialogButtonBox::accepted, this, &BBAConfigWidget::Submit);
    connect(button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    vbox_layout->addWidget(button_box, 1);
  }

  setLayout(vbox_layout);
}

QString BBAConfigWidget::MacAddr() const
{
  return m_mac_addr->text();
}

void BBAConfigWidget::SetMacAddr(const QString& mac_addr)
{
  m_mac_addr->setText(mac_addr);
}

void BBAConfigWidget::Submit()
{
  if (!MacAddr().isEmpty() && !Common::StringToMacAddress(MacAddr().toStdString()))
  {
    QMessageBox::warning(this, tr("Invalid MAC address!"), tr("Invalid MAC address!"));
    return;
  }

  accept();
}

void BBAConfigWidget::GenerateMac()
{
  m_mac_addr->setText(QString::fromStdString(
      Common::MacAddressToString(Common::GenerateMacAddress(Common::MACConsumer::BBA))));
}

void BBAConfigWidget::TextChanged(const QString& text)
{
  QString inputMask;
  if (!text.isEmpty() && text != QStringLiteral(":::::"))
    inputMask = QStringLiteral("HH:HH:HH:HH:HH:HH;_");
  if (m_mac_addr->inputMask() != inputMask)
    m_mac_addr->setInputMask(inputMask);
}
