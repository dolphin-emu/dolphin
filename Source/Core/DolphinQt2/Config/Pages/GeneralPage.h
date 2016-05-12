#pragma once

#include <QWidget>

class QCheckBox;
class QComboBox;
class QVBoxLayout;

class GeneralPage final : public QWidget
{
public:
	GeneralPage(QWidget* parent = nullptr);
	void LoadConfig();

private slots:
	void SaveConfig();

private:
	void BuildBasicSettings();
	void BuildAdvancedSettings();
	void BuildOptions();
	void ConnectOptions();
	QVBoxLayout* m_main_layout;

	// Basic Group
	QComboBox* m_combobox_language;
	QCheckBox* m_checkbox_enable_cheats;
	QComboBox* m_checkbox_speed_limit;

	// Advanced Group
	QCheckBox* m_checkbox_force_ntsc;
};
