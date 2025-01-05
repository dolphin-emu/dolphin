#pragma once
// This file must be encoded in UTF-8 with signatures on Windows

#include <regex>
#include <stdarg.h>
#include <string>
#include <unordered_map>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"

class SlippiPremadeText
{
public:
  enum
  {
    SPT_CHAT_P1 = 0x1,
    SPT_CHAT_P2 = 0x2,
    SPT_CHAT_P3 = 0x3,
    SPT_CHAT_P4 = 0x4,
    SPT_LOGOUT = 0x5,
    SPT_CHAT_DISABLED = 0x6,

    CHAT_MSG_U_PAD_LEFT = 0x81,
    CHAT_MSG_U_PAD_RIGHT = 0x82,
    CHAT_MSG_U_PAD_DOWN = 0x84,
    CHAT_MSG_U_PAD_UP = 0x88,

    CHAT_MSG_L_PAD_LEFT = 0x11,
    CHAT_MSG_L_PAD_RIGHT = 0x12,
    CHAT_MSG_L_PAD_DOWN = 0x14,
    CHAT_MSG_L_PAD_UP = 0x18,

    CHAT_MSG_R_PAD_LEFT = 0x21,
    CHAT_MSG_R_PAD_RIGHT = 0x22,
    CHAT_MSG_R_PAD_DOWN = 0x24,
    CHAT_MSG_R_PAD_UP = 0x28,

    CHAT_MSG_D_PAD_LEFT = 0x41,
    CHAT_MSG_D_PAD_RIGHT = 0x42,
    CHAT_MSG_D_PAD_DOWN = 0x44,
    CHAT_MSG_D_PAD_UP = 0x48,

    CHAT_MSG_CHAT_DISABLED = 0x10,
  };

  const std::unordered_map<u8, std::string> premadeTextsParams = {

      {CHAT_MSG_U_PAD_UP, "ggs"},
      {CHAT_MSG_U_PAD_LEFT, "one more"},
      {CHAT_MSG_U_PAD_RIGHT, "brb"},
      {CHAT_MSG_U_PAD_DOWN, "good luck"},

      {CHAT_MSG_L_PAD_UP, "well played"},
      {CHAT_MSG_L_PAD_LEFT, "that was fun"},
      {CHAT_MSG_L_PAD_RIGHT, "thanks"},
      {CHAT_MSG_L_PAD_DOWN, "too good"},

      {CHAT_MSG_R_PAD_UP, "sorry"},
      {CHAT_MSG_R_PAD_LEFT, "my b"},
      {CHAT_MSG_R_PAD_RIGHT, "lol"},
      {CHAT_MSG_R_PAD_DOWN, "wow"},

      {CHAT_MSG_D_PAD_UP, "gotta go"},
      {CHAT_MSG_D_PAD_LEFT, "one sec"},
      {CHAT_MSG_D_PAD_RIGHT, "lets play again later"},
      {CHAT_MSG_D_PAD_DOWN, "bad connection"},

      {CHAT_MSG_CHAT_DISABLED, "player has chat disabled"},
  };

  const std::unordered_map<u8, std::string> premadeTexts = {
      {SPT_CHAT_P1, "<LEFT><KERN><COLOR, 229, 76, 76>%s:<S><COLOR, 255, 255, 255>%s<END>"},
      {SPT_CHAT_P2, "<LEFT><KERN><COLOR, 59, 189, 255>%s:<S><COLOR, 255, 255, 255>%s<END>"},
      {SPT_CHAT_P3, "<LEFT><KERN><COLOR, 255, 203, 4>%s:<S><COLOR, 255, 255, 255>%s<END>"},
      {SPT_CHAT_P4, "<LEFT><KERN><COLOR, 0, 178, 2>%s:<S><COLOR, 255, 255, 255>%s<END>"},
      {SPT_LOGOUT, "<FIT><COLOR, 243, 75, 75>Are<S>You<COLOR, 0, 175, 75><S>Sure?<END>"},
      {SPT_CHAT_DISABLED,
       "<LEFT><KERN><COLOR, 0, 178, 2>%s<S><COLOR, 255, 255, 255>has<S>chat<S>disabled<S><END>"},
  };

  // This is just a map of delimiters and temporary replacements to remap them before the name is
  // converted to Slippi Premade Text format
  const std::unordered_map<std::string, std::string> unsupportedStringMap = {
      {"<", "\\"}, {">", "`"}, {",", ""},  // DELETE U+007F
  };

  // TODO: use va_list to handle any no. or args
  std::string GetPremadeTextString(u8 textId) { return premadeTexts.at(textId); }

  std::vector<u8> GetPremadeTextData(u8 textId, ...)
  {
    std::string format = GetPremadeTextString(textId);
    char str[400];
    va_list args;
    va_start(args, textId);
    vsprintf(str, format.c_str(), args);
    va_end(args);
    //		INFO_LOG_FMT(SLIPPI, "%s", str);

    std::vector<u8> data = {};
    std::vector<u8> empty = {};

    std::vector<std::string> matches = std::vector<std::string>();

    // NOTE: This code is converted from HSDRaw C# code
    // Fuck Regex, current cpp version does not support positive lookaheads to match this pattern
    // "((?<=<).+?(?=>))|((?<=>*)([^>]+?)(?=<) Good ol' fashioned nested loop :)
    auto splitted = split(str, ">");
    for (int i = 0; i < splitted.size(); i++)
    {
      auto splitted2 = split(splitted[i], "<");
      for (int j = 0; j < splitted2.size(); j++)
      {
        if (splitted2[j].length() > 0)
          matches.push_back(splitted2[j]);
      }
    }

    std::string match;
    for (int m = 0; m < matches.size(); m++)
    {
      match = matches[m];

      auto splittedMatches = split(match, ",");
      if (splittedMatches.size() == 0)
        continue;
      std::string firstMatch = splittedMatches[0];
      auto utfMatch = UTF8ToUTF32(firstMatch);

      std::pair<TEXT_OP_CODE, std::pair<std::string, std::string>> key = findCodeKey(firstMatch);
      if (key.first != TEXT_OP_CODE::CUSTOM_NULL)
      {
        if (splittedMatches.size() - 1 != strlen(key.second.second.c_str()))
          return empty;

        data.push_back((u8)key.first);

        std::string res;
        std::string res2;
        for (int j = 0; j < strlen(key.second.second.c_str()); j++)
        {
          switch (key.second.second.c_str()[j])
          {
          case 'b':
            res = splittedMatches[j + 1];
            trim(res);
            if (static_cast<u8>(atoi(res.c_str())))
              data.push_back(static_cast<u8>(atoi(res.c_str())));
            else
              data.push_back(0);
            break;
          case 's':
            res2 = splittedMatches[j + 1];
            trim(res2);
            u16 sht = static_cast<u16>(atoi(res2.c_str()));
            if (sht)
            {
              data.push_back(static_cast<u8>(sht >> 8));
              data.push_back(static_cast<u8>(sht & 0xFF));
            }
            else
            {
              data.push_back(0);
              data.push_back(0);
            }
            break;
          }
        }
      }
      else
      {
        // process std::string otherwise

        if (splittedMatches.size() >= 2 && firstMatch == "CHR")
        {
          std::string res3 = splittedMatches[1];
          trim(res3);
          u16 ch = static_cast<u16>(atoi(res3.c_str()));
          if (ch)
          {
            u16 sht = (static_cast<u16>(TEXT_OP_CODE::SPECIAL_CHARACTER) << 8) | ch;
            u8 r = static_cast<u8>(sht >> 8);
            u8 r2 = static_cast<u8>(sht & 0xFF);
            data.push_back(r);
            data.push_back(r2);
          }
        }
        else
        {
          for (unsigned long c = 0; c < utfMatch.length(); c++)
          {
            int chr = utfMatch[c];

            // We are manually replacing "<" for "\" and ">" for "`" because I don't want to handle
            // vargs and we need to prevent "format injection" lol...
            for (auto it = unsupportedStringMap.begin(); it != unsupportedStringMap.end(); it++)
            {
              if (it->second.find(chr) != std::string::npos || (chr == U'Ç' && it->first[0] == ','))
              {  // Need to figure out how to find extended ascii chars (Ç)
                chr = it->first[0];
              }
            }

            // Yup, fuck strchr and cpp too, I'm not in the mood to spend 4 more hours researching
            // how to get Japanese characters properly working with a map, so I put everything on an
            // int array in hex
            int pos = -1;
            for (int ccc = 0; ccc < 287; ccc++)
            {
              if (static_cast<int>(CHAR_MAP[ccc]) == chr)
              {
                pos = ccc;
                break;
              }
            }

            if (pos >= 0)
            {
              u16 sht = static_cast<u16>((TEXT_OP_CODE::COMMON_CHARACTER << 8) | pos);
              u8 r = static_cast<u8>(sht >> 8);
              u8 r2 = static_cast<u8>(sht & 0xFF);
              // INFO_LOG_FMT(SLIPPI, "%x %x %x %c", sht, r, r2, chr);

              data.push_back(r);
              data.push_back(r2);
            }
            // otherwise ignore
          }
        }
      }
    }
    data.push_back(0x00);  // Always add end, just in case
    return data;
  }

private:
  enum TEXT_OP_CODE
  {
    END = 0x00,
    RESET = 0x01,
    UNKNOWN_02 = 0x02,
    LINE_BREAK = 0x03,
    UNKNOWN_04 = 0x04,
    UNKNOWN_05 = 0x05,
    UNKNOWN_06 = 0x06,
    OFFSET = 0x07,
    UNKNOWN_08 = 0x08,
    UNKNOWN_09 = 0x09,
    SCALING = 0x0A,
    RESET_SCALING = 0x0B,
    COLOR = 0x0C,
    CLEAR_COLOR = 0x0D,
    SET_TEXTBOX = 0x0E,
    RESET_TEXTBOX = 0x0F,
    CENTERED = 0x10,
    RESET_CENTERED = 0x11,
    LEFT_ALIGNED = 0x12,
    RESET_LEFT_ALIGN = 0x13,
    RIGHT_ALIGNED = 0x14,
    RESET_RIGHT_ALIGN = 0x15,
    KERNING = 0x16,
    NO_KERNING = 0x17,
    FITTING = 0x18,
    NO_FITTING = 0x19,
    SPACE = 0x1A,
    COMMON_CHARACTER = 0x20,
    SPECIAL_CHARACTER = 0x40,
    CUSTOM_NULL = 0x99,
  };

  std::vector<std::tuple<TEXT_OP_CODE, std::vector<u16>>> OPCODES;
  const std::unordered_map<TEXT_OP_CODE, std::pair<std::string, std::string>> CODES = {
      {TEXT_OP_CODE::CENTERED, std::pair<std::string, std::string>("CENTER", "")},
      {TEXT_OP_CODE::RESET_CENTERED, std::pair<std::string, std::string>("/CENTER", "")},
      {TEXT_OP_CODE::CLEAR_COLOR, std::pair<std::string, std::string>("/COLOR", "")},
      {TEXT_OP_CODE::COLOR, std::pair<std::string, std::string>("COLOR", "bbb")},
      {TEXT_OP_CODE::END, std::pair<std::string, std::string>("END", "")},
      {TEXT_OP_CODE::FITTING, std::pair<std::string, std::string>("FIT", "")},
      {TEXT_OP_CODE::KERNING, std::pair<std::string, std::string>("KERN", "")},
      {TEXT_OP_CODE::LEFT_ALIGNED, std::pair<std::string, std::string>("LEFT", "")},
      {TEXT_OP_CODE::LINE_BREAK, std::pair<std::string, std::string>("BR", "")},
      {TEXT_OP_CODE::NO_FITTING, std::pair<std::string, std::string>("/FIT", "")},
      {TEXT_OP_CODE::NO_KERNING, std::pair<std::string, std::string>("/KERN", "")},
      {TEXT_OP_CODE::OFFSET, std::pair<std::string, std::string>("OFFSET", "ss")},
      {TEXT_OP_CODE::RESET, std::pair<std::string, std::string>("RESET", "")},
      {TEXT_OP_CODE::RESET_LEFT_ALIGN, std::pair<std::string, std::string>("/LEFT", "")},
      {TEXT_OP_CODE::RESET_RIGHT_ALIGN, std::pair<std::string, std::string>("/RIGHT", "")},
      {TEXT_OP_CODE::RESET_SCALING, std::pair<std::string, std::string>("/SCALE", "")},
      {TEXT_OP_CODE::RESET_TEXTBOX, std::pair<std::string, std::string>("/TEXTBOX", "")},
      {TEXT_OP_CODE::RIGHT_ALIGNED, std::pair<std::string, std::string>("/RIGHT", "")},
      {TEXT_OP_CODE::SCALING, std::pair<std::string, std::string>("SCALE", "bbbb")},
      {TEXT_OP_CODE::SET_TEXTBOX, std::pair<std::string, std::string>("TEXTBOX", "ss")},
      {TEXT_OP_CODE::UNKNOWN_02, std::pair<std::string, std::string>("UNK02", "")},
      {TEXT_OP_CODE::UNKNOWN_04, std::pair<std::string, std::string>("UNK04", "")},
      {TEXT_OP_CODE::UNKNOWN_05, std::pair<std::string, std::string>("UNK05", "s")},
      {TEXT_OP_CODE::UNKNOWN_06, std::pair<std::string, std::string>("UNK06", "ss")},
      {TEXT_OP_CODE::UNKNOWN_08, std::pair<std::string, std::string>("UNK08", "")},
      {TEXT_OP_CODE::UNKNOWN_09, std::pair<std::string, std::string>("UNK09", "")},
      {TEXT_OP_CODE::SPACE, std::pair<std::string, std::string>("S", "")},
  };

  std::pair<TEXT_OP_CODE, std::pair<std::string, std::string>> findCodeKey(std::string p)
  {
    for (auto it = CODES.begin(); it != CODES.end(); it++)
    {
      if (it->second.first == p)
      {
        return *it;
      }
    }
    return std::pair<TEXT_OP_CODE, std::pair<std::string, std::string>>(
        TEXT_OP_CODE::CUSTOM_NULL, std::pair<std::string, std::string>("", ""));
  }

  std::vector<std::tuple<TEXT_OP_CODE, std::vector<u16>>> DeserializeCodes(std::vector<u8> data)
  {
    std::vector<std::tuple<TEXT_OP_CODE, std::vector<u16>>> d =
        std::vector<std::tuple<TEXT_OP_CODE, std::vector<u16>>>();

    for (int i = 0; i < data.size();)
    {
      auto opcode = (TEXT_OP_CODE)data[i++];
      std::vector<u16> param = std::vector<u16>(0);

      u8 textCode = static_cast<u8>(opcode);

      if ((textCode >> 4) == 2 || (textCode >> 4) == 4)
        param = std::vector<u16>{static_cast<u16>(((textCode << 8) | (data[i++] & 0xFF)) & 0xFFF)};
      else if (!CODES.count(opcode))
      {
        ERROR_LOG_FMT(SLIPPI, "Opcode Not Supported!");
      }
      else
      {
        std::pair<std::string, std::string> code = CODES.at(opcode);
        auto p = code.second.c_str();
        param = std::vector<u16>(strlen(p));
        for (int j = 0; j < param.size(); j++)
        {
          switch (p[j])
          {
          case 'b':
            param[j] = static_cast<u16>(data[i++] & 0xFF);
            break;
          case 's':
            param[j] = static_cast<u16>(((data[i++] & 0xFF) << 8) | (data[i++] & 0xFF));
            break;
          }
        }
      }

      std::pair<TEXT_OP_CODE, std::vector<u16>> c =
          std::pair<TEXT_OP_CODE, std::vector<u16>>(opcode, param);
      d.push_back(c);

      if (opcode == TEXT_OP_CODE::END)
        break;
    }

    return d;
  }

  // https://stackoverflow.com/questions/216823/whats-the-best-way-to-trim-stdstring
  // trim from start (in place)
  static inline void ltrim(std::string& s)
  {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) { return !std::isspace(ch); }));
  }

  // trim from end (in place)
  static inline void rtrim(std::string& s)
  {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) { return !std::isspace(ch); }).base(),
            s.end());
  }

  // trim from both ends (in place)
  static inline void trim(std::string& s)
  {
    ltrim(s);
    rtrim(s);
  }

  std::vector<std::string> split(const std::string& str, const std::string& delim)
  {
    std::vector<std::string> tokens;
    size_t prev = 0, pos = 0;
    do
    {
      pos = str.find(delim, prev);
      if (pos == std::string::npos)
        pos = str.length();
      std::string token = str.substr(prev, pos - prev);
      if (!token.empty())
        tokens.push_back(token);
      prev = pos + delim.length();
    } while (pos < str.length() && prev < str.length());
    return tokens;
  }

  // region CharMAPS
  const int CHAR_MAP[287] = {
      U'0',  U'1',  U'2',  U'3',  U'4',  U'5',  U'6',  U'7',  U'8',  U'9',  U'A',  U'B',  U'C',
      U'D',  U'E',  U'F',  U'G',  U'H',  U'I',  U'J',  U'K',  U'L',  U'M',  U'N',  U'O',  U'P',
      U'Q',  U'R',  U'S',  U'T',  U'U',  U'V',  U'W',  U'X',  U'Y',  U'Z',  U'a',  U'b',  U'c',
      U'd',  U'e',  U'f',  U'g',  U'h',  U'i',  U'j',  U'k',  U'l',  U'm',  U'n',  U'o',  U'p',
      U'q',  U'r',  U's',  U't',  U'u',  U'v',  U'w',  U'x',  U'y',  U'z',  U'ぁ', U'あ', U'ぃ',
      U'い', U'ぅ', U'う', U'ぇ', U'え', U'ぉ', U'お', U'か', U'が', U'き', U'ぎ', U'く', U'ぐ',
      U'け', U'げ', U'こ', U'ご', U'さ', U'ざ', U'し', U'じ', U'す', U'ず', U'せ', U'ぜ', U'そ',
      U'ぞ', U'た', U'だ', U'ち', U'ぢ', U'っ', U'つ', U'づ', U'て', U'で', U'と', U'ど', U'な',
      U'に', U'ぬ', U'ね', U'の', U'は', U'ば', U'ぱ', U'ひ', U'び', U'ぴ', U'ふ', U'ぶ', U'ぷ',
      U'へ', U'べ', U'ぺ', U'ほ', U'ぼ', U'ぽ', U'ま', U'み', U'む', U'め', U'も', U'ゃ', U'や',
      U'ゅ', U'ゆ', U'ょ', U'よ', U'ら', U'り', U'る', U'れ', U'ろ', U'ゎ', U'わ', U'を', U'ん',
      U'ァ', U'ア', U'ィ', U'イ', U'ゥ', U'ウ', U'ェ', U'エ', U'ォ', U'オ', U'カ', U'ガ', U'キ',
      U'ギ', U'ク', U'グ', U'ケ', U'ゲ', U'コ', U'ゴ', U'サ', U'ザ', U'シ', U'ジ', U'ス', U'ズ',
      U'セ', U'ゼ', U'ソ', U'ゾ', U'タ', U'ダ', U'チ', U'ヂ', U'ッ', U'ツ', U'ヅ', U'テ', U'デ',
      U'ト', U'ド', U'ナ', U'ニ', U'ヌ', U'ネ', U'ノ', U'ハ', U'バ', U'パ', U'ヒ', U'ビ', U'ピ',
      U'フ', U'ブ', U'プ', U'ヘ', U'ベ', U'ペ', U'ホ', U'ボ', U'ポ', U'マ', U'ミ', U'ム', U'メ',
      U'モ', U'ャ', U'ヤ', U'ュ', U'ユ', U'ョ', U'ヨ', U'ラ', U'リ', U'ル', U'レ', U'ロ', U'ヮ',
      U'ワ', U'ヲ', U'ン', U'ヴ', U'ヵ', U'ヶ', U'　', U'、', U'。', U',',  U'.',  U'•',  U':',
      U';',  U'?',  U'!',  U'^',  U'_',  U'—',  U'/',  U'~',  U'|',  U'\'', U'"',  U'(',  U')',
      U'[',  U']',  U'{',  U'}',  U'+',  '-',   U'×',  U'=',  U'<',  U'>',  U'¥',  U'$',  U'%',
      U'#',  U'&',  U'*',  U'@',  U'扱', U'押', U'軍', U'源', U'個', U'込', U'指', U'示', U'取',
      U'書', U'詳', U'人', U'生', U'説', U'体', U'団', U'電', U'読', U'発', U'抜', U'閑', U'本',
      U'明'};
};
