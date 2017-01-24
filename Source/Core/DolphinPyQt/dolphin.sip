%Module sip_dolphin

%Import QtWidgets/QtWidgetsmod.sip

class MenuBar : public QMenuBar
{

%TypeHeaderCode
#include "../DolphinQt2/MenuBar.h"
%End

public:
  MenuBar(QWidget* parent /TransferThis/ = 0);

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
};

class ToolBar : public QToolBar
{

%TypeHeaderCode
#include "../DolphinQt2/ToolBar.h"
%End

public:
  ToolBar(QWidget* parent /TransferThis/ = 0);

public slots:
  void EmulationStarted();
  void EmulationPaused();
  void EmulationStopped();

signals:
  void OpenPressed();
  void PlayPressed();
  void PausePressed();
  void StopPressed();
  void FullScreenPressed();
  void ScreenShotPressed();

  void PathsPressed();
  void SettingsPressed();
};

class GameList : public QStackedWidget
{

%TypeHeaderCode
#include "../DolphinQt2/GameList/GameList.h"
%End

public:
  GameList(QWidget* parent /TransferThis/ = 0);
  QString GetSelectedGame() const;

public slots:
  void SetTableView();
  void SetListView();
  void SetViewColumn(int col, bool view);

signals:
  void GameSelected();
  void DirectoryAdded(const QString& dir);
  void DirectoryRemoved(const QString& dir);
};

class PathDialog : public QDialog
{

%TypeHeaderCode
#include "../DolphinQt2/Config/PathDialog.h"
%End

public:
  PathDialog(QWidget* parent /TransferThis/ = 0);

public slots:
  void Browse();
  void BrowseDefaultGame();
  void BrowseDVDRoot();
  void BrowseApploader();
  void BrowseWiiNAND();

signals:
  void PathAdded(QString path);
  void PathRemoved(QString path);
};

class SettingsWindow : public QDialog
{

%TypeHeaderCode
#include "../DolphinQt2/Config/SettingsWindow.h"
%End

public:
  SettingsWindow(QWidget* parent /TransferThis/ = 0);

public slots:
  void changePage(QListWidgetItem* current, QListWidgetItem* previous);
};

class Settings : public QSettings
{

%TypeHeaderCode
#include "../DolphinQt2/Settings.h"
%End

public:
  Settings(QObject* parent /TransferThis/ = 0);

  // UI
  QString GetThemeDir() const;
  bool IsInDevelopmentWarningEnabled() const;

  // GameList
  QString GetLastGame() const;
  void SetLastGame(const QString& path);
  QStringList GetPaths() const;
  void SetPaths(const QStringList& paths);
  void RemovePath(int i);
  QString GetDefaultGame() const;
  void SetDefaultGame(const QString& path);
  QString GetDVDRoot() const;
  void SetDVDRoot(const QString& path);
  QString GetApploader() const;
  void SetApploader(const QString& path);
  QString GetWiiNAND() const;
  void SetWiiNAND(const QString& path);
  // Skipping bindings for Get*SystemLanguage(), so I don't have to deal
  // with the DiscIO::Language type; we don't need to call those methods
  // from Python anyway
  bool GetPreferredView() const;
  void SetPreferredView(bool table);

  // Emulation
  bool GetConfirmStop() const;
  int GetStateSlot() const;
  void SetStateSlot(int);

  // Graphics
  bool GetRenderToMain() const;
  bool GetFullScreen() const;
  QSize GetRenderWindowSize() const;
};
