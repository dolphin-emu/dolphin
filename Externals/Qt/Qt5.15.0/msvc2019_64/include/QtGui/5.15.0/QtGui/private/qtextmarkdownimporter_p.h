/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtGui module of the Qt Toolkit.
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

#ifndef QTEXTMARKDOWNIMPORTER_H
#define QTEXTMARKDOWNIMPORTER_H

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

#include <QtGui/qfont.h>
#include <QtGui/qtguiglobal.h>
#include <QtGui/qpalette.h>
#include <QtGui/qtextdocument.h>
#include <QtGui/qtextlist.h>
#include <QtCore/qpointer.h>
#include <QtCore/qstack.h>

QT_BEGIN_NAMESPACE

class QTextCursor;
class QTextDocument;
class QTextTable;

class Q_GUI_EXPORT QTextMarkdownImporter
{
public:
    enum Feature {
        FeatureCollapseWhitespace =       0x0001,
        FeaturePermissiveATXHeaders =     0x0002,
        FeaturePermissiveURLAutoLinks =   0x0004,
        FeaturePermissiveMailAutoLinks =  0x0008,
        FeatureNoIndentedCodeBlocks =     0x0010,
        FeatureNoHTMLBlocks =             0x0020,
        FeatureNoHTMLSpans =              0x0040,
        FeatureTables =                   0x0100,
        FeatureStrikeThrough =            0x0200,
        FeaturePermissiveWWWAutoLinks =   0x0400,
        FeatureTasklists =                0x0800,
        // composite flags
        FeaturePermissiveAutoLinks = FeaturePermissiveMailAutoLinks
            | FeaturePermissiveURLAutoLinks | FeaturePermissiveWWWAutoLinks,
        FeatureNoHTML = QTextDocument::MarkdownNoHTML,
        DialectCommonMark = QTextDocument::MarkdownDialectCommonMark,
        DialectGitHub = QTextDocument::MarkdownDialectGitHub
    };
    Q_DECLARE_FLAGS(Features, Feature)

    QTextMarkdownImporter(Features features);
    QTextMarkdownImporter(QTextDocument::MarkdownFeatures features);

    void import(QTextDocument *doc, const QString &markdown);

public:
    // MD4C callbacks
    int cbEnterBlock(int blockType, void* detail);
    int cbLeaveBlock(int blockType, void* detail);
    int cbEnterSpan(int spanType, void* detail);
    int cbLeaveSpan(int spanType, void* detail);
    int cbText(int textType, const char* text, unsigned size);

private:
    void insertBlock();

private:
    QTextDocument *m_doc = nullptr;
    QTextCursor *m_cursor = nullptr;
    QTextTable *m_currentTable = nullptr; // because m_cursor->currentTable() doesn't work
#if QT_CONFIG(regularexpression)
    QString m_htmlAccumulator;
#endif
    QString m_blockCodeLanguage;
    QVector<int> m_nonEmptyTableCells; // in the current row
    QStack<QPointer<QTextList>> m_listStack;
    QStack<QTextCharFormat> m_spanFormatStack;
    QFont m_monoFont;
    QPalette m_palette;
#if QT_CONFIG(regularexpression)
    int m_htmlTagDepth = 0;
#endif
    int m_blockQuoteDepth = 0;
    int m_tableColumnCount = 0;
    int m_tableRowCount = 0;
    int m_tableCol = -1; // because relative cell movements (e.g. m_cursor->movePosition(QTextCursor::NextCell)) don't work
    int m_paragraphMargin = 0;
    int m_blockType = 0;
    char m_blockCodeFence = 0;
    Features m_features;
    QTextImageFormat m_imageFormat;
    QTextListFormat m_listFormat;
    QTextBlockFormat::MarkerType m_markerType = QTextBlockFormat::MarkerType::NoMarker;
    bool m_needsInsertBlock = false;
    bool m_needsInsertList = false;
    bool m_listItem = false; // true from the beginning of LI to the end of the first P
    bool m_codeBlock = false;
    bool m_imageSpan = false;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QTextMarkdownImporter::Features)

QT_END_NAMESPACE

#endif // QTEXTMARKDOWNIMPORTER_H
