// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QAction>
#include <QDesktopServices>
#include <QUrl>

#include "Core/State.h"
#include "DolphinQt2/AboutDialog.h"
#include "DolphinQt2/MenuBar.h"
#include "DolphinQt2/Settings.h"

MenuBar::MenuBar(QWidget* parent)
	: QMenuBar(parent)
{
	AddFileMenu();
	AddEmulationMenu();
	addMenu(tr("Movie"));
	addMenu(tr("Options"));
	addMenu(tr("Tools"));
	AddViewMenu();
	AddHelpMenu();

	EmulationStopped();
}

void MenuBar::EmulationStarted()
{
	// Emulation
	m_play_action->setEnabled(false);
	m_play_action->setVisible(false);
	m_pause_action->setEnabled(true);
	m_pause_action->setVisible(true);
	m_stop_action->setEnabled(true);
	m_reset_action->setEnabled(true);
	m_fullscreen_action->setEnabled(true);
	m_frame_advance_action->setEnabled(true);
	m_screenshot_action->setEnabled(true);
	m_state_load_menu->setEnabled(true);
	m_state_save_menu->setEnabled(true);
	UpdateStateSlotMenu();
}
void MenuBar::EmulationPaused()
{
	m_play_action->setEnabled(true);
	m_play_action->setVisible(true);
	m_pause_action->setEnabled(false);
	m_pause_action->setVisible(false);
}
void MenuBar::EmulationStopped()
{
	// Emulation
	m_play_action->setEnabled(true);
	m_play_action->setVisible(true);
	m_pause_action->setEnabled(false);
	m_pause_action->setVisible(false);
	m_stop_action->setEnabled(false);
	m_reset_action->setEnabled(false);
	m_fullscreen_action->setEnabled(false);
	m_frame_advance_action->setEnabled(false);
	m_screenshot_action->setEnabled(false);
	m_state_load_menu->setEnabled(false);
	m_state_save_menu->setEnabled(false);
	UpdateStateSlotMenu();
}

void MenuBar::AddFileMenu()
{
	QMenu* file_menu = addMenu(tr("File"));
	m_open_action = file_menu->addAction(tr("Open"), this, SIGNAL(Open()));
	m_exit_action = file_menu->addAction(tr("Exit"), this, SIGNAL(Exit()));
}

void MenuBar::AddEmulationMenu()
{
	QMenu* emu_menu = addMenu(tr("Emulation"));
	m_play_action = emu_menu->addAction(tr("Play"), this, SIGNAL(Play()));
	m_pause_action = emu_menu->addAction(tr("Pause"), this, SIGNAL(Pause()));
	m_stop_action = emu_menu->addAction(tr("Stop"), this, SIGNAL(Stop()));
	m_reset_action = emu_menu->addAction(tr("Reset"), this, SIGNAL(Reset()));
	m_fullscreen_action = emu_menu->addAction(tr("Fullscreen"), this, SIGNAL(Fullscreen()));
	m_frame_advance_action = emu_menu->addAction(tr("Frame Advance"), this, SIGNAL(FrameAdvance()));
	m_screenshot_action = emu_menu->addAction(tr("Take Screenshot"), this, SIGNAL(Screenshot()));
	AddStateLoadMenu(emu_menu);
	AddStateSaveMenu(emu_menu);
	AddStateSlotMenu(emu_menu);
	UpdateStateSlotMenu();
}

void MenuBar::AddStateLoadMenu(QMenu* emu_menu)
{
	m_state_load_menu = emu_menu->addMenu(tr("Load State"));
	m_state_load_menu->addAction(tr("Load State from File"), this, SIGNAL(StateLoad()));
	m_state_load_menu->addAction(tr("Load State from Selected Slot"), this, SIGNAL(StateLoadSlot()));
	m_state_load_slots_menu = m_state_load_menu->addMenu(tr("Load State from Slot"));
	m_state_load_menu->addAction(tr("Undo Load State"), this, SIGNAL(StateLoadUndo()));

	for (int i = 1; i <= 10; i++)
	{
		QAction* action = m_state_load_slots_menu->addAction(QStringLiteral(""));

		connect(action, &QAction::triggered, this, [=]() {
			emit StateLoadSlotAt(i);
		});
	}
}

void MenuBar::AddStateSaveMenu(QMenu* emu_menu)
{
	m_state_save_menu = emu_menu->addMenu(tr("Save State"));
	m_state_save_menu->addAction(tr("Save State to File"), this, SIGNAL(StateSave()));
	m_state_save_menu->addAction(tr("Save State to Selected Slot"), this, SIGNAL(StateSaveSlot()));
	m_state_save_menu->addAction(tr("Save State to Oldest Slot"), this, SIGNAL(StateSaveOldest()));
	m_state_save_slots_menu = m_state_save_menu->addMenu(tr("Save State to Slot"));
	m_state_save_menu->addAction(tr("Undo Save State"), this, SIGNAL(StateSaveUndo()));

	for (int i = 1; i <= 10; i++)
	{
		QAction* action = m_state_save_slots_menu->addAction(QStringLiteral(""));

		connect(action, &QAction::triggered, this, [=]() {
			emit StateSaveSlotAt(i);
		});
	}
}

void MenuBar::AddStateSlotMenu(QMenu* emu_menu)
{
	m_state_slot_menu = emu_menu->addMenu(tr("Select State Slot"));
	m_state_slots = new QActionGroup(this);

	for (int i = 1; i <= 10; i++)
	{
		QAction* action = m_state_slot_menu->addAction(QStringLiteral(""));
		action->setCheckable(true);
		action->setActionGroup(m_state_slots);
		if (Settings().GetStateSlot() == i)
			action->setChecked(true);

		connect(action, &QAction::triggered, this, [=]() {
			emit SetStateSlot(i);
		});
	}
}

void MenuBar::UpdateStateSlotMenu()
{
	QList<QAction*> actions_slot = m_state_slots->actions();
	QList<QAction*> actions_load = m_state_load_slots_menu->actions();
	QList<QAction*> actions_save = m_state_save_slots_menu->actions();
	for (int i = 0; i < actions_slot.length(); i++)
	{
		int slot = i + 1;
		QString info = QString::fromStdString(State::GetInfoStringOfSlot(slot));
		QString action_string = tr(" Slot %1 - %2").arg(slot).arg(info);
		actions_load.at(i)->setText(tr("Load from") + action_string);
		actions_save.at(i)->setText(tr("Save to") + action_string);
		actions_slot.at(i)->setText(tr("Select") + action_string);
	}
}

void MenuBar::AddViewMenu()
{
	QMenu* view_menu = addMenu(tr("View"));
	AddGameListTypeSection(view_menu);
	view_menu->addSeparator();
	AddTableColumnsMenu(view_menu);
}

void MenuBar::AddHelpMenu()
{
	QMenu* help_menu = addMenu(tr("Help"));
	QAction* documentation = help_menu->addAction(tr("Online Documentation"));
	connect(documentation, &QAction::triggered, this, [=]() {
	    QDesktopServices::openUrl(QUrl(QStringLiteral("https://dolphin-emu.org/docs/guides")));
	});
	help_menu->addAction(tr("About"), this, SIGNAL(ShowAboutDialog()));
}


void MenuBar::AddGameListTypeSection(QMenu* view_menu)
{
	QAction* table_view = view_menu->addAction(tr("Table"));
	table_view->setCheckable(true);

	QAction* list_view = view_menu->addAction(tr("List"));
	list_view->setCheckable(true);

	QActionGroup* list_group = new QActionGroup(this);
	list_group->addAction(table_view);
	list_group->addAction(list_view);

	bool prefer_table = Settings().GetPreferredView();
	table_view->setChecked(prefer_table);
	list_view->setChecked(!prefer_table);

	connect(table_view, &QAction::triggered, this, &MenuBar::ShowTable);
	connect(list_view, &QAction::triggered, this, &MenuBar::ShowList);
}

// TODO implement this
void MenuBar::AddTableColumnsMenu(QMenu* view_menu)
{
	QActionGroup* column_group = new QActionGroup(this);
	QMenu* cols_menu = view_menu->addMenu(tr("Table Columns"));
	column_group->setExclusive(false);

	QStringList col_names{
		tr("Platform"),
		tr("ID"),
		tr("Banner"),
		tr("Title"),
		tr("Description"),
		tr("Maker"),
		tr("Size"),
		tr("Country"),
		tr("Quality")
	};
	for (int i = 0; i < col_names.count(); i++)
	{
		QAction* action = column_group->addAction(cols_menu->addAction(col_names[i]));
		action->setCheckable(true);
	}
}
