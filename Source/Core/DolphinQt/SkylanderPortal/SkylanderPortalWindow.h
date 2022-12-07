#pragma once

#include <optional>

#include <QDialog>
#include <QLineEdit>
#include <QWidget>

#include "Core/Core.h"

class QDialogButtonBox;
class QLabel;
class QPushButton;
class QSpinBox;
class QTabWidget;

constexpr auto UI_SKY_NUM = 4;

class SkylanderPortalWindow : public QWidget
{
  Q_OBJECT
public:
  explicit SkylanderPortalWindow(QWidget* parent = nullptr);
  ~SkylanderPortalWindow();

protected:
  QLineEdit* edit_skylanders[UI_SKY_NUM]{};
  static std::optional<std::tuple<u8, u16, u16>> sky_slots[UI_SKY_NUM];

private:
  void CreateMainWindow();
  void CreateSkylander(u8 slot);
  void ClearSkylander(u8 slot);
  void LoadSkylander(u8 slot);
  void LoadSkylanderPath(u8 slot, const QString& path);
  void ConnectWidgets();
  void UpdateEdits();
  void closeEvent(QCloseEvent* bar) override;

  static SkylanderPortalWindow* inst;

  QDialogButtonBox* m_button_box;

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
