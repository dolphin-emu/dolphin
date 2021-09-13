// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Mapping/IOWindow.h"

#include <optional>
#include <thread>

#include <QComboBox>
#include <QDialogButtonBox>
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
#include <QToolTip>
#include <QVBoxLayout>

#include "Core/ConfigManager.h"
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
#include "InputCommon/ControlReference/FunctionExpression.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"

constexpr int RANGE_PERCENTAGE = 100;

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

// Only works if there is at least one empty space per length.
// Might be useful to have somewhere generic but we don't have such class.
QString SplitTooltip(QString text, int length)
{
  QString result;
  for (;;)
  {
    int i = 0;
    while (i < text.length())
    {
      if (text.left(++i + 1).length() > length)
      {
        int j = text.lastIndexOf(QLatin1Char{' '}, i);
        if (j > 0)
          i = j;
        result.append(text.left(i));
        result.append(QLatin1Char{'\n'});
        text = text.mid(i + 1);
        break;
      }
    }
    if (i >= text.length())
      break;
  }
  return result + text;
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

// Also used for output
class InputStateLineEdit : public QLineEdit
{
public:
  explicit InputStateLineEdit(std::function<ControlState()> state_evaluator, ControlState min = 0.0,
                              ControlState max = 1.0);
  void SetShouldPaintStateIndicator(bool value);
  void paintEvent(QPaintEvent* event) override;

private:
  std::function<ControlState()> m_state_evaluator;
  bool m_should_paint_state_indicator;
  ControlState m_min;
  ControlState m_max;
};

IOWindow::IOWindow(MappingWidget* parent, ControllerEmu::EmulatedController* controller,
                   ControlReference* ref, IOWindow::Type type, const QString& name,
                   ControllerEmu::NumericSettingBase* numeric_setting)
    : QDialog(parent), m_reference(ref), m_controller(controller), m_devq(), m_type(type),
      m_numeric_setting(numeric_setting)
{
  CreateMainLayout();

  connect(parent, &MappingWidget::Update, this, &IOWindow::Update);
  connect(parent->GetParent(), &MappingWindow::ConfigChanged, this, &IOWindow::ConfigChanged);
  connect(&Settings::Instance(), &Settings::ConfigChanged, this, &IOWindow::ConfigChanged);

  QString title =
      m_type == IOWindow::Type::Output ?
          tr("Configure Output") :
          (m_type == IOWindow::Type::Input ? tr("Configure Input") : tr("Configure Input Setting"));
  if (!name.isEmpty())
    title.append(QStringLiteral(": %1").arg(name));
  setWindowTitle(title);
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
  m_help_button = new QPushButton(tr("Help"));
  m_select_button = new QPushButton(tr("Add Selected"));
  m_detect_button = new QPushButton(tr("Detect"), this);
  m_test_selected_button = new QPushButton(tr("Test Selected"), this);
  m_test_results_button = new QPushButton(tr("Test Results"), this);
  m_button_box = new QDialogButtonBox();
  m_clear_button = new QPushButton(tr("Clear"));
  m_range_slider = new QSlider(Qt::Horizontal);
  m_range_spinbox = new QSpinBox();
  m_focus_label = new QLabel();

  m_select_button->setEnabled(false);
  m_test_selected_button->setEnabled(false);

  m_detect_button->setToolTip(
      tr("Detect input from the current device.\nNote that some inputs can't be detected."));
  m_focus_label->setToolTip(
      tr("Window Focus.\nThese requirements could be ignored depending on your settings"));

  if (m_numeric_setting)
  {
    m_parse_text = new InputStateLineEdit(
        [this] {
          if (m_reference->IsInput())
          {
            // This is needed because numeric settings simplify the expression (and set it to
            // nullptr/"") when they have a simple value, so we don't want to read the expression
            // value anymore, but their cached in custom value.
            return m_numeric_setting->GetGenericValue();
          }
          return 0.0;
        },
        m_numeric_setting->GetGenericMinValue(), m_numeric_setting->GetGenericMaxValue());
  }
  else
  {
    m_parse_text = new InputStateLineEdit([this] {
      // Note that ControlReference values are cached upfront, so they might look
      // different from the values in the inputs list
      if (m_reference->IsInput())
        return m_reference->GetState<ControlState>();
      return 0.0;
    });
  }
  m_parse_text->setReadOnly(true);

  m_expression_text = new QPlainTextEdit();
  m_expression_text->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  new ControlExpressionSyntaxHighlighter(m_expression_text->document());

  m_operators_combo = new QComboBoxWithMouseWheelDisabled(this);
  m_operators_combo->addItem(tr("Operators"));
  m_operators_combo->insertSeparator(1);
  // These operators won't be needed with Outputs
  if (m_type == Type::Input || m_type == Type::InputSetting)
  {
    m_operators_combo->addItem(tr("! Not"));
    m_operators_combo->addItem(tr("* Multiply"));
    m_operators_combo->addItem(tr("/ Divide"));
    m_operators_combo->addItem(tr("% Modulo"));
    m_operators_combo->addItem(tr("+ Add"));
    m_operators_combo->addItem(tr("- Subtract"));
    m_operators_combo->addItem(tr("> Greater than"));
    m_operators_combo->addItem(tr("< Less than"));
    m_operators_combo->addItem(tr("& And"));
  }
  m_operators_combo->addItem(tr("| Or"));
  m_operators_combo->addItem(tr("$ User Variable"));
  if (m_type == Type::Input || m_type == Type::InputSetting)
  {
    m_operators_combo->addItem(tr("^ Xor"));
    m_operators_combo->addItem(tr("= Assign"));
    m_operators_combo->addItem(tr("$ User variable"));
    m_operators_combo->addItem(tr(", Comma"));
  }

  m_functions_combo = new QComboBoxWithMouseWheelDisabled(this);
  m_functions_combo->addItem(tr("Functions"));
  m_functions_combo->setToolTip(
      tr("Most functions take arguments (arg): you can either put a control there,\na constant "
         "number or any other function.\n[arg] means that it's optional and can be not "
         "inserted.\n\"arg = x\" means that it will "
         "default to that value if you don't specify it.\n\"...\" means unlimited args are "
         "supported.\n\nWhen functions ask for a number of frames, they are usually input update "
         "frames,\nwhich differ from video frames (the video refresh rate, not the game "
         "FPS).\nWhen functions ask for a time, it's usually used as game time, not real world "
         "time.\nDon't try to mix input and output functions and expect them work.\nTheir state is "
         "updated even when emulation is not running and will carry over"));
  // This might cause an empty unselectable whitespace at the end of the ComboBox selection.
  // Given that a separator is also an item, we add an empty "func param" to keep indexes aligned.
  m_functions_combo->insertSeparator(m_functions_combo->count());
  m_functions_parameters.push_back(QStringLiteral(""));
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
  if (m_type == Type::Output)
  {
    AddFunction("scaleSet");
  }
  m_functions_combo->insertSeparator(m_functions_combo->count());
  m_functions_parameters.push_back(QStringLiteral(""));
  // State/time based:
  AddFunction("onPress");
  AddFunction("onRelease");
  AddFunction("onChange");
  AddFunction("onHold");
  AddFunction("onTap");
  AddFunction("cache");
  AddFunction("toggle");
  AddFunction("sharedRelative");
  AddFunction("relativeToSpeed");
  AddFunction("smooth");
  AddFunction("pulse");
  AddFunction("timer");
  AddFunction("interval");
  m_functions_combo->insertSeparator(m_functions_combo->count());
  m_functions_parameters.push_back(QStringLiteral(""));
  // Recordings:
  AddFunction("average");
  AddFunction("sum");
  AddFunction("record");
  AddFunction("sequence");
  AddFunction("lag");
  m_functions_combo->insertSeparator(m_functions_combo->count());
  m_functions_parameters.push_back(QStringLiteral(""));
  // Stick helpers:
  AddFunction("deadzone");
  AddFunction("antiDeadzone");
  AddFunction("bezierCurve");
  AddFunction("antiAcceleration");
  m_functions_combo->insertSeparator(m_functions_combo->count());
  m_functions_parameters.push_back(QStringLiteral(""));
  // Meta/Focus:
  AddFunction("gameSpeed");
  AddFunction("aspectRatio");
  AddFunction("timeToInputFrames");
  AddFunction("videoToInputFrames");
  AddFunction("hasFocus");
  // These functions can't be used on input settings (they would just be ignored)
  if (m_type != Type::InputSetting)
  {
    AddFunction("requireFocus");
  }
  AddFunction("ignoreFocus");
  // These wouldn't make sense on outputs or inputs settings
  if (m_type == Type::Input)
  {
    AddFunction("ignoreOnFocusChange");
  }

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

  // Range
  auto* range_hbox = new QHBoxLayout();
  range_hbox->addWidget(new QLabel(tr("Range (%)")));
  range_hbox->addWidget(m_range_slider);
  range_hbox->addWidget(m_range_spinbox);
  // Being a double spin box would allow us more accuracy, but that's why we made it a percentage
  m_range_spinbox->setRange(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
  m_range_slider->setRange(-RANGE_PERCENTAGE * 5, RANGE_PERCENTAGE * 5);
  m_range_slider->setTickPosition(QSlider::TickPosition::TicksBelow);
  m_range_slider->setTickInterval(RANGE_PERCENTAGE);
  m_range_slider->setToolTip(tr("Multiplies (not clamp) the final result. Defaults to %1")
                                 .arg(m_reference->default_range * RANGE_PERCENTAGE));
  m_range_spinbox->setToolTip(m_range_slider->toolTip());
  // If this is an input setting, the range won't be saved in our config,
  // nor it would make sense to change it, so just hide it
  if (m_type != Type::InputSetting)
    m_main_layout->addLayout(range_hbox);

  // Options (Buttons, Outputs) and action buttons

  // Preview input values
  if (m_type == Type::Input || m_type == Type::InputSetting)
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

  m_option_list->setTabKeyNavigation(false);
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

  button_vbox->addWidget(m_help_button);
  button_vbox->addWidget(m_select_button);

  if (m_type == Type::Input || m_type == Type::InputSetting)
  {
    m_test_selected_button->hide();
    m_test_results_button->hide();
    button_vbox->addWidget(m_detect_button);
  }
  else
  {
    m_detect_button->hide();
    button_vbox->addWidget(m_test_selected_button);
    button_vbox->addWidget(m_test_results_button);
  }

  button_vbox->addWidget(m_variables_combo);

  button_vbox->addWidget(m_operators_combo);

  if (m_functions_combo->count() > 2)
    button_vbox->addWidget(m_functions_combo);
  else
    m_functions_combo->hide();

  m_main_layout->addLayout(hbox, 2);
  m_main_layout->addWidget(m_expression_text, 1);
  m_main_layout->addWidget(m_parse_text);

  auto* hbox2 = new QHBoxLayout();
  hbox2->addWidget(m_focus_label, Qt::AlignLeft);

  // Button Box
  m_button_box->addButton(m_clear_button, QDialogButtonBox::ActionRole);
  m_button_box->addButton(QDialogButtonBox::Ok);
  hbox2->addWidget(m_button_box, Qt::AlignRight);

  m_main_layout->addLayout(hbox2, 2);

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

  // Try to set the empty function arguments, which will return a corrected version
  // if they are not valid.
  std::vector<std::unique_ptr<Expression>> fake_args;
  const auto argument_validation = func->SetArguments(std::move(fake_args));

  QString func_tooltip;

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

    // Arguments won't be translated, just like function names
    func_tooltip = tr("Arguments: %1").arg(QString::fromStdString(correct_args));
    m_functions_parameters.push_back(QString::fromStdString(commas));
  }
  // Note that there might still be some optional arguments in this case,
  // we can't easily tell without spamming any params num to the func
  else
  {
    func_tooltip = tr("No required arguments");
    m_functions_parameters.push_back(QStringLiteral(""));
  }

  if (const char* description = func->GetDescription(m_type != Type::Output))
  {
    func_tooltip.append(QStringLiteral("\n%1").arg(tr(description)));
  }

  m_functions_combo->setItemData(m_functions_combo->count() - 1, SplitTooltip(func_tooltip, 72),
                                 Qt::ToolTipRole);
}

void IOWindow::ConfigChanged()
{
  const QSignalBlocker blocker(this);
  const auto lock = ControllerEmu::EmulatedController::GetStateLock();

  if (m_numeric_setting && m_numeric_setting->IsSimpleValue())
    m_numeric_setting->SetExpressionFromValue();

  m_original_expression = m_reference->GetExpression();

  m_original_range = m_reference->range;
  OnRangeChanged();

  // ensure m_parse_text is in the right state
  UpdateExpression(m_reference->GetExpression(), UpdateMode::Force);

  {
    const QSignalBlocker blocker_expression(m_expression_text);
    // The expression could be empty in this case so fill it up
    if (m_numeric_setting && m_numeric_setting->IsSimpleValue())
    {
      m_expression_text->setPlainText(
          QString::fromStdString(m_numeric_setting->GetExpressionFromValue()));
    }
    else
    {
      m_expression_text->setPlainText(QString::fromStdString(m_reference->GetExpression()));
    }
  }
  m_expression_text->moveCursor(QTextCursor::End, QTextCursor::MoveAnchor);

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
  connect(m_option_list, &QTableWidget::itemSelectionChanged, [this] {
    const bool any_selected = m_option_list->currentRow() >= 0;
    m_test_selected_button->setEnabled(any_selected);
    m_select_button->setEnabled(any_selected);
  });

  connect(m_help_button, &QPushButton::clicked, [this] {
    QString help_tooltip =
        tr("You can either simply bind an %1 or use functions\nand operators to achieve more "
           "complex results.\nYou can bind controls from multiple devices.%2\nSee more "
           "at https://wiki.dolphin-emu.org/index.php?title=Input_Syntax")
            .arg(m_type != Type::Input ? tr("output") : tr("input"))
            .arg(m_type != Type::Input ?
                     QString() :
                     tr("\nSome inputs are relative and might need to be converted\nto a rate or "
                        "smoothed over time to be correctly mapped."));
    QToolTip::showText(QCursor::pos(), help_tooltip, this);
  });

  connect(&Settings::Instance(), &Settings::ReleaseDevices, this, &IOWindow::ReleaseDevices);
  connect(&Settings::Instance(), &Settings::DevicesChanged, this, &IOWindow::UpdateDeviceList);

  connect(m_detect_button, &QPushButton::clicked, this, &IOWindow::OnDetectButtonPressed);
  connect(m_test_selected_button, &QPushButton::clicked, this,
          &IOWindow::OnTestSelectedButtonPressed);
  connect(m_test_results_button, &QPushButton::clicked, this,
          &IOWindow::OnTestResultsButtonPressed);

  connect(m_button_box, &QDialogButtonBox::clicked, this, &IOWindow::OnDialogButtonPressed);
  connect(m_devices_combo, &QComboBox::currentTextChanged, this, &IOWindow::OnDeviceChanged);
  connect(m_range_spinbox, qOverload<int>(&QSpinBox::valueChanged), this,
          &IOWindow::OnUIRangeChanged);
  connect(m_range_slider, &QSlider::valueChanged, this, &IOWindow::OnUIRangeChanged);

  connect(m_expression_text, &QPlainTextEdit::textChanged,
          [this] { UpdateExpression(m_expression_text->toPlainText().toStdString()); });

  connect(m_variables_combo, qOverload<int>(&QComboBox::activated), [this](int index) {
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

  connect(m_operators_combo, qOverload<int>(&QComboBox::activated), [this](int index) {
    if (index == 0)
      return;

    m_expression_text->insertPlainText(m_operators_combo->currentText().left(1));

    m_operators_combo->setCurrentIndex(0);
  });

  connect(m_functions_combo, qOverload<int>(&QComboBox::activated), [this](int index) {
    if (index == 0)
      return;

    m_expression_text->insertPlainText(m_functions_combo->currentText() + QLatin1Char('(') +
                                       m_functions_parameters[index - 1] + QLatin1Char(')'));

    m_functions_combo->setCurrentIndex(0);
  });

  // revert the expression and range changes when the window closes without using the OK button
  connect(this, &IOWindow::finished, [this] {
    // Lock just to be sure, range is atomic. Also it can't have changed if we are an input setting.
    const auto lock = ControllerEmu::EmulatedController::GetStateLock();
    m_reference->range = m_original_range;
    UpdateExpression(m_original_expression);
  });
}

void IOWindow::AppendSelectedOption()
{
  if (m_option_list->currentRow() < 0)
    return;

  m_expression_text->insertPlainText(MappingCommon::GetExpressionForControl(
      m_option_list->item(m_option_list->currentRow(), 0)->text(), m_devq,
      m_controller->GetDefaultDevice()));
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
  // Clear button won't save the results unless we click OK
  if (button == m_clear_button)
  {
    m_expression_text->clear();
    return;
  }

  const auto lock = ControllerEmu::EmulatedController::GetStateLock();

  // Update the original expression and range as they will be set back when this widget closes.

  m_original_range = m_reference->range;

  UpdateExpression(m_expression_text->toPlainText().toStdString());
  // Numeric setting string needs to be reconstructed or it will break the state.
  if (m_numeric_setting && m_numeric_setting->IsSimpleValue())
    m_original_expression = m_numeric_setting->GetExpressionFromValue();
  else
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
  // In this case, we don't want input to be redirected to the parent input
  // (e.g. LCTRL to combined CTRL)
  const auto expression = MappingCommon::DetectExpression(
      m_detect_button, g_controller_interface, {m_devq.ToString()}, m_devq,
      MappingCommon::ExpressionType::QuoteOffAndRedirectToParentInputOff);

  if (expression.isEmpty())
    return;

  const auto list = m_option_list->findItems(expression, Qt::MatchFixedString);

  // Try to select the first. If this fails, the last selected item would still appear as such
  if (!list.empty())
    m_option_list->setCurrentItem(list[0]);
}

void IOWindow::OnTestSelectedButtonPressed()
{
  std::lock_guard lock(m_selected_device_mutex);
  if (m_option_list->currentRow() < 0 || GetSelectedDevice() == nullptr)
    return;

  MappingCommon::TestOutput(
      m_test_selected_button, g_controller_interface, GetSelectedDevice().get(),
      m_option_list->item(m_option_list->currentRow(), 0)->text().toStdString());
}

void IOWindow::OnTestResultsButtonPressed()
{
  MappingCommon::TestOutput(m_test_results_button, static_cast<OutputReference*>(m_reference));
}

void IOWindow::OnUIRangeChanged(int value)
{
  // Lock just to be sure, range is atomic
  const auto lock = ControllerEmu::EmulatedController::GetStateLock();
  m_reference->range = static_cast<ControlState>(value) / RANGE_PERCENTAGE;
  OnRangeChanged();
}

void IOWindow::OnRangeChanged()
{
  // Rounding is needed or it loses accuracy
  const int qt_value = int(std::round(m_reference->range * RANGE_PERCENTAGE));
  const int slider_range = std::max(m_range_slider->maximum(), std::abs(qt_value));
  // Increase the range of the slider if necessary, we don't want any limts
  m_range_slider->setRange(-slider_range, slider_range);
  if (slider_range > 1000)  // Hide ticks above 1000 or it becomes too expensive to render
    m_range_slider->setTickPosition(QSlider::TickPosition::NoTicks);
  const QSignalBlocker blocker_range_spinbox(m_range_spinbox);
  const QSignalBlocker blocker_range_slider(m_range_slider);
  m_range_spinbox->setValue(qt_value);
  m_range_slider->setValue(qt_value);
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

// Note that if the emulation is running while we are editing the expression,
// it will use whatever expression we have in the editor, not the previous one
void IOWindow::UpdateExpression(std::string new_expression, UpdateMode mode)
{
  const auto lock = ControllerEmu::EmulatedController::GetStateLock();
  // Always force if it's a numeric setting or an empty expression wouldn't be simplified to 0
  if (mode != UpdateMode::Force && new_expression == m_reference->GetExpression() &&
      !m_numeric_setting)
    return;

  auto error = m_reference->SetExpression(std::move(new_expression));
  // If this is a numeric setting, we want to simplify the expression straight away,
  // otherwise this widget would try to read the value from the expression.
  // The only behavioural different from a user point of view should be that
  // inputs named as a single digit number, e.g. (0, 1, ...) that aren't wrapped
  // around `` will be treated as simple value and not as key, as in input expressions.
  if (m_numeric_setting)
  {
    m_numeric_setting->SimplifyIfPossible();
    if (m_numeric_setting->IsSimpleValue())
    {
      // Here we are actually separating the m_expression_text value from the actual internal
      // expression value. This is because if you are about to write a complex expression that
      // starts with a simple one, we don't want to prevent you from doing so.
      error.reset();
    }
  }
  const auto status = m_reference->GetParseStatus();
  m_controller->UpdateSingleControlReference(g_controller_interface, m_reference);

  // This is the only place where we need to update the user variables. keep the first 4 items.
  while (m_variables_combo->count() > 4)
  {
    m_variables_combo->removeItem(m_variables_combo->count() - 1);
  }
  for (const auto& expression : m_controller->GetExpressionVariables())
  {
    m_variables_combo->addItem(QString::fromStdString(expression.first));
  }

  QString state_appended_string;
  if (error)
  {
    // There might be a valid value here but we don't paint it to make it clear there is an error
    m_parse_text->SetShouldPaintStateIndicator(false);
    m_parse_text->setText(QString::fromStdString(*error));
  }
  else if (status == ciface::ExpressionParser::ParseStatus::EmptyExpression)
  {
    // This will always paint 0
    m_parse_text->SetShouldPaintStateIndicator(m_reference->IsInput());
    m_parse_text->setText(QString());
  }
  else if (status != ciface::ExpressionParser::ParseStatus::Successful)
  {
    m_parse_text->SetShouldPaintStateIndicator(false);
    m_parse_text->setText(tr("Invalid Expression."));
  }
  else
  {
    m_parse_text->SetShouldPaintStateIndicator(m_reference->IsInput());
    m_parse_text->setText(QString());

    // Show the focus requirements of the expression if they are not the default/expected ones.
    // Thiese might be ignored depending on your background input settings.
    if (m_reference->HasExpression())
    {
      const Device::FocusFlags focus_flags = m_reference->GetFocusFlags();
      Device::FocusFlags default_focus_flags;
      switch (m_type)
      {
      case IOWindow::Type::InputSetting:
        default_focus_flags = Device::FocusFlags::IgnoreFocus;
        break;
      case IOWindow::Type::Output:
        default_focus_flags = Device::FocusFlags::OutputDefault;
        break;
      case IOWindow::Type::Input:
      default:
        default_focus_flags = Device::FocusFlags::Default;
        break;
      }

      if (focus_flags != default_focus_flags)
      {
        if (m_type == Type::InputSetting)
        {
          state_appended_string = tr("Focus Settings Ignored");
        }
        // Flags in order of priority (show the ones that will actually win over)
        else if (u8(focus_flags) & u8(Device::FocusFlags::IgnoreFocus))
        {
          state_appended_string = tr("Ignores Focus");
        }
        else
        {
          if (u8(focus_flags) & u8(Device::FocusFlags::RequireFullFocus))
          {
            state_appended_string = tr("Requires Full Focus");
          }
          if ((u8(focus_flags) & u8(Device::FocusFlags::IgnoreOnFocusChanged)) &&
              m_type == Type::Input)  // Only applies to inputs
          {
            if (!state_appended_string.isEmpty())
              state_appended_string.append(QStringLiteral(", "));
            state_appended_string.append(tr("Ignored On Focus Change"));
          }
        }
      }
    }
  }

  m_focus_label->setText(state_appended_string);
}

InputStateDelegate::InputStateDelegate(IOWindow* parent, int column,
                                       std::function<ControlState(int row)> state_evaluator)
    : QItemDelegate(parent), m_state_evaluator(std::move(state_evaluator)), m_column(column)
{
}

InputStateLineEdit::InputStateLineEdit(std::function<ControlState()> state_evaluator,
                                       ControlState min, ControlState max)
    : m_state_evaluator(std::move(state_evaluator)), m_min(min), m_max(max)
{
}

static void PaintStateIndicator(QPainter& painter, const QRect& region, ControlState state,
                                ControlState min = 0.0, ControlState max = 1.0)
{
  QString state_string = QString::number(state, 'g', 4);

  QRect meter_region = region;
  if (std::isnan(state))  // Avoids drawing backwards (possibly over controls names)
    state = 0.0;
  meter_region.setWidth(region.width() * ((std::clamp(state, min, max) - min) / (max - min)));

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
  PaintStateIndicator(painter, this->rect(), m_state_evaluator(), m_min, m_max);
}
