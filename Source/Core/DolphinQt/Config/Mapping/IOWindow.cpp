// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Mapping/IOWindow.h"

#include <optional>

#include <QBrush>
#include <QColor>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QItemDelegate>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

#include "Core/Core.h"

#include "DolphinQt/Config/Mapping/MappingCommon.h"
#include "DolphinQt/Config/Mapping/MappingIndicator.h"
#include "DolphinQt/Config/Mapping/MappingWidget.h"
#include "DolphinQt/Config/Mapping/MappingWindow.h"
#include "DolphinQt/QtUtils/BlockUserInputFilter.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/Settings.h"

#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControlReference/ExpressionParser.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/ControllerInterface/MappingCommon.h"

namespace
{
QTextCharFormat GetSpecialCharFormat()
{
  QTextCharFormat format;
  format.setFontWeight(QFont::Weight::Bold);
  return format;
}

QTextCharFormat GetLiteralCharFormat()
{
  QTextCharFormat format;
  if (Settings::Instance().IsThemeDark())
    format.setForeground(QBrush{QColor(171, 132, 219)});
  else
    format.setForeground(QBrush{Qt::darkMagenta});
  return format;
}

QTextCharFormat GetInvalidCharFormat()
{
  QTextCharFormat format;
  format.setUnderlineStyle(QTextCharFormat::WaveUnderline);
  if (Settings::Instance().IsThemeDark())
    format.setUnderlineColor(QColor(255, 69, 0));
  else
    format.setUnderlineColor(Qt::darkRed);
  return format;
}

QTextCharFormat GetControlCharFormat()
{
  QTextCharFormat format;
  if (Settings::Instance().IsThemeDark())
    format.setForeground(QBrush{QColor(0, 220, 0)});
  else
    format.setForeground(QBrush{Qt::darkGreen});
  return format;
}

QTextCharFormat GetVariableCharFormat()
{
  QTextCharFormat format;
  if (Settings::Instance().IsThemeDark())
    format.setForeground(QBrush{QColor(226, 226, 0)});
  else
    format.setForeground(QBrush{Qt::darkYellow});
  return format;
}

QTextCharFormat GetBarewordCharFormat()
{
  QTextCharFormat format;
  if (Settings::Instance().IsThemeDark())
    format.setForeground(QBrush{QColor(66, 138, 255)});
  else
    format.setForeground(QBrush{Qt::darkBlue});
  return format;
}

QTextCharFormat GetCommentCharFormat()
{
  QTextCharFormat format;
  if (Settings::Instance().IsThemeDark())
    format.setForeground(QBrush{QColor(176, 176, 176)});
  else
    format.setForeground(QBrush{Qt::darkGray});
  return format;
}
}  // namespace

ControlExpressionSyntaxHighlighter::ControlExpressionSyntaxHighlighter(QTextDocument* parent)
    : QSyntaxHighlighter(parent)
{
}

void QComboBoxWithMouseWheelDisabled::wheelEvent(QWheelEvent* event)
{
  // Do nothing
}

void ControlExpressionSyntaxHighlighter::highlightBlock(const QString&)
{
  // TODO: This is going to result in improper highlighting with non-ascii characters:
  ciface::ExpressionParser::Lexer lexer(document()->toPlainText().toStdString());

  std::vector<ciface::ExpressionParser::Token> tokens;
  const auto tokenize_status = lexer.Tokenize(tokens);

  using ciface::ExpressionParser::TokenType;

  const auto set_block_format = [this](int start, int count, const QTextCharFormat& format) {
    if (start + count <= currentBlock().position() ||
        start >= currentBlock().position() + currentBlock().length())
    {
      // This range is not within the current block.
      return;
    }

    int block_start = start - currentBlock().position();

    if (block_start < 0)
    {
      count += block_start;
      block_start = 0;
    }

    setFormat(block_start, count, format);
  };

  for (auto& token : tokens)
  {
    std::optional<QTextCharFormat> char_format;

    switch (token.type)
    {
    case TokenType::TOK_INVALID:
      char_format = GetInvalidCharFormat();
      break;
    case TokenType::TOK_LPAREN:
    case TokenType::TOK_RPAREN:
    case TokenType::TOK_COMMA:
      char_format = GetSpecialCharFormat();
      break;
    case TokenType::TOK_LITERAL:
      char_format = GetLiteralCharFormat();
      break;
    case TokenType::TOK_CONTROL:
      char_format = GetControlCharFormat();
      break;
    case TokenType::TOK_BAREWORD:
      char_format = GetBarewordCharFormat();
      break;
    case TokenType::TOK_VARIABLE:
      char_format = GetVariableCharFormat();
      break;
    case TokenType::TOK_COMMENT:
      char_format = GetCommentCharFormat();
      break;
    default:
      if (token.IsBinaryOperator())
        char_format = GetSpecialCharFormat();
      break;
    }

    if (char_format.has_value())
      set_block_format(int(token.string_position), int(token.string_length), *char_format);
  }

  // This doesn't need to be run for every "block", but it works.
  if (ciface::ExpressionParser::ParseStatus::Successful == tokenize_status)
  {
    ciface::ExpressionParser::RemoveInertTokens(&tokens);
    const auto parse_status = ciface::ExpressionParser::ParseTokens(tokens);

    if (ciface::ExpressionParser::ParseStatus::Successful != parse_status.status)
    {
      const auto token = *parse_status.token;
      set_block_format(int(token.string_position), int(token.string_length),
                       GetInvalidCharFormat());
    }
  }
}

class InputStateDelegate : public QItemDelegate
{
public:
  explicit InputStateDelegate(IOWindow* parent, int column,
                              std::function<ControlState(int row)> state_evaluator);

  void paint(QPainter* painter, const QStyleOptionViewItem& option,
             const QModelIndex& index) const override;

private:
  std::function<ControlState(int row)> m_state_evaluator;
  int m_column;
};

class InputStateLineEdit : public QLineEdit
{
public:
  explicit InputStateLineEdit(std::function<ControlState()> state_evaluator);
  void SetShouldPaintStateIndicator(bool value);
  void paintEvent(QPaintEvent* event) override;

private:
  std::function<ControlState()> m_state_evaluator;
  bool m_should_paint_state_indicator = false;
};

IOWindow::IOWindow(MappingWidget* parent, ControllerEmu::EmulatedController* controller,
                   ControlReference* ref, IOWindow::Type type)
    : QDialog(parent), m_reference(ref), m_original_expression(ref->GetExpression()),
      m_controller(controller), m_type(type)
{
  CreateMainLayout();

  connect(parent, &MappingWidget::Update, this, &IOWindow::Update);
  connect(parent->GetParent(), &MappingWindow::ConfigChanged, this, &IOWindow::ConfigChanged);
  connect(&Settings::Instance(), &Settings::ConfigChanged, this, &IOWindow::ConfigChanged);

  setWindowTitle(type == IOWindow::Type::Input ? tr("Configure Input") : tr("Configure Output"));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  ConfigChanged();

  ConnectWidgets();
}

std::shared_ptr<ciface::Core::Device> IOWindow::GetSelectedDevice() const
{
  return m_selected_device;
}

void IOWindow::CreateMainLayout()
{
  m_main_layout = new QVBoxLayout();

  m_devices_combo = new QComboBox();
  m_option_list = new QTableWidget();
  m_select_button = new QPushButton(tr("Select"));
  m_detect_button = new QPushButton(tr("Detect"), this);
  m_test_button = new QPushButton(tr("Test"), this);
  m_button_box = new QDialogButtonBox();
  m_clear_button = new QPushButton(tr("Clear"));
  m_scalar_spinbox = new QSpinBox();

  m_parse_text = new InputStateLineEdit([this] {
    const auto lock = m_controller->GetStateLock();
    return m_reference->GetState<ControlState>();
  });
  m_parse_text->setReadOnly(true);

  m_expression_text = new QPlainTextEdit();
  m_expression_text->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  new ControlExpressionSyntaxHighlighter(m_expression_text->document());

  m_operators_combo = new QComboBoxWithMouseWheelDisabled(this);
  m_operators_combo->addItem(tr("Operators"));
  m_operators_combo->insertSeparator(1);
  if (m_type == Type::Input)
  {
    m_operators_combo->addItem(tr("! Not"));
    m_operators_combo->addItem(tr("* Multiply"));
    m_operators_combo->addItem(tr("/ Divide"));
    m_operators_combo->addItem(tr("% Modulo"));
    m_operators_combo->addItem(tr("+ Add"));
    m_operators_combo->addItem(tr("- Subtract"));
    m_operators_combo->addItem(tr("> Greater-than"));
    m_operators_combo->addItem(tr("< Less-than"));
    m_operators_combo->addItem(tr("& And"));
    m_operators_combo->addItem(tr("^ Xor"));
  }
  m_operators_combo->addItem(tr("| Or"));
  m_operators_combo->addItem(tr("$ User Variable"));
  if (m_type == Type::Input)
  {
    m_operators_combo->addItem(tr(", Comma"));
  }

  m_functions_combo = new QComboBoxWithMouseWheelDisabled(this);
  m_functions_combo->addItem(tr("Functions"));
  m_functions_combo->insertSeparator(1);
  m_functions_combo->addItem(QStringLiteral("if"));
  m_functions_combo->addItem(QStringLiteral("timer"));
  m_functions_combo->addItem(QStringLiteral("toggle"));
  m_functions_combo->addItem(QStringLiteral("deadzone"));
  m_functions_combo->addItem(QStringLiteral("smooth"));
  m_functions_combo->addItem(QStringLiteral("hold"));
  m_functions_combo->addItem(QStringLiteral("tap"));
  m_functions_combo->addItem(QStringLiteral("relative"));
  m_functions_combo->addItem(QStringLiteral("pulse"));
  m_functions_combo->addItem(QStringLiteral("sin"));
  m_functions_combo->addItem(QStringLiteral("cos"));
  m_functions_combo->addItem(QStringLiteral("tan"));
  m_functions_combo->addItem(QStringLiteral("asin"));
  m_functions_combo->addItem(QStringLiteral("acos"));
  m_functions_combo->addItem(QStringLiteral("atan"));
  m_functions_combo->addItem(QStringLiteral("atan2"));
  m_functions_combo->addItem(QStringLiteral("sqrt"));
  m_functions_combo->addItem(QStringLiteral("pow"));
  m_functions_combo->addItem(QStringLiteral("min"));
  m_functions_combo->addItem(QStringLiteral("max"));
  m_functions_combo->addItem(QStringLiteral("clamp"));
  m_functions_combo->addItem(QStringLiteral("abs"));

  m_variables_combo = new QComboBoxWithMouseWheelDisabled(this);
  m_variables_combo->addItem(tr("User Variables"));
  m_variables_combo->setToolTip(
      tr("User defined variables usable in the control expression.\nYou can use them to save or "
         "retrieve values between\ninputs and outputs of the same parent controller."));
  m_variables_combo->insertSeparator(m_variables_combo->count());
  m_variables_combo->addItem(tr("Reset Values"));
  m_variables_combo->insertSeparator(m_variables_combo->count());

  // Devices
  m_main_layout->addWidget(m_devices_combo);

  // Scalar
  auto* scalar_hbox = new QHBoxLayout();
  // i18n: Controller input values are multiplied by this percentage value.
  scalar_hbox->addWidget(new QLabel(tr("Multiplier")));
  scalar_hbox->addWidget(m_scalar_spinbox);

  // Outputs are not bounds checked and greater than 100% has no use case.
  // (incoming values are always 0 or 1)
  // Negative 100% can be used to invert force feedback wheel direction.
  const int scalar_min_max = (m_type == Type::Input) ? 1000 : 100;
  m_scalar_spinbox->setMinimum(-scalar_min_max);
  m_scalar_spinbox->setMaximum(scalar_min_max);
  // i18n: Percentage symbol.
  m_scalar_spinbox->setSuffix(tr("%"));

  // Options (Buttons, Outputs) and action buttons

  m_option_list->setTabKeyNavigation(false);

  if (m_type == Type::Input)
  {
    m_option_list->setColumnCount(2);
    m_option_list->setColumnWidth(1, 64);
    m_option_list->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);

    m_option_list->setItemDelegate(new InputStateDelegate(this, 1, [&](int row) {
      std::lock_guard lock(m_selected_device_mutex);
      // Clamp off negative values but allow greater than one in the text display.
      return std::max(GetSelectedDevice()->Inputs()[row]->GetState(), 0.0);
    }));
  }
  else
  {
    m_option_list->setColumnCount(1);
  }

  m_option_list->horizontalHeader()->hide();
  m_option_list->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
  m_option_list->verticalHeader()->hide();
  m_option_list->verticalHeader()->setDefaultSectionSize(
      m_option_list->verticalHeader()->minimumSectionSize());
  m_option_list->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_option_list->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_option_list->setSelectionMode(QAbstractItemView::SingleSelection);

  auto* hbox = new QHBoxLayout();
  auto* button_vbox = new QVBoxLayout();
  hbox->addWidget(m_option_list, 8);
  hbox->addLayout(button_vbox, 1);

  button_vbox->addWidget(m_select_button);

  if (m_type == Type::Input)
  {
    m_test_button->hide();
    button_vbox->addWidget(m_detect_button);
  }
  else
  {
    m_detect_button->hide();
    button_vbox->addWidget(m_test_button);
  }

  button_vbox->addWidget(m_variables_combo);

  button_vbox->addWidget(m_operators_combo);

  if (m_type == Type::Input)
    button_vbox->addWidget(m_functions_combo);
  else
    m_functions_combo->hide();

  button_vbox->addLayout(scalar_hbox);

  m_main_layout->addLayout(hbox, 2);
  m_main_layout->addWidget(m_expression_text, 1);
  m_main_layout->addWidget(m_parse_text);

  // Button Box
  m_main_layout->addWidget(m_button_box);
  m_button_box->addButton(m_clear_button, QDialogButtonBox::ActionRole);
  m_button_box->addButton(QDialogButtonBox::Ok);

  setLayout(m_main_layout);
}

void IOWindow::ConfigChanged()
{
  const QSignalBlocker blocker(this);
  const auto lock = ControllerEmu::EmulatedController::GetStateLock();

  // ensure m_parse_text is in the right state
  UpdateExpression(m_reference->GetExpression(), UpdateMode::Force);

  m_expression_text->setPlainText(QString::fromStdString(m_reference->GetExpression()));
  m_expression_text->moveCursor(QTextCursor::End, QTextCursor::MoveAnchor);
  m_scalar_spinbox->setValue(m_reference->range * 100.0);

  if (m_devq.ToString().empty())
    m_devq = m_controller->GetDefaultDevice();

  UpdateDeviceList();
}

void IOWindow::Update()
{
  m_option_list->viewport()->update();
  m_parse_text->update();
}

void IOWindow::ConnectWidgets()
{
  connect(m_select_button, &QPushButton::clicked, [this] { AppendSelectedOption(); });
  connect(m_option_list, &QTableWidget::cellDoubleClicked, [this] { AppendSelectedOption(); });
  connect(&Settings::Instance(), &Settings::ReleaseDevices, this, &IOWindow::ReleaseDevices);
  connect(&Settings::Instance(), &Settings::DevicesChanged, this, &IOWindow::UpdateDeviceList);

  connect(m_detect_button, &QPushButton::clicked, this, &IOWindow::OnDetectButtonPressed);
  connect(m_test_button, &QPushButton::clicked, this, &IOWindow::OnTestButtonPressed);

  connect(m_button_box, &QDialogButtonBox::clicked, this, &IOWindow::OnDialogButtonPressed);
  connect(m_devices_combo, &QComboBox::currentTextChanged, this, &IOWindow::OnDeviceChanged);
  connect(m_scalar_spinbox, &QSpinBox::valueChanged, this, &IOWindow::OnRangeChanged);

  connect(m_expression_text, &QPlainTextEdit::textChanged,
          [this] { UpdateExpression(m_expression_text->toPlainText().toStdString()); });

  connect(m_variables_combo, &QComboBox::activated, [this](int index) {
    if (index == 0)
      return;

    // Reset button. 1 and 3 are separators.
    if (index == 2)
    {
      const auto lock = ControllerEmu::EmulatedController::GetStateLock();
      m_controller->ResetExpressionVariables();
    }
    else
    {
      m_expression_text->insertPlainText(QLatin1Char('$') + m_variables_combo->currentText());
    }

    m_variables_combo->setCurrentIndex(0);
  });

  connect(m_operators_combo, &QComboBox::activated, [this](int index) {
    if (index == 0)
      return;

    m_expression_text->insertPlainText(m_operators_combo->currentText().left(1));

    m_operators_combo->setCurrentIndex(0);
  });

  connect(m_functions_combo, &QComboBox::activated, [this](int index) {
    if (index == 0)
      return;

    m_expression_text->insertPlainText(m_functions_combo->currentText() + QStringLiteral("()"));

    m_functions_combo->setCurrentIndex(0);
  });

  // revert the expression when the window closes without using the OK button
  connect(this, &IOWindow::finished, [this] { UpdateExpression(m_original_expression); });
}

void IOWindow::AppendSelectedOption()
{
  if (m_option_list->currentRow() < 0)
    return;

  m_expression_text->insertPlainText(
      QString::fromStdString(ciface::MappingCommon::GetExpressionForControl(
          m_option_list->item(m_option_list->currentRow(), 0)->text().toStdString(), m_devq,
          m_controller->GetDefaultDevice())));
}

void IOWindow::OnDeviceChanged()
{
  const std::string device_name =
      m_devices_combo->count() > 0 ? m_devices_combo->currentData().toString().toStdString() : "";
  m_devq.FromString(device_name);
  UpdateOptionList();
}

void IOWindow::OnDialogButtonPressed(QAbstractButton* button)
{
  if (button == m_clear_button)
  {
    m_expression_text->clear();
    return;
  }

  const auto lock = ControllerEmu::EmulatedController::GetStateLock();

  UpdateExpression(m_expression_text->toPlainText().toStdString());

  if (ciface::ExpressionParser::ParseStatus::SyntaxError == m_reference->GetParseStatus())
  {
    ModalMessageBox::warning(this, tr("Error"), tr("The expression contains a syntax error."));
  }
  else
  {
    // must be the OK button
    m_original_expression = m_reference->GetExpression();
    accept();
  }
}

void IOWindow::OnDetectButtonPressed()
{
  const auto expression =
      MappingCommon::DetectExpression(m_detect_button, g_controller_interface, {m_devq.ToString()},
                                      m_devq, ciface::MappingCommon::Quote::Off);

  if (expression.isEmpty())
    return;

  const auto list = m_option_list->findItems(expression, Qt::MatchFixedString);

  // Try to select the first. If this fails, the last selected item would still appear as such
  if (!list.empty())
    m_option_list->setCurrentItem(list[0]);
}

void IOWindow::OnTestButtonPressed()
{
  MappingCommon::TestOutput(m_test_button, static_cast<OutputReference*>(m_reference));
}

void IOWindow::OnRangeChanged(int value)
{
  m_reference->range = value / 100.0;
}

void IOWindow::ReleaseDevices()
{
  std::lock_guard lock(m_selected_device_mutex);
  m_selected_device = nullptr;
}

void IOWindow::UpdateOptionList()
{
  std::lock_guard lock(m_selected_device_mutex);
  m_selected_device = g_controller_interface.FindDevice(m_devq);
  m_option_list->setRowCount(0);

  if (m_selected_device == nullptr)
    return;

  const auto add_rows = [this](auto& container) {
    int row = 0;
    for (ciface::Core::Device::Control* control : container)
    {
      m_option_list->insertRow(row);

      if (control->IsHidden())
        m_option_list->hideRow(row);

      m_option_list->setItem(row, 0,
                             new QTableWidgetItem(QString::fromStdString(control->GetName())));
      ++row;
    }
  };

  if (m_reference->IsInput())
    add_rows(m_selected_device->Inputs());
  else
    add_rows(m_selected_device->Outputs());
}

void IOWindow::UpdateDeviceList()
{
  const QSignalBlocker blocker(m_devices_combo);

  const auto previous_device_name = m_devices_combo->currentData().toString().toStdString();

  m_devices_combo->clear();

  // Default to the default device or to the first device if there isn't a default.
  // Try to the keep the previous selected device, mark it as disconnected if it's gone, as it could
  // reconnect soon after if this is a devices refresh and it would be annoying to lose the value.
  const auto default_device_name = m_controller->GetDefaultDevice().ToString();
  int default_device_index = -1;
  int previous_device_index = -1;
  for (const auto& name : g_controller_interface.GetAllDeviceStrings())
  {
    QString qname = QString();
    if (name == default_device_name)
    {
      default_device_index = m_devices_combo->count();
      // Specify "default" even if we only have one device
      qname.append(QLatin1Char{'['} + tr("default") + QStringLiteral("] "));
    }
    if (name == previous_device_name)
    {
      previous_device_index = m_devices_combo->count();
    }
    qname.append(QString::fromStdString(name));
    m_devices_combo->addItem(qname, QString::fromStdString(name));
  }

  if (previous_device_index >= 0)
  {
    m_devices_combo->setCurrentIndex(previous_device_index);
  }
  else if (!previous_device_name.empty())
  {
    const QString qname = QString::fromStdString(previous_device_name);
    QString adjusted_qname;
    if (previous_device_name == default_device_name)
    {
      adjusted_qname.append(QLatin1Char{'['} + tr("default") + QStringLiteral("] "));
    }
    adjusted_qname.append(QLatin1Char{'['} + tr("disconnected") + QStringLiteral("] "))
        .append(qname);
    m_devices_combo->addItem(adjusted_qname, qname);
    m_devices_combo->setCurrentIndex(m_devices_combo->count() - 1);
  }
  else if (default_device_index >= 0)
  {
    m_devices_combo->setCurrentIndex(default_device_index);
  }
  else if (m_devices_combo->count() > 0)
  {
    m_devices_combo->setCurrentIndex(0);
  }
  // The device object might have changed so we need to always refresh it
  OnDeviceChanged();
}

void IOWindow::UpdateExpression(std::string new_expression, UpdateMode mode)
{
  const auto lock = m_controller->GetStateLock();
  if (mode != UpdateMode::Force && new_expression == m_reference->GetExpression())
    return;

  const auto error = m_reference->SetExpression(std::move(new_expression));
  const auto status = m_reference->GetParseStatus();
  m_controller->UpdateSingleControlReference(g_controller_interface, m_reference);

  // This is the only place where we need to update the user variables. Keep the first 4 items.
  while (m_variables_combo->count() > 4)
  {
    m_variables_combo->removeItem(m_variables_combo->count() - 1);
  }
  for (const auto& expression : m_controller->GetExpressionVariables())
  {
    m_variables_combo->addItem(QString::fromStdString(expression.first));
  }

  if (error)
  {
    m_parse_text->SetShouldPaintStateIndicator(false);
    m_parse_text->setText(QString::fromStdString(*error));
  }
  else if (status == ciface::ExpressionParser::ParseStatus::EmptyExpression)
  {
    m_parse_text->SetShouldPaintStateIndicator(false);
    m_parse_text->setText(QString());
  }
  else if (status != ciface::ExpressionParser::ParseStatus::Successful)
  {
    m_parse_text->SetShouldPaintStateIndicator(false);
    m_parse_text->setText(tr("Invalid Expression."));
  }
  else
  {
    m_parse_text->SetShouldPaintStateIndicator(true);
    m_parse_text->setText(QString());
  }
}

InputStateDelegate::InputStateDelegate(IOWindow* parent, int column,
                                       std::function<ControlState(int row)> state_evaluator)
    : QItemDelegate(parent), m_state_evaluator(std::move(state_evaluator)), m_column(column)
{
}

InputStateLineEdit::InputStateLineEdit(std::function<ControlState()> state_evaluator)
    : m_state_evaluator(std::move(state_evaluator))
{
}

static void PaintStateIndicator(QPainter& painter, const QRect& region, ControlState state)
{
  const QString state_string = QString::number(state, 'g', 4);

  QRect meter_region = region;
  meter_region.setWidth(region.width() * std::clamp(state, 0.0, 1.0));

  // Create a temporary indicator object to retreive color constants.
  MappingIndicator indicator;

  // Normal text.
  painter.setPen(indicator.GetTextColor());
  painter.drawText(region, Qt::AlignCenter, state_string);

  // Input state meter.
  painter.fillRect(meter_region, indicator.GetAdjustedInputColor());

  // Text on top of meter.
  painter.setPen(indicator.GetAltTextColor());
  painter.setClipping(true);
  painter.setClipRect(meter_region);
  painter.drawText(region, Qt::AlignCenter, state_string);
}

void InputStateDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                               const QModelIndex& index) const
{
  QItemDelegate::paint(painter, option, index);

  if (index.column() != m_column)
    return;

  painter->save();
  PaintStateIndicator(*painter, option.rect, m_state_evaluator(index.row()));
  painter->restore();
}

void InputStateLineEdit::SetShouldPaintStateIndicator(bool value)
{
  m_should_paint_state_indicator = value;
}

void InputStateLineEdit::paintEvent(QPaintEvent* event)
{
  QLineEdit::paintEvent(event);

  if (!m_should_paint_state_indicator)
    return;

  QPainter painter(this);
  PaintStateIndicator(painter, this->rect(), m_state_evaluator());
}
