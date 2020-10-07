/****************************************************************************
 **
 ** Copyright (C) 2016 Ivan Vizir <define-true-false@yandex.com>
 ** Copyright (C) 2016 The Qt Company Ltd.
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

#ifndef QWINEVENT_H
#define QWINEVENT_H

#include <QtGui/qrgb.h>
#include <QtCore/qcoreevent.h>
#include <QtWinExtras/qwinextrasglobal.h>

QT_BEGIN_NAMESPACE

class Q_WINEXTRAS_EXPORT QWinEvent : public QEvent
{
public:
    static const int ColorizationChange;
    static const int CompositionChange;
    static const int TaskbarButtonCreated;
    static const int ThemeChange;

    explicit QWinEvent(int type);
    ~QWinEvent();
};

class Q_WINEXTRAS_EXPORT QWinColorizationChangeEvent : public QWinEvent
{
public:
    QWinColorizationChangeEvent(QRgb color, bool opaque);
    ~QWinColorizationChangeEvent();

    inline QRgb color() const { return rgb; }
    inline bool opaqueBlend() const { return opaque; }

private:
    QRgb rgb;
    bool opaque;
};

class Q_WINEXTRAS_EXPORT QWinCompositionChangeEvent : public QWinEvent
{
public:
    explicit QWinCompositionChangeEvent(bool enabled);
    ~QWinCompositionChangeEvent();

    inline bool isCompositionEnabled() const { return enabled; }

private:
    bool enabled;
};

QT_END_NAMESPACE

#endif // QWINEVENT_H
