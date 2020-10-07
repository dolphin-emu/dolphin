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

#ifndef QPLATFORMBACKINGSTORE_H
#define QPLATFORMBACKINGSTORE_H

//
//  W A R N I N G
//  -------------
//
// This file is part of the QPA API and is not meant to be used
// in applications. Usage of this API may make your code
// source and binary incompatible with future versions of Qt.
//

#include <QtGui/qtguiglobal.h>
#include <QtCore/qloggingcategory.h>
#include <QtCore/qrect.h>
#include <QtCore/qobject.h>

#include <QtGui/qwindow.h>
#include <QtGui/qregion.h>
#include <QtGui/qopengl.h>

QT_BEGIN_NAMESPACE

Q_GUI_EXPORT Q_DECLARE_LOGGING_CATEGORY(lcQpaBackingStore)

class QRegion;
class QRect;
class QPoint;
class QImage;
class QPlatformBackingStorePrivate;
class QPlatformWindow;
class QPlatformTextureList;
class QPlatformTextureListPrivate;
class QOpenGLContext;
class QPlatformGraphicsBuffer;

#ifndef QT_NO_OPENGL
class Q_GUI_EXPORT QPlatformTextureList : public QObject
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QPlatformTextureList)
public:
    enum Flag {
        StacksOnTop = 0x01,
        TextureIsSrgb = 0x02,
        NeedsPremultipliedAlphaBlending = 0x04
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    explicit QPlatformTextureList(QObject *parent = nullptr);
    ~QPlatformTextureList();

    int count() const;
    bool isEmpty() const { return count() == 0; }
    GLuint textureId(int index) const;
    QRect geometry(int index) const;
    QRect clipRect(int index) const;
    void *source(int index);
    Flags flags(int index) const;
    void lock(bool on);
    bool isLocked() const;

    void appendTexture(void *source, GLuint textureId, const QRect &geometry,
                       const QRect &clipRect = QRect(), Flags flags = { });
    void clear();

 Q_SIGNALS:
    void locked(bool);
};
Q_DECLARE_OPERATORS_FOR_FLAGS(QPlatformTextureList::Flags)
#endif

class Q_GUI_EXPORT QPlatformBackingStore
{
public:
    explicit QPlatformBackingStore(QWindow *window);
    virtual ~QPlatformBackingStore();

    QWindow *window() const;
    QBackingStore *backingStore() const;

    virtual QPaintDevice *paintDevice() = 0;

    virtual void flush(QWindow *window, const QRegion &region, const QPoint &offset) = 0;
#ifndef QT_NO_OPENGL
    virtual void composeAndFlush(QWindow *window, const QRegion &region, const QPoint &offset,
                                 QPlatformTextureList *textures,
                                 bool translucentBackground);
#endif
    virtual QImage toImage() const;
#ifndef QT_NO_OPENGL
    enum TextureFlag {
        TextureSwizzle = 0x01,
        TextureFlip = 0x02,
        TexturePremultiplied = 0x04,
    };
    Q_DECLARE_FLAGS(TextureFlags, TextureFlag)
    virtual GLuint toTexture(const QRegion &dirtyRegion, QSize *textureSize, TextureFlags *flags) const;
#endif

    virtual QPlatformGraphicsBuffer *graphicsBuffer() const;

    virtual void resize(const QSize &size, const QRegion &staticContents) = 0;

    virtual bool scroll(const QRegion &area, int dx, int dy);

    virtual void beginPaint(const QRegion &);
    virtual void endPaint();

private:
    QPlatformBackingStorePrivate *d_ptr;

    void setBackingStore(QBackingStore *);
    friend class QBackingStore;
};

#ifndef QT_NO_OPENGL
Q_DECLARE_OPERATORS_FOR_FLAGS(QPlatformBackingStore::TextureFlags)
#endif

QT_END_NAMESPACE

#endif // QPLATFORMBACKINGSTORE_H
