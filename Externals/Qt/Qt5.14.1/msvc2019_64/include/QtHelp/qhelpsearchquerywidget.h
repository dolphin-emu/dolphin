/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Assistant of the Qt Toolkit.
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

#ifndef QHELPSEARCHQUERYWIDGET_H
#define QHELPSEARCHQUERYWIDGET_H

#include <QtHelp/qhelp_global.h>
#include <QtHelp/qhelpsearchengine.h>

#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE


class QFocusEvent;
class QHelpSearchQueryWidgetPrivate;

class QHELP_EXPORT QHelpSearchQueryWidget : public QWidget
{
    Q_OBJECT

public:
    explicit QHelpSearchQueryWidget(QWidget *parent = nullptr);
    ~QHelpSearchQueryWidget() override;

    void expandExtendedSearch();
    void collapseExtendedSearch();

#if QT_DEPRECATED_SINCE(5, 9)
    QT_DEPRECATED QList<QHelpSearchQuery> query() const;
    QT_DEPRECATED void setQuery(const QList<QHelpSearchQuery> &queryList);
#endif

    QString searchInput() const;
    void setSearchInput(const QString &searchInput);

    bool isCompactMode() const;
    Q_SLOT void setCompactMode(bool on);

Q_SIGNALS:
    void search();

private:
    void focusInEvent(QFocusEvent *focusEvent) override;
    void changeEvent(QEvent *event) override;

private:
    QHelpSearchQueryWidgetPrivate *d;
};

QT_END_NAMESPACE

#endif  // QHELPSEARCHQUERYWIDGET_H
