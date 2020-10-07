/****************************************************************************
 **
 ** Copyright (C) 2016 Ivan Vizir <define-true-false@yandex.com>
 ** Contact: https://www.qt.io/licensing/
 **
 ** This file is part of the QtWinExtras module of the Qt Toolkit.
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

#ifndef QWINTASKBARBUTTON_H
#define QWINTASKBARBUTTON_H

#include <QtGui/qicon.h>
#include <QtCore/qobject.h>
#include <QtWinExtras/qwinextrasglobal.h>

QT_BEGIN_NAMESPACE

class QWindow;
class QWinTaskbarProgress;
class QWinTaskbarButtonPrivate;

class Q_WINEXTRAS_EXPORT QWinTaskbarButton : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QIcon overlayIcon READ overlayIcon WRITE setOverlayIcon RESET clearOverlayIcon)
    Q_PROPERTY(QString overlayAccessibleDescription READ overlayAccessibleDescription WRITE setOverlayAccessibleDescription)
    Q_PROPERTY(QWinTaskbarProgress *progress READ progress)
    Q_PROPERTY(QWindow *window READ window WRITE setWindow)

public:
    explicit QWinTaskbarButton(QObject *parent = nullptr);
    ~QWinTaskbarButton();

    void setWindow(QWindow *window);
    QWindow *window() const;

    QIcon overlayIcon() const;
    QString overlayAccessibleDescription() const;

    QWinTaskbarProgress *progress() const;

    bool eventFilter(QObject *, QEvent *) override;

public Q_SLOTS:
    void setOverlayIcon(const QIcon &icon);
    void setOverlayAccessibleDescription(const QString &description);

    void clearOverlayIcon();

private:
    Q_DISABLE_COPY(QWinTaskbarButton)
    Q_DECLARE_PRIVATE(QWinTaskbarButton)
    QScopedPointer<QWinTaskbarButtonPrivate> d_ptr;

    Q_PRIVATE_SLOT(d_func(), void _q_updateProgress())
};

QT_END_NAMESPACE

#endif // QWINTASKBARBUTTON_H
