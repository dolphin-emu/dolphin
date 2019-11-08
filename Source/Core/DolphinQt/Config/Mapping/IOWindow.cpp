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

#include "Core/Core.h"

#include "DolphinQt/Config/Mapping/MappingCommon.h"
#include "DolphinQt/Config/Mapping/MappingIndicator.h"
#include "DolphinQt/Config/Mapping/MappingWidget.h"
#include "DolphinQt/Config/Mapping/MappingWindow.h"
#include "DolphinQt/QtUtils/BlockUserInputFilter.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"

#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControlReference/ExpressionParser.h"
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

ControlExpressionSyntaxHighlighter::ControlExpressionSyntaxHighlighter(QTextDocument* parent,
                                                                       QLineEdit* result)
    : QSyntaxHighlighter(parent), m_result_text(result)
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
  if (ciface::ExpressionParser::ParseStatus::Successful != tokenize_status)
  {
    m_result_text->setText(tr("Invalid Token."));
  }
  else
  {
    ciface::ExpressionParser::RemoveInertTokens(&tokens);
    const auto parse_status = ciface::ExpressionParser::ParseTokens(tokens);

    m_result_text->setText(
        QString::fromStdString(parse_status.description.value_or(_trans("Success."))));

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
  explicit InputStateDelegate(IOWindow* parent);

  void paint(QPainter* painter, const QStyleOptionViewItem& option,
             const QModelIndex& index) const override;

private:
  IOWindow* m_parent;
};

IOWindow::IOWindow(MappingWidget* parent, ControllerEmu::EmulatedController* controller,
                   ControlReference* ref, IOWindow::Type type)
    : QDialog(parent), m_reference(ref), m_controller(controller), m_type(type)
{
  CreateMainLayout();

  connect(parent, &MappingWidget::Update, this, &IOWindow::Update);

  setWindowTitle(type == IOWindow::Type::Input ? tr("Configure Input") : tr("Configure Output"));
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
  m_apply_button = new QPushButton(tr("Apply"));
  m_range_slider = new QSlider(Qt::Horizontal);
  m_range_spinbox = new QSpinBox();

  m_parse_text = new QLineEdit();
  m_parse_text->setReadOnly(true);

  m_expression_text = new QPlainTextEdit();
  m_expression_text->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  new ControlExpressionSyntaxHighlighter(m_expression_text->document(), m_parse_text);

  m_operators_combo = new QComboBox();
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
  if (m_type == Type::Input)
  {
    m_operators_combo->addItem(tr(", Comma"));
  }

  m_functions_combo = new QComboBox(this);
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

  // Devices
  m_main_layout->addWidget(m_devices_combo);

  // Range
  auto* range_hbox = new QHBoxLayout();
  range_hbox->addWidget(new QLabel(tr("Range")));
  range_hbox->addWidget(m_range_slider);
  range_hbox->addWidget(m_range_spinbox);
  m_range_slider->setMinimum(-500);
  m_range_slider->setMaximum(500);
  m_range_spinbox->setMinimum(-500);
  m_range_spinbox->setMaximum(500);
  m_main_layout->addLayout(range_hbox);

  // Options (Buttons, Outputs) and action buttons

  if (m_type == Type::Input)
  {
    m_option_list->setColumnCount(2);
    m_option_list->setColumnWidth(1, 64);
    m_option_list->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);

    m_option_list->setItemDelegate(new InputStateDelegate(this));
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

  button_vbox->addWidget(m_operators_combo);

  if (m_type == Type::Input)
    button_vbox->addWidget(m_functions_combo);
  else
    m_functions_combo->hide();

  m_main_layout->addLayout(hbox, 2);
  m_main_layout->addWidget(m_expression_text, 1);
  m_main_layout->addWidget(m_parse_text);

  // Button Box
  m_main_layout->addWidget(m_button_box);
  m_button_box->addButton(m_clear_button, QDialogButtonBox::ActionRole);
  m_button_box->addButton(m_apply_button, QDialogButtonBox::ActionRole);
  m_button_box->addButton(QDialogButtonBox::Ok);

  setLayout(m_main_layout);
}

void IOWindow::ConfigChanged()
{
  const QSignalBlocker blocker(this);

  m_expression_text->setPlainText(QString::fromStdString(m_reference->GetExpression()));
  m_expression_text->moveCursor(QTextCursor::End, QTextCursor::MoveAnchor);
  m_range_spinbox->setValue(m_reference->range * SLIDER_TICK_COUNT);
  m_range_slider->setValue(m_reference->range * SLIDER_TICK_COUNT);

  m_devq = m_controller->GetDefaultDevice();

  UpdateDeviceList();
  UpdateOptionList();
}

void IOWindow::Update()
{
  m_option_list->viewport()->update();
}

void IOWindow::ConnectWidgets()
{
  connect(m_select_button, &QPushButton::clicked, [this] { AppendSelectedOption(); });

  connect(m_detect_button, &QPushButton::clicked, this, &IOWindow::OnDetectButtonPressed);
  connect(m_test_button, &QPushButton::clicked, this, &IOWindow::OnTestButtonPressed);

  connect(m_button_box, &QDialogButtonBox::clicked, this, &IOWindow::OnDialogButtonPressed);
  connect(m_devices_combo, &QComboBox::currentTextChanged, this, &IOWindow::OnDeviceChanged);
  connect(m_range_spinbox, static_cast<void (QSpinBox::*)(int value)>(&QSpinBox::valueChanged),
          this, &IOWindow::OnRangeChanged);
  connect(m_range_slider, static_cast<void (QSlider::*)(int value)>(&QSlider::valueChanged), this,
          &IOWindow::OnRangeChanged);

  connect(m_expression_text, &QPlainTextEdit::textChanged, [this] {
    m_apply_button->setText(m_apply_button->text().remove(QStringLiteral("*")));
    m_apply_button->setText(m_apply_button->text() + QStringLiteral("*"));
  });

  connect(m_operators_combo, QOverload<int>::of(&QComboBox::activated), [this](int index) {
    if (0 == index)
      return;

    m_expression_text->insertPlainText(m_operators_combo->currentText().left(1));

    m_operators_combo->setCurrentIndex(0);
  });

  connect(m_functions_combo, QOverload<int>::of(&QComboBox::activated), [this](int index) {
    if (0 == index)
      return;

    m_expression_text->insertPlainText(m_functions_combo->currentText() + QStringLiteral("()"));

    m_functions_combo->setCurrentIndex(0);
  });
}

void IOWindow::AppendSelectedOption()
{
  if (m_option_list->currentItem() == nullptr)
    return;

  m_expression_text->insertPlainText(MappingCommon::GetExpressionForControl(
      m_option_list->currentItem()->text(), m_devq, m_controller->GetDefaultDevice()));
}

void IOWindow::OnDeviceChanged(const QString& device)
{
  m_devq.FromString(device.toStdString());
  UpdateOptionList();
}

void IOWindow::OnDialogButtonPressed(QAbstractButton* button)
{
  if (button == m_clear_button)
  {
    m_expression_text->clear();
    return;
  }

  m_reference->SetExpression(m_expression_text->toPlainText().toStdString());
  m_controller->UpdateSingleControlReference(g_controller_interface, m_reference);

  m_apply_button->setText(m_apply_button->text().remove(QStringLiteral("*")));

  if (ciface::ExpressionParser::ParseStatus::SyntaxError == m_reference->GetParseStatus())
  {
    ModalMessageBox::warning(this, tr("Error"), tr("The expression contains a syntax error."));
  }

  if (button != m_apply_button)
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
  m_reference->range = static_cast<double>(value) / SLIDER_TICK_COUNT;
  m_range_spinbox->setValue(m_reference->range * SLIDER_TICK_COUNT);
  m_range_slider->setValue(m_reference->range * SLIDER_TICK_COUNT);
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

void IOWindow::UpdateDeviceList()
{
  m_devices_combo->clear();

  for (const auto& name : g_controller_interface.GetAllDeviceStrings())
    m_devices_combo->addItem(QString::fromStdString(name));

  m_devices_combo->setCurrentText(
      QString::fromStdString(m_controller->GetDefaultDevice().ToString()));
}

InputStateDelegate::InputStateDelegate(IOWindow* parent) : QItemDelegate(parent), m_parent(parent)
{
}

void InputStateDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                               const QModelIndex& index) const
{
  QItemDelegate::paint(painter, option, index);

  // Don't do anything special for the first column.
  if (index.column() == 0)
    return;

  // Clamp off negative values but allow greater than one in the text display.
  const auto state =
      std::max(m_parent->GetSelectedDevice()->Inputs()[index.row()]->GetState(), 0.0);
  const auto state_str = QString::number(state, 'g', 4);

  QRect rect = option.rect;
  rect.setWidth(rect.width() * std::clamp(state, 0.0, 1.0));

  // Create a temporary indicator object to retreive color constants.
  MappingIndicator indicator(nullptr);

  painter->save();

  // Normal text.
  painter->setPen(indicator.GetTextColor());
  painter->drawText(option.rect, Qt::AlignCenter, state_str);

  // Input state meter.
  painter->fillRect(rect, indicator.GetAdjustedInputColor());

  // Text on top of meter.
  painter->setPen(indicator.GetAltTextColor());
  painter->setClipping(true);
  painter->setClipRect(rect);
  painter->drawText(option.rect, Qt::AlignCenter, state_str);

  painter->restore();
}
