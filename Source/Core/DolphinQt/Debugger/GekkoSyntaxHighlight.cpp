#include "DolphinQt/Debugger/GekkoSyntaxHighlight.h"

#include "Common/Assembler/GekkoParser.h"

namespace
{
using namespace Common::GekkoAssembler;
using namespace Common::GekkoAssembler::detail;

class HighlightParsePlugin : public ParsePlugin
{
public:
  virtual ~HighlightParsePlugin() = default;

  std::vector<std::pair<int, int>>&& MoveParens() { return std::move(m_matched_parens); }
  std::vector<std::tuple<int, int, HighlightFormat>>&& MoveFormatting()
  {
    return std::move(m_formatting);
  }

  void OnDirectivePre(GekkoDirective) override { HighlightCurToken(HighlightFormat::Directive); }

  void OnInstructionPre(const ParseInfo&, bool) override
  {
    HighlightCurToken(HighlightFormat::Mnemonic);
  }

  void OnTerminal(Terminal type, const AssemblerToken& val) override
  {
    switch (type)
    {
    case Terminal::Id:
      HighlightCurToken(HighlightFormat::Symbol);
      break;

    case Terminal::Hex:
    case Terminal::Dec:
    case Terminal::Oct:
    case Terminal::Bin:
    case Terminal::Flt:
      HighlightCurToken(HighlightFormat::Immediate);
      break;

    case Terminal::GPR:
      HighlightCurToken(HighlightFormat::GPR);
      break;

    case Terminal::FPR:
      HighlightCurToken(HighlightFormat::GPR);
      break;

    case Terminal::SPR:
      HighlightCurToken(HighlightFormat::SPR);
      break;

    case Terminal::CRField:
      HighlightCurToken(HighlightFormat::CRField);
      break;

    case Terminal::Lt:
    case Terminal::Gt:
    case Terminal::Eq:
    case Terminal::So:
      HighlightCurToken(HighlightFormat::CRFlag);
      break;

    case Terminal::Str:
      HighlightCurToken(HighlightFormat::Str);
      break;

    default:
      break;
    }
  }

  void OnHiaddr(std::string_view) override
  {
    HighlightCurToken(HighlightFormat::Symbol);
    auto&& [ha_pos, ha_tok] = m_owner->lexer.LookaheadTagRef(2);
    m_formatting.emplace_back(static_cast<int>(ha_pos.col),
                              static_cast<int>(ha_tok.token_val.length()), HighlightFormat::HaLa);
  }

  void OnLoaddr(std::string_view id) override { OnHiaddr(id); }

  void OnOpenParen(ParenType type) override
  {
    m_paren_stack.push_back(static_cast<int>(m_owner->lexer.ColNumber()));
  }

  void OnCloseParen(ParenType type) override
  {
    if (m_paren_stack.empty())
    {
      return;
    }

    m_matched_parens.emplace_back(m_paren_stack.back(),
                                  static_cast<int>(m_owner->lexer.ColNumber()));
    m_paren_stack.pop_back();
  }

  void OnError() override
  {
    m_formatting.emplace_back(static_cast<int>(m_owner->error->col),
                              static_cast<int>(m_owner->error->len), HighlightFormat::Error);
  }

  void OnLabelDecl(std::string_view name) override
  {
    const int len = static_cast<int>(m_owner->lexer.LookaheadRef().token_val.length());
    const int off = static_cast<int>(m_owner->lexer.ColNumber());
    m_formatting.emplace_back(len, off, HighlightFormat::Symbol);
  }

  void OnVarDecl(std::string_view name) override { OnLabelDecl(name); }

private:
  std::vector<int> m_paren_stack;
  std::vector<std::pair<int, int>> m_matched_parens;
  std::vector<std::tuple<int, int, HighlightFormat>> m_formatting;

  void HighlightCurToken(HighlightFormat format)
  {
    const int len = static_cast<int>(m_owner->lexer.LookaheadRef().token_val.length());
    const int off = static_cast<int>(m_owner->lexer.ColNumber());
    m_formatting.emplace_back(off, len, format);
  }
};
}  // namespace

void GekkoSyntaxHighlight::highlightBlock(const QString& text)
{
  BlockInfo* info = static_cast<BlockInfo*>(currentBlockUserData());
  if (info == nullptr)
  {
    info = new BlockInfo;
    setCurrentBlockUserData(info);
  }

  qsizetype comment_idx = text.indexOf(QLatin1Char('#'));
  if (comment_idx != -1)
  {
    HighlightSubstr(comment_idx, text.length() - comment_idx, HighlightFormat::Comment);
  }

  if (m_mode == 0)
  {
    HighlightParsePlugin plugin;
    ParseWithPlugin(&plugin, text.toStdString());

    info->block_format = plugin.MoveFormatting();
    info->parens = plugin.MoveParens();
    info->error = std::move(plugin.Error());
    info->error_at_eol = info->error && info->error->len == 0;
  }
  else if (m_mode == 1)
  {
    auto paren_it = std::find_if(info->parens.begin(), info->parens.end(),
                                 [this](const std::pair<int, int>& p) {
                                   return p.first == m_cursor_loc || p.second == m_cursor_loc;
                                 });
    if (paren_it != info->parens.end())
    {
      HighlightSubstr(paren_it->first, 1, HighlightFormat::Paren);
      HighlightSubstr(paren_it->second, 1, HighlightFormat::Paren);
    }
  }

  for (auto&& [off, len, format] : info->block_format)
  {
    HighlightSubstr(off, len, format);
  }
}

void GekkoSyntaxHighlight::HighlightSubstr(int start, int len, HighlightFormat format)
{
  QTextCharFormat hl_format;
  const QColor NORMAL_COLOR = QColor(0x3c, 0x38, 0x36);     // Gruvbox fg
  const QColor DIRECTIVE_COLOR = QColor(0x9d, 0x00, 0x06);  // Gruvbox darkred
  const QColor MNEMONIC_COLOR = QColor(0x79, 0x74, 0x0e);   // Gruvbox darkgreen
  const QColor IMM_COLOR = QColor(0xb5, 0x76, 0x14);        // Gruvbox darkyellow
  const QColor BUILTIN_COLOR = QColor(0x07, 0x66, 0x78);    // Gruvbox darkblue
  const QColor HA_LA_COLOR = QColor(0xaf, 0x3a, 0x03);      // Gruvbox darkorange
  const QColor HOVER_BG_COLOR = QColor(0xfb, 0xf1, 0xc7);   // Gruvbox bg0
  const QColor STRING_COLOR = QColor(0x98, 0x97, 0x1a);     // Gruvbox green
  const QColor COMMENT_COLOR = QColor(0x68, 0x9d, 0x6a);    // Gruvbox aqua

  switch (format)
  {
  case HighlightFormat::Directive:
    hl_format.setForeground(DIRECTIVE_COLOR);
    break;
  case HighlightFormat::Mnemonic:
    hl_format.setForeground(MNEMONIC_COLOR);
    break;
  case HighlightFormat::Symbol:
    hl_format.setForeground(NORMAL_COLOR);
    break;
  case HighlightFormat::Immediate:
    hl_format.setForeground(IMM_COLOR);
    break;
  case HighlightFormat::GPR:
    hl_format.setForeground(BUILTIN_COLOR);
    break;
  case HighlightFormat::FPR:
    hl_format.setForeground(BUILTIN_COLOR);
    break;
  case HighlightFormat::SPR:
    hl_format.setForeground(BUILTIN_COLOR);
    break;
  case HighlightFormat::CRField:
    hl_format.setForeground(BUILTIN_COLOR);
    break;
  case HighlightFormat::CRFlag:
    hl_format.setForeground(BUILTIN_COLOR);
    break;
  case HighlightFormat::Str:
    hl_format.setForeground(STRING_COLOR);
    break;
  case HighlightFormat::HaLa:
    hl_format.setForeground(HA_LA_COLOR);
    break;
  case HighlightFormat::Paren:
    hl_format.setForeground(NORMAL_COLOR);
    hl_format.setBackground(HOVER_BG_COLOR);
    break;
  case HighlightFormat::Default:
    hl_format.clearForeground();
    hl_format.clearBackground();
    break;
  case HighlightFormat::Comment:
    hl_format.setForeground(COMMENT_COLOR);
    break;
  case HighlightFormat::Error:
    hl_format.setForeground(NORMAL_COLOR);
    hl_format.setUnderlineColor(Qt::red);
    hl_format.setUnderlineStyle(QTextCharFormat::WaveUnderline);
    break;
  }

  setFormat(start, len, hl_format);
}
