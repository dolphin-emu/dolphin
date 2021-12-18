#pragma once

#include <regex>
#include <stdarg.h>
#include <string>
#include <unordered_map>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"

using namespace std;

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
  };

  unordered_map<u8, string> premadeTextsParams = {

      {CHAT_MSG_U_PAD_UP, "ggs"},
      {CHAT_MSG_U_PAD_LEFT, "one more"},
      {CHAT_MSG_U_PAD_RIGHT, "brb"},
      {CHAT_MSG_U_PAD_DOWN, "good luck"},

      {CHAT_MSG_L_PAD_UP, "well played"},
      {CHAT_MSG_L_PAD_LEFT, "that was fun"},
      {CHAT_MSG_L_PAD_RIGHT, "thanks"},
      {CHAT_MSG_L_PAD_DOWN, "too good"},

      {CHAT_MSG_R_PAD_UP, "oof"},
      {CHAT_MSG_R_PAD_LEFT, "my b"},
      {CHAT_MSG_R_PAD_RIGHT, "lol"},
      {CHAT_MSG_R_PAD_DOWN, "wow"},

      {CHAT_MSG_D_PAD_UP, "okay"},
      {CHAT_MSG_D_PAD_LEFT, "thinking"},
      {CHAT_MSG_D_PAD_RIGHT, "lets play again later"},
      {CHAT_MSG_D_PAD_DOWN, "bad connection"},
  };

  unordered_map<u8, string> premadeTexts = {
      {SPT_CHAT_P1, "<LEFT><KERN><COLOR, 229, 76, 76>%s-<S><COLOR, 255, 255, 255>%s<END>"},
      {SPT_CHAT_P2, "<LEFT><KERN><COLOR, 59, 189, 255>%s-<S><COLOR, 255, 255, 255>%s<END>"},
      {SPT_CHAT_P3, "<LEFT><KERN><COLOR, 255, 203, 4>%s-<S><COLOR, 255, 255, 255>%s<END>"},
      {SPT_CHAT_P4, "<LEFT><KERN><COLOR, 0, 178, 2>%s-<S><COLOR, 255, 255, 255>%s<END>"},
      {SPT_LOGOUT, "<FIT><COLOR, 243, 75, 75>Are<S>You<COLOR, 0, 175, 75><S>Sure?<END>"},
  };

  // TODO: use va_list to handle any no. or args
  string GetPremadeTextString(u8 textId) { return premadeTexts[textId]; }

  vector<u8> GetPremadeTextData(u8 textId, ...)
  {
    string format = GetPremadeTextString(textId);
    char str[400];
    va_list args;
    va_start(args, textId);
    vsprintf(str, format.c_str(), args);
    va_end(args);
    //		INFO_LOG(SLIPPI, "%s", str);

    vector<u8> data = {};
    vector<u8> empty = {};

    vector<string> matches = vector<string>();

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

    string match;
    for (int m = 0; m < matches.size(); m++)
    {
      match = matches[m];

      auto splittedMatches = split(match, ",");
      if (splittedMatches.size() == 0)
        continue;
      string firstMatch = splittedMatches[0];

      pair<TEXT_OP_CODE, pair<string, string>> key = findCodeKey(firstMatch);
      if (key.first != TEXT_OP_CODE::CUSTOM_NULL)
      {
        if (splittedMatches.size() - 1 != strlen(key.second.second.c_str()))
          return empty;

        data.push_back((u8)key.first);

        string res;
        string res2;
        for (int j = 0; j < strlen(key.second.second.c_str()); j++)
        {
          switch (key.second.second.c_str()[j])
          {
          case 'b':
            res = splittedMatches[j + 1];
            trim(res);
            if ((u8)atoi(res.c_str()))
              data.push_back((u8)atoi(res.c_str()));
            else
              data.push_back(0);
            break;
          case 's':
            res2 = splittedMatches[j + 1];
            trim(res2);
            u16 sht = (u16)atoi(res2.c_str());
            if (sht)
            {
              data.push_back((u8)(sht >> 8));
              data.push_back((u8)(sht & 0xFF));
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
        // process string otherwise

        if (splittedMatches.size() >= 2 && firstMatch == "CHR")
        {
          string res3 = splittedMatches[1];
          trim(res3);
          u16 ch = (u16)atoi(res3.c_str());
          if (ch)
          {
            u16 sht = (u16)(((u16)TEXT_OP_CODE::SPECIAL_CHARACTER << 8) | ch);
            u8 r = (u8)(sht >> 8);
            u8 r2 = (u8)(sht & 0xFF);
            data.push_back(r);
            data.push_back(r2);
          }
        }
        else
        {
          for (int c = 0; c < strlen(firstMatch.c_str()); c++)
          {
            char chr = firstMatch[c];

            // Yup, fuck strchr and cpp too, I'm not in the mood to spend 4 more hours researching
            // how to get Japanese characters properly working with a map, so I put everything on an
            // int array in hex
            int pos = -1;
            for (int ccc = 0; ccc < 287; ccc++)
            {
              if ((char)CHAR_MAP[ccc] == chr)
              {
                pos = ccc;
                break;
              }
            }

            if (pos >= 0)
            {
              u16 sht = (u16)(((u16)TEXT_OP_CODE::COMMON_CHARACTER << 8) | pos);
              u8 r = (u8)(sht >> 8);
              u8 r2 = (u8)(sht & 0xFF);
              // INFO_LOG(SLIPPI, "%x %x %x %c", sht, r, r2, chr);

              data.push_back(r);
              data.push_back(r2);
            }
            else
              return empty;
          }
        }
      }
    }

    //        INFO_LOG(SLIPPI, "DATA:");
    //        for(int i=0;i<data.size();i++){
    //            INFO_LOG(SLIPPI, "%x", data[i]);
    //        }
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

  vector<tuple<TEXT_OP_CODE, vector<u16>>> OPCODES;
  unordered_map<TEXT_OP_CODE, pair<string, string>> CODES = {
      {TEXT_OP_CODE::CENTERED, pair<string, string>("CENTER", "")},
      {TEXT_OP_CODE::RESET_CENTERED, pair<string, string>("/CENTER", "")},
      {TEXT_OP_CODE::CLEAR_COLOR, pair<string, string>("/COLOR", "")},
      {TEXT_OP_CODE::COLOR, pair<string, string>("COLOR", "bbb")},
      {TEXT_OP_CODE::END, pair<string, string>("END", "")},
      {TEXT_OP_CODE::FITTING, pair<string, string>("FIT", "")},
      {TEXT_OP_CODE::KERNING, pair<string, string>("KERN", "")},
      {TEXT_OP_CODE::LEFT_ALIGNED, pair<string, string>("LEFT", "")},
      {TEXT_OP_CODE::LINE_BREAK, pair<string, string>("BR", "")},
      {TEXT_OP_CODE::NO_FITTING, pair<string, string>("/FIT", "")},
      {TEXT_OP_CODE::NO_KERNING, pair<string, string>("/KERN", "")},
      {TEXT_OP_CODE::OFFSET, pair<string, string>("OFFSET", "ss")},
      {TEXT_OP_CODE::RESET, pair<string, string>("RESET", "")},
      {TEXT_OP_CODE::RESET_LEFT_ALIGN, pair<string, string>("/LEFT", "")},
      {TEXT_OP_CODE::RESET_RIGHT_ALIGN, pair<string, string>("/RIGHT", "")},
      {TEXT_OP_CODE::RESET_SCALING, pair<string, string>("/SCALE", "")},
      {TEXT_OP_CODE::RESET_TEXTBOX, pair<string, string>("/TEXTBOX", "")},
      {TEXT_OP_CODE::RIGHT_ALIGNED, pair<string, string>("/RIGHT", "")},
      {TEXT_OP_CODE::SCALING, pair<string, string>("SCALE", "bbbb")},
      {TEXT_OP_CODE::SET_TEXTBOX, pair<string, string>("TEXTBOX", "ss")},
      {TEXT_OP_CODE::UNKNOWN_02, pair<string, string>("UNK02", "")},
      {TEXT_OP_CODE::UNKNOWN_04, pair<string, string>("UNK04", "")},
      {TEXT_OP_CODE::UNKNOWN_05, pair<string, string>("UNK05", "s")},
      {TEXT_OP_CODE::UNKNOWN_06, pair<string, string>("UNK06", "ss")},
      {TEXT_OP_CODE::UNKNOWN_08, pair<string, string>("UNK08", "")},
      {TEXT_OP_CODE::UNKNOWN_09, pair<string, string>("UNK09", "")},
      {TEXT_OP_CODE::SPACE, pair<string, string>("S", "")},
  };

  pair<TEXT_OP_CODE, pair<string, string>> findCodeKey(string p)
  {
    unordered_map<TEXT_OP_CODE, pair<string, string>>::iterator it;

    for (it = CODES.begin(); it != CODES.end(); it++)
    {
      if (it->second.first == p)
      {
        return *it;
      }
    }
    return pair<TEXT_OP_CODE, pair<string, string>>(TEXT_OP_CODE::CUSTOM_NULL,
                                                    pair<string, string>("", ""));
  }

  vector<tuple<TEXT_OP_CODE, vector<u16>>> DeserializeCodes(vector<u8> data)
  {
    vector<tuple<TEXT_OP_CODE, vector<u16>>> d = vector<tuple<TEXT_OP_CODE, vector<u16>>>();

    for (int i = 0; i < data.size();)
    {
      auto opcode = (TEXT_OP_CODE)data[i++];
      vector<u16> param = vector<u16>(0);

      int textCode = (u8)opcode;

      if ((textCode >> 4) == 2)
        param = vector<u16>{(u16)(((textCode << 8) | (data[i++] & 0xFF)) & 0xFFF)};
      else if ((textCode >> 4) == 4)
        param = vector<u16>{(u16)(((textCode << 8) | (data[i++] & 0xFF)) & 0xFFF)};
      else if (!CODES.count(opcode))
      {
        ERROR_LOG_FMT(SLIPPI, "Opcode Not Supported!");
      }
      else
      {
        pair<string, string> code = CODES[opcode];
        auto p = code.second.c_str();
        param = vector<u16>(strlen(p));
        for (int j = 0; j < param.size(); j++)
        {
          switch (p[j])
          {
          case 'b':
            param[j] = (u16)(data[i++] & 0xFF);
            break;
          case 's':
            param[j] = (u16)(((data[i++] & 0xFF) << 8) | (data[i++] & 0xFF));
            break;
          }
        }
      }

      pair<TEXT_OP_CODE, vector<u16>> c = pair<TEXT_OP_CODE, vector<u16>>(opcode, param);
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

  vector<string> split(const string& str, const string& delim)
  {
    vector<string> tokens;
    size_t prev = 0, pos = 0;
    do
    {
      pos = str.find(delim, prev);
      if (pos == string::npos)
        pos = str.length();
      string token = str.substr(prev, pos - prev);
      if (!token.empty())
        tokens.push_back(token);
      prev = pos + delim.length();
    } while (pos < str.length() && prev < str.length());
    return tokens;
  }

  // region CharMAPS
  int CHAR_MAP[287] = {
      '0',
      '1',
      '2',
      '3',
      '4',
      '5',
      '6',
      '7',
      '8',
      '9',
      'A',
      'B',
      'C',
      'D',
      'E',
      'F',
      'G',
      'H',
      'I',
      'J',
      'K',
      'L',
      'M',
      'N',
      'O',
      'P',
      'Q',
      'R',
      'S',
      'T',
      'U',
      'V',
      'W',
      'X',
      'Y',
      'Z',
      'a',
      'b',
      'c',
      'd',
      'e',
      'f',
      'g',
      'h',
      'i',
      'j',
      'k',
      'l',
      'm',
      'n',
      'o',
      'p',
      'q',
      'r',
      's',
      't',
      'u',
      'v',
      'w',
      'x',
      0x0079 /*'y'*/,
      0x007a /*'z'*/,
      0x3041 /*'ぁ'*/,
      0x3042 /*'あ'*/,
      0x3043 /*'ぃ'*/,
      0x3044 /*'い'*/,
      0x3045 /*'ぅ'*/,
      0x3046 /*'う'*/,
      0x3047 /*'ぇ'*/,
      0x3048 /*'え'*/,
      0x3049 /*'ぉ'*/,
      0x304a /*'お'*/,
      0x304b /*'か'*/,
      0x304c /*'が'*/,
      0x304d /*'き'*/,
      0x304e /*'ぎ'*/,
      0x304f /*'く'*/,
      0x3050 /*'ぐ'*/,
      0x3051 /*'け'*/,
      0x3052 /*'げ'*/,
      0x3053 /*'こ'*/,
      0x3054 /*'ご'*/,
      0x3055 /*'さ'*/,
      0x3056 /*'ざ'*/,
      0x3057 /*'し'*/,
      0x3058 /*'じ'*/,
      0x3059 /*'す'*/,
      0x305a /*'ず'*/,
      0x305b /*'せ'*/,
      0x305c /*'ぜ'*/,
      0x305d /*'そ'*/,
      0x305e /*'ぞ'*/,
      0x305f /*'た'*/,
      0x3060 /*'だ'*/,
      0x3061 /*'ち'*/,
      0x3062 /*'ぢ'*/,
      0x3063 /*'っ'*/,
      0x3064 /*'つ'*/,
      0x3065 /*'づ'*/,
      0x3066 /*'て'*/,
      0x3067 /*'で'*/,
      0x3068 /*'と'*/,
      0x3069 /*'ど'*/,
      0x306a /*'な'*/,
      0x306b /*'に'*/,
      0x306c /*'ぬ'*/,
      0x306d /*'ね'*/,
      0x306e /*'の'*/,
      0x306f /*'は'*/,
      0x3070 /*'ば'*/,
      0x3071 /*'ぱ'*/,
      0x3072 /*'ひ'*/,
      0x3073 /*'び'*/,
      0x3074 /*'ぴ'*/,
      0x3075 /*'ふ'*/,
      0x3076 /*'ぶ'*/,
      0x3077 /*'ぷ'*/,
      0x3078 /*'へ'*/,
      0x3079 /*'べ'*/,
      0x307a /*'ぺ'*/,
      0x307b /*'ほ'*/,
      0x307c /*'ぼ'*/,
      0x307d /*'ぽ'*/,
      0x307e /*'ま'*/,
      0x307f /*'み'*/,
      0x3080 /*'む'*/,
      0x3081 /*'め'*/,
      0x3082 /*'も'*/,
      0x3083 /*'ゃ'*/,
      0x3084 /*'や'*/,
      0x3085 /*'ゅ'*/,
      0x3086 /*'ゆ'*/,
      0x3087 /*'ょ'*/,
      0x3088 /*'よ'*/,
      0x3089 /*'ら'*/,
      0x308a /*'り'*/,
      0x308b /*'る'*/,
      0x308c /*'れ'*/,
      0x308d /*'ろ'*/,
      0x308e /*'ゎ'*/,
      0x308f /*'わ'*/,
      0x3092 /*'を'*/,
      0x3093 /*'ん'*/,
      0x30a1 /*'ァ'*/,
      0x30a2 /*'ア'*/,
      0x30a3 /*'ィ'*/,
      0x30a4 /*'イ'*/,
      0x30a5 /*'ゥ'*/,
      0x30a6 /*'ウ'*/,
      0x30a7 /*'ェ'*/,
      0x30a8 /*'エ'*/,
      0x30a9 /*'ォ'*/,
      0x30aa /*'オ'*/,
      0x30ab /*'カ'*/,
      0x30ac /*'ガ'*/,
      0x30ad /*'キ'*/,
      0x30ae /*'ギ'*/,
      0x30af /*'ク'*/,
      0x30b0 /*'グ'*/,
      0x30b1 /*'ケ'*/,
      0x30b2 /*'ゲ'*/,
      0x30b3 /*'コ'*/,
      0x30b4 /*'ゴ'*/,
      0x30b5 /*'サ'*/,
      0x30b6 /*'ザ'*/,
      0x30b7 /*'シ'*/,
      0x30b8 /*'ジ'*/,
      0x30b9 /*'ス'*/,
      0x30ba /*'ズ'*/,
      0x30bb /*'セ'*/,
      0x30bc /*'ゼ'*/,
      0x30bd /*'ソ'*/,
      0x30be /*'ゾ'*/,
      0x30bf /*'タ'*/,
      0x30c0 /*'ダ'*/,
      0x30c1 /*'チ'*/,
      0x30c2 /*'ヂ'*/,
      0x30c3 /*'ッ'*/,
      0x30c4 /*'ツ'*/,
      0x30c5 /*'ヅ'*/,
      0x30c6 /*'テ'*/,
      0x30c7 /*'デ'*/,
      0x30c8 /*'ト'*/,
      0x30c9 /*'ド'*/,
      0x30ca /*'ナ'*/,
      0x30cb /*'ニ'*/,
      0x30cc /*'ヌ'*/,
      0x30cd /*'ネ'*/,
      0x30ce /*'ノ'*/,
      0x30cf /*'ハ'*/,
      0x30d0 /*'バ'*/,
      0x30d1 /*'パ'*/,
      0x30d2 /*'ヒ'*/,
      0x30d3 /*'ビ'*/,
      0x30d4 /*'ピ'*/,
      0x30d5 /*'フ'*/,
      0x30d6 /*'ブ'*/,
      0x30d7 /*'プ'*/,
      0x30d8 /*'ヘ'*/,
      0x30d9 /*'ベ'*/,
      0x30da /*'ペ'*/,
      0x30db /*'ホ'*/,
      0x30dc /*'ボ'*/,
      0x30dd /*'ポ'*/,
      0x30de /*'マ'*/,
      0x30df /*'ミ'*/,
      0x30e0 /*'ム'*/,
      0x30e1 /*'メ'*/,
      0x30e2 /*'モ'*/,
      0x30e3 /*'ャ'*/,
      0x30e4 /*'ヤ'*/,
      0x30e5 /*'ュ'*/,
      0x30e6 /*'ユ'*/,
      0x30e7 /*'ョ'*/,
      0x30e8 /*'ヨ'*/,
      0x30e9 /*'ラ'*/,
      0x30ea /*'リ'*/,
      0x30eb /*'ル'*/,
      0x30ec /*'レ'*/,
      0x30ed /*'ロ'*/,
      0x30ee /*'ヮ'*/,
      0x30ef /*'ワ'*/,
      0x30f2 /*'ヲ'*/,
      0x30f3 /*'ン'*/,
      0x30f4 /*'ヴ'*/,
      0x30f5 /*'ヵ'*/,
      0x30f6 /*'ヶ'*/,
      0x3000 /*'　'*/,
      0x3001 /*'、'*/,
      0x3002 /*'。'*/,
      0x002c /*','*/,
      0x002e /*'.'*/,
      0x2022 /*'•'*/,
      0x002c /*','*/,
      0x003b /*';'*/,
      0x003f /*'?'*/,
      0x0021 /*'!'*/,
      0x005e /*'^'*/,
      0x005f /*'_'*/,
      0x2014 /*'—'*/,
      0x002f /*'/'*/,
      0x007e /*'~'*/,
      0x007c /*'|'*/,
      0x0027 /*'\''*/,
      0x0022 /*'"'*/,
      0x0028 /*'('*/,
      0x0029 /*')'*/,
      0x005b /*'['*/,
      0x005d /*']'*/,
      0x007b /*'{'*/,
      0x007d /*'}'*/,
      0x002b /*'+'*/,
      '-',
      0x00d7 /*'×'*/,
      0x003d /*'='*/,
      0x003c /*'<'*/,
      0x003e /*'>'*/,
      0x00a5 /*'¥'*/,
      0x0024 /*'$'*/,
      0x0025 /*'%'*/,
      0x0023 /*'#'*/,
      0x0026 /*'&'*/,
      0x002a /*'*'*/,
      0x0040 /*'@'*/,
      0x6271 /*'扱'*/,
      0x62bc /*'押'*/,
      0x8ecd /*'軍'*/,
      0x6e90 /*'源'*/,
      0x500b /*'個'*/,
      0x8fbc /*'込'*/,
      0x6307 /*'指'*/,
      0x793a /*'示'*/,
      0x53d6 /*'取'*/,
      0x66f8 /*'書'*/,
      0x8a73 /*'詳'*/,
      0x4eba /*'人'*/,
      0x751f /*'生'*/,
      0x8aac /*'説'*/,
      0x4f53 /*'体'*/,
      0x56e3 /*'団'*/,
      0x96fb /*'電'*/,
      0x8aad /*'読'*/,
      0x767a /*'発'*/,
      0x629c /*'抜'*/,
      0x9591 /*'閑'*/,
      0x672c /*'本'*/,
      0x660e /*'明'*/,
  };
};
