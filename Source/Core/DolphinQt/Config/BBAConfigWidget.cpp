// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/BBAConfigWidget.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QToolButton>
#include <QLabel>
#include <QSpinBox>
#include <QDialogButtonBox>
#include <QMessageBox>

#include "Common/Network.h"

BBAConfigWidget::BBAConfigWidget(bool showServer, QWidget* parent) :
    QDialog(parent),
    m_server(nullptr),
    m_port(nullptr)
{
  setWindowTitle(tr("Broadband Adapter Configuration"));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  auto vbox_layout = new QVBoxLayout(this);

  {
    auto form_layout = new QFormLayout;

    {
      auto hbox_layout = new QHBoxLayout;

      m_mac_addr = new QLineEdit(this);
      m_mac_addr->setPlaceholderText(tr("Leave empty for random"));
      connect(m_mac_addr, &QLineEdit::textChanged, this, &BBAConfigWidget::TextChanged);
      hbox_layout->addWidget(m_mac_addr);

      {
        auto button = new QToolButton(this);
        button->setText(tr("Randomize"));
        connect(button, &QAbstractButton::pressed, this, &BBAConfigWidget::GenerateMac);
        hbox_layout->addWidget(button);
      }

      form_layout->addRow(tr("MAC address:"), hbox_layout);
    }

    if (showServer)
    {
      auto hboxLayout = new QHBoxLayout;

      m_server = new QLineEdit(this);
      hboxLayout->addWidget(m_server);

      auto portLabel = new QLabel(tr("Port:"), this);
      hboxLayout->addWidget(portLabel);

      m_port = new QSpinBox(this);
      portLabel->setBuddy(m_port);
      m_port->setRange(std::numeric_limits<quint16>::min(), std::numeric_limits<quint16>::max());
      hboxLayout->addWidget(m_port);

      form_layout->addRow(tr("Server:"), hboxLayout);
    }

    vbox_layout->addLayout(form_layout, 1);
  }

  {
    auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Close, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &BBAConfigWidget::Submit);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    vbox_layout->addWidget(buttonBox, 1);
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

QString BBAConfigWidget::Server() const
{
  return m_server ? m_server->text() : QString();
}

void BBAConfigWidget::SetServer(const QString& server)
{
  m_server->setText(server);
}

quint16 BBAConfigWidget::Port() const
{
  return m_port ? m_port->value() : 0;
}

void BBAConfigWidget::SetPort(quint16 port)
{
  m_port->setValue(port);
}

void BBAConfigWidget::Submit()
{
  if(!MacAddr().isEmpty() && !Common::StringToMacAddress(MacAddr().toStdString()))
  {
    QMessageBox::warning(this, tr("Invalid MAC address!"), tr("Invalid MAC address!"));
    return;
  }

  accept();
}

void BBAConfigWidget::GenerateMac()
{
  m_mac_addr->setText(QString::fromStdString(Common::MacAddressToString(Common::GenerateMacAddress(Common::MACConsumer::BBA))));
}

void BBAConfigWidget::TextChanged(const QString& text)
{
  QString inputMask;
  if(!text.isEmpty() && text != QStringLiteral(":::::"))
    inputMask = QStringLiteral("HH:HH:HH:HH:HH:HH;_");
  if (m_mac_addr->inputMask() != inputMask)
    m_mac_addr->setInputMask(inputMask);
}
