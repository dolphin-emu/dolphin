/****************************************************************************
**
** Copyright (C) 2020 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Assistant of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QFILTERNAMEDIALOG_H
#define QFILTERNAMEDIALOG_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API. It exists for the convenience
// of the help generator tools. This header file may change from version
// to version without notice, or even be removed.
//
// We mean it.
//

#include <QtWidgets/QDialog>
#include "ui_qfilternamedialog.h"

QT_BEGIN_NAMESPACE

class QFilterNameDialog : public QDialog
{
    Q_OBJECT

public:
    QFilterNameDialog(QWidget *parent = nullptr);

    void setFilterName(const QString &filter);
    QString filterName() const;

private slots:
    void updateOkButton();

private:
    Ui::FilterNameDialogClass m_ui;
};

QT_END_NAMESPACE

#endif // QFILTERNAMEDIALOG_H
