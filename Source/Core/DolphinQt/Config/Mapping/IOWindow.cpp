// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/Mapping/IOWindow.h"

#include <thread>

#include <QComboBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QSyntaxHighlighter>
#include <QVBoxLayout>

#include "Core/Core.h"

#include "DolphinQt/Config/Mapping/MappingCommon.h"
#include "DolphinQt/Config/Mapping/MappingWindow.h"
#include "DolphinQt/QtUtils/BlockUserInputFilter.h"

#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControlReference/ExpressionParser.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"

constexpr int SLIDER_TICK_COUNT = 100;

namespace
{
QTextCharFormat GetSpecialCharFormat()
{
  QTextCharFormat format;
  format.setFontWeight(QFont::Weight::Bold);
  return format;
}

QTextCharFormat GetOperatorCharFormat()
{
  QTextCharFormat format;
  format.setFontWeight(QFont::Weight::Bold);
  format.setForeground(QBrush{Qt::darkBlue});
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
  format.setForeground(QBrush{Qt::magenta});
  return format;
}

QTextCharFormat GetFunctionCharFormat()
{
  QTextCharFormat format;
  format.setForeground(QBrush{Qt::darkCyan});
  return format;
}

class SyntaxHighlighter : public QSyntaxHighlighter
{
public:
  SyntaxHighlighter(QTextDocument* parent) : QSyntaxHighlighter(parent) {}

  void highlightBlock(const QString& text) final override
  {
    // TODO: This is going to result in improper highlighting with non-ascii characters:
    ciface::ExpressionParser::Lexer lexer(text.toStdString());

    std::vector<ciface::ExpressionParser::Token> tokens;
    lexer.Tokenize(tokens);

    using ciface::ExpressionParser::TokenType;

    for (auto& token : tokens)
    {
      switch (token.type)
      {
      case TokenType::TOK_INVALID:
        setFormat(token.string_position, token.string_length, GetInvalidCharFormat());
        break;
      case TokenType::TOK_LPAREN:
      case TokenType::TOK_RPAREN:
      case TokenType::TOK_COMMA:
        setFormat(token.string_position, token.string_length, GetSpecialCharFormat());
        break;
      case TokenType::TOK_LITERAL:
        setFormat(token.string_position, token.string_length, GetLiteralCharFormat());
        break;
      case TokenType::TOK_CONTROL:
        setFormat(token.string_position, token.string_length, GetControlCharFormat());
        break;
      case TokenType::TOK_FUNCTION:
        setFormat(token.string_position, token.string_length, GetFunctionCharFormat());
        break;
      case TokenType::TOK_VARIABLE:
        setFormat(token.string_position, token.string_length, GetVariableCharFormat());
        break;
      default:
        if (token.IsBinaryOperator())
          setFormat(token.string_position, token.string_length, GetOperatorCharFormat());
        break;
      }
    }
  }
};

}  // namespace

IOWindow::IOWindow(QWidget* parent, ControllerEmu::EmulatedController* controller,
                   ControlReference* ref, IOWindow::Type type)
    : QDialog(parent), m_reference(ref), m_controller(controller), m_type(type)
{
  CreateMainLayout();

  setWindowTitle(type == IOWindow::Type::Input ? tr("Configure Input") : tr("Configure Output"));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  ConfigChanged();

  ConnectWidgets();
}

void IOWindow::CreateMainLayout()
{
  m_main_layout = new QVBoxLayout();

  m_devices_combo = new QComboBox();
  m_option_list = new QListWidget();
  m_select_button = new QPushButton(tr("Select"));
  m_detect_button = new QPushButton(tr("Detect"));
  m_test_button = new QPushButton(tr("Test"));
  m_button_box = new QDialogButtonBox();
  m_clear_button = new QPushButton(tr("Clear"));
  m_apply_button = new QPushButton(tr("Apply"));
  m_range_slider = new QSlider(Qt::Horizontal);
  m_range_spinbox = new QSpinBox();

  m_expression_text = new QPlainTextEdit();
  m_expression_text->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  new SyntaxHighlighter(m_expression_text->document());

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
  }
  m_operators_combo->addItem(tr("| Or"));
  if (m_type == Type::Input)
  {
    m_operators_combo->addItem(tr(", Comma"));
  }

  m_functions_combo = new QComboBox();
  m_functions_combo->addItem(tr("Functions"));
  m_functions_combo->insertSeparator(1);
  m_functions_combo->addItem(QStringLiteral("!if"));
  m_functions_combo->addItem(QStringLiteral("!sin"));
  m_functions_combo->addItem(QStringLiteral("!timer"));
  m_functions_combo->addItem(QStringLiteral("!toggle"));
  m_functions_combo->addItem(QStringLiteral("!while"));
  m_functions_combo->addItem(QStringLiteral("!deadzone"));
  m_functions_combo->addItem(QStringLiteral("!smooth"));
  m_functions_combo->addItem(QStringLiteral("!hold"));
  m_functions_combo->addItem(QStringLiteral("!tap"));
  m_functions_combo->addItem(QStringLiteral("!relative"));
  m_functions_combo->addItem(QStringLiteral("!pulse"));

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

  auto* hbox = new QHBoxLayout();
  auto* button_vbox = new QVBoxLayout();
  hbox->addWidget(m_option_list, 8);
  hbox->addLayout(button_vbox, 1);

  button_vbox->addWidget(m_select_button);
  button_vbox->addWidget(m_type == Type::Input ? m_detect_button : m_test_button);
  button_vbox->addWidget(m_operators_combo);
  if (m_type == Type::Input)
  {
    button_vbox->addWidget(m_functions_combo);
  }

  m_main_layout->addLayout(hbox, 2);
  m_main_layout->addWidget(m_expression_text, 1);

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
    QMessageBox error(this);
    error.setIcon(QMessageBox::Critical);
    error.setWindowTitle(tr("Error"));
    error.setText(tr("The expression contains a syntax error."));
    error.setWindowModality(Qt::WindowModal);
    error.exec();
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
  m_option_list->clear();

  const auto device = g_controller_interface.FindDevice(m_devq);

  if (device == nullptr)
    return;

  if (m_reference->IsInput())
  {
    for (const auto* input : device->Inputs())
    {
      m_option_list->addItem(QString::fromStdString(input->GetName()));
    }
  }
  else
  {
    for (const auto* output : device->Outputs())
    {
      m_option_list->addItem(QString::fromStdString(output->GetName()));
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
