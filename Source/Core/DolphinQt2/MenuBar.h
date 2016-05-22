// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QMenu>
#include <QMenuBar>

class MenuBar final : public QMenuBar
{
	Q_OBJECT

public:
	explicit MenuBar(QWidget* parent = nullptr);

signals:
	// File
	void Open();
	void Exit();

	// Emulation
	void Play();
	void Pause();
	void Stop();
	void Reset();
	void Fullscreen();
	void FrameAdvance();
	void Screenshot();
	void StateLoad();
	void StateSave();
	void StateLoadSlot();
	void StateSaveSlot();
	void StateLoadSlotAt(int slot);
	void StateSaveSlotAt(int slot);
	void StateLoadUndo();
	void StateSaveUndo();
	void StateSaveOldest();
	void SetStateSlot(int slot);

	// View
	void ShowTable();
	void ShowList();

	void ShowAboutDialog();

public slots:
	void EmulationStarted();
	void EmulationPaused();
	void EmulationStopped();
	void UpdateStateSlotMenu();

private:
	void AddFileMenu();

	void AddEmulationMenu();
	void AddStateLoadMenu(QMenu* emu_menu);
	void AddStateSaveMenu(QMenu* emu_menu);
	void AddStateSlotMenu(QMenu* emu_menu);

	void AddViewMenu();
	void AddGameListTypeSection(QMenu* view_menu);
	void AddTableColumnsMenu(QMenu* view_menu);

	void AddHelpMenu();

	// File
	QAction* m_open_action;
	QAction* m_exit_action;

	// Emulation
	QAction* m_play_action;
	QAction* m_pause_action;
	QAction* m_stop_action;
	QAction* m_reset_action;
	QAction* m_fullscreen_action;
	QAction* m_frame_advance_action;
	QAction* m_screenshot_action;
	QMenu* m_state_load_menu;
	QMenu* m_state_save_menu;
	QMenu* m_state_slot_menu;
	QActionGroup* m_state_slots;
	QMenu* m_state_load_slots_menu;
	QMenu* m_state_save_slots_menu;
};
