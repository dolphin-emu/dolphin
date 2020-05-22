// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QCompleter>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLineEdit>
#include <QStackedWidget>
#include <QString>
#include <QTreeWidget>

#include "Common/Config/Config.h"
#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"

#include "DolphinQt/Config/CommonControllersWidget.h"
#include "DolphinQt/Config/FreeLookWidget.h"
#include "DolphinQt/Config/GamecubeControllersWidget.h"
#include "DolphinQt/Config/Graphics/AdvancedWidget.h"
#include "DolphinQt/Config/Graphics/EnhancementsWidget.h"
#include "DolphinQt/Config/Graphics/GeneralWidget.h"
#include "DolphinQt/Config/Graphics/HacksWidget.h"
#include "DolphinQt/Config/Graphics/SoftwareRendererWidget.h"
#include "DolphinQt/Config/WiimoteControllersWidget.h"
#include "DolphinQt/GrandSettingsWindow.h"
#include "DolphinQt/Settings/AdvancedPane.h"
#include "DolphinQt/Settings/AudioPane.h"
#include "DolphinQt/Settings/GameCubePane.h"
#include "DolphinQt/Settings/GeneralPane.h"
#include "DolphinQt/Settings/InterfacePane.h"
#include "DolphinQt/Settings/PathPane.h"
#include "DolphinQt/Settings/WiiPane.h"

#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoConfig.h"

GrandSettingsWindow::GrandSettingsWindow(X11Utils::XRRConfiguration* xrr_config, QWidget* parent)
    : GraphicsDialog(parent), m_xrr_config(xrr_config)
{
  // Set Window Properties
  setWindowTitle(tr("Grand Settings"));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  m_search_bar = new QLineEdit();
  m_search_bar->setPlaceholderText(tr("Search settings"));
  m_search_bar->setClearButtonEnabled(true);

  m_widget_stack = new QStackedWidget();

  m_tree = new QTreeWidget();
  m_tree->setHeaderHidden(true);

  PopulateWidgets();
  OnBackendChanged(QString::fromStdString(Config::Get(Config::MAIN_GFX_BACKEND)));

  connect(m_search_bar, &QLineEdit::textChanged, this, &GrandSettingsWindow::OnSearch);

  // Main Layout
  QGridLayout* layout = new QGridLayout;

  // Add content to layout before dialog buttons.
  layout->addWidget(m_search_bar, 0, 0, 1, 1);
  layout->addWidget(m_tree, 1, 0, 1, 1);
  layout->addWidget(m_widget_stack, 0, 1, 2, 1);

  // Dialog box buttons
  QDialogButtonBox* close_box = new QDialogButtonBox(QDialogButtonBox::Close);

  connect(close_box, &QDialogButtonBox::rejected, this, &QDialog::reject);

  layout->addWidget(close_box, 2, 0, 1, 2, Qt::AlignRight);

  setLayout(layout);
}

void GrandSettingsWindow::PopulateWidgets()
{
  m_tree->clear();

  QStringList all_tags;

  auto create_item = [this](QTreeWidgetItem* parent, const QString& name,
                            QStringList tags) -> QTreeWidgetItem* {
    QTreeWidgetItem* tree_item = new QTreeWidgetItem();
    tree_item->setData(0, Qt::UserRole, QVariant{tags});
    tree_item->setText(0, name);
    if (parent)
      parent->addChild(tree_item);
    else
      m_tree->addTopLevelItem(tree_item);
    return tree_item;
  };

  auto create_item_with_widget = [this, &create_item](QTreeWidgetItem* parent, const QString& name,
                                                      QStringList tags,
                                                      QWidget* widget) -> QTreeWidgetItem* {
    auto child = create_item(parent, name, tags);
    if (!child)
      return nullptr;
    if (!widget)
      return child;
    child->setData(1, Qt::UserRole, QVariant(m_widget_stack->addWidget(widget)));
    return child;
  };

  connect(m_tree, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* item, int column) {
    auto the_data = item->data(1, Qt::UserRole);
    if (!the_data.isValid() || the_data.isNull())
      return;
    m_widget_stack->setCurrentIndex(the_data.toInt());
  });

  auto emulator = create_item(nullptr, tr("Emulator"), {});
  emulator->setExpanded(true);
  auto general = create_item_with_widget(emulator, tr("General"), {}, new GeneralPane);
  general->setSelected(true);
  create_item_with_widget(emulator, tr("Interface"), {}, new InterfacePane);
  create_item_with_widget(emulator, tr("Paths"), {}, new PathPane);

  auto console = create_item(nullptr, tr("Console"), {});

  QStringList gamecube_console_tags = {QStringLiteral("GC")};
  all_tags.append(gamecube_console_tags);
  create_item_with_widget(console, tr("Gamecube"), gamecube_console_tags, new GameCubePane);
  create_item_with_widget(console, tr("Wii"), {}, new WiiPane);

  create_item_with_widget(nullptr, tr("Audio"), {}, new AudioPane);

  create_item_with_widget(nullptr, tr("Advanced"), {}, new AdvancedPane);

  auto graphics = create_item(nullptr, tr("Graphics"), {});

  if (Config::Get(Config::MAIN_GFX_BACKEND) != "Software Renderer")
  {
    auto* general_widget = new GeneralWidget(m_xrr_config, this);
    auto* enhancements_widget = new EnhancementsWidget(this);
    auto* hacks_widget = new HacksWidget(this);
    auto* advanced_widget = new AdvancedWidget(this);

    connect(general_widget, &GeneralWidget::BackendChanged, this,
            &GrandSettingsWindow::OnBackendChanged);

    create_item_with_widget(graphics, tr("General"), {}, general_widget);
    create_item_with_widget(graphics, tr("Enhancements"), {}, enhancements_widget);
    create_item_with_widget(graphics, tr("Hacks"), {}, hacks_widget);
    create_item_with_widget(graphics, tr("Advanced"), {}, advanced_widget);
  }
  else
  {
    auto* software_renderer = new SoftwareRendererWidget(this);

    connect(software_renderer, &SoftwareRendererWidget::BackendChanged, this,
            &GrandSettingsWindow::OnBackendChanged);

    create_item_with_widget(graphics, tr("Software Renderer"), {}, software_renderer);
  }

  auto inputs = create_item(nullptr, tr("Inputs"), {});

  auto gamecube_inputs = new GamecubeControllersWidget(this);
  create_item_with_widget(inputs, tr("Gamecube"), {}, gamecube_inputs);

  auto wii_inputs = new WiimoteControllersWidget(this);
  create_item_with_widget(inputs, tr("Wii"), {}, wii_inputs);

  QStringList common_tags = {QStringLiteral("DSU"), QStringLiteral("Background"),
                             QStringLiteral("DS4"), QStringLiteral("Joycons")};
  all_tags.append(common_tags);
  auto common_inputs = new CommonControllersWidget(this);
  create_item_with_widget(inputs, tr("Common"), common_tags, common_inputs);

  create_item_with_widget(nullptr, tr("FreeLook"), {}, new FreeLookWidget(this));

  QCompleter* tags_completer = new QCompleter(all_tags, this);
  tags_completer->setCaseSensitivity(Qt::CaseInsensitive);
  tags_completer->setCompletionMode(QCompleter::CompletionMode::PopupCompletion);
  m_search_bar->setCompleter(tags_completer);
}

void GrandSettingsWindow::OnSearch(const QString& text)
{
  QTreeWidgetItemIterator it(m_tree);
  while (*it)
  {
    TreeNodeContainsText(text, *it);
    ++it;
  }
}

bool GrandSettingsWindow::TreeNodeContainsText(const QString& text, QTreeWidgetItem* item)
{
  if (item->text(0).contains(text))
  {
    item->setHidden(false);
    return true;
  }

  auto the_data = item->data(0, Qt::UserRole);
  if (the_data.isValid() && !the_data.isNull())
  {
    auto tags = the_data.toStringList();
    if (!tags.filter(text, Qt::CaseInsensitive).empty())
    {
      item->setHidden(false);
      return true;
    }
  }

  bool item_children_contains_text = false;
  for (int i = 0; i < item->childCount(); i++)
  {
    item_children_contains_text |= TreeNodeContainsText(text, item->child(i));
  }

  item->setHidden(!item_children_contains_text);
  if (item_children_contains_text)
  {
    item->setExpanded(true);
  }

  return item_children_contains_text;
}

void GrandSettingsWindow::OnBackendChanged(const QString& backend_name)
{
  Config::SetBase(Config::MAIN_GFX_BACKEND, backend_name.toStdString());
  VideoBackendBase::PopulateBackendInfoFromUI();

  PopulateWidgets();

  emit BackendChanged(backend_name);
}
