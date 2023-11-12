// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Settings/InterfacePane.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QRadioButton>
#include <QVBoxLayout>
#include <QWidget>

#include "Common/CommonPaths.h"
#include "Common/FileSearch.h"
#include "Common/FileUtil.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"

#include "Core/AchievementManager.h"
#include "Core/Config/MainSettings.h"
#include "Core/Config/UISettings.h"

#include "DolphinQt/Config/ConfigControls/ConfigBool.h"
#include "DolphinQt/Config/ConfigControls/ConfigChoice.h"
#include "DolphinQt/Config/ToolTipControls/ToolTipCheckBox.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/QtUtils/SignalBlocking.h"
#include "DolphinQt/Settings.h"

#include "UICommon/GameFile.h"

static ConfigStringChoice* MakeLanguageComboBox()
{
  using QPair = std::pair<QString, QString>;
  std::vector<QPair> languages = {
      QPair{QObject::tr("<System Language>"), QString{}},
      QPair{QStringLiteral(u"Bahasa Melayu"), QStringLiteral("ms")},      // Malay
      QPair{QStringLiteral(u"Catal\u00E0"), QStringLiteral("ca")},        // Catalan
      QPair{QStringLiteral(u"\u010Ce\u0161tina"), QStringLiteral("cs")},  // Czech
      QPair{QStringLiteral(u"Dansk"), QStringLiteral("da")},              // Danish
      QPair{QStringLiteral(u"Deutsch"), QStringLiteral("de")},            // German
      QPair{QStringLiteral(u"English"), QStringLiteral("en")},            // English
      QPair{QStringLiteral(u"Espa\u00F1ol"), QStringLiteral("es")},       // Spanish
      QPair{QStringLiteral(u"Fran\u00E7ais"), QStringLiteral("fr")},      // French
      QPair{QStringLiteral(u"Hrvatski"), QStringLiteral("hr")},           // Croatian
      QPair{QStringLiteral(u"Italiano"), QStringLiteral("it")},           // Italian
      QPair{QStringLiteral(u"Magyar"), QStringLiteral("hu")},             // Hungarian
      QPair{QStringLiteral(u"Nederlands"), QStringLiteral("nl")},         // Dutch
      QPair{QStringLiteral(u"Norsk bokm\u00E5l"), QStringLiteral("nb")},  // Norwegian
      QPair{QStringLiteral(u"Polski"), QStringLiteral("pl")},             // Polish
      QPair{QStringLiteral(u"Portugu\u00EAs"), QStringLiteral("pt")},     // Portuguese
      QPair{QStringLiteral(u"Portugu\u00EAs (Brasil)"),
            QStringLiteral("pt_BR")},                                    // Portuguese (Brazil)
      QPair{QStringLiteral(u"Rom\u00E2n\u0103"), QStringLiteral("ro")},  // Romanian
      QPair{QStringLiteral(u"Srpski"), QStringLiteral("sr")},            // Serbian
      QPair{QStringLiteral(u"Suomi"), QStringLiteral("fi")},             // Finnish
      QPair{QStringLiteral(u"Svenska"), QStringLiteral("sv")},           // Swedish
      QPair{QStringLiteral(u"T\u00FCrk\u00E7e"), QStringLiteral("tr")},  // Turkish
      QPair{QStringLiteral(u"\u0395\u03BB\u03BB\u03B7\u03BD\u03B9\u03BA\u03AC"),
            QStringLiteral("el")},  // Greek
      QPair{QStringLiteral(u"\u0420\u0443\u0441\u0441\u043A\u0438\u0439"),
            QStringLiteral("ru")},  // Russian
      QPair{QStringLiteral(u"\u0627\u0644\u0639\u0631\u0628\u064A\u0629"),
            QStringLiteral("ar")},                                                     // Arabic
      QPair{QStringLiteral(u"\u0641\u0627\u0631\u0633\u06CC"), QStringLiteral("fa")},  // Farsi
      QPair{QStringLiteral(u"\uD55C\uAD6D\uC5B4"), QStringLiteral("ko")},              // Korean
      QPair{QStringLiteral(u"\u65E5\u672C\u8A9E"), QStringLiteral("ja")},              // Japanese
      QPair{QStringLiteral(u"\u7B80\u4F53\u4E2D\u6587"),
            QStringLiteral("zh_CN")},  // Simplified Chinese
      QPair{QStringLiteral(u"\u7E41\u9AD4\u4E2D\u6587"),
            QStringLiteral("zh_TW")},  // Traditional Chinese
  };

  auto* const combobox = new ConfigStringChoice(languages, Config::MAIN_INTERFACE_LANGUAGE);

  // The default, QComboBox::AdjustToContentsOnFirstShow, causes a noticeable pause when opening the
  // SettingWindow for the first time. The culprit seems to be non-Latin graphemes in the above
  // list. QComboBox::AdjustToContents still has some lag but it's much less noticeable.
  combobox->setSizeAdjustPolicy(QComboBox::AdjustToContents);

  return combobox;
}

InterfacePane::InterfacePane(QWidget* parent) : QWidget(parent)
{
  CreateLayout();
  LoadConfig();
  ConnectLayout();

  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this,
          &InterfacePane::UpdateShowDebuggingCheckbox);
}

void InterfacePane::CreateLayout()
{
  m_main_layout = new QVBoxLayout;
  // Create layout here
  CreateUI();
  CreateInGame();
  AddDescriptions();

  m_main_layout->addStretch(1);
  setLayout(m_main_layout);
}

void InterfacePane::CreateUI()
{
  auto* groupbox = new QGroupBox(tr("User Interface"));
  auto* groupbox_layout = new QVBoxLayout;
  groupbox->setLayout(groupbox_layout);
  m_main_layout->addWidget(groupbox);

  auto* combobox_layout = new QFormLayout;
  combobox_layout->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
  combobox_layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

  groupbox_layout->addLayout(combobox_layout);

  m_combobox_language = MakeLanguageComboBox();
  combobox_layout->addRow(tr("&Language:"), m_combobox_language);

  // List avalable themes
  auto theme_paths =
      Common::DoFileSearch({File::GetUserPath(D_THEMES_IDX), File::GetSysDirectory() + THEMES_DIR});
  std::vector<std::string> theme_names;
  theme_names.reserve(theme_paths.size());
  std::transform(theme_paths.cbegin(), theme_paths.cend(), std::back_inserter(theme_names),
                 PathToFileName);

  // Theme Combobox
  m_combobox_theme = new ConfigStringChoice(theme_names, Config::MAIN_THEME_NAME);
  combobox_layout->addRow(tr("&Theme:"), m_combobox_theme);

  // User Style Combobox
  m_combobox_userstyle = new QComboBox;
  m_label_userstyle = new QLabel(tr("Style:"));
  combobox_layout->addRow(m_label_userstyle, m_combobox_userstyle);

  auto userstyle_search_results = Common::DoFileSearch({File::GetUserPath(D_STYLES_IDX)});

  m_combobox_userstyle->addItem(tr("(System)"), static_cast<int>(Settings::StyleType::System));

  // TODO: Support forcing light/dark on other OSes too.
#ifdef _WIN32
  m_combobox_userstyle->addItem(tr("(Light)"), static_cast<int>(Settings::StyleType::Light));
  m_combobox_userstyle->addItem(tr("(Dark)"), static_cast<int>(Settings::StyleType::Dark));
#endif

  for (const std::string& path : userstyle_search_results)
  {
    const QFileInfo file_info(QString::fromStdString(path));
    m_combobox_userstyle->addItem(file_info.completeBaseName(), file_info.fileName());
  }

  // Checkboxes
  m_checkbox_use_builtin_title_database = new ConfigBool(tr("Use Built-In Database of Game Names"),
                                                         Config::MAIN_USE_BUILT_IN_TITLE_DATABASE);
  m_checkbox_use_covers =
      new QCheckBox(tr("Download Game Covers from GameTDB.com for Use in Grid Mode"));
  m_checkbox_show_debugging_ui = new ToolTipCheckBox(tr("Enable Debugging UI"));
  m_checkbox_focused_hotkeys = new QCheckBox(tr("Hotkeys Require Window Focus"));
  m_checkbox_disable_screensaver = new QCheckBox(tr("Inhibit Screensaver During Emulation"));

  groupbox_layout->addWidget(m_checkbox_use_builtin_title_database);
  groupbox_layout->addWidget(m_checkbox_use_covers);
  groupbox_layout->addWidget(m_checkbox_show_debugging_ui);
  groupbox_layout->addWidget(m_checkbox_focused_hotkeys);
  groupbox_layout->addWidget(m_checkbox_disable_screensaver);
}

void InterfacePane::CreateInGame()
{
  auto* groupbox = new QGroupBox(tr("Render Window"));
  auto* groupbox_layout = new QVBoxLayout;
  groupbox->setLayout(groupbox_layout);
  m_main_layout->addWidget(groupbox);

  m_checkbox_top_window = new ConfigBool(tr("Keep Window on Top"), Config::MAIN_KEEP_WINDOW_ON_TOP);
  m_checkbox_confirm_on_stop = new QCheckBox(tr("Confirm on Stop"));
  m_checkbox_use_panic_handlers = new QCheckBox(tr("Use Panic Handlers"));
  m_checkbox_enable_osd = new QCheckBox(tr("Show On-Screen Display Messages"));
  m_checkbox_show_active_title = new QCheckBox(tr("Show Active Title in Window Title"));
  m_checkbox_pause_on_focus_lost = new QCheckBox(tr("Pause on Focus Loss"));

  auto* mouse_groupbox = new QGroupBox(tr("Mouse Cursor Visibility"));
  auto* m_vboxlayout_hide_mouse = new QVBoxLayout;
  mouse_groupbox->setLayout(m_vboxlayout_hide_mouse);

  m_radio_cursor_visible_movement = new QRadioButton(tr("On Movement"));
  m_radio_cursor_visible_movement->setToolTip(
      tr("Mouse Cursor hides after inactivity and returns upon Mouse Cursor movement."));
  m_radio_cursor_visible_never = new QRadioButton(tr("Never"));
  m_radio_cursor_visible_never->setToolTip(
      tr("Mouse Cursor will never be visible while a game is running."));
  m_radio_cursor_visible_always = new QRadioButton(tr("Always"));
  m_radio_cursor_visible_always->setToolTip(tr("Mouse Cursor will always be visible."));

  m_vboxlayout_hide_mouse->addWidget(m_radio_cursor_visible_movement);
  m_vboxlayout_hide_mouse->addWidget(m_radio_cursor_visible_never);
  m_vboxlayout_hide_mouse->addWidget(m_radio_cursor_visible_always);

  // this ends up not being managed unless _WIN32, so lets not leak
  m_checkbox_lock_mouse = new QCheckBox(tr("Lock Mouse Cursor"), this);
  m_checkbox_lock_mouse->setToolTip(tr("Will lock the Mouse Cursor to the Render Widget as long as "
                                       "it has focus. You can set a hotkey to unlock it."));

  mouse_groupbox->setLayout(m_vboxlayout_hide_mouse);
  groupbox_layout->addWidget(m_checkbox_top_window);
  groupbox_layout->addWidget(m_checkbox_confirm_on_stop);
  groupbox_layout->addWidget(m_checkbox_use_panic_handlers);
  groupbox_layout->addWidget(m_checkbox_enable_osd);
  groupbox_layout->addWidget(m_checkbox_show_active_title);
  groupbox_layout->addWidget(m_checkbox_pause_on_focus_lost);
  groupbox_layout->addWidget(mouse_groupbox);
#ifdef _WIN32
  groupbox_layout->addWidget(m_checkbox_lock_mouse);
#else
  m_checkbox_lock_mouse->hide();
#endif
}

void InterfacePane::ConnectLayout()
{
  connect(m_checkbox_use_builtin_title_database, &QCheckBox::toggled, &Settings::Instance(),
          &Settings::GameListRefreshRequested);
  connect(m_checkbox_use_covers, &QCheckBox::toggled, this, &InterfacePane::OnSaveConfig);
  connect(m_checkbox_disable_screensaver, &QCheckBox::toggled, this, &InterfacePane::OnSaveConfig);
  connect(m_checkbox_show_debugging_ui, &QCheckBox::toggled, &Settings::Instance(),
          &Settings::SetDebugModeEnabled);
  connect(m_checkbox_focused_hotkeys, &QCheckBox::toggled, this, &InterfacePane::OnSaveConfig);
  connect(m_combobox_theme, &QComboBox::currentIndexChanged, this,
          [this](int index) { Settings::Instance().TriggerThemeChanged(); });
  connect(m_combobox_userstyle, &QComboBox::currentIndexChanged, this,
          &InterfacePane::OnSaveConfig);
  connect(m_combobox_language, &QComboBox::currentIndexChanged, this,
          [this]() { OnLanguageChanged(); });
  connect(m_checkbox_top_window, &QCheckBox::toggled, &Settings::Instance(),
          &Settings::KeepWindowOnTopChanged);
  connect(m_checkbox_confirm_on_stop, &QCheckBox::toggled, this, &InterfacePane::OnSaveConfig);
  connect(m_checkbox_use_panic_handlers, &QCheckBox::toggled, this, &InterfacePane::OnSaveConfig);
  connect(m_checkbox_show_active_title, &QCheckBox::toggled, this, &InterfacePane::OnSaveConfig);
  connect(m_checkbox_enable_osd, &QCheckBox::toggled, this, &InterfacePane::OnSaveConfig);
  connect(m_checkbox_pause_on_focus_lost, &QCheckBox::toggled, this, &InterfacePane::OnSaveConfig);
  connect(m_radio_cursor_visible_movement, &QRadioButton::toggled, this,
          &InterfacePane::OnCursorVisibleMovement);
  connect(m_radio_cursor_visible_never, &QRadioButton::toggled, this,
          &InterfacePane::OnCursorVisibleNever);
  connect(m_radio_cursor_visible_always, &QRadioButton::toggled, this,
          &InterfacePane::OnCursorVisibleAlways);
  connect(m_checkbox_lock_mouse, &QCheckBox::toggled, &Settings::Instance(),
          &Settings::SetLockCursor);
}

void InterfacePane::UpdateShowDebuggingCheckbox()
{
  SignalBlocking(m_checkbox_show_debugging_ui)
      ->setChecked(Settings::Instance().IsDebugModeEnabled());

  static constexpr char TR_SHOW_DEBUGGING_UI_DESCRIPTION[] = QT_TR_NOOP(
      "Shows Dolphin's debugging User Interface. This lets you view and modify a game's code and "
      "memory contents, set debugging breakpoints, examine network requests, and more."
      "<br><br><dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>");
  static constexpr char TR_DISABLED_IN_HARDCORE_DESCRIPTION[] =
      QT_TR_NOOP("<dolphin_emphasis>Disabled in Hardcore Mode.</dolphin_emphasis>");

#ifdef USE_RETRO_ACHIEVEMENTS
  bool hardcore = AchievementManager::GetInstance().IsHardcoreModeActive();
  SignalBlocking(m_checkbox_show_debugging_ui)->setEnabled(!hardcore);
  if (hardcore)
  {
    m_checkbox_show_debugging_ui->SetDescription(tr("%1<br><br>%2")
                                                     .arg(tr(TR_SHOW_DEBUGGING_UI_DESCRIPTION))
                                                     .arg(tr(TR_DISABLED_IN_HARDCORE_DESCRIPTION)));
  }
  else
  {
    m_checkbox_show_debugging_ui->SetDescription(tr(TR_SHOW_DEBUGGING_UI_DESCRIPTION));
  }
#else
  m_checkbox_show_debugging_ui->SetDescription(tr(TR_SHOW_DEBUGGING_UI_DESCRIPTION));
#endif  // USE_RETRO_ACHIEVEMENTS
}

void InterfacePane::LoadConfig()
{
  UpdateShowDebuggingCheckbox();

  const Settings::StyleType style_type = Settings::Instance().GetStyleType();
  const QString userstyle = Settings::Instance().GetUserStyleName();
  const int index = style_type == Settings::StyleType::User ?
                        m_combobox_userstyle->findData(userstyle) :
                        m_combobox_userstyle->findData(static_cast<int>(style_type));

  if (index > 0)
    SignalBlocking(m_combobox_userstyle)->setCurrentIndex(index);

  // Render Window Options
  SignalBlocking(m_checkbox_confirm_on_stop)->setChecked(Config::Get(Config::MAIN_CONFIRM_ON_STOP));
  SignalBlocking(m_checkbox_use_panic_handlers)
      ->setChecked(Config::Get(Config::MAIN_USE_PANIC_HANDLERS));
  SignalBlocking(m_checkbox_enable_osd)->setChecked(Config::Get(Config::MAIN_OSD_MESSAGES));
  SignalBlocking(m_checkbox_show_active_title)
      ->setChecked(Config::Get(Config::MAIN_SHOW_ACTIVE_TITLE));
  SignalBlocking(m_checkbox_pause_on_focus_lost)
      ->setChecked(Config::Get(Config::MAIN_PAUSE_ON_FOCUS_LOST));
  SignalBlocking(m_checkbox_use_covers)->setChecked(Config::Get(Config::MAIN_USE_GAME_COVERS));
  SignalBlocking(m_checkbox_focused_hotkeys)->setChecked(Config::Get(Config::MAIN_FOCUSED_HOTKEYS));
  SignalBlocking(m_radio_cursor_visible_movement)
      ->setChecked(Settings::Instance().GetCursorVisibility() == Config::ShowCursor::OnMovement);
  SignalBlocking(m_radio_cursor_visible_always)
      ->setChecked(Settings::Instance().GetCursorVisibility() == Config::ShowCursor::Constantly);
  SignalBlocking(m_radio_cursor_visible_never)
      ->setChecked(Settings::Instance().GetCursorVisibility() == Config::ShowCursor::Never);

  SignalBlocking(m_checkbox_lock_mouse)->setChecked(Settings::Instance().GetLockCursor());
  SignalBlocking(m_checkbox_disable_screensaver)
      ->setChecked(Config::Get(Config::MAIN_DISABLE_SCREENSAVER));
}

void InterfacePane::OnSaveConfig()
{
  const auto selected_style = m_combobox_userstyle->currentData();
  bool is_builtin_type = false;
  const int style_type_int = selected_style.toInt(&is_builtin_type);
  Settings::Instance().SetStyleType(is_builtin_type ?
                                        static_cast<Settings::StyleType>(style_type_int) :
                                        Settings::StyleType::User);
  if (!is_builtin_type)
    Settings::Instance().SetUserStyleName(selected_style.toString());
  Settings::Instance().ApplyStyle();

  // Render Window Options
  Config::SetBase(Config::MAIN_CONFIRM_ON_STOP, m_checkbox_confirm_on_stop->isChecked());
  Config::SetBase(Config::MAIN_USE_PANIC_HANDLERS, m_checkbox_use_panic_handlers->isChecked());
  Config::SetBase(Config::MAIN_OSD_MESSAGES, m_checkbox_enable_osd->isChecked());
  Config::SetBase(Config::MAIN_SHOW_ACTIVE_TITLE, m_checkbox_show_active_title->isChecked());
  Config::SetBase(Config::MAIN_PAUSE_ON_FOCUS_LOST, m_checkbox_pause_on_focus_lost->isChecked());

  const bool use_covers = m_checkbox_use_covers->isChecked();

  if (use_covers != Config::Get(Config::MAIN_USE_GAME_COVERS))
  {
    Config::SetBase(Config::MAIN_USE_GAME_COVERS, use_covers);
    Settings::Instance().RefreshMetadata();
  }

  Config::SetBase(Config::MAIN_FOCUSED_HOTKEYS, m_checkbox_focused_hotkeys->isChecked());
  Config::SetBase(Config::MAIN_DISABLE_SCREENSAVER, m_checkbox_disable_screensaver->isChecked());

  Config::Save();
}

void InterfacePane::OnCursorVisibleMovement()
{
  Settings::Instance().SetCursorVisibility(Config::ShowCursor::OnMovement);
}

void InterfacePane::OnCursorVisibleNever()
{
  Settings::Instance().SetCursorVisibility(Config::ShowCursor::Never);
}

void InterfacePane::OnCursorVisibleAlways()
{
  Settings::Instance().SetCursorVisibility(Config::ShowCursor::Constantly);
}

void InterfacePane::OnLanguageChanged()
{
  ModalMessageBox::information(
      this, tr("Restart Required"),
      tr("You must restart Dolphin in order for the change to take effect."));
}

void InterfacePane::AddDescriptions()
{
  static constexpr char TR_TITLE_DATABASE_DESCRIPTION[] = QT_TR_NOOP(
      "Uses Dolphin's database of properly formatted names in the Game List Title column."
      "<br><br><dolphin_emphasis>If unsure, leave this checked.</dolphin_emphasis>");
  static constexpr char TR_THEME_DESCRIPTION[] =
      QT_TR_NOOP("Changes the appearance and color of Dolphin's buttons."
                 "<br><br><dolphin_emphasis>If unsure, select Clean.</dolphin_emphasis>");
  static constexpr char TR_TOP_WINDOW_DESCRIPTION[] =
      QT_TR_NOOP("Forces the render window to stay on top of other windows and applications."
                 "<br><br><dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>");
  static constexpr char TR_LANGUAGE_DESCRIPTION[] = QT_TR_NOOP(
      "Sets the language displayed by Dolphin's User Interface."
      "<br><br>Changes to this setting only take effect once Dolphin is restarted."
      "<br><br><dolphin_emphasis>If unsure, select &lt;System Language&gt;.</dolphin_emphasis>");

  m_checkbox_use_builtin_title_database->SetDescription(tr(TR_TITLE_DATABASE_DESCRIPTION));

  m_combobox_theme->SetTitle(tr("Theme"));
  m_combobox_theme->SetDescription(tr(TR_THEME_DESCRIPTION));

  m_checkbox_top_window->SetDescription(tr(TR_TOP_WINDOW_DESCRIPTION));

  m_combobox_language->SetTitle(tr("Language"));
  m_combobox_language->SetDescription(tr(TR_LANGUAGE_DESCRIPTION));
}
