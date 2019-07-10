// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Settings/InterfacePane.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

#include "Common/CommonPaths.h"
#include "Common/FileSearch.h"
#include "Common/FileUtil.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"

#include "Core/Config/UISettings.h"
#include "Core/ConfigManager.h"

#include "DolphinQt/GameList/GameListModel.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"
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
  combobox->addItem(QObject::tr("<System Language>"), QStringLiteral(""));
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
  for (const std::string& filename : theme_search_results)
  {
    std::string name, ext;
    SplitPath(filename, nullptr, &name, &ext);
    name += ext;
    QString qt_name = QString::fromStdString(name);
    m_combobox_theme->addItem(qt_name);
  }

  // User Style Combobox
  m_combobox_userstyle = new QComboBox;
  m_label_userstyle = new QLabel(tr("User Style:"));
  combobox_layout->addRow(m_label_userstyle, m_combobox_userstyle);

  auto userstyle_search_results = Common::DoFileSearch({File::GetUserPath(D_STYLES_IDX)});

  m_combobox_userstyle->addItem(tr("(None)"), QStringLiteral(""));

  for (const std::string& filename : userstyle_search_results)
  {
    std::string name, ext;
    SplitPath(filename, nullptr, &name, &ext);
    QString qt_name = QString::fromStdString(name);
    m_combobox_userstyle->addItem(qt_name, QString::fromStdString(filename));
  }

  // Checkboxes
  m_checkbox_use_builtin_title_database = new QCheckBox(tr("Use Built-In Database of Game Names"));
  m_checkbox_use_userstyle = new QCheckBox(tr("Use Custom User Style"));
  m_checkbox_use_covers =
      new QCheckBox(tr("Download Game Covers from GameTDB.com for Use in Grid Mode"));
  m_checkbox_show_debugging_ui = new QCheckBox(tr("Show Debugging UI"));

  groupbox_layout->addWidget(m_checkbox_use_builtin_title_database);
  groupbox_layout->addWidget(m_checkbox_use_userstyle);
  groupbox_layout->addWidget(m_checkbox_use_covers);
  groupbox_layout->addWidget(m_checkbox_show_debugging_ui);
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
  m_checkbox_hide_mouse = new QCheckBox(tr("Always Hide Mouse Cursor"));

  groupbox_layout->addWidget(m_checkbox_top_window);
  groupbox_layout->addWidget(m_checkbox_confirm_on_stop);
  groupbox_layout->addWidget(m_checkbox_use_panic_handlers);
  groupbox_layout->addWidget(m_checkbox_enable_osd);
  groupbox_layout->addWidget(m_checkbox_show_active_title);
  groupbox_layout->addWidget(m_checkbox_pause_on_focus_lost);
  groupbox_layout->addWidget(m_checkbox_hide_mouse);
}

void InterfacePane::ConnectLayout()
{
  connect(m_checkbox_use_builtin_title_database, &QCheckBox::toggled, this,
          &InterfacePane::OnSaveConfig);
  connect(m_checkbox_use_covers, &QCheckBox::toggled, this, &InterfacePane::OnSaveConfig);
  connect(m_checkbox_show_debugging_ui, &QCheckBox::toggled, this, &InterfacePane::OnSaveConfig);
  connect(m_combobox_theme,
          static_cast<void (QComboBox::*)(const QString&)>(&QComboBox::currentIndexChanged),
          &Settings::Instance(), &Settings::SetThemeName);
  connect(m_combobox_userstyle,
          static_cast<void (QComboBox::*)(const QString&)>(&QComboBox::currentIndexChanged), this,
          &InterfacePane::OnSaveConfig);
  connect(m_combobox_language,
          static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
          &InterfacePane::OnSaveConfig);
  connect(m_checkbox_top_window, &QCheckBox::toggled, this, &InterfacePane::OnSaveConfig);
  connect(m_checkbox_confirm_on_stop, &QCheckBox::toggled, this, &InterfacePane::OnSaveConfig);
  connect(m_checkbox_use_panic_handlers, &QCheckBox::toggled, this, &InterfacePane::OnSaveConfig);
  connect(m_checkbox_show_active_title, &QCheckBox::toggled, this, &InterfacePane::OnSaveConfig);
  connect(m_checkbox_enable_osd, &QCheckBox::toggled, this, &InterfacePane::OnSaveConfig);
  connect(m_checkbox_pause_on_focus_lost, &QCheckBox::toggled, this, &InterfacePane::OnSaveConfig);
  connect(m_checkbox_hide_mouse, &QCheckBox::toggled, &Settings::Instance(),
          &Settings::SetHideCursor);
  connect(m_checkbox_use_userstyle, &QCheckBox::toggled, this, &InterfacePane::OnSaveConfig);
}

void InterfacePane::LoadConfig()
{
  const SConfig& startup_params = SConfig::GetInstance();
  m_checkbox_use_builtin_title_database->setChecked(startup_params.m_use_builtin_title_database);
  m_checkbox_show_debugging_ui->setChecked(Settings::Instance().IsDebugModeEnabled());
  m_combobox_language->setCurrentIndex(m_combobox_language->findData(
      QString::fromStdString(SConfig::GetInstance().m_InterfaceLanguage)));
  m_combobox_theme->setCurrentIndex(
      m_combobox_theme->findText(QString::fromStdString(SConfig::GetInstance().theme_name)));

  const QString userstyle = Settings::Instance().GetCurrentUserStyle();
  const int index = m_combobox_userstyle->findText(QFileInfo(userstyle).baseName());

  if (index > 0)
    m_combobox_userstyle->setCurrentIndex(index);

  m_checkbox_use_userstyle->setChecked(Settings::Instance().AreUserStylesEnabled());

  const bool visible = m_checkbox_use_userstyle->isChecked();

  m_combobox_userstyle->setVisible(visible);
  m_label_userstyle->setVisible(visible);

  // Render Window Options
  m_checkbox_top_window->setChecked(Settings::Instance().IsKeepWindowOnTopEnabled());
  m_checkbox_confirm_on_stop->setChecked(startup_params.bConfirmStop);
  m_checkbox_use_panic_handlers->setChecked(startup_params.bUsePanicHandlers);
  m_checkbox_enable_osd->setChecked(startup_params.bOnScreenDisplayMessages);
  m_checkbox_show_active_title->setChecked(startup_params.m_show_active_title);
  m_checkbox_pause_on_focus_lost->setChecked(startup_params.m_PauseOnFocusLost);
  m_checkbox_use_covers->setChecked(Config::Get(Config::MAIN_USE_GAME_COVERS));
  m_checkbox_hide_mouse->setChecked(Settings::Instance().GetHideCursor());
}

void InterfacePane::OnSaveConfig()
{
  SConfig& settings = SConfig::GetInstance();
  settings.m_use_builtin_title_database = m_checkbox_use_builtin_title_database->isChecked();
  Settings::Instance().SetDebugModeEnabled(m_checkbox_show_debugging_ui->isChecked());
  Settings::Instance().SetUserStylesEnabled(m_checkbox_use_userstyle->isChecked());
  Settings::Instance().SetCurrentUserStyle(m_combobox_userstyle->currentData().toString());

  const bool visible = m_checkbox_use_userstyle->isChecked();

  m_combobox_userstyle->setVisible(visible);
  m_label_userstyle->setVisible(visible);

  // Render Window Options
  Settings::Instance().SetKeepWindowOnTop(m_checkbox_top_window->isChecked());
  settings.bConfirmStop = m_checkbox_confirm_on_stop->isChecked();
  settings.bUsePanicHandlers = m_checkbox_use_panic_handlers->isChecked();
  settings.bOnScreenDisplayMessages = m_checkbox_enable_osd->isChecked();
  settings.m_show_active_title = m_checkbox_show_active_title->isChecked();
  settings.m_PauseOnFocusLost = m_checkbox_pause_on_focus_lost->isChecked();

  Common::SetEnableAlert(settings.bUsePanicHandlers);

  auto new_language = m_combobox_language->currentData().toString().toStdString();
  if (new_language != SConfig::GetInstance().m_InterfaceLanguage)
  {
    SConfig::GetInstance().m_InterfaceLanguage = new_language;
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

  settings.SaveSettings();
}
