/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
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

#ifndef RANGECONTROLS_H
#define RANGECONTROLS_H

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

#include <QtWidgets/private/qtwidgetsglobal_p.h>
#include <QtWidgets/qaccessiblewidget.h>

QT_BEGIN_NAMESPACE

#ifndef QT_NO_ACCESSIBILITY

class QAbstractSpinBox;
class QAbstractSlider;
class QScrollBar;
class QSlider;
class QSpinBox;
class QDoubleSpinBox;
class QDial;
class QAccessibleLineEdit;

#if QT_CONFIG(spinbox)
class QAccessibleAbstractSpinBox:
        public QAccessibleWidget,
        public QAccessibleValueInterface,
        public QAccessibleTextInterface,
        public QAccessibleEditableTextInterface
{
public:
    explicit QAccessibleAbstractSpinBox(QWidget *w);
    virtual ~QAccessibleAbstractSpinBox();

    QString text(QAccessible::Text t) const override;
    void *interface_cast(QAccessible::InterfaceType t) override;

    // QAccessibleValueInterface
    QVariant currentValue() const override;
    void setCurrentValue(const QVariant &value) override;
    QVariant maximumValue() const override;
    QVariant minimumValue() const override;
    QVariant minimumStepSize() const override;

    // QAccessibleTextInterface
    void addSelection(int startOffset, int endOffset) override;
    QString attributes(int offset, int *startOffset, int *endOffset) const override;
    int cursorPosition() const override;
    QRect characterRect(int offset) const override;
    int selectionCount() const override;
    int offsetAtPoint(const QPoint &point) const override;
    void selection(int selectionIndex, int *startOffset, int *endOffset) const override;
    QString text(int startOffset, int endOffset) const override;
    QString textBeforeOffset (int offset, QAccessible::TextBoundaryType boundaryType,
            int *endOffset, int *startOffset) const override;
    QString textAfterOffset(int offset, QAccessible::TextBoundaryType boundaryType,
            int *startOffset, int *endOffset) const override;
    QString textAtOffset(int offset, QAccessible::TextBoundaryType boundaryType,
            int *startOffset, int *endOffset) const override;
    void removeSelection(int selectionIndex) override;
    void setCursorPosition(int position) override;
    void setSelection(int selectionIndex, int startOffset, int endOffset) override;
    int characterCount() const override;
    void scrollToSubstring(int startIndex, int endIndex) override;

    // QAccessibleEditableTextInterface
    void deleteText(int startOffset, int endOffset) override;
    void insertText(int offset, const QString &text) override;
    void replaceText(int startOffset, int endOffset, const QString &text) override;

protected:
    QAbstractSpinBox *abstractSpinBox() const;
    QAccessibleInterface *lineEditIface() const;
private:
    mutable QAccessibleLineEdit *lineEdit;
};

class QAccessibleSpinBox : public QAccessibleAbstractSpinBox
{
public:
    explicit QAccessibleSpinBox(QWidget *w);

protected:
    QSpinBox *spinBox() const;
};

class QAccessibleDoubleSpinBox : public QAccessibleAbstractSpinBox
{
public:
    explicit QAccessibleDoubleSpinBox(QWidget *widget);

    QString text(QAccessible::Text t) const override;

    using QAccessibleAbstractSpinBox::text;
protected:
    QDoubleSpinBox *doubleSpinBox() const;
};
#endif // QT_CONFIG(spinbox)

#if QT_CONFIG(slider)
class QAccessibleAbstractSlider: public QAccessibleWidget, public QAccessibleValueInterface
{
public:
    explicit QAccessibleAbstractSlider(QWidget *w, QAccessible::Role r = QAccessible::Slider);
    void *interface_cast(QAccessible::InterfaceType t) override;

    // QAccessibleValueInterface
    QVariant currentValue() const override;
    void setCurrentValue(const QVariant &value) override;
    QVariant maximumValue() const override;
    QVariant minimumValue() const override;
    QVariant minimumStepSize() const override;

protected:
    QAbstractSlider *abstractSlider() const;
};
#endif // QT_CONFIG(slider)

#if QT_CONFIG(scrollbar)
class QAccessibleScrollBar : public QAccessibleAbstractSlider
{
public:
    explicit QAccessibleScrollBar(QWidget *w);
    QString text(QAccessible::Text t) const override;

protected:
    QScrollBar *scrollBar() const;
};
#endif // QT_CONFIG(scrollbar)

#if QT_CONFIG(slider)
class QAccessibleSlider : public QAccessibleAbstractSlider
{
public:
    explicit QAccessibleSlider(QWidget *w);
    QString text(QAccessible::Text t) const override;

protected:
    QSlider *slider() const;
};
#endif // QT_CONFIG(slider)

#if QT_CONFIG(dial)
class QAccessibleDial : public QAccessibleAbstractSlider
{
public:
    explicit QAccessibleDial(QWidget *w);

    QString text(QAccessible::Text textType) const override;

protected:
    QDial *dial() const;
};
#endif // QT_CONFIG(dial)

#endif // QT_NO_ACCESSIBILITY

QT_END_NAMESPACE

#endif // RANGECONTROLS_H
