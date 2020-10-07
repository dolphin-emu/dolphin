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

#ifndef QPLATFORMGRAPHICSBUFFER_H
#define QPLATFORMGRAPHICSBUFFER_H

//
//  W A R N I N G
//  -------------
//
// This file is part of the QPA API and is not meant to be used
// in applications. Usage of this API may make your code
// source and binary incompatible with future versions of Qt.
//


#include <QtGui/qtguiglobal.h>
#include <QtCore/QSize>
#include <QtCore/QRect>
#include <QtGui/QPixelFormat>
#include <QtCore/qflags.h>
#include <QtCore/QObject>

QT_BEGIN_NAMESPACE

class Q_GUI_EXPORT QPlatformGraphicsBuffer : public QObject
{
Q_OBJECT
public:
    enum AccessType
    {
        None                = 0x00,
        SWReadAccess        = 0x01,
        SWWriteAccess       = 0x02,
        TextureAccess       = 0x04,
        HWCompositor        = 0x08
    };
    Q_ENUM(AccessType);
    Q_DECLARE_FLAGS(AccessTypes, AccessType);

    enum Origin {
        OriginBottomLeft,
        OriginTopLeft
    };
    Q_ENUM(Origin);

    ~QPlatformGraphicsBuffer();

    AccessTypes isLocked() const { return m_lock_access; }
    bool lock(AccessTypes access, const QRect &rect = QRect());
    void unlock();

    virtual bool bindToTexture(const QRect &rect = QRect()) const;

    virtual const uchar *data() const;
    virtual uchar *data();
    virtual int bytesPerLine() const;
    int byteCount() const;

    virtual Origin origin() const;

    QSize size() const { return m_size; }
    QPixelFormat format() const { return m_format; }

Q_SIGNALS:
    void unlocked(AccessTypes previousAccessTypes);

protected:
    QPlatformGraphicsBuffer(const QSize &size, const QPixelFormat &format);

    virtual bool doLock(AccessTypes access, const QRect &rect = QRect()) = 0;
    virtual void doUnlock() = 0;

private:
    QSize m_size;
    QPixelFormat m_format;
    AccessTypes m_lock_access;
};

QT_END_NAMESPACE

#endif //QPLATFORMGRAPHICSBUFFER_H
