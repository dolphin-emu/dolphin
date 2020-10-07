/***************************************************************************
**
** Copyright (C) 2014 BlackBerry Limited. All rights reserved.
** Copyright (C) 2015 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWidgets module of the Qt Toolkit.
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

#ifndef QPIXMAPSTYLE_H
#define QPIXMAPSTYLE_H

#include <QtWidgets/private/qtwidgetsglobal_p.h>
#include <QtWidgets/QCommonStyle>
#include <QtWidgets/QTileRules>

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

class QPixmapStylePrivate;

class Q_WIDGETS_EXPORT QPixmapStyle : public QCommonStyle
{
    Q_OBJECT

public:
    enum ControlDescriptor {
        BG_Background,
        LE_Enabled,             // QLineEdit
        LE_Disabled,
        LE_Focused,
        PB_Enabled,             // QPushButton
        PB_Pressed,
        PB_PressedDisabled,
        PB_Checked,
        PB_Disabled,
        TE_Enabled,             // QTextEdit
        TE_Disabled,
        TE_Focused,
        PB_HBackground,         // Horizontal QProgressBar
        PB_HContent,
        PB_HComplete,
        PB_VBackground,         // Vertical QProgressBar
        PB_VContent,
        PB_VComplete,
        SG_HEnabled,            // Horizontal QSlider groove
        SG_HDisabled,
        SG_HActiveEnabled,
        SG_HActivePressed,
        SG_HActiveDisabled,
        SG_VEnabled,            // Vertical QSlider groove
        SG_VDisabled,
        SG_VActiveEnabled,
        SG_VActivePressed,
        SG_VActiveDisabled,
        DD_ButtonEnabled,       // QComboBox (DropDown)
        DD_ButtonDisabled,
        DD_ButtonPressed,
        DD_PopupDown,
        DD_PopupUp,
        DD_ItemSelected,
        ID_Selected,            // QStyledItemDelegate
        SB_Horizontal,          // QScrollBar
        SB_Vertical
    };

    enum ControlPixmap {
        CB_Enabled,             // QCheckBox
        CB_Checked,
        CB_Pressed,
        CB_PressedChecked,
        CB_Disabled,
        CB_DisabledChecked,
        RB_Enabled,             // QRadioButton
        RB_Checked,
        RB_Pressed,
        RB_Disabled,
        RB_DisabledChecked,
        SH_HEnabled,            // Horizontal QSlider handle
        SH_HDisabled,
        SH_HPressed,
        SH_VEnabled,            // Vertical QSlider handle
        SH_VDisabled,
        SH_VPressed,
        DD_ArrowEnabled,        // QComboBox (DropDown) arrow
        DD_ArrowDisabled,
        DD_ArrowPressed,
        DD_ArrowOpen,
        DD_ItemSeparator,
        ID_Separator            // QStyledItemDelegate separator
    };

public:
    QPixmapStyle();
    ~QPixmapStyle();

    void polish(QApplication *application) override;
    void polish(QPalette &palette) override;
    void polish(QWidget *widget) override;
    void unpolish(QApplication *application) override;
    void unpolish(QWidget *widget) override;

    void drawPrimitive(PrimitiveElement element, const QStyleOption *option,
            QPainter *painter, const QWidget *widget = nullptr) const override;
    void drawControl(ControlElement element, const QStyleOption *option,
            QPainter *painter, const QWidget *widget = nullptr) const override;
    void drawComplexControl(ComplexControl cc, const QStyleOptionComplex *option,
                            QPainter *painter, const QWidget *widget=nullptr) const override;

    QSize sizeFromContents(ContentsType type, const QStyleOption *option,
            const QSize &contentsSize, const QWidget *widget = nullptr) const override;
    QRect subElementRect(SubElement element, const QStyleOption *option,
            const QWidget *widget = nullptr) const override;
    QRect subControlRect(ComplexControl cc, const QStyleOptionComplex *option,
                         SubControl sc, const QWidget *widget = nullptr) const override;

    int pixelMetric(PixelMetric metric, const QStyleOption *option = nullptr,
            const QWidget *widget = nullptr) const override;
    int styleHint(StyleHint hint, const QStyleOption *option,
                  const QWidget *widget, QStyleHintReturn *returnData) const override;
    SubControl hitTestComplexControl(ComplexControl control, const QStyleOptionComplex *option,
                                     const QPoint &pos, const QWidget *widget) const override;

    bool eventFilter(QObject *watched, QEvent *event) override;

    void addDescriptor(ControlDescriptor control, const QString &fileName,
                       QMargins margins = QMargins(),
                       QTileRules tileRules = QTileRules(Qt::RepeatTile, Qt::RepeatTile));
    void copyDescriptor(ControlDescriptor source, ControlDescriptor dest);
    void drawCachedPixmap(ControlDescriptor control, const QRect &rect, QPainter *p) const;

    void addPixmap(ControlPixmap control, const QString &fileName,
                   QMargins margins = QMargins());
    void copyPixmap(ControlPixmap source, ControlPixmap dest);

protected:
    void drawPushButton(const QStyleOption *option,
                        QPainter *painter, const QWidget *widget) const;
    void drawLineEdit(const QStyleOption *option,
                      QPainter *painter, const QWidget *widget) const;
    void drawTextEdit(const QStyleOption *option,
                      QPainter *painter, const QWidget *widget) const;
    void drawCheckBox(const QStyleOption *option,
                      QPainter *painter, const QWidget *widget) const;
    void drawRadioButton(const QStyleOption *option,
                         QPainter *painter, const QWidget *widget) const;
    void drawPanelItemViewItem(const QStyleOption *option,
                               QPainter *painter, const QWidget *widget) const;
    void drawProgressBarBackground(const QStyleOption *option,
                                   QPainter *painter, const QWidget *widget) const;
    void drawProgressBarLabel(const QStyleOption *option,
                              QPainter *painter, const QWidget *widget) const;
    void drawProgressBarFill(const QStyleOption *option,
                             QPainter *painter, const QWidget *widget) const;
    void drawSlider(const QStyleOptionComplex *option,
                    QPainter *painter, const QWidget *widget) const;
    void drawComboBox(const QStyleOptionComplex *option,
                      QPainter *painter, const QWidget *widget) const;
    void drawScrollBar(const QStyleOptionComplex *option,
                       QPainter *painter, const QWidget *widget) const;

    QSize pushButtonSizeFromContents(const QStyleOption *option,
                                     const QSize &contentsSize, const QWidget *widget) const;
    QSize lineEditSizeFromContents(const QStyleOption *option,
                                   const QSize &contentsSize, const QWidget *widget) const;
    QSize progressBarSizeFromContents(const QStyleOption *option,
                                      const QSize &contentsSize, const QWidget *widget) const;
    QSize sliderSizeFromContents(const QStyleOption *option,
                                 const QSize &contentsSize, const QWidget *widget) const;
    QSize comboBoxSizeFromContents(const QStyleOption *option,
                                   const QSize &contentsSize, const QWidget *widget) const;
    QSize itemViewSizeFromContents(const QStyleOption *option,
                                   const QSize &contentsSize, const QWidget *widget) const;

    QRect comboBoxSubControlRect(const QStyleOptionComplex *option, QPixmapStyle::SubControl sc,
                                 const QWidget *widget) const;
    QRect scrollBarSubControlRect(const QStyleOptionComplex *option, QPixmapStyle::SubControl sc,
                                  const QWidget *widget) const;

protected:
    QPixmapStyle(QPixmapStylePrivate &dd);

private:
    Q_DECLARE_PRIVATE(QPixmapStyle)
};

QT_END_NAMESPACE

#endif // QPIXMAPSTYLE_H
