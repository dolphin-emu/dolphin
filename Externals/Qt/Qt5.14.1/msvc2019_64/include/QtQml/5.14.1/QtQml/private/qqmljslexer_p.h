/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QQMLJSLEXER_P_H
#define QQMLJSLEXER_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <private/qqmljsglobal_p.h>
#include <private/qqmljsgrammar_p.h>

#include <QtCore/qstring.h>
#include <QtCore/qstack.h>

QT_BEGIN_NAMESPACE

namespace QQmlJS {

class Engine;
struct DiagnosticMessage;
class Directives;

class QML_PARSER_EXPORT Lexer: public QQmlJSGrammar
{
public:
    enum {
        T_ABSTRACT = T_RESERVED_WORD,
        T_BOOLEAN = T_RESERVED_WORD,
        T_BYTE = T_RESERVED_WORD,
        T_CHAR = T_RESERVED_WORD,
        T_DOUBLE = T_RESERVED_WORD,
        T_FINAL = T_RESERVED_WORD,
        T_FLOAT = T_RESERVED_WORD,
        T_GOTO = T_RESERVED_WORD,
        T_IMPLEMENTS = T_RESERVED_WORD,
        T_INT = T_RESERVED_WORD,
        T_INTERFACE = T_RESERVED_WORD,
        T_LONG = T_RESERVED_WORD,
        T_NATIVE = T_RESERVED_WORD,
        T_PACKAGE = T_RESERVED_WORD,
        T_PRIVATE = T_RESERVED_WORD,
        T_PROTECTED = T_RESERVED_WORD,
        T_SHORT = T_RESERVED_WORD,
        T_SYNCHRONIZED = T_RESERVED_WORD,
        T_THROWS = T_RESERVED_WORD,
        T_TRANSIENT = T_RESERVED_WORD,
        T_VOLATILE = T_RESERVED_WORD
    };

    enum Error {
        NoError,
        IllegalCharacter,
        IllegalNumber,
        UnclosedStringLiteral,
        IllegalEscapeSequence,
        IllegalUnicodeEscapeSequence,
        UnclosedComment,
        IllegalExponentIndicator,
        IllegalIdentifier,
        IllegalHexadecimalEscapeSequence
    };

    enum RegExpBodyPrefix {
        NoPrefix,
        EqualPrefix
    };

    enum RegExpFlag {
        RegExp_Global     = 0x01,
        RegExp_IgnoreCase = 0x02,
        RegExp_Multiline  = 0x04,
        RegExp_Unicode    = 0x08,
        RegExp_Sticky     = 0x10
    };

    enum ParseModeFlags {
        QmlMode = 0x1,
        YieldIsKeyword = 0x2,
        StaticIsKeyword = 0x4
    };

    enum class ImportState {
        SawImport,
        NoQmlImport
    };

public:
    Lexer(Engine *engine);

    int parseModeFlags() const {
        int flags = 0;
        if (qmlMode())
            flags |= QmlMode|StaticIsKeyword;
        if (yieldIsKeyWord())
            flags |= YieldIsKeyword;
        if (_staticIsKeyword)
            flags |= StaticIsKeyword;
        return flags;
    }

    bool qmlMode() const;
    bool yieldIsKeyWord() const { return _generatorLevel != 0; }
    void setStaticIsKeyword(bool b) { _staticIsKeyword = b; }

    QString code() const;
    void setCode(const QString &code, int lineno, bool qmlMode = true);

    int lex();

    bool scanRegExp(RegExpBodyPrefix prefix = NoPrefix);
    bool scanDirectives(Directives *directives, DiagnosticMessage *error);

    int regExpFlags() const { return _patternFlags; }
    QString regExpPattern() const { return _tokenText; }

    int tokenKind() const { return _tokenKind; }
    int tokenOffset() const { return _tokenStartPtr - _code.unicode(); }
    int tokenLength() const { return _tokenLength; }

    int tokenStartLine() const { return _tokenLine; }
    int tokenStartColumn() const { return _tokenColumn; }

    inline QStringRef tokenSpell() const { return _tokenSpell; }
    inline QStringRef rawString() const { return _rawString; }
    double tokenValue() const { return _tokenValue; }
    QString tokenText() const;

    Error errorCode() const;
    QString errorMessage() const;

    bool prevTerminator() const;
    bool followsClosingBrace() const;
    bool canInsertAutomaticSemicolon(int token) const;

    enum ParenthesesState {
        IgnoreParentheses,
        CountParentheses,
        BalancedParentheses
    };

    void enterGeneratorBody() { ++_generatorLevel; }
    void leaveGeneratorBody() { --_generatorLevel; }

protected:
    static int classify(const QChar *s, int n, int parseModeFlags);

private:
    inline void scanChar();
    int scanToken();
    int scanNumber(QChar ch);
    int scanVersionNumber(QChar ch);
    enum ScanStringMode {
        SingleQuote = '\'',
        DoubleQuote = '"',
        TemplateHead = '`',
        TemplateContinuation = 0
    };
    int scanString(ScanStringMode mode);

    bool isLineTerminator() const;
    unsigned isLineTerminatorSequence() const;
    static bool isIdentLetter(QChar c);
    static bool isDecimalDigit(ushort c);
    static bool isHexDigit(QChar c);
    static bool isOctalDigit(ushort c);

    void syncProhibitAutomaticSemicolon();
    uint decodeUnicodeEscapeCharacter(bool *ok);
    QChar decodeHexEscapeCharacter(bool *ok);

private:
    Engine *_engine;

    QString _code;
    QString _tokenText;
    QString _errorMessage;
    QStringRef _tokenSpell;
    QStringRef _rawString;

    const QChar *_codePtr;
    const QChar *_endPtr;
    const QChar *_tokenStartPtr;

    QChar _char;
    Error _errorCode;

    int _currentLineNumber;
    int _currentColumnNumber;
    double _tokenValue;

    // parentheses state
    ParenthesesState _parenthesesState;
    int _parenthesesCount;

    // template string stack
    QStack<int> _outerTemplateBraceCount;
    int _bracesCount = -1;

    int _stackToken;

    int _patternFlags;
    int _tokenKind;
    int _tokenLength;
    int _tokenLine;
    int _tokenColumn;
    ImportState _importState = ImportState::NoQmlImport;

    bool _validTokenText;
    bool _prohibitAutomaticSemicolon;
    bool _restrictedKeyword;
    bool _terminator;
    bool _followsClosingBrace;
    bool _delimited;
    bool _qmlMode;
    bool _skipLinefeed = false;
    int _generatorLevel = 0;
    bool _staticIsKeyword = false;
    bool _handlingDirectives = false;
};

} // end of namespace QQmlJS

QT_END_NAMESPACE

#endif // LEXER_H
