// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/BBAServerWindow.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QTime>
#include <QScrollBar>

#include <limits>

BBAServerWindow::BBAServerWindow(QWidget *parent) :
  QDialog(parent),
  m_server(this)
{
  auto vbox_layout = new QVBoxLayout(this);

  {
    auto hbox_layout = new QHBoxLayout;

    auto host_addr_label = new QLabel(tr("Host address:"), this);
    hbox_layout->addWidget(host_addr_label);

    m_host_addr = new QLineEdit(this);
    host_addr_label->setBuddy(m_host_addr);
    m_host_addr->setPlaceholderText(tr("Leave empty for Any"));
    hbox_layout->addWidget(m_host_addr);

    auto port_label = new QLabel(tr("Port:"), this);
    hbox_layout->addWidget(port_label);

    m_port = new QSpinBox(this);
    port_label->setBuddy(m_port);
    m_port->setRange(std::numeric_limits<quint16>::min(), std::numeric_limits<quint16>::max());
    m_port->setValue(50000);
    hbox_layout->addWidget(m_port);

    m_toggle = new QPushButton(this);
    connect(m_toggle, &QAbstractButton::pressed, this, &BBAServerWindow::Toggle);
    hbox_layout->addWidget(m_toggle);

    vbox_layout->addLayout(hbox_layout);
  }

  m_log_output = new QPlainTextEdit(this);
  m_log_output->setLineWrapMode(QPlainTextEdit::NoWrap);
  m_log_output->setReadOnly(true);
  vbox_layout->addWidget(m_log_output, 1);

  {
    auto button_box = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    vbox_layout->addWidget(button_box, 1);
  }

  setLayout(vbox_layout);

  connect(&m_server, &BBAServer::Information, this, &BBAServerWindow::LogOutput);

  Update();
}

void BBAServerWindow::Toggle()
{
  if (m_server.IsListening())
  {
    m_server.Close();
  }
  else
  {
    if (!m_server.Listen(m_host_addr->text(), m_port->value()))
      QMessageBox::warning(this, tr("Could not start listening!"), tr("Could not start listening:\n\n%0").arg(m_server.ErrorString()));
  }

  Update();
}

void BBAServerWindow::LogOutput(const QDateTime &timestamp, const QString &log_line)
{
  m_log_output->appendPlainText(QStringLiteral("%0: %1").arg(timestamp.toString(QStringLiteral("HH:mm:ss.zzz")), log_line));
  auto scrollBar = m_log_output->verticalScrollBar();
  scrollBar->setValue(scrollBar->maximum());
}

void BBAServerWindow::Update()
{
  const auto is_listening = m_server.IsListening();

  m_host_addr->setEnabled(!is_listening);
  m_port->setEnabled(!is_listening);

  m_toggle->setText(is_listening ? tr("Stop") : tr("Start"));
}
