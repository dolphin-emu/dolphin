/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQuick module of the Qt Toolkit.
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

#include <QtCore/qlist.h>
#include <QtCore/qvarlengtharray.h>
#include <QtGui/qcolor.h>
#include <QtGui/qglyphrun.h>
#include <QtGui/qimage.h>
#include <QtGui/qtextdocument.h>
#include <QtGui/qtextlayout.h>
#include "qquickclipnode_p.h"
#include "qquicktextnode_p.h"

#ifndef QQUICKTEXTNODEENGINE_P_H
#define QQUICKTEXTNODEENGINE_P_H

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

QT_BEGIN_NAMESPACE

// Engine that takes glyph runs as input, and produces a set of glyph nodes, clip nodes,
// and rectangle nodes to represent the text, decorations and selection. Will try to minimize
// number of nodes, and join decorations in neighbouring items

class QQuickTextNodeEngine {
public:
    enum Decoration {
        NoDecoration = 0x0,
        Underline    = 0x1,
        Overline     = 0x2,
        StrikeOut    = 0x4,
        Background   = 0x8
    };
    Q_DECLARE_FLAGS(Decorations, Decoration)

    enum SelectionState {
        Unselected,
        Selected
    };

    struct BinaryTreeNode {

        BinaryTreeNode()
            : selectionState(Unselected), clipNode(0), decorations(Decoration::NoDecoration)
            , ascent(0.0), leftChildIndex(-1), rightChildIndex(-1)
        {
        }

        BinaryTreeNode(const QRectF &brect, const QImage &i, SelectionState selState, qreal a)
            : boundingRect(brect), selectionState(selState), clipNode(0), decorations(Decoration::NoDecoration)
            , image(i), ascent(a), leftChildIndex(-1), rightChildIndex(-1)
        {
        }

        BinaryTreeNode(const QGlyphRun &g, SelectionState selState, const QRectF &brect,
                       const Decorations &decs, const QColor &c, const QColor &bc,
                       const QPointF &pos, qreal a);

        QGlyphRun glyphRun;
        QRectF boundingRect;
        SelectionState selectionState;
        QQuickDefaultClipNode *clipNode;
        Decorations decorations;
        QColor color;
        QColor backgroundColor;
        QPointF position;
        QImage image;
        qreal ascent;

        int leftChildIndex;
        int rightChildIndex;

        QList<QPair<int, int> > ranges;

        static void insert(QVarLengthArray<BinaryTreeNode, 16> *binaryTree, const QRectF &rect, const QImage &image, qreal ascent, SelectionState selectionState)
        { insert(binaryTree, BinaryTreeNode(rect, image, selectionState, ascent)); }

        static void insert(QVarLengthArray<BinaryTreeNode, 16> *binaryTree, const QGlyphRun &glyphRun, SelectionState selectionState,
                           Decorations decorations, const QColor &textColor, const QColor &backgroundColor, const QPointF &position);
        static void insert(QVarLengthArray<BinaryTreeNode, 16> *binaryTree, const BinaryTreeNode &binaryTreeNode);
        static void inOrder(const QVarLengthArray<BinaryTreeNode, 16> &binaryTree, QVarLengthArray<int> *sortedIndexes, int currentIndex = 0);
    };

    struct BinaryTreeNodeKey
    {
        BinaryTreeNodeKey(BinaryTreeNode *node);

        bool operator==(const BinaryTreeNodeKey &otherKey) const
        {
            return fontEngine == otherKey.fontEngine
                    && clipNode == otherKey.clipNode
                    && color == otherKey.color
                    && selectionState == otherKey.selectionState;
        }

        QFontEngine *fontEngine;
        QQuickDefaultClipNode *clipNode;
        QRgb color;
        int selectionState;
    };

    QQuickTextNodeEngine()
        : m_currentTextDirection(Qt::LeftToRight)
        , m_hasSelection(false)
        , m_hasContents(false)
    {}

    bool hasContents() const { return m_hasContents; }
    void addTextBlock(QTextDocument *, const QTextBlock &, const QPointF &position, const QColor &textColor, const QColor& anchorColor, int selectionStart, int selectionEnd);
    QTextLine currentLine() const { return m_currentLine; }

    void setCurrentLine(const QTextLine &currentLine)
    {
        if (m_currentLine.isValid())
            processCurrentLine();

        m_currentLine = currentLine;
    }

    void setCurrentTextDirection(Qt::LayoutDirection textDirection)
    {
        m_currentTextDirection = textDirection;
    }

    void addBorder(const QRectF &rect, qreal border, QTextFrameFormat::BorderStyle borderStyle,
                   const QBrush &borderBrush);
    void addFrameDecorations(QTextDocument *document, QTextFrame *frame);
    void addImage(const QRectF &rect, const QImage &image, qreal ascent,
                  SelectionState selectionState,
                  QTextFrameFormat::Position layoutPosition);
    int addText(const QTextBlock &block,
                const QTextCharFormat &charFormat,
                const QColor &textColor,
                const QVarLengthArray<QTextLayout::FormatRange> &colorChanges,
                int textPos, int fragmentEnd,
                int selectionStart, int selectionEnd);
    void addTextObject(const QTextBlock &block, const QPointF &position, const QTextCharFormat &format,
                       SelectionState selectionState,
                       QTextDocument *textDocument, int pos,
                       QTextFrameFormat::Position layoutPosition = QTextFrameFormat::InFlow);
    void addSelectedGlyphs(const QGlyphRun &glyphRun);
    void addUnselectedGlyphs(const QGlyphRun &glyphRun);
    void addGlyphsInRange(int rangeStart, int rangeEnd,
                          const QColor &color, const QColor &backgroundColor,
                          int selectionStart, int selectionEnd);
    void addGlyphsForRanges(const QVarLengthArray<QTextLayout::FormatRange> &ranges,
                            int start, int end,
                            int selectionStart, int selectionEnd);

    void mergeProcessedNodes(QList<BinaryTreeNode *> *regularNodes,
                             QList<BinaryTreeNode *> *imageNodes);
    void addToSceneGraph(QQuickTextNode *parent,
                         QQuickText::TextStyle style = QQuickText::Normal,
                         const QColor &styleColor = QColor());

    void setSelectionColor(const QColor &selectionColor)
    {
        m_selectionColor = selectionColor;
    }

    void setSelectedTextColor(const QColor &selectedTextColor)
    {
        m_selectedTextColor = selectedTextColor;
    }

    void setTextColor(const QColor &textColor)
    {
        m_textColor = textColor;
    }

    void setAnchorColor(const QColor &anchorColor)
    {
        m_anchorColor = anchorColor;
    }

    void setPosition(const QPointF &position)
    {
        m_position = position;
    }




private:
    struct TextDecoration
    {
        TextDecoration() : selectionState(Unselected) {}
        TextDecoration(const SelectionState &s,
                       const QRectF &r,
                       const QColor &c)
            : selectionState(s)
            , rect(r)
            , color(c)
        {
        }

        SelectionState selectionState;
        QRectF rect;
        QColor color;
    };

    void processCurrentLine();
    void addTextDecorations(const QVarLengthArray<TextDecoration> &textDecorations, qreal offset, qreal thickness);
    void mergeFormats(QTextLayout *textLayout, QVarLengthArray<QTextLayout::FormatRange> *mergedFormats);

    QColor m_selectionColor;
    QColor m_textColor;
    QColor m_backgroundColor;
    QColor m_selectedTextColor;
    QColor m_anchorColor;
    QPointF m_position;

    QTextLine m_currentLine;
    Qt::LayoutDirection m_currentTextDirection;

    QList<QPair<QRectF, QColor> > m_backgrounds;
    QList<QRectF> m_selectionRects;
    QVarLengthArray<BinaryTreeNode, 16> m_currentLineTree;

    QList<TextDecoration> m_lines;
    QVector<BinaryTreeNode> m_processedNodes;

    bool m_hasSelection : 1;
    bool m_hasContents : 1;
    friend class QQuickTextNode;

};

QT_END_NAMESPACE

#endif // QQUICKTEXTNODEENGINE_P_H
