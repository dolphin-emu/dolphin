/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#ifndef QQUICKTEXTCONTROL_P_H
#define QQUICKTEXTCONTROL_P_H

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

#include <QtGui/qtextdocument.h>
#include <QtGui/qtextoption.h>
#include <QtGui/qtextcursor.h>
#include <QtGui/qtextformat.h>
#include <QtCore/qrect.h>
#include <QtGui/qabstracttextdocumentlayout.h>
#include <QtGui/qtextdocumentfragment.h>
#include <QtGui/qclipboard.h>
#include <QtGui/private/qinputcontrol_p.h>
#include <QtCore/qmimedata.h>

QT_BEGIN_NAMESPACE


class QStyleSheet;
class QTextDocument;
class QQuickTextControlPrivate;
class QAbstractScrollArea;
class QEvent;
class QTimerEvent;
class QTransform;

class Q_AUTOTEST_EXPORT QQuickTextControl : public QInputControl
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QQuickTextControl)
public:
    explicit QQuickTextControl(QTextDocument *doc, QObject *parent = nullptr);
    virtual ~QQuickTextControl();

    QTextDocument *document() const;

    void setTextCursor(const QTextCursor &cursor);
    QTextCursor textCursor() const;

    void setTextInteractionFlags(Qt::TextInteractionFlags flags);
    Qt::TextInteractionFlags textInteractionFlags() const;

    QString toPlainText() const;

#if QT_CONFIG(texthtmlparser)
    QString toHtml() const;
#endif
#if QT_CONFIG(textmarkdownwriter)
    QString toMarkdown() const;
#endif

    bool hasImState() const;
    bool overwriteMode() const;
    void setOverwriteMode(bool overwrite);
    bool cursorVisible() const;
    void setCursorVisible(bool visible);
    QRectF anchorRect() const;
    QRectF cursorRect(const QTextCursor &cursor) const;
    QRectF cursorRect() const;
    QRectF selectionRect(const QTextCursor &cursor) const;
    QRectF selectionRect() const;

    QString hoveredLink() const;
    QString anchorAt(const QPointF &pos) const;
    QTextBlock blockWithMarkerAt(const QPointF &pos) const;

    void setCursorWidth(int width);

    void setAcceptRichText(bool accept);

    void moveCursor(QTextCursor::MoveOperation op, QTextCursor::MoveMode mode = QTextCursor::MoveAnchor);

    bool canPaste() const;

    void setCursorIsFocusIndicator(bool b);
    void setWordSelectionEnabled(bool enabled);

    void updateCursorRectangle(bool force);

    virtual int hitTest(const QPointF &point, Qt::HitTestAccuracy accuracy) const;
    virtual QRectF blockBoundingRect(const QTextBlock &block) const;

    QString preeditText() const;

public Q_SLOTS:
    void setPlainText(const QString &text);
    void setMarkdownText(const QString &text);
    void setHtml(const QString &text);

#if QT_CONFIG(clipboard)
    void cut();
    void copy();
    void paste(QClipboard::Mode mode = QClipboard::Clipboard);
#endif

    void undo();
    void redo();
    void clear();

    void selectAll();

Q_SIGNALS:
    void textChanged();
    void preeditTextChanged();
    void contentsChange(int from, int charsRemoved, int charsAdded);
    void undoAvailable(bool b);
    void redoAvailable(bool b);
    void currentCharFormatChanged(const QTextCharFormat &format);
    void copyAvailable(bool b);
    void selectionChanged();
    void cursorPositionChanged();
    void overwriteModeChanged(bool overwriteMode);

    // control signals
    void updateCursorRequest();
    void updateRequest();
    void cursorRectangleChanged();
    void linkActivated(const QString &link);
    void linkHovered(const QString &link);
    void markerClicked();
    void markerHovered(bool marker);

public:
    virtual void processEvent(QEvent *e, const QTransform &transform);
    void processEvent(QEvent *e, const QPointF &coordinateOffset = QPointF());

#if QT_CONFIG(im)
    virtual QVariant inputMethodQuery(Qt::InputMethodQuery property) const;
    Q_INVOKABLE QVariant inputMethodQuery(Qt::InputMethodQuery query, const QVariant &argument) const;
#endif

    virtual QMimeData *createMimeDataFromSelection() const;
    virtual bool canInsertFromMimeData(const QMimeData *source) const;
    virtual void insertFromMimeData(const QMimeData *source);

    bool cursorOn() const;

protected:
    void timerEvent(QTimerEvent *e) override;

    bool event(QEvent *e) override;

private:
    Q_DISABLE_COPY(QQuickTextControl)
    Q_PRIVATE_SLOT(d_func(), void _q_updateCurrentCharFormatAndSelection())
    Q_PRIVATE_SLOT(d_func(), void _q_updateCursorPosChanged(const QTextCursor &))
};


// also used by QLabel
class QQuickTextEditMimeData : public QMimeData
{
public:
    inline QQuickTextEditMimeData(const QTextDocumentFragment &aFragment) : fragment(aFragment) {}

    QStringList formats() const override;

protected:
    QVariant retrieveData(const QString &mimeType, QVariant::Type type) const override;

private:
    void setup() const;

    mutable QTextDocumentFragment fragment;
};

QT_END_NAMESPACE

#endif // QQuickTextControl_H
