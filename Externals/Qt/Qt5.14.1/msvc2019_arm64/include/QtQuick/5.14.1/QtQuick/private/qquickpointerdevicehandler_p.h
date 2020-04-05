/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
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
#include "qquickpointerhandler_p.h"

#ifndef QQUICKPOINTERDEVICEHANDLER_H
#define QQUICKPOINTERDEVICEHANDLER_H

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

class QQuickPointerDeviceHandlerPrivate;

class Q_QUICK_PRIVATE_EXPORT QQuickPointerDeviceHandler : public QQuickPointerHandler
{
    Q_OBJECT
    Q_PROPERTY(QQuickPointerDevice::DeviceTypes acceptedDevices READ acceptedDevices WRITE setAcceptedDevices NOTIFY acceptedDevicesChanged)
    Q_PROPERTY(QQuickPointerDevice::PointerTypes acceptedPointerTypes READ acceptedPointerTypes WRITE setAcceptedPointerTypes NOTIFY acceptedPointerTypesChanged)
    Q_PROPERTY(Qt::MouseButtons acceptedButtons READ acceptedButtons WRITE setAcceptedButtons NOTIFY acceptedButtonsChanged)
    Q_PROPERTY(Qt::KeyboardModifiers acceptedModifiers READ acceptedModifiers WRITE setAcceptedModifiers NOTIFY acceptedModifiersChanged)

public:
    explicit QQuickPointerDeviceHandler(QQuickItem *parent = nullptr);

    QQuickPointerDevice::DeviceTypes acceptedDevices() const;
    QQuickPointerDevice::PointerTypes acceptedPointerTypes() const;
    Qt::MouseButtons acceptedButtons() const;
    Qt::KeyboardModifiers acceptedModifiers() const;

public Q_SLOTS:
    void setAcceptedDevices(QQuickPointerDevice::DeviceTypes acceptedDevices);
    void setAcceptedPointerTypes(QQuickPointerDevice::PointerTypes acceptedPointerTypes);
    void setAcceptedButtons(Qt::MouseButtons buttons);
    void setAcceptedModifiers(Qt::KeyboardModifiers acceptedModifiers);

Q_SIGNALS:
    void acceptedDevicesChanged();
    void acceptedPointerTypesChanged();
    void acceptedButtonsChanged();
    void acceptedModifiersChanged();

protected:
    QQuickPointerDeviceHandler(QQuickPointerDeviceHandlerPrivate &dd, QQuickItem *parent = nullptr);

    bool wantsPointerEvent(QQuickPointerEvent *event) override;

    Q_DECLARE_PRIVATE(QQuickPointerDeviceHandler)
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QQuickPointerDeviceHandler)

#endif // QQUICKPOINTERDEVICEHANDLER_H
