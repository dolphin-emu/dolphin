// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt2/Settings/GeneralPane.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>
#include <QWidget>

#include "Core/Analytics.h"
#include "Core/ConfigManager.h"
#include "DolphinQt2/Settings.h"

GeneralPane::GeneralPane(QWidget* parent) : QWidget(parent)
{
  CreateLayout();
  ConnectLayout();

  LoadConfig();
}

void GeneralPane::CreateLayout()
{
  m_main_layout = new QVBoxLayout;
  // Create layout here
  CreateBasic();

#if defined(USE_ANALYTICS) && USE_ANALYTICS
  CreateAnalytics();
#endif

  CreateAdvanced();

  m_main_layout->addStretch(1);
  setLayout(m_main_layout);
}

void GeneralPane::ConnectLayout()
{
  connect(m_combobox_language, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated),
          [this](int index) { OnSaveConfig(); });

  // Advanced
  connect(m_checkbox_force_ntsc, &QCheckBox::clicked, this, &GeneralPane::OnSaveConfig);
  connect(m_slider_speedlimit, static_cast<void (QSlider::*)(int)>(&QSlider::valueChanged),
          [this](int index) { OnSaveConfig(); });

#if defined(USE_ANALYTICS) && USE_ANALYTICS
  connect(m_checkbox_enable_analytics, &QCheckBox::clicked, this, &GeneralPane::OnSaveConfig);
  connect(m_button_generate_new_identity, &QPushButton::clicked, this,
          &GeneralPane::GenerateNewIdentity);
#endif
}

void GeneralPane::CreateBasic()
{
  auto* basic_group = new QGroupBox(tr("Basic Settings"));
  auto* basic_group_layout = new QVBoxLayout;
  basic_group->setLayout(basic_group_layout);
  m_main_layout->addWidget(basic_group);

  auto* language_layout = new QFormLayout;
  basic_group_layout->addLayout(language_layout);

  m_combobox_language = new QComboBox;
  // TODO: Support more languages other then English
  m_combobox_language->addItem(tr("English"));
  language_layout->addRow(tr("&Language:"), m_combobox_language);
}

#if defined(USE_ANALYTICS) && USE_ANALYTICS
void GeneralPane::CreateAnalytics()
{
  auto* analytics_group = new QGroupBox(tr("Usage Statistics Reporting"));
  auto* analytics_group_layout = new QVBoxLayout;
  analytics_group->setLayout(analytics_group_layout);
  m_main_layout->addWidget(analytics_group);

  m_checkbox_enable_analytics = new QCheckBox(tr("Enable Usage Statistics Reporting"));
  m_button_generate_new_identity = new QPushButton(tr("Generate a New Statistics Identity"));
  analytics_group_layout->addWidget(m_checkbox_enable_analytics);
  analytics_group_layout->addWidget(m_button_generate_new_identity);
}
#endif

void GeneralPane::CreateAdvanced()
{
  auto* advanced_group = new QGroupBox(tr("Advanced Settings"));
  auto* advanced_group_layout = new QVBoxLayout;
  advanced_group->setLayout(advanced_group_layout);
  m_main_layout->addWidget(advanced_group);

  // Speed Limit
  auto* speed_limit_layout = new QFormLayout;
  auto* speed_limit_container = new QHBoxLayout;
  speed_limit_container->addLayout(speed_limit_layout);
  advanced_group_layout->addLayout(speed_limit_container);

  m_slider_speedlimit = new QSlider(Qt::Orientation::Horizontal);
  m_slider_speedlimit->setTickInterval(1);
  m_slider_speedlimit->setMinimum(1);
  m_slider_speedlimit->setMaximum(21);
  m_slider_speedlimit->setTickPosition(QSlider::TicksBelow);
  m_slider_speedlimit->setSingleStep(1);
  speed_limit_layout->addRow(tr("&Speed Limit:"), m_slider_speedlimit);

  m_label_speedlimit = new QLabel(tr("Unlimited"));
  m_label_speedlimit->setMinimumWidth(48);
  m_label_speedlimit->setAlignment(Qt::AlignRight | Qt::AlignCenter);
  speed_limit_container->addWidget(m_label_speedlimit);

  // NTSC-J
  m_checkbox_force_ntsc = new QCheckBox(tr("Force Console as NTSC-J"));
  advanced_group_layout->addWidget(m_checkbox_force_ntsc);
}

void GeneralPane::LoadConfig()
{
  m_checkbox_force_ntsc->setChecked(Settings().GetForceNTSCJ());
  m_checkbox_enable_analytics->setChecked(Settings().GetAnalyticsEnabled());

  int selection = qRound(Settings().GetEmulationSpeed() * 10);
  if (selection < m_slider_speedlimit->maximum())
  {
    if (selection == 0)
    {
      m_slider_speedlimit->setValue(21);
      m_slider_speedlimit->setToolTip(tr("Unlimited"));
      m_label_speedlimit->setText(tr("Unlimited"));
    }
    else
    {
      m_slider_speedlimit->setValue(selection);

      QString val = QString::fromStdString(std::to_string(m_slider_speedlimit->value() * 10)) +
                    QStringLiteral("%");
      m_slider_speedlimit->setToolTip(val);
      m_label_speedlimit->setText(val);
    }
  }
}

void GeneralPane::OnSaveConfig()
{
  Settings().SetForceNTSCJ(m_checkbox_force_ntsc->isChecked());
  Settings().SetAnalyticsEnabled(m_checkbox_enable_analytics->isChecked());
  if (m_slider_speedlimit->value() < 21)
  {
    Settings().SetEmulationSpeed(m_slider_speedlimit->value() * 0.1f);
    QString val = QString::fromStdString(std::to_string(m_slider_speedlimit->value() * 10)) +
                  QStringLiteral("%");
    m_slider_speedlimit->setToolTip(val);
    m_label_speedlimit->setText(val);
  }
  else
  {
    Settings().SetEmulationSpeed(0);
    m_slider_speedlimit->setToolTip(tr("Unlimited"));
    m_label_speedlimit->setText(tr("Unlimited"));
  }
}

#if defined(USE_ANALYTICS) && USE_ANALYTICS
void GeneralPane::GenerateNewIdentity()
{
  DolphinAnalytics::Instance()->GenerateNewIdentity();
  QMessageBox message_box;
  message_box.setIcon(QMessageBox::Information);
  message_box.setWindowTitle(tr("Identity Generation"));
  message_box.setText(tr("New identity generated."));
  message_box.exec();
}
#endif
