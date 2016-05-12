#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

#include "Core/ConfigManager.h"
#include "DolphinQt2/Config/Pages/GeneralPage.h"

GeneralPage::GeneralPage(QWidget* parent)
	: QWidget(parent)
{
	m_main_layout = new QVBoxLayout;

	BuildOptions();
	LoadConfig();
	ConnectOptions();

	m_main_layout->addStretch(1);
	setLayout(m_main_layout);
}

void GeneralPage::LoadConfig()
{
	const SConfig& startup_params = SConfig::GetInstance();

	m_checkbox_enable_cheats->setChecked(startup_params.bEnableCheats);
	m_checkbox_force_ntsc->setChecked(startup_params.bForceNTSCJ);

	u32 selection = qRound(startup_params.m_EmulationSpeed * 10.0f);
	if (selection < (u32)m_checkbox_speed_limit->count())
		m_checkbox_speed_limit->setCurrentIndex(selection);
}

void GeneralPage::SaveConfig()
{
	SConfig::GetInstance().bEnableCheats = m_checkbox_enable_cheats->isChecked();
	SConfig::GetInstance().bForceNTSCJ = m_checkbox_force_ntsc->isChecked();
	SConfig::GetInstance().m_EmulationSpeed = m_checkbox_speed_limit->currentIndex() * 0.1f;
}

void GeneralPage::BuildBasicSettings()
{
	QGroupBox* basic_group = new QGroupBox(tr("Basic Settings"));
	QVBoxLayout* basic_group_layout = new QVBoxLayout;
	basic_group->setLayout(basic_group_layout);
	m_main_layout->addWidget(basic_group);

	m_combobox_language = new QComboBox;
	QHBoxLayout* language_layout = new QHBoxLayout;

	QLabel* language_label = new QLabel(tr("Language:"));
	language_label->setMaximumWidth(150);

	// TODO: Support more languages other then English
	m_combobox_language->addItem(tr("English"));
	m_checkbox_enable_cheats = new QCheckBox(tr("Enable Cheats"));

	language_layout->addWidget(language_label);
	language_layout->addWidget(m_combobox_language);

	basic_group_layout->addLayout(language_layout);
	basic_group_layout->addWidget(m_checkbox_enable_cheats);
}

void GeneralPage::BuildAdvancedSettings()
{
	QGroupBox* advanced_group = new QGroupBox(tr("Advanced Settings"));
	QVBoxLayout* advanced_group_layout = new QVBoxLayout;
	advanced_group->setLayout(advanced_group_layout);
	m_main_layout->addWidget(advanced_group);

	m_checkbox_force_ntsc = new QCheckBox(tr("Force Console as NTSC-J"));

	QHBoxLayout* speed_limit_layout = new QHBoxLayout;
	QLabel* speed_limit_label = new QLabel(tr("Speed Limit:"));

	speed_limit_label->setMaximumWidth(150);
	m_checkbox_speed_limit = new QComboBox;

	m_checkbox_speed_limit->addItem(tr("Unlimited"));
	for (int i = 10; i <= 200; i += 10) // from 10% to 200%
	{
		QString str;
		if (i == 100)
			str.sprintf("%i%% (Normal Speed)",i);
		else
			str.sprintf("%i%%",i);

		m_checkbox_speed_limit->addItem(str);
	}

	speed_limit_layout->addWidget(speed_limit_label);
	speed_limit_layout->addWidget(m_checkbox_speed_limit);

	advanced_group_layout->addLayout(speed_limit_layout);
	advanced_group_layout->addWidget(m_checkbox_force_ntsc);
}

void GeneralPage::BuildOptions()
{
	BuildBasicSettings();
	BuildAdvancedSettings();
}

void GeneralPage::ConnectOptions()
{
	// Basic
	connect(m_checkbox_enable_cheats, &QCheckBox::clicked, this, &GeneralPage::SaveConfig);
	connect(m_combobox_language, static_cast<void(QComboBox::*)(int)>(&QComboBox::activated),
			[=](int index){GeneralPage::SaveConfig();});

	// Advanced
	connect(m_checkbox_force_ntsc, &QCheckBox::clicked, this, &GeneralPage::SaveConfig);
	connect(m_checkbox_speed_limit, static_cast<void(QComboBox::*)(int)>(&QComboBox::activated),
			[=](int index){GeneralPage::SaveConfig();});
}
