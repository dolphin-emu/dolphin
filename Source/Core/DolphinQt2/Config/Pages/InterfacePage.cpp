#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

#include "Common/CommonPaths.h"
#include "Common/FileSearch.h"
#include "Common/FileUtil.h"
#include "Core/ConfigManager.h"
#include "DolphinQt2/Settings.h"
#include "DolphinQt2/Config/Pages/InterfacePage.h"


InterfacePage::InterfacePage(QWidget* parent)
	: QWidget(parent)
{
	m_mainLayout = new QVBoxLayout;
	BuildOptions();
	ConnectOptions();
	LoadConfig();
	m_mainLayout->addStretch(1);
	setLayout(m_mainLayout);
}

void InterfacePage::LoadConfig()
{
	const SConfig& startup_params = SConfig::GetInstance();

	// UI Options
	m_checkbox_auto_adjust_window->setChecked(startup_params.bRenderWindowAutoSize);
	m_checkbox_keep_dolphin_on_top->setChecked(startup_params.bKeepWindowOnTop);
	m_checkbox_render_to_main_window->setChecked(startup_params.bRenderToMain);
	m_combobox_theme_selector->setCurrentIndex(m_combobox_theme_selector->findText(QString::fromStdString(SConfig::GetInstance().theme_name)));

	// In Game Options
	m_checkbox_confirm_on_stop->setChecked(startup_params.bConfirmStop);
	m_checkbox_use_panic_handlers->setChecked(startup_params.bUsePanicHandlers);
	m_checkbox_enable_osd->setChecked(startup_params.bOnScreenDisplayMessages);
	m_checkbox_pause_on_focus_lost->setChecked(startup_params.m_PauseOnFocusLost);
	m_checkbox_hide_mouse->setChecked(startup_params.bAutoHideCursor);
}

void InterfacePage::SaveConfig()
{
	// UI Options
	SConfig::GetInstance().bRenderWindowAutoSize = m_checkbox_auto_adjust_window->isChecked();
	SConfig::GetInstance().bKeepWindowOnTop = m_checkbox_keep_dolphin_on_top->isChecked();
	SConfig::GetInstance().bRenderToMain = m_checkbox_render_to_main_window->isChecked();
	SConfig::GetInstance().theme_name = m_combobox_theme_selector->currentText().toStdString();

	// In Game Options
	SConfig::GetInstance().bConfirmStop = m_checkbox_confirm_on_stop->isChecked();
	SConfig::GetInstance().bUsePanicHandlers = m_checkbox_use_panic_handlers->isChecked();
	SConfig::GetInstance().bOnScreenDisplayMessages = m_checkbox_enable_osd->isChecked();
	SConfig::GetInstance().m_PauseOnFocusLost = m_checkbox_pause_on_focus_lost->isChecked();
	SConfig::GetInstance().bAutoHideCursor = m_checkbox_hide_mouse->isChecked();

	SConfig::GetInstance().SaveSettings();
}

void InterfacePage::BuildUIOptions()
{
	QGroupBox* ui_group = new QGroupBox(tr("User Interface"));
	QVBoxLayout* ui_group_layout = new QVBoxLayout;
	ui_group->setLayout(ui_group_layout);
	m_mainLayout->addWidget(ui_group);

	{
		QHBoxLayout* theme_layout = new QHBoxLayout;
		QLabel* theme_label = new QLabel(tr("Theme:"));
		m_combobox_theme_selector = new QComboBox;
		auto file_search_results = DoFileSearch({""}, {
			File::GetUserPath(D_THEMES_IDX),
			File::GetSysDirectory() + THEMES_DIR
		}, /*recursive*/ false);

		for (const std::string& filename : file_search_results)
		{
			std::string name, ext;
			SplitPath(filename, nullptr, &name, &ext);
			name += ext;
			QString qt_name = QString::fromStdString(name);
			m_combobox_theme_selector->addItem(qt_name);
		}

		theme_label->setMaximumWidth(150);

		theme_layout->addWidget(theme_label);
		theme_layout->addWidget(m_combobox_theme_selector);

		ui_group_layout->addLayout(theme_layout);
	}

	m_checkbox_auto_adjust_window = new QCheckBox(tr("Auto Adjust Window Size"));
	m_checkbox_keep_dolphin_on_top = new QCheckBox(tr("Keep Dolphin on Top"));
	m_checkbox_render_to_main_window = new QCheckBox(tr("Render to Main Window"));

	ui_group_layout->addWidget(m_checkbox_auto_adjust_window);
	ui_group_layout->addWidget(m_checkbox_keep_dolphin_on_top);
	ui_group_layout->addWidget(m_checkbox_render_to_main_window);
}

void InterfacePage::BuildInGameOptions()
{
	QGroupBox* in_game_group = new QGroupBox(tr("In Game"));
	QVBoxLayout* in_game_group_layout = new QVBoxLayout;
	in_game_group->setLayout(in_game_group_layout);
	m_mainLayout->addWidget(in_game_group);

	m_checkbox_confirm_on_stop = new QCheckBox(tr("Confirm on Stop"));
	m_checkbox_use_panic_handlers = new QCheckBox(tr("Use Panic Handlers"));
	m_checkbox_enable_osd = new QCheckBox(tr("On Screen Messages"));
	m_checkbox_pause_on_focus_lost = new QCheckBox(tr("Pause on Focus Lost"));
	m_checkbox_hide_mouse = new QCheckBox(tr("Hide Mouse Cursor"));

	in_game_group_layout->addWidget(m_checkbox_confirm_on_stop);
	in_game_group_layout->addWidget(m_checkbox_use_panic_handlers);
	in_game_group_layout->addWidget(m_checkbox_enable_osd);
	in_game_group_layout->addWidget(m_checkbox_pause_on_focus_lost);
	in_game_group_layout->addWidget(m_checkbox_hide_mouse);
}

void InterfacePage::BuildOptions()
{
	BuildUIOptions();
	BuildInGameOptions();
}

void InterfacePage::ConnectOptions()
{
	// UI Options
	connect(m_checkbox_auto_adjust_window, &QCheckBox::clicked, this, &InterfacePage::SaveConfig);
	connect(m_checkbox_keep_dolphin_on_top, &QCheckBox::clicked, this, &InterfacePage::SaveConfig);
	connect(m_checkbox_render_to_main_window, &QCheckBox::clicked, this, &InterfacePage::SaveConfig);
	connect(m_combobox_theme_selector, static_cast<void(QComboBox::*)(const QString&)>(&QComboBox::activated),
			[=](const QString& text){ SaveConfig(); });
	// In Game
	connect(m_checkbox_confirm_on_stop, &QCheckBox::clicked, this, &InterfacePage::SaveConfig);
	connect(m_checkbox_use_panic_handlers, &QCheckBox::clicked, this, &InterfacePage::SaveConfig);
	connect(m_checkbox_enable_osd, &QCheckBox::clicked, this, &InterfacePage::SaveConfig);
	connect(m_checkbox_pause_on_focus_lost, &QCheckBox::clicked, this, &InterfacePage::SaveConfig);
	connect(m_checkbox_hide_mouse, &QCheckBox::clicked, this, &InterfacePage::SaveConfig);
}
