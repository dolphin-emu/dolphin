#pragma once

#include <QWidget>

class QCheckBox;
class QComboBox;
class QVBoxLayout;

class InterfacePage final : public QWidget
{
public:
    InterfacePage(QWidget* parent = nullptr);
    void LoadConfig();

private slots:
    void SaveConfig();

private:
	void ConnectOptions();
    void BuildOptions();
    void BuildUIOptions();
    void BuildInGameOptions();

    QVBoxLayout* m_mainLayout;

    QCheckBox* m_checkbox_auto_adjust_window;
    QCheckBox* m_checkbox_keep_dolphin_on_top;
    QCheckBox* m_checkbox_render_to_main_window;
    QComboBox* m_combobox_theme_selector;
    QCheckBox* m_checkbox_confirm_on_stop;
    QCheckBox* m_checkbox_use_panic_handlers;
    QCheckBox* m_checkbox_enable_osd;
    QCheckBox* m_checkbox_pause_on_focus_lost;
    QCheckBox* m_checkbox_hide_mouse;
};
