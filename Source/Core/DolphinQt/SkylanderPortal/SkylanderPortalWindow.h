#pragma once

#include <optional>

#include <QCheckBox>
#include <QDialog>
#include <QGroupBox>
#include <QLineEdit>
#include <QWidget>

#include "Core/Core.h"
#include "Core/IOS/USB/Emulated/Skylander.h"

class QDialogButtonBox;
class QLabel;
class QPushButton;
class QSpinBox;
class QTabWidget;

class SkylanderPortalWindow : public QWidget
{
  Q_OBJECT
public:
  explicit SkylanderPortalWindow(QWidget* parent = nullptr);
  ~SkylanderPortalWindow();

protected:
  QLineEdit* edit_skylanders[MAX_SKYLANDERS]{};
  static std::optional<std::tuple<u8, u16, u16>> sky_slots[MAX_SKYLANDERS];

private:
  void CreateMainWindow();
  void OnEmulationStateChanged(Core::State state);
  void CreateSkylander(u8 slot);
  void ClearSkylander(u8 slot);
  void EmulatePortal(bool emulate);
  void LoadSkylander(u8 slot);
  void LoadSkylanderPath(u8 slot, const QString& path);
  void UpdateEdits();
  void closeEvent(QCloseEvent* bar) override;

  static SkylanderPortalWindow* inst;

  QCheckBox* checkbox;
  QGroupBox* group_skylanders;

  bool eventFilter(QObject* object, QEvent* event) final override;
};

class CreateSkylanderDialog : public QDialog
{
  Q_OBJECT

public:
  explicit CreateSkylanderDialog(QWidget* parent);
  QString GetFilePath() const;

protected:
  QString file_path;
};
