// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QMainWindow>
#include <QStackedWidget>
#include <QString>
#include <QToolBar>
#include <wobjectdefs.h>

#include "DolphinQt2/GameList/GameList.h"
#include "DolphinQt2/MenuBar.h"
#include "DolphinQt2/RenderWidget.h"
#include "DolphinQt2/ToolBar.h"

class HotkeyScheduler;
class MappingWindow;
class SettingsWindow;
class ControllersWindow;
class DragEnterEvent;
class GraphicsWindow;

class MainWindow final : public QMainWindow
{
  W_OBJECT(MainWindow)

public:
  explicit MainWindow();
  ~MainWindow();

  bool eventFilter(QObject* object, QEvent* event) override;

  void EmulationStarted() W_SIGNAL(EmulationStarted);
  void EmulationPaused() W_SIGNAL(EmulationPaused);
  void EmulationStopped() W_SIGNAL(EmulationStopped);

private:
  void Open();
  W_SLOT(Open, W_Access::Private);
  void Play();
  W_SLOT(Play, W_Access::Private);
  void Pause();
  W_SLOT(Pause, W_Access::Private);

  // May ask for confirmation. Returns whether or not it actually stopped.
  bool Stop();
  W_SLOT(Stop, W_Access::Private);
  void ForceStop();
  W_SLOT(ForceStop, W_Access::Private);
  void Reset();
  W_SLOT(Reset, W_Access::Private);
  void FrameAdvance();
  W_SLOT(FrameAdvance, W_Access::Private);
  void StateLoad();
  W_SLOT(StateLoad, W_Access::Private);
  void StateSave();
  W_SLOT(StateSave, W_Access::Private);
  void StateLoadSlot();
  W_SLOT(StateLoadSlot, W_Access::Private);
  void StateSaveSlot();
  W_SLOT(StateSaveSlot, W_Access::Private);
  void StateLoadSlotAt(int slot);
  W_SLOT(StateLoadSlotAt, (int), W_Access::Private);
  void StateSaveSlotAt(int slot);
  W_SLOT(StateSaveSlotAt, (int), W_Access::Private);
  void StateLoadUndo();
  W_SLOT(StateLoadUndo, W_Access::Private);
  void StateSaveUndo();
  W_SLOT(StateSaveUndo, W_Access::Private);
  void StateSaveOldest();
  W_SLOT(StateSaveOldest, W_Access::Private);
  void SetStateSlot(int slot);
  W_SLOT(SetStateSlot, (int), W_Access::Private);

  void PerformOnlineUpdate(const std::string& region);
  W_SLOT(PerformOnlineUpdate, W_Access::Private);

  void FullScreen();
  W_SLOT(FullScreen, W_Access::Private);
  void ScreenShot();
  W_SLOT(ScreenShot, W_Access::Private);

private:
  void CreateComponents();

  void ConnectGameList();
  void ConnectHotkeys();
  void ConnectMenuBar();
  void ConnectRenderWidget();
  void ConnectStack();
  void ConnectToolBar();

  void InitControllers();
  void ShutdownControllers();

  void InitCoreCallbacks();

  void StartGame(const QString& path);
  void ShowRenderWidget();
  void HideRenderWidget();

  void ShowSettingsWindow();
  void ShowControllersWindow();
  void ShowGraphicsWindow();
  void ShowAboutDialog();
  void ShowHotkeyDialog();

  void dragEnterEvent(QDragEnterEvent* event) override;
  void dropEvent(QDropEvent* event) override;

  QStackedWidget* m_stack;
  ToolBar* m_tool_bar;
  MenuBar* m_menu_bar;
  GameList* m_game_list;
  RenderWidget* m_render_widget;
  bool m_rendering_to_main;
  bool m_stop_requested = false;
  int m_state_slot = 1;

  HotkeyScheduler* m_hotkey_scheduler;
  ControllersWindow* m_controllers_window;
  SettingsWindow* m_settings_window;
  MappingWindow* m_hotkey_window;
  GraphicsWindow* m_graphics_window;
};
