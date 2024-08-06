// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/CheatSearchWidget.h"
#include "DolphinQt/QtUtils/WrapInScrollArea.h"

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QCursor>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QSettings>
#include <QSignalBlocker>
#include <QString>
#include <QTableWidget>
#include <QVBoxLayout>

#include <fmt/format.h>

#include "Common/Align.h"
#include "Common/BitUtils.h"
#include "Common/StringUtil.h"
#include "Common/Swap.h"

#include "Core/ActionReplay.h"
#include "Core/CheatGeneration.h"
#include "Core/CheatSearch.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

#include "DolphinQt/Config/CheatCodeEditor.h"
#include "DolphinQt/Config/CheatWarningWidget.h"
#include "DolphinQt/Host.h"
#include "DolphinQt/Settings.h"

#include "UICommon/GameFile.h"

namespace
{
struct AutoupdateEvent : public QEvent
{
  AutoupdateEvent(std::unique_ptr<Cheats::CheatSearchSessionBase> autoupdate_session)
      : QEvent(GetType()), session(std::move(autoupdate_session))
  {
  }
  static QEvent::Type GetType()
  {
    static const QEvent::Type event_type = static_cast<QEvent::Type>(QEvent::registerEventType());
    return event_type;
  }
  std::unique_ptr<Cheats::CheatSearchSessionBase> session;
};
}  // namespace

constexpr int TABLE_MAX_ROWS = 1000;

constexpr int ADDRESS_TABLE_ADDRESS_ROLE = Qt::UserRole;
constexpr int ADDRESS_TABLE_RESULT_INDEX_ROLE = Qt::UserRole + 1;
constexpr int ADDRESS_TABLE_COLUMN_INDEX_DESCRIPTION = 0;
constexpr int ADDRESS_TABLE_COLUMN_INDEX_ADDRESS = 1;
constexpr int ADDRESS_TABLE_COLUMN_INDEX_LAST_VALUE = 2;
constexpr int ADDRESS_TABLE_COLUMN_INDEX_CURRENT_VALUE = 3;

CheatSearchWidget::CheatSearchWidget(Core::System& system,
                                     std::unique_ptr<Cheats::CheatSearchSessionBase> session,
                                     QWidget* parent)
    : QWidget(parent), m_system(system), m_last_value_session(std::move(session))
{
  setAttribute(Qt::WA_DeleteOnClose);
  CreateWidgets();
  ConnectWidgets();
  OnValueSourceChanged();
  UpdateCurrentValueSessionAddressesAndValues();
  RecreateTable();
}

CheatSearchWidget::~CheatSearchWidget()
{
  auto& settings = Settings::GetQSettings();
  settings.setValue(QStringLiteral("cheatsearchwidget/displayhex"),
                    m_display_values_in_hex_checkbox->isChecked());
  settings.setValue(QStringLiteral("cheatsearchwidget/autoupdatecurrentvalues"),
                    m_autoupdate_current_values->isChecked());

  if (m_last_value_session->IsIntegerType())
  {
    settings.setValue(QStringLiteral("cheatsearchwidget/parsehex"),
                      m_parse_values_as_hex_checkbox->isChecked());
  }
}

bool CheatSearchWidget::event(QEvent* const event)
{
  if (event->type() != AutoupdateEvent::GetType())
    return QWidget::event(event);

  OnAutoupdate(static_cast<AutoupdateEvent*>(event)->session.release());
  return true;
}

void CheatSearchWidget::hideEvent(QHideEvent* const event)
{
  RemoveAutoupdateCallback();
  QWidget::hideEvent(event);
}

void CheatSearchWidget::showEvent(QShowEvent* const event)
{
  RefreshAutoupdateCallback();
  QWidget::showEvent(event);
}

Q_DECLARE_METATYPE(Cheats::CompareType);
Q_DECLARE_METATYPE(Cheats::FilterType);

void CheatSearchWidget::CreateWidgets()
{
  QLabel* session_info_label = new QLabel();
  {
    QString ranges;
    size_t range_count = m_last_value_session->GetMemoryRangeCount();
    switch (range_count)
    {
    case 1:
    {
      auto m = m_last_value_session->GetMemoryRange(0);
      ranges =
          tr("[%1, %2]")
              .arg(QString::fromStdString(fmt::format("0x{0:08x}", m.m_start)))
              .arg(QString::fromStdString(fmt::format("0x{0:08x}", m.m_start + m.m_length - 1)));
      break;
    }
    case 2:
    {
      auto m0 = m_last_value_session->GetMemoryRange(0);
      auto m1 = m_last_value_session->GetMemoryRange(1);
      ranges =
          tr("[%1, %2] and [%3, %4]")
              .arg(QString::fromStdString(fmt::format("0x{0:08x}", m0.m_start)))
              .arg(QString::fromStdString(fmt::format("0x{0:08x}", m0.m_start + m0.m_length - 1)))
              .arg(QString::fromStdString(fmt::format("0x{0:08x}", m1.m_start)))
              .arg(QString::fromStdString(fmt::format("0x{0:08x}", m1.m_start + m1.m_length - 1)));
      break;
    }
    default:
      ranges = tr("%1 memory ranges").arg(range_count);
      break;
    }

    QString space;
    switch (m_last_value_session->GetAddressSpace())
    {
    case PowerPC::RequestedAddressSpace::Effective:
      space = tr("Address space by CPU state");
      break;
    case PowerPC::RequestedAddressSpace::Physical:
      space = tr("Physical address space");
      break;
    case PowerPC::RequestedAddressSpace::Virtual:
      space = tr("Virtual address space");
      break;
    default:
      space = tr("Unknown address space");
      break;
    }

    QString type;
    switch (m_last_value_session->GetDataType())
    {
    case Cheats::DataType::U8:
      type = tr("8-bit Unsigned Integer");
      break;
    case Cheats::DataType::U16:
      type = tr("16-bit Unsigned Integer");
      break;
    case Cheats::DataType::U32:
      type = tr("32-bit Unsigned Integer");
      break;
    case Cheats::DataType::U64:
      type = tr("64-bit Unsigned Integer");
      break;
    case Cheats::DataType::S8:
      type = tr("8-bit Signed Integer");
      break;
    case Cheats::DataType::S16:
      type = tr("16-bit Signed Integer");
      break;
    case Cheats::DataType::S32:
      type = tr("32-bit Signed Integer");
      break;
    case Cheats::DataType::S64:
      type = tr("64-bit Signed Integer");
      break;
    case Cheats::DataType::F32:
      type = tr("32-bit Float");
      break;
    case Cheats::DataType::F64:
      type = tr("64-bit Float");
      break;
    default:
      type = tr("Unknown data type");
      break;
    }
    QString aligned = m_last_value_session->GetAligned() ? tr("aligned") : tr("unaligned");
    session_info_label->setText(tr("%1, %2, %3, %4").arg(ranges).arg(space).arg(type).arg(aligned));
    session_info_label->setWordWrap(true);
  }

  // i18n: This label is followed by a dropdown where the user can select things like "is equal to"
  // or "is less than or equal to", followed by another dropdown where the user can select "any
  // value", "last value", or "this value:". These three UI elements are intended to form a sentence
  // together. Because the UI elements can't be reordered by a translation, you may have to give
  // up on the idea of having them form a sentence depending on the grammar of your target language.
  auto* instructions_label = new QLabel(tr("Keep addresses where value in memory"));

  auto* value_layout = new QHBoxLayout();
  m_compare_type_dropdown = new QComboBox();
  m_compare_type_dropdown->addItem(tr("is equal to"),
                                   QVariant::fromValue(Cheats::CompareType::Equal));
  m_compare_type_dropdown->addItem(tr("is not equal to"),
                                   QVariant::fromValue(Cheats::CompareType::NotEqual));
  m_compare_type_dropdown->addItem(tr("is less than"),
                                   QVariant::fromValue(Cheats::CompareType::Less));
  m_compare_type_dropdown->addItem(tr("is less than or equal to"),
                                   QVariant::fromValue(Cheats::CompareType::LessOrEqual));
  m_compare_type_dropdown->addItem(tr("is greater than"),
                                   QVariant::fromValue(Cheats::CompareType::Greater));
  m_compare_type_dropdown->addItem(tr("is greater than or equal to"),
                                   QVariant::fromValue(Cheats::CompareType::GreaterOrEqual));
  value_layout->addWidget(m_compare_type_dropdown);

  m_value_source_dropdown = new QComboBox();
  m_value_source_dropdown->addItem(
      tr("this value:"), QVariant::fromValue(Cheats::FilterType::CompareAgainstSpecificValue));
  m_value_source_dropdown->addItem(
      tr("last value"), QVariant::fromValue(Cheats::FilterType::CompareAgainstLastValue));
  m_value_source_dropdown->addItem(tr("any value"),
                                   QVariant::fromValue(Cheats::FilterType::DoNotFilter));
  value_layout->addWidget(m_value_source_dropdown);

  m_given_value_text = new QLineEdit();
  value_layout->addWidget(m_given_value_text);

  auto& settings = Settings::GetQSettings();
  m_parse_values_as_hex_checkbox = new QCheckBox(tr("Parse as Hex"));
  if (m_last_value_session->IsIntegerType())
  {
    m_parse_values_as_hex_checkbox->setChecked(
        settings.value(QStringLiteral("cheatsearchwidget/parsehex")).toBool());
    value_layout->addWidget(m_parse_values_as_hex_checkbox);
  }

  auto* button_layout = new QHBoxLayout();
  m_next_scan_button = new QPushButton(tr("Search and Filter"));
  button_layout->addWidget(m_next_scan_button);
  m_refresh_values_button = new QPushButton(tr("Refresh Current Values"));
  button_layout->addWidget(m_refresh_values_button);
  m_reset_button = new QPushButton(tr("Reset Results"));
  button_layout->addWidget(m_reset_button);

  m_address_table = new QTableWidget();
  m_address_table->setContextMenuPolicy(Qt::CustomContextMenu);

  m_info_label_1 = new QLabel(tr("Waiting for first scan..."));
  m_info_label_2 = new QLabel();

  auto* const checkboxes_layout = new QHBoxLayout();
  m_display_values_in_hex_checkbox = new QCheckBox(tr("Display values in Hex"));
  m_display_values_in_hex_checkbox->setChecked(
      settings.value(QStringLiteral("cheatsearchwidget/displayhex")).toBool());
  checkboxes_layout->addWidget(m_display_values_in_hex_checkbox);
  checkboxes_layout->setStretchFactor(m_display_values_in_hex_checkbox, 1);

  m_autoupdate_current_values = new QCheckBox(tr("Automatically update Current Values"));
  m_autoupdate_current_values->setChecked(
      settings.value(QStringLiteral("cheatsearchwidget/autoupdatecurrentvalues"), true).toBool());
  checkboxes_layout->addWidget(m_autoupdate_current_values);
  checkboxes_layout->setStretchFactor(m_autoupdate_current_values, 2);

  QVBoxLayout* layout = new QVBoxLayout();
  layout->addWidget(session_info_label);
  layout->addWidget(instructions_label);
  layout->addLayout(value_layout);
  layout->addLayout(button_layout);
  layout->addLayout(checkboxes_layout);
  layout->addWidget(m_info_label_1);
  layout->addWidget(m_info_label_2);
  layout->addWidget(m_address_table);

  WrapInScrollArea(this, layout);
}

void CheatSearchWidget::ConnectWidgets()
{
  connect(m_next_scan_button, &QPushButton::clicked, this, &CheatSearchWidget::OnNextScanClicked);
  connect(m_refresh_values_button, &QPushButton::clicked, this,
          &CheatSearchWidget::OnRefreshClicked);
  connect(m_reset_button, &QPushButton::clicked, this, &CheatSearchWidget::OnResetClicked);
  connect(m_address_table, &QTableWidget::itemChanged, this,
          &CheatSearchWidget::OnAddressTableItemChanged);
  connect(m_address_table, &QTableWidget::customContextMenuRequested, this,
          &CheatSearchWidget::OnAddressTableContextMenu);
  connect(m_value_source_dropdown, &QComboBox::currentTextChanged, this,
          &CheatSearchWidget::OnValueSourceChanged);
  connect(m_display_values_in_hex_checkbox, &QCheckBox::toggled, this,
          &CheatSearchWidget::OnDisplayHexCheckboxStateChanged);
  connect(m_autoupdate_current_values, &QCheckBox::toggled, this,
          &CheatSearchWidget::OnAutoupdateToggled);
  connect(Host::GetInstance(), &Host::UpdateDisasmDialog, this,
          &CheatSearchWidget::OnUpdateDisasmDialog);
}

void CheatSearchWidget::OnNextScanClicked()
{
  const bool had_old_results = m_last_value_session->WasFirstSearchDone();

  const auto filter_type = m_value_source_dropdown->currentData().value<Cheats::FilterType>();
  if (filter_type == Cheats::FilterType::CompareAgainstLastValue && !had_old_results)
  {
    m_info_label_1->setText(tr("Cannot compare against last value on first search."));
    return;
  }
  m_last_value_session->SetFilterType(filter_type);
  m_last_value_session->SetCompareType(
      m_compare_type_dropdown->currentData().value<Cheats::CompareType>());
  if (filter_type == Cheats::FilterType::CompareAgainstSpecificValue)
  {
    QString search_value = m_given_value_text->text();
    if (m_last_value_session->IsIntegerType() || m_last_value_session->IsFloatingType())
      search_value = search_value.simplified().remove(QLatin1Char(' '));
    if (!m_last_value_session->SetValueFromString(search_value.toStdString(),
                                                  m_parse_values_as_hex_checkbox->isChecked()))
    {
      m_info_label_1->setText(tr("Failed to parse given value into target data type."));
      return;
    }
  }

  const size_t old_count = m_last_value_session->GetResultCount();
  const Cheats::SearchErrorCode error_code =
      m_last_value_session->RunSearch(Core::CPUThreadGuard{m_system});

  if (error_code == Cheats::SearchErrorCode::Success)
  {
    // The previous Autoupdate callback (if any) might be for the wrong set of addresses. Remove it
    // now so it doesn't generate an Autoupdate after we clear out any pending ones but before we
    // refresh the callback.
    RemoveAutoupdateCallback();
    // Any pending Autoupdates will have data older than the search we just did, so discard them.
    RemovePendingAutoupdates();

    UpdateCurrentValueSessionAddressesAndValues();
    RefreshAutoupdateCallback();

    const size_t new_count = m_last_value_session->GetResultCount();
    if (old_count == new_count)
    {
      // We only need to recreate the table if the set of displayed addresses has changed. Since
      // old_count == new_count that isn't the case, because the initial scan can only add new
      // addresses and followup scans can only remove old ones.
      UpdateTableLastAndCurrentValues();
    }
    else
    {
      RecreateTable();
    }

    const size_t new_valid_count = m_last_value_session->GetValidValueCount();
    m_info_label_1->setText(tr("Scan succeeded."));

    if (had_old_results)
    {
      const QString removed_str =
          tr("%n address(es) were removed.", "", static_cast<int>(old_count - new_count));
      const QString remain_str = tr("%n address(es) remain.", "", static_cast<int>(new_count));

      if (new_valid_count == new_count)
      {
        m_info_label_2->setText(tr("%1 %2").arg(removed_str).arg(remain_str));
      }
      else
      {
        const QString inaccessible_str =
            tr("%n address(es) could not be accessed in emulated memory.", "",
               static_cast<int>(new_count - new_valid_count));

        m_info_label_2->setText(
            tr("%1 %2 %3").arg(removed_str).arg(remain_str).arg(inaccessible_str));
      }
    }
    else
    {
      const QString found_str = tr("Found %n address(es).", "", static_cast<int>(new_count));

      if (new_valid_count == new_count)
      {
        m_info_label_2->setText(found_str);
      }
      else
      {
        const QString inaccessible_str =
            tr("%n address(es) could not be accessed in emulated memory.", "",
               static_cast<int>(new_count - new_valid_count));

        m_info_label_2->setText(tr("%1 %2").arg(found_str).arg(inaccessible_str));
      }
    }
  }
  else
  {
    DisplaySharedSearchErrorMessage(error_code);
  }
}

// Most error messages show the same text whether they were filtering scans or refreshes
void CheatSearchWidget::DisplaySharedSearchErrorMessage(const Cheats::SearchErrorCode result)
{
  switch (result)
  {
  case Cheats::SearchErrorCode::NoEmulationActive:
    m_info_label_1->setText(tr("No game is running."));
    return;
  case Cheats::SearchErrorCode::InvalidParameters:
    m_info_label_1->setText(tr("Invalid parameters given to search."));
    return;
  case Cheats::SearchErrorCode::VirtualAddressesCurrentlyNotAccessible:
    m_info_label_1->setText(tr("Search currently not possible in virtual address space. Please run "
                               "the game for a bit and try again."));
    return;
  case Cheats::SearchErrorCode::DisabledInHardcoreMode:
    m_info_label_1->setText(
        tr("Cheat Search is disabled when Hardcore mode is enabled in the Achievements window."));
    return;
  case Cheats::SearchErrorCode::NoRemainingAddresses:
    m_info_label_1->setText(tr("No addresses left. Press 'Reset Results' to start a new search."));
    return;
  case Cheats::SearchErrorCode::Success:
    // Scans and refreshes show different messages for successful searches.
    return;
  default:
    m_info_label_1->setText(tr("Unknown error occurred."));
    return;
  }
}

Cheats::SearchErrorCode CheatSearchWidget::UpdateCurrentValueSessionAndTable()
{
  if (m_current_value_session->WasFirstSearchDone() &&
      m_current_value_session->GetResultCount() == 0)
    return Cheats::SearchErrorCode::NoRemainingAddresses;
  const Cheats::SearchErrorCode result =
      m_current_value_session->RunSearch(Core::CPUThreadGuard{m_system});
  if (result == Cheats::SearchErrorCode::Success)
    UpdateTableCurrentValues();

  return result;
}

void CheatSearchWidget::OnRefreshClicked()
{
  if (!m_current_value_session->WasFirstSearchDone())
  {
    m_info_label_1->setText(tr("Cannot refresh current values before a search is run."));
    return;
  }
  const Cheats::SearchErrorCode refresh_result = UpdateCurrentValueSessionAndTable();
  switch (refresh_result)
  {
  case Cheats::SearchErrorCode::Success:
    m_info_label_1->setText(tr("Refreshed current values."));
    return;
  default:
    DisplaySharedSearchErrorMessage(refresh_result);
    return;
  }
}

void CheatSearchWidget::OnResetClicked()
{
  RemoveAutoupdateCallback();
  RemovePendingAutoupdates();

  m_last_value_session->ResetResults();
  m_current_value_session->ResetResults();

  RecreateTable();

  m_info_label_1->setText(tr("Waiting for first scan..."));
  m_info_label_2->clear();
}

void CheatSearchWidget::OnAddressTableItemChanged(QTableWidgetItem* item)
{
  const u32 address = item->data(ADDRESS_TABLE_ADDRESS_ROLE).toUInt();
  const int column = item->column();

  switch (column)
  {
  case ADDRESS_TABLE_COLUMN_INDEX_DESCRIPTION:
  {
    m_address_table_user_data[address].m_description = item->text().toStdString();
    break;
  }
  default:
    break;
  }
}

void CheatSearchWidget::UpdateCurrentValueSessionAddressesAndValues()
{
  m_current_value_session = m_last_value_session->ClonePartial(0, GetTableRowCount());
  m_current_value_session->SetFilterType(Cheats::FilterType::DoNotFilter);
}

void CheatSearchWidget::OnAutoupdate(Cheats::CheatSearchSessionBase* const autoupdate_session)
{
  m_current_value_session.reset(autoupdate_session);
  UpdateTableCurrentValues();
}

void CheatSearchWidget::OnAutoupdateToggled(const bool enabled)
{
  if (enabled && Core::GetState(m_system) == Core::State::Paused)
  {
    // When enabling Autoupdates the Current Values need to be updated. If the game is running the
    // next Autoupdate will handle it, but if it's paused then update them now.
    AutoupdateAllCurrentValueRowsIfNeeded();
  }

  RefreshAutoupdateCallback();
}

// When pausing or a breakpoint is triggered, all CheatSearchWidgets with Autoupdates enabled run a
// scan to ensure their Current Values are up to date. If this widget has any queued
// AutoupdateEvents the data in their sessions may be outdated, so prevent them from updating the
// table afterward.
void CheatSearchWidget::RemovePendingAutoupdates()
{
  QCoreApplication::removePostedEvents(this, AutoupdateEvent::GetType());
}

void CheatSearchWidget::OnUpdateDisasmDialog()
{
  const bool pause_or_breakpoint_triggered = Core::GetState(m_system) == Core::State::Paused;
  if (pause_or_breakpoint_triggered)
    AutoupdateAllCurrentValueRowsIfNeeded();
}

void CheatSearchWidget::AutoupdateAllCurrentValueRowsIfNeeded()
{
  if (m_autoupdate_current_values->isChecked() && m_current_value_session->WasFirstSearchDone())
  {
    // We don't update the status text for automatically triggered searches, so the return value can
    // be ignored.
    static_cast<void>(UpdateCurrentValueSessionAndTable());
    // The data in any pending Autoupdates is out of date, so remove them to prevent them from
    // overwriting the scan we just did.
    RemovePendingAutoupdates();
  }
}

int CheatSearchWidget::GetTableRowCount() const
{
  return std::min(TABLE_MAX_ROWS, static_cast<int>(m_last_value_session->GetResultCount()));
}

void CheatSearchWidget::RefreshAutoupdateCallback()
{
  // Autoupdates have several design requirements:
  //  * Minimize the performance impact by having the CPU thread update CheatSearch session values
  //    when the frame ends instead of during it.
  //  * Update the Current Values in CheatSearchWidget's table from the Host thread, both to avoid
  //    threading issues caused by accessing QWidget-derived classes from a thread that isn't the
  //    Host thread, and so the CPU thread doesn't have to spend time updating the table.
  //  * Minimize lock contention between the CPU and Host threads.
  //
  // While the game is actively running, only the currently visible CheatSearchWidget with updates
  // enabled (if any) will receive them. Skipped updates to other widgets won't be visible until the
  // user switches to a different widget, at which point the newly shown widget will be updated by
  // the next Autoupdate in a small fraction of a second.
  //
  // When the game is paused or a breakpoint is triggered, all CheatSearchWidgets with Autoupdates
  // enabled will be updated. This is because newly shown widgets will no longer receive updates
  // immediately after and so their tables need to be updated manually so they'll display the
  // correct values when the user switches to them.
  //
  // The overall flow of the system is:
  //  * When a CheatSearchWidget with Autoupdates enabled and a nonempty set of addresses is
  //    visible, the Host thread registers a lambda callback for the VIEndFieldEvent which will run
  //    on the CPU thread at the end of each frame. The lambda contains a CheatSearchSession with
  //    the set of addresses that need an Autoupdate.
  //  * When the VIEndFieldEvent is triggered, the CPU thread runs a scan on those addresses to
  //    get their new values. These are passed via postEvent to Qt's event loop on the Host thread,
  //    which calls OnAutoupdate with the session.
  //  * OnAutoupdate replaces the CheatSearchWidget's m_current_value_session with the one from the
  //    Autoupdate and updates the table.
  //  * The VIEndFieldEvent needs to be removed (and maybe replaced with a new one with an updated
  //    set of addresses) when certain events cause the set of visible addresses to change. These
  //    are: when a CheatSearchWidget is shown or hidden, the Reset Results button is pressed,
  //    Autoupdates are enabled or disabled for the shown widget, or a filtering scan is run.
  //
  // Thread safety is handled by the VIEndFieldEvent::Register and ::Trigger functions (which lock a
  // mutex preventing the callback from being changed while it's running and vice versa) and the
  // call to postEvent with the updated session. In particular, the CPU thread should rarely have to
  // wait on the Host thread, and even then only for long enough to add a new lambda to the
  // VIEndFieldEvent callback list.

  RemoveAutoupdateCallback();

  if (!m_autoupdate_current_values->isChecked() || m_current_value_session->GetResultCount() == 0)
    return;

  // This can't be a unique_ptr because the lambda is passed to VIEndFieldEvent::Register, which
  // converts it to a std::function which requires the lambda to be CopyConstructible.
  // TODO: Convert this to unique_ptr if/when HookableEvent is converted to std::move_only_function.
  std::shared_ptr<Cheats::CheatSearchSessionBase> autoupdate_session_shared =
      m_current_value_session->Clone();

  const auto frame_end_callback = [this, system = std::ref(m_system),
                                   autoupdate_session = std::move(autoupdate_session_shared)]() {
    const Cheats::SearchErrorCode error_code =
        autoupdate_session->RunSearch(Core::CPUThreadGuard{system});

    if (error_code != Cheats::SearchErrorCode::Success)
      return;

    const auto autoupdate_event = new AutoupdateEvent(autoupdate_session->Clone());

    // Posting the AutoupdateEvent for this CheatSearchWidget is safe even if the widget gets
    // deleted by the Host thread while this callback is being run on the CPU thread.
    // m_VI_end_field_event's destructor will block while acquiring VIEndFieldEvent's storage lock
    // until the event's callbacks are finished running on the CPU thread, at which point the
    // CheatSearchWidget's inherited QObject destructor will disconnect all its signals and remove
    // any pending events from the event queue that target it. This prevents the AutoupdateEvent
    // from running on a destroyed object.
    QCoreApplication::postEvent(this, autoupdate_event);
  };

  m_VI_end_field_event = VIEndFieldEvent::Register(frame_end_callback, "CheatSearchWidget");
}

// Any action that might change which addresses need to be Autoupdated (such as filtering scans,
// resets, or CheatSearchWidget visibility changes) should call this function first. Actions that
// only update the values for those addresses don't need to call this.
void CheatSearchWidget::RemoveAutoupdateCallback()
{
  m_VI_end_field_event.reset();
}

void CheatSearchWidget::OnAddressTableContextMenu()
{
  if (m_address_table->selectedItems().isEmpty())
    return;

  auto* item = m_address_table->selectedItems()[0];
  const u32 address = item->data(ADDRESS_TABLE_ADDRESS_ROLE).toUInt();

  QMenu* menu = new QMenu(this);
  menu->setAttribute(Qt::WA_DeleteOnClose, true);

  menu->addAction(tr("Show in memory"), [this, address] { emit ShowMemory(address); });
  menu->addAction(tr("Add to watch"), this, [this, address] {
    const QString name = QStringLiteral("mem_%1").arg(address, 8, 16, QLatin1Char('0'));
    emit RequestWatch(name, address);
  });
  menu->addAction(tr("Generate Action Replay Code(s)"), this, &CheatSearchWidget::GenerateARCodes);

  menu->exec(QCursor::pos());
}

void CheatSearchWidget::OnValueSourceChanged()
{
  const auto filter_type = m_value_source_dropdown->currentData().value<Cheats::FilterType>();
  const bool is_value_search = filter_type == Cheats::FilterType::CompareAgainstSpecificValue;
  m_given_value_text->setEnabled(is_value_search);
  m_parse_values_as_hex_checkbox->setEnabled(is_value_search &&
                                             m_last_value_session->IsIntegerType());
}

void CheatSearchWidget::OnDisplayHexCheckboxStateChanged()
{
  if (!m_last_value_session->WasFirstSearchDone())
    return;

  UpdateTableLastAndCurrentValues();
}

void CheatSearchWidget::GenerateARCodes()
{
  if (m_address_table->selectedItems().isEmpty())
    return;

  bool had_success = false;
  bool had_error = false;
  std::optional<Cheats::GenerateActionReplayCodeErrorCode> error_code;

  for (auto* const item : m_address_table->selectedItems())
  {
    const u32 index = item->data(ADDRESS_TABLE_RESULT_INDEX_ROLE).toUInt();
    auto result = Cheats::GenerateActionReplayCode(*m_last_value_session, index);
    if (result)
    {
      emit ActionReplayCodeGenerated(*result);
      had_success = true;
    }
    else
    {
      const auto new_error_code = result.Error();
      if (!had_error)
      {
        error_code = new_error_code;
      }
      else if (error_code != new_error_code)
      {
        // If we have a different error code signify multiple errors with an empty optional<>.
        error_code.reset();
      }

      had_error = true;
    }
  }

  if (had_error)
  {
    if (error_code.has_value())
    {
      switch (*error_code)
      {
      case Cheats::GenerateActionReplayCodeErrorCode::NotVirtualMemory:
        m_info_label_1->setText(tr("Can only generate AR code for values in virtual memory."));
        break;
      case Cheats::GenerateActionReplayCodeErrorCode::InvalidAddress:
        m_info_label_1->setText(tr("Cannot generate AR code for this address."));
        break;
      default:
        m_info_label_1->setText(tr("Internal error while generating AR code."));
        break;
      }
    }
    else
    {
      m_info_label_1->setText(tr("Multiple errors while generating AR codes."));
    }
  }
  else if (had_success)
  {
    m_info_label_1->setText(tr("Generated AR code(s)."));
  }
}

QString CheatSearchWidget::GetValueStringFromSessionIndex(
    const std::unique_ptr<Cheats::CheatSearchSessionBase>& session, const int index) const
{
  const bool show_in_hex = m_display_values_in_hex_checkbox->isChecked();
  return QString::fromStdString(session->GetResultValueAsString(index, show_in_hex));
}

void CheatSearchWidget::UpdateTableLastAndCurrentValues()
{
  const int result_count_to_display = GetTableRowCount();

  for (int i = 0; i < result_count_to_display; ++i)
  {
    QTableWidgetItem* const last_value_table_item =
        m_address_table->item(i, ADDRESS_TABLE_COLUMN_INDEX_LAST_VALUE);
    if (last_value_table_item == nullptr)
      return;

    const auto value_string = GetValueStringFromSessionIndex(m_last_value_session, i);
    last_value_table_item->setText(value_string);
  }

  UpdateTableCurrentValues();
}

void CheatSearchWidget::UpdateTableCurrentValues()
{
  const int session_result_count = static_cast<int>(m_current_value_session->GetResultCount());
  for (int table_row = 0; table_row < session_result_count; ++table_row)
  {
    QTableWidgetItem* const current_value_table_item =
        m_address_table->item(table_row, ADDRESS_TABLE_COLUMN_INDEX_CURRENT_VALUE);
    if (current_value_table_item == nullptr)
      return;

    const auto value_string = GetValueStringFromSessionIndex(m_current_value_session, table_row);
    current_value_table_item->setText(value_string);
  }
}

void CheatSearchWidget::RecreateTable()
{
  const QSignalBlocker blocker(m_address_table);

  m_address_table->clear();
  m_address_table->setColumnCount(4);
  m_address_table->setHorizontalHeaderLabels(
      {tr("Description"), tr("Address"), tr("Last Value"), tr("Current Value")});

  const int result_count_to_display = GetTableRowCount();
  m_address_table->setRowCount(result_count_to_display);

  for (int i = 0; i < result_count_to_display; ++i)
  {
    const u32 address = m_last_value_session->GetResultAddress(i);
    const auto user_data_it = m_address_table_user_data.find(address);
    const bool has_user_data = user_data_it != m_address_table_user_data.end();

    auto* description_item = new QTableWidgetItem();
    description_item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable);
    if (has_user_data)
      description_item->setText(QString::fromStdString(user_data_it->second.m_description));
    description_item->setData(ADDRESS_TABLE_ADDRESS_ROLE, address);
    description_item->setData(ADDRESS_TABLE_RESULT_INDEX_ROLE, static_cast<u32>(i));
    m_address_table->setItem(i, ADDRESS_TABLE_COLUMN_INDEX_DESCRIPTION, description_item);

    auto* address_item = new QTableWidgetItem();
    address_item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    address_item->setText(QStringLiteral("0x%1").arg(address, 8, 16, QLatin1Char('0')));
    address_item->setData(ADDRESS_TABLE_ADDRESS_ROLE, address);
    address_item->setData(ADDRESS_TABLE_RESULT_INDEX_ROLE, static_cast<u32>(i));
    m_address_table->setItem(i, ADDRESS_TABLE_COLUMN_INDEX_ADDRESS, address_item);

    const bool show_in_hex = m_display_values_in_hex_checkbox->isChecked();
    auto* last_value_item = new QTableWidgetItem();
    last_value_item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    last_value_item->setText(
        QString::fromStdString(m_last_value_session->GetResultValueAsString(i, show_in_hex)));
    last_value_item->setData(ADDRESS_TABLE_ADDRESS_ROLE, address);
    last_value_item->setData(ADDRESS_TABLE_RESULT_INDEX_ROLE, static_cast<u32>(i));
    m_address_table->setItem(i, ADDRESS_TABLE_COLUMN_INDEX_LAST_VALUE, last_value_item);

    auto* current_value_item = new QTableWidgetItem();
    current_value_item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    current_value_item->setText(
        QString::fromStdString(m_current_value_session->GetResultValueAsString(i, show_in_hex)));
    current_value_item->setData(ADDRESS_TABLE_ADDRESS_ROLE, address);
    current_value_item->setData(ADDRESS_TABLE_RESULT_INDEX_ROLE, static_cast<u32>(i));
    m_address_table->setItem(i, ADDRESS_TABLE_COLUMN_INDEX_CURRENT_VALUE, current_value_item);
  }
}
