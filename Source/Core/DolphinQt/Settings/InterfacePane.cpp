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

#include "Core/Config/AchievementSettings.h"
#include "Core/Config/MainSettings.h"
#include "Core/Config/UISettings.h"

#include "DolphinQt/Config/ToolTipControls/ToolTipCheckBox.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/QtUtils/SignalBlocking.h"
#include "DolphinQt/Settings.h"

#include "UICommon/GameFile.h"

static QComboBox* MakeLanguageComboBox()
{
  static const struct
  {
    const QString name;
    const char* id;
  } languages[] = {
      {QStringLiteral(u"Bahasa Melayu"), "ms"},               // Malay
      {QStringLiteral(u"Catal\u00E0"), "ca"},                 // Catalan
      {QStringLiteral(u"\u010Ce\u0161tina"), "cs"},           // Czech
      {QStringLiteral(u"Dansk"), "da"},                       // Danish
      {QStringLiteral(u"Deutsch"), "de"},                     // German
      {QStringLiteral(u"English"), "en"},                     // English
      {QStringLiteral(u"Espa\u00F1ol"), "es"},                // Spanish
      {QStringLiteral(u"Fran\u00E7ais"), "fr"},               // French
      {QStringLiteral(u"Hrvatski"), "hr"},                    // Croatian
      {QStringLiteral(u"Italiano"), "it"},                    // Italian
      {QStringLiteral(u"Magyar"), "hu"},                      // Hungarian
      {QStringLiteral(u"Nederlands"), "nl"},                  // Dutch
      {QStringLiteral(u"Norsk bokm\u00E5l"), "nb"},           // Norwegian
      {QStringLiteral(u"Polski"), "pl"},                      // Polish
      {QStringLiteral(u"Portugu\u00EAs"), "pt"},              // Portuguese
      {QStringLiteral(u"Portugu\u00EAs (Brasil)"), "pt_BR"},  // Portuguese (Brazil)
      {QStringLiteral(u"Rom\u00E2n\u0103"), "ro"},            // Romanian
      {QStringLiteral(u"Srpski"), "sr"},                      // Serbian
      {QStringLiteral(u"Suomi"), "fi"},                       // Finnish
      {QStringLiteral(u"Svenska"), "sv"},                     // Swedish
      {QStringLiteral(u"T\u00FCrk\u00E7e"), "tr"},            // Turkish
      {QStringLiteral(u"\u0395\u03BB\u03BB\u03B7\u03BD\u03B9\u03BA\u03AC"), "el"},  // Greek
      {QStringLiteral(u"\u0420\u0443\u0441\u0441\u043A\u0438\u0439"), "ru"},        // Russian
      {QStringLiteral(u"\u0627\u0644\u0639\u0631\u0628\u064A\u0629"), "ar"},        // Arabic
      {QStringLiteral(u"\u0641\u0627\u0631\u0633\u06CC"), "fa"},                    // Farsi
      {QStringLiteral(u"\uD55C\uAD6D\uC5B4"), "ko"},                                // Korean
      {QStringLiteral(u"\u65E5\u672C\u8A9E"), "ja"},                                // Japanese
      {QStringLiteral(u"\u7B80\u4F53\u4E2D\u6587"), "zh_CN"},  // Simplified Chinese
      {QStringLiteral(u"\u7E41\u9AD4\u4E2D\u6587"), "zh_TW"},  // Traditional Chinese
  };

  auto* combobox = new QComboBox();
  combobox->addItem(QObject::tr("<System Language>"), QString{});
  for (const auto& lang : languages)
    combobox->addItem(lang.name, QString::fromLatin1(lang.id));

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
          &InterfacePane::LoadConfig);
}

void InterfacePane::CreateLayout()
{
  m_main_layout = new QVBoxLayout;
  // Create layout here
  CreateUI();
  CreateInGame();

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

  // Theme Combobox
  m_combobox_theme = new QComboBox;
  combobox_layout->addRow(tr("&Theme:"), m_combobox_theme);

  // List avalable themes
  auto theme_search_results =
      Common::DoFileSearch({File::GetUserPath(D_THEMES_IDX), File::GetSysDirectory() + THEMES_DIR});
  for (const std::string& path : theme_search_results)
  {
    const QString qt_name = QString::fromStdString(PathToFileName(path));
    m_combobox_theme->addItem(qt_name);
  }

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
  m_checkbox_use_builtin_title_database = new QCheckBox(tr("Use Built-In Database of Game Names"));
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

  m_checkbox_top_window = new QCheckBox(tr("Keep Window on Top"));
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
  connect(m_checkbox_use_builtin_title_database, &QCheckBox::toggled, this,
          &InterfacePane::OnSaveConfig);
  connect(m_checkbox_use_covers, &QCheckBox::toggled, this, &InterfacePane::OnSaveConfig);
  connect(m_checkbox_disable_screensaver, &QCheckBox::toggled, this, &InterfacePane::OnSaveConfig);
  connect(m_checkbox_show_debugging_ui, &QCheckBox::toggled, this, &InterfacePane::OnSaveConfig);
  connect(m_checkbox_focused_hotkeys, &QCheckBox::toggled, this, &InterfacePane::OnSaveConfig);
  connect(m_combobox_theme, &QComboBox::currentIndexChanged, this, [this](int index) {
    Settings::Instance().SetThemeName(m_combobox_theme->itemText(index));
  });
  connect(m_combobox_userstyle, &QComboBox::currentIndexChanged, this,
          &InterfacePane::OnSaveConfig);
  connect(m_combobox_language, &QComboBox::currentIndexChanged, this, &InterfacePane::OnSaveConfig);
  connect(m_checkbox_top_window, &QCheckBox::toggled, this, &InterfacePane::OnSaveConfig);
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

void InterfacePane::LoadConfig()
{
  SignalBlocking(m_checkbox_use_builtin_title_database)
      ->setChecked(Config::Get(Config::MAIN_USE_BUILT_IN_TITLE_DATABASE));
  SignalBlocking(m_checkbox_show_debugging_ui)
      ->setChecked(Settings::Instance().IsDebugModeEnabled());

#ifdef USE_RETRO_ACHIEVEMENTS
  bool hardcore = Config::Get(Config::RA_HARDCORE_ENABLED);
  SignalBlocking(m_checkbox_show_debugging_ui)->setEnabled(!hardcore);
  if (hardcore)
  {
    m_checkbox_show_debugging_ui->SetDescription(
        tr("<dolphin_emphasis>Disabled in Hardcore Mode.</dolphin_emphasis>"));
  }
  else
  {
    m_checkbox_show_debugging_ui->SetDescription({});
  }
#endif  // USE_RETRO_ACHIEVEMENTS

  SignalBlocking(m_combobox_language)
      ->setCurrentIndex(m_combobox_language->findData(
          QString::fromStdString(Config::Get(Config::MAIN_INTERFACE_LANGUAGE))));
  SignalBlocking(m_combobox_theme)
      ->setCurrentIndex(
          m_combobox_theme->findText(QString::fromStdString(Config::Get(Config::MAIN_THEME_NAME))));

  const Settings::StyleType style_type = Settings::Instance().GetStyleType();
  const QString userstyle = Settings::Instance().GetUserStyleName();
  const int index = style_type == Settings::StyleType::User ?
                        m_combobox_userstyle->findData(userstyle) :
                        m_combobox_userstyle->findData(static_cast<int>(style_type));

  if (index > 0)
    SignalBlocking(m_combobox_userstyle)->setCurrentIndex(index);

  // Render Window Options
  SignalBlocking(m_checkbox_top_window)
      ->setChecked(Settings::Instance().IsKeepWindowOnTopEnabled());
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
  Config::SetBase(Config::MAIN_USE_BUILT_IN_TITLE_DATABASE,
                  m_checkbox_use_builtin_title_database->isChecked());
  Settings::Instance().SetDebugModeEnabled(m_checkbox_show_debugging_ui->isChecked());
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
  Settings::Instance().SetKeepWindowOnTop(m_checkbox_top_window->isChecked());
  Config::SetBase(Config::MAIN_CONFIRM_ON_STOP, m_checkbox_confirm_on_stop->isChecked());
  Config::SetBase(Config::MAIN_USE_PANIC_HANDLERS, m_checkbox_use_panic_handlers->isChecked());
  Config::SetBase(Config::MAIN_OSD_MESSAGES, m_checkbox_enable_osd->isChecked());
  Config::SetBase(Config::MAIN_SHOW_ACTIVE_TITLE, m_checkbox_show_active_title->isChecked());
  Config::SetBase(Config::MAIN_PAUSE_ON_FOCUS_LOST, m_checkbox_pause_on_focus_lost->isChecked());

  auto new_language = m_combobox_language->currentData().toString().toStdString();
  if (new_language != Config::Get(Config::MAIN_INTERFACE_LANGUAGE))
  {
    Config::SetBase(Config::MAIN_INTERFACE_LANGUAGE, new_language);
    ModalMessageBox::information(
        this, tr("Restart Required"),
        tr("You must restart Dolphin in order for the change to take effect."));
  }

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
