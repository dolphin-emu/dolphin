// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Graphics/PostProcessingShaderListWidget.h"

#include <cmath>
#include <variant>

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

#include "Common/VariantUtil.h"
#include "Core/Config/GraphicsSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"

#include "DolphinQt/Config/Graphics/GraphicsColor.h"
#include "DolphinQt/Config/Graphics/GraphicsImage.h"
#include "DolphinQt/Config/Graphics/PostProcessingAddShaderDialog.h"
#include "DolphinQt/Config/ToolTipControls/ToolTipCheckBox.h"
#include "DolphinQt/Config/ToolTipControls/ToolTipComboBox.h"
#include "DolphinQt/Config/ToolTipControls/ToolTipDoubleSpinBox.h"
#include "DolphinQt/Config/ToolTipControls/ToolTipPushButton.h"
#include "DolphinQt/Config/ToolTipControls/ToolTipSpinBox.h"
#include "DolphinQt/Settings.h"

#include "VideoCommon/PEShaderSystem/Config/PEShaderConfigInput.h"
#include "VideoCommon/PEShaderSystem/Config/PEShaderConfigOption.h"
#include "VideoCommon/PEShaderSystem/Config/PEShaderConfigPass.h"
#include "VideoCommon/VideoConfig.h"

namespace
{
struct OptionVisitor
{
  static inline const std::array<QString, 4> vector_labels = {
      QStringLiteral("X"), QStringLiteral("Y"), QStringLiteral("Z"), QStringLiteral("W")};

  VideoCommon::PE::ShaderConfigGroup* m_group;
  u32 m_shader_index;
  QWidget* m_widget;
  QGridLayout* m_option_layout;
  int m_row_count;

  template <std::size_t N>
  void operator()(VideoCommon::PE::FloatOption<N>& option)
  {
    m_option_layout->addWidget(new QLabel(QString::fromStdString(option.m_common.m_ui_name)),
                               m_row_count, 0);

    auto* values_layout = new QHBoxLayout;
    m_option_layout->addLayout(values_layout, m_row_count, 1);
    for (std::size_t i = 0; i < option.m_value.size(); i++)
    {
      if constexpr (N > 1)
      {
        auto axis_label = new QLabel(vector_labels[i] + QStringLiteral(":"));
        values_layout->addWidget(axis_label);
      }
      auto box = new ToolTipDoubleSpinBox;
      if (option.m_decimal_precision[i] != 0.0)
      {
        box->setDecimals(static_cast<int>(option.m_decimal_precision[i]));
      }
      box->setMinimum(static_cast<float>(option.m_min_value[i]));
      box->setMaximum(static_cast<float>(option.m_max_value[i]));
      box->setSingleStep(static_cast<float>(option.m_step_size[i]));
      box->setValue(static_cast<float>(option.m_value[i]));
      if (option.m_common.m_ui_description != "")
      {
        box->SetDescription(QString::fromStdString(option.m_common.m_ui_description));
        box->SetTitle(QString::fromStdString(option.m_common.m_ui_name));
      }
      m_widget->connect(
          box, QOverload<double>::of(&QDoubleSpinBox::valueChanged), m_widget,
          [&option, i, group = m_group, shader_index = m_shader_index](double value) mutable {
            option.m_value[i] = static_cast<float>(value);

            auto& shader = group->m_shaders[shader_index];
            if (option.m_common.m_compile_time_constant)
            {
              group->m_changes++;
            }
            else
            {
              shader.m_changes++;
            }
          });
      values_layout->addWidget(box);
    }
  }

  template <std::size_t N>
  void operator()(VideoCommon::PE::IntOption<N>& option)
  {
    m_option_layout->addWidget(new QLabel(QString::fromStdString(option.m_common.m_ui_name)),
                               m_row_count, 0);

    auto* values_layout = new QHBoxLayout;
    m_option_layout->addLayout(values_layout, m_row_count, 1);
    for (std::size_t i = 0; i < option.m_value.size(); i++)
    {
      if constexpr (N > 1)
      {
        auto axis_label = new QLabel(vector_labels[i] + QStringLiteral(":"));
        values_layout->addWidget(axis_label);
      }
      auto box = new ToolTipSpinBox;
      box->setMinimum(option.m_min_value[i]);
      box->setMaximum(option.m_max_value[i]);
      box->setSingleStep(option.m_step_size[i]);
      box->setValue(option.m_value[i]);
      if (option.m_common.m_ui_description != "")
      {
        box->SetDescription(QString::fromStdString(option.m_common.m_ui_description));
        box->SetTitle(QString::fromStdString(option.m_common.m_ui_name));
      }
      m_widget->connect(
          box, QOverload<int>::of(&QSpinBox::valueChanged), m_widget,
          [&option, i, group = m_group, shader_index = m_shader_index](int value) mutable {
            option.m_value[i] = value;

            auto& shader = group->m_shaders[shader_index];
            if (option.m_common.m_compile_time_constant)
            {
              group->m_changes++;
            }
            else
            {
              shader.m_changes++;
            }
          });
      values_layout->addWidget(box);
    }
  }

  void operator()(VideoCommon::PE::EnumChoiceOption& option)
  {
    auto* combobox = new ToolTipComboBox;
    for (const auto& choice : option.m_ui_choices)
    {
      combobox->addItem(QString::fromStdString(choice));
    }
    combobox->setCurrentIndex(option.m_index);
    if (option.m_common.m_ui_description != "")
    {
      combobox->SetDescription(QString::fromStdString(option.m_common.m_ui_description));
      combobox->SetTitle(QString::fromStdString(option.m_common.m_ui_name));
    }
    m_widget->connect(combobox, QOverload<int>::of(&QComboBox::currentIndexChanged), m_widget,
                      [&option, group = m_group, shader_index = m_shader_index](int index) {
                        option.m_index = static_cast<u32>(index);

                        auto& shader = group->m_shaders[shader_index];
                        if (option.m_common.m_compile_time_constant)
                        {
                          group->m_changes++;
                        }
                        else
                        {
                          shader.m_changes++;
                        }
                      });
    m_option_layout->addWidget(new QLabel(QString::fromStdString(option.m_common.m_ui_name)),
                               m_row_count, 0);
    m_option_layout->addWidget(combobox, m_row_count, 1);
  }

  void operator()(VideoCommon::PE::BoolOption& option)
  {
    auto* checkbox = new ToolTipCheckBox;
    checkbox->setChecked(option.m_value);
    if (option.m_common.m_ui_description != "")
    {
      checkbox->SetDescription(QString::fromStdString(option.m_common.m_ui_description));
      checkbox->SetTitle(QString::fromStdString(option.m_common.m_ui_name));
    }
    m_widget->connect(checkbox, &QCheckBox::stateChanged, m_widget,
                      [&option, group = m_group, shader_index = m_shader_index](int state) {
                        option.m_value = state == 2;

                        auto& shader = group->m_shaders[shader_index];
                        if (option.m_common.m_compile_time_constant)
                        {
                          group->m_changes++;
                        }
                        else
                        {
                          shader.m_changes++;
                        }
                      });
    m_option_layout->addWidget(new QLabel(QString::fromStdString(option.m_common.m_ui_name)),
                               m_row_count, 0);
    m_option_layout->addWidget(checkbox, m_row_count, 1);
  }

  void operator()(VideoCommon::PE::ColorOption& option)
  {
    m_option_layout->addWidget(new QLabel(QString::fromStdString(option.m_common.m_ui_name)),
                               m_row_count, 0);

    auto* color_button = new GraphicsColor(m_widget, false);

    QColor initial_color;
    initial_color.setRgbF(option.m_value[0], option.m_value[1], option.m_value[2]);
    color_button->SetColor(initial_color);
    m_widget->connect(color_button, &GraphicsColor::ColorIsUpdated, m_widget,
                      [&option, group = m_group, shader_index = m_shader_index, color_button]() {
                        const auto& color = color_button->GetColor();
                        option.m_value[0] = static_cast<float>(color.redF());
                        option.m_value[1] = static_cast<float>(color.greenF());
                        option.m_value[2] = static_cast<float>(color.blueF());

                        auto& shader = group->m_shaders[shader_index];
                        if (option.m_common.m_compile_time_constant)
                        {
                          group->m_changes++;
                        }
                        else
                        {
                          shader.m_changes++;
                        }
                      });
    m_option_layout->addWidget(color_button, m_row_count, 1);
  }

  void operator()(VideoCommon::PE::ColorAlphaOption& option)
  {
    m_option_layout->addWidget(new QLabel(QString::fromStdString(option.m_common.m_ui_name)),
                               m_row_count, 0);

    auto* color_button = new GraphicsColor(m_widget, true);

    QColor initial_color;
    initial_color.setRgbF(option.m_value[0], option.m_value[1], option.m_value[2],
                          option.m_value[3]);
    color_button->SetColor(initial_color);
    m_widget->connect(color_button, &GraphicsColor::ColorIsUpdated, m_widget,
                      [&option, group = m_group, shader_index = m_shader_index, color_button]() {
                        const auto& color = color_button->GetColor();
                        option.m_value[0] = static_cast<float>(color.redF());
                        option.m_value[1] = static_cast<float>(color.greenF());
                        option.m_value[2] = static_cast<float>(color.blueF());
                        option.m_value[3] = static_cast<float>(color.alphaF());

                        auto& shader = group->m_shaders[shader_index];
                        if (option.m_common.m_compile_time_constant)
                        {
                          group->m_changes++;
                        }
                        else
                        {
                          shader.m_changes++;
                        }
                      });
    m_option_layout->addWidget(color_button, m_row_count, 1);
  }
};
}  // namespace

PostProcessingShaderListWidget::PostProcessingShaderListWidget(
    QWidget* parent, VideoCommon::PE::ShaderConfigGroup* group)
    : QWidget(parent), m_group(group)
{
  CreateWidgets();
  ConnectWidgets();

  RefreshShaderList();
}

void PostProcessingShaderListWidget::CreateWidgets()
{
  auto* main_layout = new QHBoxLayout(this);

  auto* left_v_layout = new QVBoxLayout;

  m_shader_list = new QListWidget;
  m_shader_list->setSortingEnabled(false);
  m_shader_list->setSelectionBehavior(QAbstractItemView::SelectionBehavior::SelectItems);
  m_shader_list->setSelectionMode(QAbstractItemView::SelectionMode::SingleSelection);
  m_shader_list->setSelectionRectVisible(true);
  m_shader_list->setDragDropMode(QAbstractItemView::InternalMove);

  m_add_shader = new QPushButton(tr("Add..."));
  m_remove_shader = new QPushButton(tr("Remove"));
  QHBoxLayout* hlayout = new QHBoxLayout;
  hlayout->addStretch();
  hlayout->addWidget(m_add_shader);
  hlayout->addWidget(m_remove_shader);

  left_v_layout->addWidget(m_shader_list);
  left_v_layout->addLayout(hlayout);

  auto* right_v_layout = new QVBoxLayout;

  auto default_shader_text = tr("No shader selected");
  m_selected_shader_name = new QLabel(default_shader_text);
  right_v_layout->addWidget(m_selected_shader_name);

  m_shader_meta_layout = new QVBoxLayout;
  right_v_layout->addLayout(m_shader_meta_layout);

  m_shader_buttons_layout = new QHBoxLayout;
  right_v_layout->addLayout(m_shader_buttons_layout);

  m_passes_box = new QGroupBox(tr("Passes"));
  auto* passes_v_layout = new QVBoxLayout;
  m_passes_box->setLayout(passes_v_layout);
  right_v_layout->addWidget(m_passes_box);

  m_options_snapshots_box = new QGroupBox(tr("Snapshots"));
  auto* snapshots_v_layout = new QVBoxLayout;
  m_options_snapshots_box->setLayout(snapshots_v_layout);
  right_v_layout->addWidget(m_options_snapshots_box);

  m_options_box = new QGroupBox(tr("Options"));
  auto* options_v_layout = new QVBoxLayout;
  m_options_box->setLayout(options_v_layout);
  right_v_layout->addWidget(m_options_box);
  right_v_layout->addStretch(1);

  main_layout->addLayout(left_v_layout);
  main_layout->addLayout(right_v_layout, 1);

  setLayout(main_layout);
}

void PostProcessingShaderListWidget::ConnectWidgets()
{
  connect(m_shader_list, &QListWidget::itemSelectionChanged, this,
          [this] { ShaderSelectionChanged(); });

  connect(m_shader_list, &QListWidget::itemChanged, this,
          &PostProcessingShaderListWidget::ShaderItemChanged);

  connect(m_shader_list->model(), &QAbstractItemModel::rowsMoved, this,
          [this] { SaveShaderList(); });

  connect(m_add_shader, &QPushButton::clicked, this,
          &PostProcessingShaderListWidget::OnShaderAdded);
  connect(m_remove_shader, &QPushButton::clicked, this,
          &PostProcessingShaderListWidget::OnShaderRemoved);

  /* connect(&Settings::Instance(), &Settings::ShaderConfigLoaded, this,
          [this] { RefreshShaderList(); });*/
}

void PostProcessingShaderListWidget::RefreshShaderList()
{
  m_shader_list->setCurrentItem(nullptr);
  m_shader_list->clear();

  for (const auto& index : m_group->m_shader_order)
  {
    const auto& shader = m_group->m_shaders[index];
    QListWidgetItem* item = new QListWidgetItem(QString::fromStdString(shader.m_name));
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setData(Qt::UserRole, static_cast<int>(index));

    if (shader.m_enabled)
    {
      item->setCheckState(Qt::Checked);
    }
    else
    {
      item->setCheckState(Qt::Unchecked);
    }
    m_shader_list->addItem(item);
  }
}

void PostProcessingShaderListWidget::ShaderSelectionChanged()
{
  if (m_shader_list->currentItem() == nullptr)
    return;
  if (m_shader_list->count() == 0)
    return;
  const int index = m_shader_list->currentItem()->data(Qt::UserRole).toInt();
  OnShaderChanged(index);
}

void PostProcessingShaderListWidget::ShaderItemChanged(QListWidgetItem* item)
{
  const int index = item->data(Qt::UserRole).toInt();
  auto& shader = m_group->m_shaders[index];
  const bool was_enabled = shader.m_enabled;
  const bool should_enable = item->checkState() == Qt::Checked;
  shader.m_enabled = should_enable;
  if (was_enabled != should_enable)
  {
    m_group->m_changes++;
  }
}

void PostProcessingShaderListWidget::OnShaderChanged(std::optional<u32> index)
{
  QVBoxLayout* options_layout = static_cast<QVBoxLayout*>(m_options_box->layout());
  ClearLayoutRecursively(options_layout);

  QVBoxLayout* passes_layout = static_cast<QVBoxLayout*>(m_passes_box->layout());
  ClearLayoutRecursively(passes_layout);

  QVBoxLayout* snapshot_layout = static_cast<QVBoxLayout*>(m_options_snapshots_box->layout());
  ClearLayoutRecursively(snapshot_layout);

  ClearLayoutRecursively(m_shader_meta_layout);
  ClearLayoutRecursively(m_shader_buttons_layout);

  adjustSize();

  if (!index)
  {
    m_selected_shader_name->setText(QStringLiteral("No shader selected"));
  }
  else
  {
    auto& shader = m_group->m_shaders[*index];
    m_selected_shader_name->setText(QString::fromStdString(shader.m_name));
    QFont font = m_selected_shader_name->font();
    font.setWeight(QFont::Bold);
    m_selected_shader_name->setFont(font);

    if (!shader.m_author.empty())
    {
      auto* author_label = new QLabel(QString::fromStdString(shader.m_author));
      m_shader_meta_layout->addWidget(author_label);
    }

    if (!shader.m_description.empty())
    {
      auto* description_label = new QLabel(QString::fromStdString(shader.m_description));
      m_shader_meta_layout->addWidget(description_label);
    }

    auto reset_button = new ToolTipPushButton(tr("Reset to Defaults"));
    reset_button->SetTitle(tr("Reset To Defaults"));
    reset_button->SetDescription(tr("Resets all shader options to use their default values"));
    connect(reset_button, &QPushButton::clicked, [index, this] {
      m_group->m_shaders[*index].Reset();
      OnShaderChanged(index);
    });

    auto reload_button = new ToolTipPushButton(tr("Reload"));
    reload_button->SetTitle(tr("Reload"));
    reload_button->SetDescription(tr("Reloads the shader causing it to be recompiled"));
    connect(reload_button, &QPushButton::clicked, [index, this] {
      m_group->m_changes++;
      OnShaderChanged(index);
    });

    m_shader_buttons_layout->addWidget(reset_button);
    m_shader_buttons_layout->addWidget(reload_button);

    const bool has_error = shader.m_runtime_info && shader.m_runtime_info->HasError();

    if (std::none_of(shader.m_passes.begin(), shader.m_passes.end(),
                     [](const VideoCommon::PE::ShaderConfigPass& pass) {
                       return std::any_of(pass.m_inputs.begin(), pass.m_inputs.end(),
                                          [](const VideoCommon::PE::ShaderConfigInput& input) {
                                            return std::get_if<VideoCommon::PE::UserImage>(
                                                       &input) != nullptr;
                                          });
                     }))
    {
      passes_layout->addWidget(new QLabel(QStringLiteral("No passes are editable")));
    }
    else
    {
      if (has_error)
      {
        passes_layout->addWidget(
            new QLabel(QStringLiteral("Shader error, fix and reload to edit passes")));
      }
      else
      {
        passes_layout->addLayout(BuildPassesLayout(*index));
      }
    }

    if (shader.m_options.empty())
    {
      options_layout->addWidget(new QLabel(QStringLiteral("No options available")));
    }
    else
    {
      if (has_error)
      {
        options_layout->addWidget(
            new QLabel(QStringLiteral("Shader error, fix and reload to edit options")));
      }
      else
      {
        snapshot_layout->addLayout(BuildSnapshotsLayout(*index));
        options_layout->addLayout(BuildOptionsLayout(*index));
      }
    }
  }
}

QLayout* PostProcessingShaderListWidget::BuildPassesLayout(u32 index)
{
  auto& shader = m_group->m_shaders[index];
  QVBoxLayout* passes_layout = new QVBoxLayout;
  for (std::size_t i = 0; i < shader.m_passes.size(); i++)
  {
    auto& pass = shader.m_passes[i];
    QGridLayout* pass_layout = new QGridLayout;
    pass_layout->addWidget(new QLabel(QStringLiteral("Pass %1").arg(i)), 0, 0, 1, 2);

    int input_row = 1;
    for (auto& input : pass.m_inputs)
    {
      std::visit(
          [&, this](auto&& input) {
            using T = std::decay_t<decltype(input)>;
            if constexpr (std::is_same_v<T, VideoCommon::PE::UserImage>)
            {
              pass_layout->addWidget(
                  new QLabel(QStringLiteral("Input%1 Image Source").arg(input.m_texture_unit)),
                  input_row, 0);

              GraphicsImage* image = new GraphicsImage(this);
              connect(image, &GraphicsImage::PathIsUpdated, this, [image, &input, group = m_group] {
                input.m_path = image->GetPath();
                group->m_changes++;
              });
              pass_layout->addWidget(image, input_row, 1);

              input_row++;
            }
          },
          input);
    }

    passes_layout->addLayout(pass_layout);
  }
  return passes_layout;
}

QLayout* PostProcessingShaderListWidget::BuildSnapshotsLayout(u32 index)
{
  QGridLayout* snapshots_layout = new QGridLayout;

  const int max_snapshots = static_cast<int>(VideoCommon::PE::MAX_SNAPSHOTS);

  for (int column = 0; column < max_snapshots; column++)
  {
    auto* label = new QLabel(QString::number(column + 1));
    snapshots_layout->addWidget(label, 0, column, Qt::AlignCenter);
  }

  auto& shader = m_group->m_shaders[index];
  for (int row = 1; row < 3; row++)
  {
    for (int column = 0; column < max_snapshots; column++)
    {
      ToolTipPushButton* button;
      if (row == 1)
      {
        button = new ToolTipPushButton(tr("L"));
        button->SetTitle(QStringLiteral("Load Slot %1").arg(column + 1));
        button->SetDescription(
            QStringLiteral("Resets the options in the shader to those that are in slot %1")
                .arg(column + 1));
        if (shader.HasSnapshot(column))
        {
          button->setEnabled(true);
          connect(button, &QPushButton::clicked, this, [index, column, this]() {
            auto& shader = m_group->m_shaders[index];
            shader.LoadSnapshot(column);
            OnShaderChanged(index);
          });
        }
        else
        {
          button->setEnabled(false);
        }
      }
      else
      {
        button = new ToolTipPushButton(tr("S"));
        button->SetTitle(QStringLiteral("Save Slot %1").arg(column + 1));
        button->SetDescription(
            QStringLiteral("Saves the options in the shader to slot %1").arg(column + 1));
        connect(button, &QPushButton::clicked, this, [index, column, this]() {
          auto& shader = m_group->m_shaders[index];
          shader.SaveSnapshot(column);
        });
      }

      button->setMaximumWidth(button->fontMetrics().boundingRect(button->text()).width() * 4);

      snapshots_layout->addWidget(button, row, column, Qt::AlignCenter);
    }
  }

  return snapshots_layout;
}

QLayout* PostProcessingShaderListWidget::BuildOptionsLayout(u32 index)
{
  int rows = 0;

  QVBoxLayout* options_layout = new QVBoxLayout;

  QTabWidget* tabs_widget = new QTabWidget;

  connect(tabs_widget, &QTabWidget::currentChanged, this, [tabs_widget, this](int index) {
    for (int i = 0; i < tabs_widget->count(); i++)
    {
      if (i != index)
        tabs_widget->widget(i)->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    }

    auto* widget = tabs_widget->widget(index);
    widget->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    widget->resize(widget->minimumSizeHint());
    widget->adjustSize();
    resize(minimumSizeHint());
    adjustSize();
  });

  options_layout->addWidget(tabs_widget);

  auto& shader = m_group->m_shaders[index];
  auto groups = shader.GetGroups();
  for (auto& [group_name, options] : groups)
  {
    QWidget* group_widget = new QWidget;
    QGridLayout* option_layout = new QGridLayout;

    group_widget->setLayout(option_layout);

    tabs_widget->addTab(group_widget, QString::fromStdString(group_name));

    for (auto* option : options)
    {
      OptionVisitor visitor;
      visitor.m_option_layout = option_layout;
      visitor.m_widget = this;
      visitor.m_shader_index = index;
      visitor.m_group = m_group;
      visitor.m_row_count = rows;
      std::visit(visitor, *option);

      rows++;
    }
  }

  return options_layout;
}

void PostProcessingShaderListWidget::SaveShaderList()
{
  std::vector<u32> shader_order;
  shader_order.resize(m_shader_list->count());
  for (int i = 0; i < m_shader_list->count(); ++i)
  {
    QListWidgetItem* item = m_shader_list->item(i);
    const int index = item->data(Qt::UserRole).toInt();
    shader_order[i] = static_cast<u32>(index);
  }

  m_group->m_shader_order = shader_order;
  m_group->m_changes++;
}

void PostProcessingShaderListWidget::ClearLayoutRecursively(QLayout* layout)
{
  while (QLayoutItem* child = layout->takeAt(0))
  {
    if (child == nullptr)
      continue;

    if (child->widget())
    {
      layout->removeWidget(child->widget());
      delete child->widget();
    }
    else if (child->layout())
    {
      ClearLayoutRecursively(child->layout());
      layout->removeItem(child);
    }
    else
    {
      layout->removeItem(child);
    }
    delete child;
  }
}

void PostProcessingShaderListWidget::OnShaderAdded()
{
  PostProcessingAddShaderDialog add_shader_dialog(this);
  if (add_shader_dialog.exec() == QDialog::Accepted)
  {
    bool refresh_list = false;

    const auto user_shader_pathes = add_shader_dialog.ChosenUserShaderPathes();
    if (!user_shader_pathes.empty())
    {
      for (const QString& shader_path : user_shader_pathes)
      {
        m_group->AddShader(shader_path.toStdString(), VideoCommon::PE::ShaderConfig::Source::User);
      }
      refresh_list = true;
    }

    const auto system_shader_pathes = add_shader_dialog.ChosenSystemShaderPathes();
    if (!system_shader_pathes.empty())
    {
      for (const QString& shader_path : system_shader_pathes)
      {
        m_group->AddShader(shader_path.toStdString(),
                           VideoCommon::PE::ShaderConfig::Source::System);
      }
      refresh_list = true;
    }

    if (refresh_list)
    {
      RefreshShaderList();
    }
  }
}

void PostProcessingShaderListWidget::OnShaderRemoved()
{
  if (m_shader_list->currentItem() == nullptr)
    return;
  const int index = m_shader_list->currentItem()->data(Qt::UserRole).toInt();
  m_group->RemoveShader(index);

  RefreshShaderList();
  OnShaderChanged(std::nullopt);
}
