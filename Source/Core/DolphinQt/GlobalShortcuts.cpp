// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/GlobalShortcuts.h"

#include <algorithm>
#include <cctype>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QGuiApplication>

#include <fmt/format.h>

#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Core/Config/UISettings.h"
#include "Core/HotkeyManager.h"
#include "DolphinQt/Settings.h"
#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerEmu/Control/Control.h"
#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"

namespace
{
constexpr char PORTAL_SERVICE[] = "org.freedesktop.portal.Desktop";
constexpr char PORTAL_PATH[] = "/org/freedesktop/portal/desktop";
constexpr char SHORTCUTS_INTERFACE[] = "org.freedesktop.portal.GlobalShortcuts";

// A shortcut as the portal sends and receives it, (sa{sv}).
struct PortalShortcut
{
  QString id;
  QVariantMap properties;
};

QDBusArgument& operator<<(QDBusArgument& argument, const PortalShortcut& shortcut)
{
  argument.beginStructure();
  argument << shortcut.id << shortcut.properties;
  argument.endStructure();
  return argument;
}

const QDBusArgument& operator>>(const QDBusArgument& argument, PortalShortcut& shortcut)
{
  argument.beginStructure();
  argument >> shortcut.id >> shortcut.properties;
  argument.endStructure();
  return argument;
}
}  // namespace

Q_DECLARE_METATYPE(PortalShortcut)

// Turns a name into a shortcut ID, which desktops like Hyprland have users type in their config,
// like "General Toggle Pause" into "general-toggle-pause".
static QString ToShortcutId(std::string_view name)
{
  std::string id;
  for (const char c : name)
  {
    if (std::isalnum(static_cast<unsigned char>(c)))
      id += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    else if (!id.empty() && id.back() != '-')
      id += '-';
  }
  if (!id.empty() && id.back() == '-')
    id.pop_back();
  return QString::fromStdString(id);
}

static std::optional<QString> GetModifier(std::string_view key)
{
  if (key == "Ctrl" || key == "Control_L" || key == "Control_R")
    return QStringLiteral("CTRL");
  if (key == "Shift" || key == "Shift_L" || key == "Shift_R")
    return QStringLiteral("SHIFT");
  if (key == "Alt" || key == "Alt_L" || key == "Alt_R")
    return QStringLiteral("ALT");
  if (key == "Super_L" || key == "Super_R")
    return QStringLiteral("LOGO");
  return std::nullopt;
}

// Converts a binding of a single key, optionally held with modifiers, like "F10" or
// "@(Ctrl+`Shift`+O)", to a trigger in the format of the XDG shortcuts specification, like
// "CTRL+SHIFT+o". Returns nothing for any other binding, including mouse buttons.
static std::optional<QString> GetTrigger(std::string_view expression)
{
  expression = StripWhitespace(expression);
  std::vector<std::string> keys;
  if (expression.starts_with("@(") && expression.ends_with(')'))
    keys = SplitString(std::string(expression.substr(2, expression.size() - 3)), '+');
  else
    keys.emplace_back(expression);

  QStringList trigger;
  for (std::size_t i = 0; i < keys.size(); ++i)
  {
    std::string_view key = StripWhitespace(keys[i]);
    if (key.size() >= 2 && key.front() == '`' && key.back() == '`')
      key = key.substr(1, key.size() - 2);

    // Key names are keysym names. Anything else, like "Click 1", is not a key.
    if (key.empty() || !std::ranges::all_of(key, [](char c) {
          return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
        }))
    {
      return std::nullopt;
    }

    // Modifiers have to come before the key.
    const std::optional<QString> modifier = GetModifier(key);
    if (modifier.has_value() == (i + 1 == keys.size()))
      return std::nullopt;

    if (modifier)
      trigger.push_back(*modifier);
    // Letters are named in upper case, but their keysyms are in lower case.
    else if (key.size() == 1)
      trigger.push_back(QString(QChar::fromLatin1(key[0]).toLower()));
    else
      trigger.push_back(QString::fromUtf8(key.data(), static_cast<qsizetype>(key.size())));
  }
  return trigger.join(QLatin1Char('+'));
}

static QString GetSessionHandle(const QVariantMap& results)
{
  // The specification says this is a string, but some implementations send an object path.
  const QVariant handle = results.value(QStringLiteral("session_handle"));
  if (handle.metaType() == QMetaType::fromType<QDBusObjectPath>())
    return handle.value<QDBusObjectPath>().path();
  return handle.toString();
}

static void SendCloseSession(const QString& session)
{
  QDBusConnection::sessionBus().send(QDBusMessage::createMethodCall(
      QString::fromLatin1(PORTAL_SERVICE), session,
      QStringLiteral("org.freedesktop.portal.Session"), QStringLiteral("Close")));
}

// Calls the portal, logging any error. Responses arrive through OnResponse.
static void CallPortal(QObject* parent, const QDBusMessage& message)
{
  auto* const watcher =
      new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message), parent);
  QObject::connect(watcher, &QDBusPendingCallWatcher::finished, parent,
                   [method = message.member()](QDBusPendingCallWatcher* call) {
                     if (call->isError())
                     {
                       INFO_LOG_FMT(COMMON, "GlobalShortcuts.{} failed: {}", method.toStdString(),
                                    call->error().message().toStdString());
                     }
                     call->deleteLater();
                   });
}

GlobalShortcuts::GlobalShortcuts(QObject* parent) : QObject(parent)
{
  qDBusRegisterMetaType<PortalShortcut>();
  qDBusRegisterMetaType<QList<PortalShortcut>>();

  auto bus = QDBusConnection::sessionBus();

  // Without an application ID, which Flatpak would provide, the portal can't tell which desktop
  // file the shortcuts belong to. Recent versions of Qt register it already, which makes this fail.
  bus.asyncCall(QDBusMessage::createMethodCall(
                    QString::fromLatin1(PORTAL_SERVICE), QString::fromLatin1(PORTAL_PATH),
                    QStringLiteral("org.freedesktop.host.portal.Registry"),
                    QStringLiteral("Register"))
                << QGuiApplication::desktopFileName() << QVariantMap{});

  // Responses are sent only to the application that made the request, and are matched up with
  // the request by their object path.
  bus.connect(QString::fromLatin1(PORTAL_SERVICE), {},
              QStringLiteral("org.freedesktop.portal.Request"), QStringLiteral("Response"), this,
              SLOT(OnResponse(uint, QVariantMap, QDBusMessage)));
  bus.connect(QString::fromLatin1(PORTAL_SERVICE), QString::fromLatin1(PORTAL_PATH),
              QString::fromLatin1(SHORTCUTS_INTERFACE), QStringLiteral("Activated"), this,
              SLOT(OnActivated(QDBusObjectPath, QString, qulonglong, QVariantMap)));
  bus.connect(QString::fromLatin1(PORTAL_SERVICE), QString::fromLatin1(PORTAL_PATH),
              QString::fromLatin1(SHORTCUTS_INTERFACE), QStringLiteral("Deactivated"), this,
              SLOT(OnDeactivated(QDBusObjectPath, QString, qulonglong, QVariantMap)));

  connect(&Settings::Instance(), &Settings::ConfigChanged, this, &GlobalShortcuts::Refresh);

  Refresh();
}

GlobalShortcuts::~GlobalShortcuts()
{
  CloseSession();
}

std::vector<GlobalShortcuts::Shortcut> GlobalShortcuts::GetShortcuts()
{
  if (Config::Get(Config::MAIN_FOCUSED_HOTKEYS))
    return {};

  const auto* const manager =
      static_cast<HotkeyManager*>(HotkeyManagerEmu::GetConfig()->GetController(0));

  // Only the keyboard's inputs are named after keys.
  const std::string& source = manager->GetDefaultDevice().source;
  if (source != "Wayland" && source != "XInput2")
    return {};

  const auto lock = ControllerEmu::EmulatedController::GetStateLock();
  std::vector<Shortcut> shortcuts;
  for (int hotkey = 0; hotkey < NUM_HOTKEYS; ++hotkey)
  {
    const int group_index = manager->FindGroupByID(hotkey);
    const auto* const group = manager->GetHotkeyGroup(static_cast<HotkeyGroup>(group_index));
    const auto& control = group->controls[manager->GetIndexForGroup(group_index, hotkey)];

    std::optional<QString> trigger = GetTrigger(control->control_ref->GetExpression());
    if (!trigger)
      continue;

    shortcuts.push_back({
        .id = ToShortcutId(fmt::format("{} {}", group->name, control->name)),
        .hotkey = hotkey,
        .description =
            QStringLiteral("%1: %2").arg(tr(group->ui_name.c_str()), tr(control->ui_name.c_str())),
        .trigger = std::move(*trigger),
    });
  }
  return shortcuts;
}

void GlobalShortcuts::Refresh()
{
  std::vector<Shortcut> shortcuts = GetShortcuts();
  if (shortcuts == m_shortcuts)
    return;

  CloseSession();
  m_shortcuts = std::move(shortcuts);
  if (!m_shortcuts.empty())
    CreateSession();
}

// The portal creates the object for a request at a path made from our connection's name and a
// token we choose, so that we can tell which request a response is for.
QString GlobalShortcuts::NewRequestPath(QString* token)
{
  *token = QStringLiteral("dolphin%1").arg(++m_last_token);
  const QString sender = QDBusConnection::sessionBus().baseService().mid(1).replace(
      QLatin1Char('.'), QLatin1Char('_'));
  return QStringLiteral("/org/freedesktop/portal/desktop/request/%1/%2").arg(sender, *token);
}

void GlobalShortcuts::CreateSession()
{
  QString token;
  m_create_request = NewRequestPath(&token);

  QDBusMessage message = QDBusMessage::createMethodCall(
      QString::fromLatin1(PORTAL_SERVICE), QString::fromLatin1(PORTAL_PATH),
      QString::fromLatin1(SHORTCUTS_INTERFACE), QStringLiteral("CreateSession"));
  message << QVariantMap{{QStringLiteral("handle_token"), token},
                         {QStringLiteral("session_handle_token"), token}};
  CallPortal(this, message);
}

void GlobalShortcuts::BindShortcuts()
{
  QList<PortalShortcut> shortcuts;
  for (const Shortcut& shortcut : m_shortcuts)
  {
    shortcuts.push_back({shortcut.id,
                         {{QStringLiteral("description"), shortcut.description},
                          {QStringLiteral("preferred_trigger"), shortcut.trigger}}});
  }

  QString token;
  m_bind_request = NewRequestPath(&token);

  QDBusMessage message = QDBusMessage::createMethodCall(
      QString::fromLatin1(PORTAL_SERVICE), QString::fromLatin1(PORTAL_PATH),
      QString::fromLatin1(SHORTCUTS_INTERFACE), QStringLiteral("BindShortcuts"));
  message << QVariant::fromValue(QDBusObjectPath(m_session)) << QVariant::fromValue(shortcuts)
          << QString() << QVariantMap{{QStringLiteral("handle_token"), token}};
  CallPortal(this, message);
}

void GlobalShortcuts::CloseSession()
{
  if (!m_create_request.isEmpty())
    m_abandoned_requests.push_back(std::exchange(m_create_request, {}));
  m_bind_request.clear();

  if (!m_session.isEmpty())
    SendCloseSession(std::exchange(m_session, {}));

  HotkeyManagerEmu::ReleaseExternalState();
}

void GlobalShortcuts::OnResponse(uint response, const QVariantMap& results,
                                 const QDBusMessage& message)
{
  const QString path = message.path();
  if (m_abandoned_requests.removeOne(path))
  {
    if (const QString session = GetSessionHandle(results); !session.isEmpty())
      SendCloseSession(session);
  }
  else if (path == m_create_request)
  {
    m_create_request.clear();
    if (response != 0)
    {
      WARN_LOG_FMT(COMMON, "Could not create a global shortcuts session: response {}", response);
      return;
    }
    m_session = GetSessionHandle(results);
    BindShortcuts();
  }
  else if (path == m_bind_request)
  {
    m_bind_request.clear();
    if (response != 0)
    {
      INFO_LOG_FMT(COMMON, "Global shortcuts were not bound: response {}", response);
      return;
    }
    const auto shortcuts =
        qdbus_cast<QList<PortalShortcut>>(results.value(QStringLiteral("shortcuts")));
    INFO_LOG_FMT(COMMON, "Bound {} global shortcuts", shortcuts.size());
    for (const PortalShortcut& shortcut : shortcuts)
    {
      DEBUG_LOG_FMT(COMMON, "Global shortcut {}: {}", shortcut.id.toStdString(),
                    shortcut.properties.value(QStringLiteral("trigger_description"))
                        .toString()
                        .toStdString());
    }
  }
}

void GlobalShortcuts::OnActivated(const QDBusObjectPath& session, const QString& id, qulonglong,
                                  const QVariantMap&)
{
  SetHotkeyState(session, id, true);
}

void GlobalShortcuts::OnDeactivated(const QDBusObjectPath& session, const QString& id, qulonglong,
                                    const QVariantMap&)
{
  SetHotkeyState(session, id, false);
}

void GlobalShortcuts::SetHotkeyState(const QDBusObjectPath& session, const QString& id,
                                     bool pressed)
{
  if (session.path() != m_session)
    return;

  const auto it = std::ranges::find(m_shortcuts, id, &Shortcut::id);
  if (it != m_shortcuts.end())
    HotkeyManagerEmu::SetExternalState(it->hotkey, pressed);
}
