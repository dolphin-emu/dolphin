// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/Mapping/IOWindow.h"

#include <optional>
#include <thread>

#include <QComboBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemDelegate>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

#include "Core/ConfigManager.h"
#include "Core/Core.h"

#include "DolphinQt/Config/Mapping/MappingCommon.h"
#include "DolphinQt/Config/Mapping/MappingIndicator.h"
#include "DolphinQt/Config/Mapping/MappingWidget.h"
#include "DolphinQt/Config/Mapping/MappingWindow.h"
#include "DolphinQt/QtUtils/BlockUserInputFilter.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"

#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControlReference/ExpressionParser.h"
#include "InputCommon/ControlReference/FunctionExpression.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"

constexpr int SLIDER_TICK_COUNT = 100;

namespace
{
// TODO: Make sure these functions return colors that will be visible in the current theme.
QTextCharFormat GetSpecialCharFormat()
{
  QTextCharFormat format;
  format.setFontWeight(QFont::Weight::Bold);
  return format;
}

QTextCharFormat GetLiteralCharFormat()
{
  QTextCharFormat format;
  format.setForeground(QBrush{Qt::darkMagenta});
  return format;
}

QTextCharFormat GetInvalidCharFormat()
{
  QTextCharFormat format;
  format.setUnderlineStyle(QTextCharFormat::WaveUnderline);
  format.setUnderlineColor(Qt::darkRed);
  return format;
}

QTextCharFormat GetControlCharFormat()
{
  QTextCharFormat format;
  format.setForeground(QBrush{Qt::darkGreen});
  return format;
}

QTextCharFormat GetVariableCharFormat()
{
  QTextCharFormat format;
  format.setForeground(QBrush{Qt::darkYellow});
  return format;
}

QTextCharFormat GetBarewordCharFormat()
{
  QTextCharFormat format;
  format.setForeground(QBrush{Qt::darkBlue});
  return format;
}

QTextCharFormat GetCommentCharFormat()
{
  QTextCharFormat format;
  format.setForeground(QBrush{Qt::darkGray});
  return format;
}
}  // namespace

ControlExpressionSyntaxHighlighter::ControlExpressionSyntaxHighlighter(QTextDocument* parent)
    : QSyntaxHighlighter(parent)
{
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

// Also used for output
class InputStateLineEdit : public QLineEdit
{
public:
  explicit InputStateLineEdit(std::function<ControlState()> state_evaluator);
  void SetShouldPaintStateIndicator(bool value);
  void paintEvent(QPaintEvent* event) override;

private:
  std::function<ControlState()> m_state_evaluator;
  bool m_should_paint_state_indicator;
};

IOWindow::IOWindow(MappingWidget* parent, ControllerEmu::EmulatedController* controller,
                   ControlReference* ref, IOWindow::Type type)
    : QDialog(parent), m_reference(ref), m_original_expression(ref->GetExpression()),
      m_controller(controller), m_type(type)
{
  CreateMainLayout();

  connect(parent, &MappingWidget::Update, this, &IOWindow::Update);

  // TODO: it would be nice to have the name of the input in the window
  // (control->ui_name from MappingWidget.cpp)
  QString title =
      m_type == IOWindow::Type::Output ?
          tr("Configure Output") :
          (m_type == IOWindow::Type::Input ? tr("Configure Input") : tr("Configure Input Setting"));
  setWindowTitle(title);
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  ConfigChanged();

  ConnectWidgets();
}

std::shared_ptr<ciface::Core::Device> IOWindow::GetSelectedDevice()
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
  m_range_slider = new QSlider(Qt::Horizontal);
  m_range_spinbox = new QSpinBox();

  m_parse_text = new InputStateLineEdit([this] {
    const auto lock = m_controller->GetStateLock();
    return m_reference->GetState<ControlState>();
  });
  m_parse_text->setReadOnly(true);

  m_expression_text = new QPlainTextEdit();
  m_expression_text->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  new ControlExpressionSyntaxHighlighter(m_expression_text->document());

  m_operators_combo = new QComboBox();
  m_operators_combo->addItem(tr("Operators"));
  m_operators_combo->insertSeparator(1);
  if (m_type == Type::Input || m_type == Type::InputSetting)
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
  if (m_type == Type::Input || m_type == Type::InputSetting)
  {
    m_operators_combo->addItem(tr(", Comma"));
  }

  m_functions_combo = new QComboBox(this);
  m_functions_combo->addItem(tr("Functions"));
  // This might an empty unselectable whitespace at the end of the ComboBox selection
  m_functions_combo->insertSeparator(m_functions_combo->count());  // A separator is also an item
  m_functions_parameters.push_back(QStringLiteral(""));            // Keep the indexes aligned
  // Logic/Math:
  AddFunction("if");
  AddFunction("not");
  AddFunction("min");
  AddFunction("max");
  AddFunction("clamp");
  AddFunction("minus");
  AddFunction("pow");
  AddFunction("sqrt");
  AddFunction("sin");
  AddFunction("cos");
  AddFunction("tan");
  AddFunction("asin");
  AddFunction("acos");
  AddFunction("atan");
  AddFunction("atan2");
  m_functions_combo->insertSeparator(m_functions_combo->count());
  m_functions_parameters.push_back(QStringLiteral(""));
  // State/time based:
  AddFunction("onPress");
  AddFunction("onRelease");
  AddFunction("onChange");
  AddFunction("cache");
  AddFunction("hold");
  AddFunction("toggle");
  AddFunction("tap");
  AddFunction("relative");
  AddFunction("smooth");
  AddFunction("pulse");
  AddFunction("timer");
  m_functions_combo->insertSeparator(m_functions_combo->count());
  m_functions_parameters.push_back(QStringLiteral(""));
  // Stick helpers:
  AddFunction("deadzone");
  AddFunction("antiDeadzone");
  AddFunction("bezierCurve");
  AddFunction("antiAcceleration");

  // Devices
  m_main_layout->addWidget(m_devices_combo);

  // Range
  auto* range_hbox = new QHBoxLayout();
  range_hbox->addWidget(new QLabel(tr("Range (%)")));
  range_hbox->addWidget(m_range_slider);
  range_hbox->addWidget(m_range_spinbox);
  m_range_slider->setMinimum(-500);
  m_range_slider->setMaximum(500);
  m_range_spinbox->setMinimum(-500);
  m_range_spinbox->setMaximum(500);
  m_range_slider->setToolTip(
      tr("Multiplies (not clamp) the final result. Usually defaults to 100"));
  m_range_spinbox->setToolTip(m_range_slider->toolTip());
  // If this is an input setting, the range won't be saved in our config,
  // nor it would make sense to change it, so just hide it
  if (m_type != Type::InputSetting)
    m_main_layout->addLayout(range_hbox);

  // Options (Buttons, Outputs) and action buttons

  m_option_list->setTabKeyNavigation(false);

  if (m_type == Type::Input || m_type == Type::InputSetting)
  {
    m_option_list->setColumnCount(2);
    m_option_list->setColumnWidth(1, 64);
    m_option_list->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);

    m_option_list->setItemDelegate(new InputStateDelegate(this, 1, [&](int row) {
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

  if (m_type == Type::Input || m_type == Type::InputSetting)
  {
    m_test_button->hide();
    button_vbox->addWidget(m_detect_button);
  }
  else
  {
    m_detect_button->hide();
    button_vbox->addWidget(m_test_button);
  }

  button_vbox->addWidget(m_operators_combo);

  if (m_type == Type::Input || m_type == Type::InputSetting)
    button_vbox->addWidget(m_functions_combo);
  else
    m_functions_combo->hide();

  m_main_layout->addLayout(hbox, 2);
  m_main_layout->addWidget(m_expression_text, 1);
  m_main_layout->addWidget(m_parse_text);

  // Button Box
  m_main_layout->addWidget(m_button_box);
  m_button_box->addButton(m_clear_button, QDialogButtonBox::ActionRole);
  m_button_box->addButton(QDialogButtonBox::Ok);

  setLayout(m_main_layout);
}

void IOWindow::AddFunction(std::string function_name)
{
  using namespace ciface::ExpressionParser;
  // Try to instantiate the function by name
  std::unique_ptr<FunctionExpression> func = MakeFunctionExpression(function_name);

  // Don't do anything if the function doesn't exist
  if (func.get() == nullptr)
  {
    return;
  }

  m_functions_combo->addItem(QString::fromStdString(function_name));

  // Try to set the function arguments, which will return a corrected version
  // if they are not valid
  std::vector<std::unique_ptr<Expression>> fake_args;
  const auto argument_validation = func->SetArguments(std::move(fake_args));

  // If the fake empty arguments are invalid, pre-fill commas between them.
  // We don't add the argument names themselves because names are always parsed correctly,
  // so the expression would be accepted. Also, errors are visible at the bottom of the widget
  if (std::holds_alternative<FunctionExpression::ExpectedArguments>(argument_validation))
  {
    std::string correct_args =
        std::get<FunctionExpression::ExpectedArguments>(argument_validation).text;

    int commas_num = std::count(correct_args.begin(), correct_args.end(), ',');
    int brackets_num = std::count(correct_args.begin(), correct_args.end(), '[');
    int equals_num = std::count(correct_args.begin(), correct_args.end(), '=');

    // Arguments that either have brackets or equal are not mandatory.
    // As an alternative we should just pass in an increasing number of argments,
    // and see the first number that gets accepted
    commas_num -= brackets_num;
    commas_num -= equals_num;

    std::string commas = "";
    while (commas_num > 0)
    {
      commas += ",";
      --commas_num;
    }

    m_functions_parameters.push_back(QString::fromStdString(commas));
  }
  else
  {
    m_functions_parameters.push_back(QStringLiteral(""));
  }
}

void IOWindow::ConfigChanged()
{
  const QSignalBlocker blocker(this);
  const auto lock = m_controller->GetStateLock();

  // ensure m_parse_text is in the right state
  UpdateExpression(m_reference->GetExpression(), UpdateMode::Force);

  m_expression_text->setPlainText(QString::fromStdString(m_reference->GetExpression()));
  m_expression_text->moveCursor(QTextCursor::End, QTextCursor::MoveAnchor);
  m_range = m_reference->range;
  m_range_spinbox->setValue(m_range * SLIDER_TICK_COUNT);
  m_range_slider->setValue(m_range * SLIDER_TICK_COUNT);

  m_devq = m_controller->GetDefaultDevice();

  UpdateDeviceList();
  UpdateOptionList();
}

void IOWindow::Update()
{
  m_option_list->viewport()->update();
  m_parse_text->update();
}

void IOWindow::ConnectWidgets()
{
  connect(m_select_button, &QPushButton::clicked, [this] { AppendSelectedOption(); });

  connect(m_detect_button, &QPushButton::clicked, this, &IOWindow::OnDetectButtonPressed);
  connect(m_test_button, &QPushButton::clicked, this, &IOWindow::OnTestButtonPressed);

  connect(m_button_box, &QDialogButtonBox::clicked, this, &IOWindow::OnDialogButtonPressed);
  connect(m_devices_combo, &QComboBox::currentTextChanged, this, &IOWindow::OnDeviceChanged);
  connect(m_range_spinbox, qOverload<int>(&QSpinBox::valueChanged), this,
          &IOWindow::OnRangeChanged);
  connect(m_range_slider, &QSlider::valueChanged, this, &IOWindow::OnRangeChanged);

  connect(m_expression_text, &QPlainTextEdit::textChanged,
          [this] { UpdateExpression(m_expression_text->toPlainText().toStdString()); });

  connect(m_operators_combo, qOverload<int>(&QComboBox::activated), [this](int index) {
    if (0 == index)
      return;

    m_expression_text->insertPlainText(m_operators_combo->currentText().left(1));

    m_operators_combo->setCurrentIndex(0);
  });

  connect(m_functions_combo, qOverload<int>(&QComboBox::activated), [this](int index) {
    if (0 == index || 1 == index)
      return;

    m_expression_text->insertPlainText(m_functions_combo->currentText() + QStringLiteral("(") +
                                       m_functions_parameters[index - 1] + QStringLiteral(")"));

    m_functions_combo->setCurrentIndex(0);
  });

  // revert the expression and range changes when the window closes without using the OK button
  connect(this, &IOWindow::finished, [this] {
    UpdateExpression(m_original_expression);
  });
  connect(this, &IOWindow::accepted, [this] {
    SaveRange();
  });
}

void IOWindow::AppendSelectedOption()
{
  if (m_option_list->currentItem() == nullptr)
    return;

  m_expression_text->insertPlainText(MappingCommon::GetExpressionForControl(
      m_option_list->currentItem()->text(), m_devq, m_controller->GetDefaultDevice()));
}

void IOWindow::OnDeviceChanged()
{
  m_devq.FromString(m_devices_combo->currentData().toString().toStdString());
  UpdateOptionList();
}

void IOWindow::OnDialogButtonPressed(QAbstractButton* button)
{
  if (button == m_clear_button)
  {
    m_expression_text->clear();
    return;
  }

  const auto lock = m_controller->GetStateLock();

  UpdateExpression(m_expression_text->toPlainText().toStdString());
  m_original_expression = m_reference->GetExpression();

  if (ciface::ExpressionParser::ParseStatus::SyntaxError == m_reference->GetParseStatus())
  {
    ModalMessageBox::warning(this, tr("Error"), tr("The expression contains a syntax error."));
  }

  // must be the OK button
  accept();
}

void IOWindow::OnDetectButtonPressed()
{
  const auto expression =
      MappingCommon::DetectExpression(m_detect_button, g_controller_interface, {m_devq.ToString()},
                                      m_devq, MappingCommon::Quote::Off);

  if (expression.isEmpty())
    return;

  const auto list = m_option_list->findItems(expression, Qt::MatchFixedString);

  if (!list.empty())
    m_option_list->setCurrentItem(list[0]);
}

void IOWindow::OnTestButtonPressed()
{
  MappingCommon::TestOutput(m_test_button, static_cast<OutputReference*>(m_reference));
}

void IOWindow::OnRangeChanged(int value)
{
  m_range = static_cast<ControlState>(value) / SLIDER_TICK_COUNT;
  m_range_spinbox->setValue(m_range * SLIDER_TICK_COUNT);
  m_range_slider->setValue(m_range * SLIDER_TICK_COUNT);
}

void IOWindow::SaveRange()
{
  m_reference->range = m_range;
}

void IOWindow::UpdateOptionList()
{
  m_selected_device = g_controller_interface.FindDevice(m_devq);
  m_option_list->setRowCount(0);

  if (m_selected_device == nullptr)
    return;

  if (m_reference->IsInput())
  {
    int row = 0;
    for (const auto* input : m_selected_device->Inputs())
    {
      m_option_list->insertRow(row);
      m_option_list->setItem(row, 0,
                             new QTableWidgetItem(QString::fromStdString(input->GetName())));
      ++row;
    }
  }
  else
  {
    int row = 0;
    for (const auto* output : m_selected_device->Outputs())
    {
      m_option_list->insertRow(row);
      m_option_list->setItem(row, 0,
                             new QTableWidgetItem(QString::fromStdString(output->GetName())));
      ++row;
    }
  }
}

// Note that this isn't refreshed when we add/remove a device
void IOWindow::UpdateDeviceList()
{
  m_devices_combo->clear();

  auto default_name = m_controller->GetDefaultDevice().ToString();
  int default_device_index = -1;
  for (const auto& name : g_controller_interface.GetAllDeviceStrings())
  {
    QString qname = QString();
    if (name == default_name)
    {
      default_device_index = m_devices_combo->count();
      // Specify "default" even if we only have one device
      qname.append(QStringLiteral("[default] "));
    }
    qname.append(QString::fromStdString(name));
    m_devices_combo->addItem(qname, QString::fromStdString(name));
  }

  if (default_device_index >= 0)
  {
    m_devices_combo->setCurrentIndex(default_device_index);
  }
  else
  {
    // Refresh to the first device
    OnDeviceChanged();
  }
}

void IOWindow::UpdateExpression(std::string new_expression, UpdateMode mode)
{
  const auto lock = m_controller->GetStateLock();
  if (mode != UpdateMode::Force && new_expression == m_reference->GetExpression())
    return;

  const auto error = m_reference->SetExpression(std::move(new_expression));
  const auto status = m_reference->GetParseStatus();
  m_controller->UpdateSingleControlReference(g_controller_interface, m_reference);

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
