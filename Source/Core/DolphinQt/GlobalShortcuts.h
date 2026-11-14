// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <vector>

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>

class QDBusMessage;
class QDBusObjectPath;

// Applications only get keyboard input on Wayland while they are focused, so hotkeys bound to the
// keyboard would stop working when "Hotkeys Require Window Focus" is turned off. This registers
// those hotkeys with the desktop's GlobalShortcuts portal instead, suggesting their bindings as
// the shortcuts, and presses them when the desktop reports that a shortcut was activated.
class GlobalShortcuts final : public QObject
{
  Q_OBJECT

public:
  explicit GlobalShortcuts(QObject* parent = nullptr);
  ~GlobalShortcuts() override;

  // Registers the hotkeys again if their bindings or the hotkey focus setting changed.
  void Refresh();

private slots:
  void OnResponse(uint response, const QVariantMap& results, const QDBusMessage& message);
  void OnActivated(const QDBusObjectPath& session, const QString& id, qulonglong timestamp,
                   const QVariantMap& options);
  void OnDeactivated(const QDBusObjectPath& session, const QString& id, qulonglong timestamp,
                     const QVariantMap& options);

private:
  struct Shortcut
  {
    QString id;
    int hotkey;
    QString description;
    QString trigger;

    bool operator==(const Shortcut&) const = default;
  };

  static std::vector<Shortcut> GetShortcuts();

  QString NewRequestPath(QString* token);
  void CreateSession();
  void BindShortcuts();
  void CloseSession();
  void SetHotkeyState(const QDBusObjectPath& session, const QString& id, bool pressed);

  std::vector<Shortcut> m_shortcuts;
  QString m_session;
  QString m_create_request;
  QString m_bind_request;
  // Requests for sessions that were no longer wanted by the time they were created.
  QStringList m_abandoned_requests;
  int m_last_token = 0;
};
