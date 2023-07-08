// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Graphics/PostProcessingConfigWindow.h"

#include <cmath>
#include <vector>

#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSlider>
#include <QString>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

#include "DolphinQt/Config/Graphics/EnhancementsWidget.h"

#include "VideoCommon/PostProcessing.h"
#include "VideoCommon/Present.h"
#include "VideoCommon/VideoConfig.h"

using ConfigurationOption = VideoCommon::PostProcessingConfiguration::ConfigurationOption;
using OptionType = ConfigurationOption::OptionType;

PostProcessingConfigWindow::PostProcessingConfigWindow(EnhancementsWidget* parent,
                                                       const std::string& shader)
    : QDialog(parent), m_shader(shader)
{
  if (g_presenter && g_presenter->GetPostProcessor())
  {
    m_post_processor = g_presenter->GetPostProcessor()->GetConfig();
  }
  else
  {
    m_post_processor = new VideoCommon::PostProcessingConfiguration();
    m_post_processor->LoadShader(m_shader);
  }

  setWindowTitle(tr("Post-Processing Shader Configuration"));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  PopulateGroups();
  Create();
  ConnectWidgets();
}

PostProcessingConfigWindow::~PostProcessingConfigWindow()
{
  m_post_processor->SaveOptionsConfiguration();
  if (!(g_presenter && g_presenter->GetPostProcessor()))
  {
    delete m_post_processor;
  }
}

void PostProcessingConfigWindow::PopulateGroups()
{
  const VideoCommon::PostProcessingConfiguration::ConfigMap& config_map =
      m_post_processor->GetOptions();

  auto config_groups = std::vector<std::unique_ptr<ConfigGroup>>();
  for (const auto& it : config_map)
  {
    auto config_group = std::make_unique<ConfigGroup>(&it.second);
    m_config_map[it.first] = config_group.get();
    config_groups.push_back(std::move(config_group));
  }

  for (auto& config_group : config_groups)
  {
    const std::string& parent_name = config_group->GetParent();
    if (parent_name.empty())
    {
      m_config_groups.emplace_back(std::move(config_group));
    }
    else
    {
      m_config_map[parent_name]->AddSubGroup(std::move(config_group));
    }
  }
}

void PostProcessingConfigWindow::Create()
{
  m_tabs = new QTabWidget();
  auto* const general = new QWidget(m_tabs);
  auto* const general_layout = new QGridLayout(general);

  u32 row = 0;
  bool add_general_page = false;
  for (auto& it : m_config_groups)
  {
    if (it->HasSubGroups())
    {
      auto* const tab = CreateDependentTab(it);
      m_tabs->addTab(tab, QString::fromStdString(it->GetGUIName()));

      if (it->GetConfigurationOption()->m_type == OptionType::Bool)
      {
        it->EnableSuboptions(it->GetCheckboxValue());
      }
    }
    else
    {
      if (!add_general_page)
      {
        add_general_page = true;
      }
      row = it->AddWidgets(this, general_layout, row);
    }
  }

  if (add_general_page)
  {
    m_tabs->insertTab(0, general, tr("General"));
  }

  m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok);

  auto* layout = new QVBoxLayout(this);
  layout->addWidget(m_tabs);
  layout->addWidget(m_buttons);
}

void PostProcessingConfigWindow::ConnectWidgets()
{
  connect(m_buttons, &QDialogButtonBox::accepted, this, &PostProcessingConfigWindow::accept);
}

QWidget*
PostProcessingConfigWindow::CreateDependentTab(const std::unique_ptr<ConfigGroup>& config_group)
{
  auto* const tab = new QWidget(m_tabs);
  auto* const layout = new QGridLayout(tab);

  u32 row = config_group->AddWidgets(this, layout, 0);
  for (const auto& child : config_group->GetSubGroups())
  {
    row = child->AddWidgets(this, layout, row);
  }

  return tab;
}

void PostProcessingConfigWindow::UpdateBool(ConfigGroup* const config_group, const bool state)
{
  m_post_processor->SetOptionb(config_group->GetOptionName(), state);

  config_group->EnableSuboptions(state);
}

void PostProcessingConfigWindow::UpdateInteger(ConfigGroup* const config_group, const int value)
{
  const ConfigurationOption& config_option =
      m_post_processor->GetOption(config_group->GetOptionName());

  const size_t vector_size = config_option.m_integer_values.size();

  for (size_t i = 0; i < vector_size; ++i)
  {
    const int current_step = config_group->GetSliderValue(i);
    const s32 current_value = config_option.m_integer_step_values[i] * current_step +
                              config_option.m_integer_min_values[i];
    m_post_processor->SetOptioni(config_option.m_option_name, static_cast<int>(i), current_value);
    config_group->SetSliderText(i, QString::number(current_value));
  }
}

void PostProcessingConfigWindow::UpdateFloat(ConfigGroup* const config_group, const int value)
{
  const ConfigurationOption& config_option =
      m_post_processor->GetOption(config_group->GetOptionName());

  const size_t vector_size = config_option.m_float_values.size();

  for (size_t i = 0; i < vector_size; ++i)
  {
    const int current_step = config_group->GetSliderValue(static_cast<unsigned int>(i));
    const float current_value =
        config_option.m_float_step_values[i] * current_step + config_option.m_float_min_values[i];
    m_post_processor->SetOptionf(config_option.m_option_name, static_cast<int>(i), current_value);
    config_group->SetSliderText(i, QString::asprintf("%f", current_value));
  }
}

PostProcessingConfigWindow::ConfigGroup::ConfigGroup(const ConfigurationOption* config_option)
    : m_config_option(config_option)
{
}

const std::string& PostProcessingConfigWindow::ConfigGroup::GetGUIName() const noexcept
{
  return m_config_option->m_gui_name;
}

const std::string& PostProcessingConfigWindow::ConfigGroup::GetParent() const noexcept
{
  return m_config_option->m_dependent_option;
}

const std::string& PostProcessingConfigWindow::ConfigGroup::GetOptionName() const noexcept
{
  return m_config_option->m_option_name;
}

void PostProcessingConfigWindow::ConfigGroup::AddSubGroup(std::unique_ptr<ConfigGroup>&& subgroup)
{
  m_subgroups.emplace_back(std::move(subgroup));
}

bool PostProcessingConfigWindow::ConfigGroup::HasSubGroups() const noexcept
{
  return !m_subgroups.empty();
}

const ConfigurationOption*
PostProcessingConfigWindow::ConfigGroup::GetConfigurationOption() const noexcept
{
  return m_config_option;
}

const std::vector<std::unique_ptr<PostProcessingConfigWindow::ConfigGroup>>&
PostProcessingConfigWindow::ConfigGroup::GetSubGroups() const noexcept
{
  return m_subgroups;
}

u32 PostProcessingConfigWindow::ConfigGroup::AddWidgets(PostProcessingConfigWindow* const parent,
                                                        QGridLayout* const grid, const u32 row)
{
  auto* const name = new QLabel(QString::fromStdString(m_config_option->m_gui_name));
  grid->addWidget(name, row, 0);

  switch (m_config_option->m_type)
  {
  case OptionType::Bool:
    return AddBool(parent, grid, row);
  case OptionType::Float:
    return AddFloat(parent, grid, row);
  case OptionType::Integer:
    return AddInteger(parent, grid, row);
  default:
    // obviously shouldn't get here
    std::abort();
  }
}

u32 PostProcessingConfigWindow::ConfigGroup::AddBool(PostProcessingConfigWindow* const parent,
                                                     QGridLayout* const grid, const u32 row)
{
  m_checkbox = new QCheckBox();
  m_checkbox->setChecked(m_config_option->m_bool_value);
  QObject::connect(m_checkbox, &QCheckBox::toggled,
                   [this, parent](bool checked) { parent->UpdateBool(this, checked); });
  grid->addWidget(m_checkbox, row, 2);

  return row + 1;
}

u32 PostProcessingConfigWindow::ConfigGroup::AddInteger(PostProcessingConfigWindow* const parent,
                                                        QGridLayout* const grid, u32 row)
{
  const size_t vector_size = m_config_option->m_integer_values.size();

  for (size_t i = 0; i < vector_size; ++i)
  {
    const double range =
        m_config_option->m_integer_max_values[i] - m_config_option->m_integer_min_values[i];
    // "How many steps we have is the range divided by the step interval configured.
    //  This may not be 100% spot on accurate since developers can have odd stepping intervals
    //  set.
    //  Round up so if it is outside our range, then set it to the minimum or maximum"
    const int steps =
        std::ceil(range / static_cast<double>(m_config_option->m_integer_step_values[i]));
    const int current_value = std::round(
        (m_config_option->m_integer_values[i] - m_config_option->m_integer_min_values[i]) /
        static_cast<double>(m_config_option->m_integer_step_values[i]));

    auto* const slider = new QSlider(Qt::Orientation::Horizontal);
    slider->setMinimum(0);
    slider->setMaximum(steps);
    slider->setValue(current_value);
    slider->setTickInterval(range / steps);
    QObject::connect(slider, &QSlider::valueChanged,
                     [this, parent](int value) { parent->UpdateInteger(this, value); });

    auto* const value_box = new QLineEdit(QString::number(m_config_option->m_integer_values[i]));
    value_box->setEnabled(false);

    grid->addWidget(slider, row, 1);
    grid->addWidget(value_box, row, 2);

    m_sliders.push_back(slider);
    m_value_boxes.push_back(value_box);
    if (vector_size > 1)
    {
      row++;
    }
  }

  return row + 1;
}

u32 PostProcessingConfigWindow::ConfigGroup::AddFloat(PostProcessingConfigWindow* const parent,
                                                      QGridLayout* const grid, u32 row)
{
  const size_t vector_size = m_config_option->m_float_values.size();

  for (size_t i = 0; i < vector_size; ++i)
  {
    const float range =
        m_config_option->m_float_max_values[i] - m_config_option->m_float_min_values[i];
    const int steps = std::ceil(range / m_config_option->m_float_step_values[i]);
    const int current_value =
        std::round((m_config_option->m_float_values[i] - m_config_option->m_float_min_values[i]) /
                   m_config_option->m_float_step_values[i]);

    auto* const slider = new QSlider(Qt::Orientation::Horizontal);
    slider->setMinimum(0);
    slider->setMaximum(steps);
    slider->setValue(current_value);
    slider->setTickInterval(range / steps);
    QObject::connect(slider, &QSlider::valueChanged,
                     [this, parent](int value) { parent->UpdateFloat(this, value); });

    auto* const value_box =
        new QLineEdit(QString::asprintf("%f", m_config_option->m_float_values[i]));
    value_box->setEnabled(false);

    grid->addWidget(slider, row, 1);
    grid->addWidget(value_box, row, 2);

    m_sliders.push_back(slider);
    m_value_boxes.push_back(value_box);
    if (vector_size > 1)
    {
      row++;
    }
  }

  return row + 1;
}

void PostProcessingConfigWindow::ConfigGroup::EnableSuboptions(const bool state)
{
  for (auto& it : m_subgroups)
  {
    if (it->m_config_option->m_type == OptionType::Bool)
    {
      it->m_checkbox->setEnabled(state);
    }
    else
    {
      for (auto& slider : it->m_sliders)
      {
        slider->setEnabled(state);
      }
    }
    it->EnableSuboptions(state);
  }
}

int PostProcessingConfigWindow::ConfigGroup::GetCheckboxValue() const
{
  return m_checkbox->isChecked();
}

int PostProcessingConfigWindow::ConfigGroup::GetSliderValue(size_t index) const
{
  return m_sliders[index]->value();
}

void PostProcessingConfigWindow::ConfigGroup::SetSliderText(size_t index, const QString& text)
{
  m_value_boxes[index]->setText(text);
}
