/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#ifndef QQUICKCONTEXT2DTEXTURE_P_H
#define QQUICKCONTEXT2DTEXTURE_P_H

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

#include <private/qtquickglobal_p.h>

QT_REQUIRE_CONFIG(quick_canvas);

#include <QtQuick/qsgtexture.h>
#include "qquickcanvasitem_p.h"
#include "qquickcontext2d_p.h"
#if QT_CONFIG(opengl)
# include <QOpenGLContext>
# include <QOpenGLFramebufferObject>
#endif
#include <QtCore/QMutex>
#include <QtCore/QWaitCondition>
#include <QtCore/QThread>

QT_BEGIN_NAMESPACE

class QQuickContext2DTile;
class QQuickContext2DCommandBuffer;

class QQuickContext2DTexture : public QObject
{
    Q_OBJECT
public:
    class PaintEvent : public QEvent {
    public:
        PaintEvent(QQuickContext2DCommandBuffer *b) : QEvent(QEvent::Type(QEvent::User + 1)), buffer(b) {}
        QQuickContext2DCommandBuffer *buffer;
    };

    class CanvasChangeEvent : public QEvent {
    public:
        CanvasChangeEvent(const QSize &cSize,
                          const QSize &tSize,
                          const QRect &cWindow,
                          const QRect &dRect,
                          bool sm,
                          bool aa)
            : QEvent(QEvent::Type(QEvent::User + 2))
            , canvasSize(cSize)
            , tileSize(tSize)
            , canvasWindow(cWindow)
            , dirtyRect(dRect)
            , smooth(sm)
            , antialiasing(aa)
        {
        }
        QSize canvasSize;
        QSize tileSize;
        QRect canvasWindow;
        QRect dirtyRect;
        bool smooth;
        bool antialiasing;
    };

    QQuickContext2DTexture();
    ~QQuickContext2DTexture();

    virtual QQuickCanvasItem::RenderTarget renderTarget() const = 0;
    static QRect tiledRect(const QRectF& window, const QSize& tileSize);

    bool setCanvasSize(const QSize &size);
    bool setTileSize(const QSize &size);
    bool setCanvasWindow(const QRect& canvasWindow);
    void setSmooth(bool smooth);
    void setAntialiasing(bool antialiasing);
    bool setDirtyRect(const QRect &dirtyRect);
    bool canvasDestroyed();
    void setOnCustomThread(bool is) { m_onCustomThread = is; }
    bool isOnCustomThread() const { return m_onCustomThread; }

    // Called during sync() on the scene graph thread while GUI is blocked.
    virtual QSGTexture *textureForNextFrame(QSGTexture *lastFrame, QQuickWindow *window) = 0;
    bool event(QEvent *e) override;
#if QT_CONFIG(opengl)
    void initializeOpenGL(QOpenGLContext *gl, QOffscreenSurface *s) {
        m_gl = gl;
        m_surface = s;
    }
#endif

Q_SIGNALS:
    void textureChanged();

public Q_SLOTS:
    void canvasChanged(const QSize& canvasSize, const QSize& tileSize, const QRect& canvasWindow, const QRect& dirtyRect, bool smooth, bool antialiasing);
    void paint(QQuickContext2DCommandBuffer *ccb);
    void markDirtyTexture();
    void setItem(QQuickCanvasItem* item);
    virtual void grabImage(const QRectF& region = QRectF()) = 0;

protected:
    virtual QVector2D scaleFactor() const { return QVector2D(1, 1); }

    void paintWithoutTiles(QQuickContext2DCommandBuffer *ccb);
    virtual QPaintDevice* beginPainting() {m_painting = true; return nullptr; }
    virtual void endPainting() {m_painting = false;}
    virtual QQuickContext2DTile* createTile() const = 0;
    virtual void compositeTile(QQuickContext2DTile* tile) = 0;

    void clearTiles();
    virtual QSize adjustedTileSize(const QSize &ts);
    QRect createTiles(const QRect& window);

    QList<QQuickContext2DTile*> m_tiles;
    QQuickContext2D *m_context;
#if QT_CONFIG(opengl)
    QOpenGLContext *m_gl;
#endif
    QSurface *m_surface;

    QQuickContext2D::State m_state;

    QQuickCanvasItem* m_item;
    QSize m_canvasSize;
    QSize m_tileSize;
    QRect m_canvasWindow;
    qreal m_canvasDevicePixelRatio;

    QMutex m_mutex;
    QWaitCondition m_condition;

    uint m_canvasWindowChanged : 1;
    uint m_dirtyTexture : 1;
    uint m_smooth : 1;
    uint m_antialiasing : 1;
    uint m_tiledCanvas : 1;
    uint m_painting : 1;
    uint m_onCustomThread : 1; // Not GUI and not SGRender
};
#if QT_CONFIG(opengl)
class QQuickContext2DFBOTexture : public QQuickContext2DTexture
{
    Q_OBJECT

public:
    QQuickContext2DFBOTexture();
    ~QQuickContext2DFBOTexture();
    QQuickContext2DTile* createTile() const override;
    QPaintDevice* beginPainting() override;
    void endPainting() override;
    QRectF normalizedTextureSubRect() const;
    QQuickCanvasItem::RenderTarget renderTarget() const override;
    void compositeTile(QQuickContext2DTile* tile) override;
    QSize adjustedTileSize(const QSize &ts) override;

    QSGTexture *textureForNextFrame(QSGTexture *, QQuickWindow *window) override;

protected:
    QVector2D scaleFactor() const override;

public Q_SLOTS:
    void grabImage(const QRectF& region = QRectF()) override;

private:
    bool doMultisampling() const;
    QOpenGLFramebufferObject *m_fbo;
    QOpenGLFramebufferObject *m_multisampledFbo;
    QSize m_fboSize;
    QPaintDevice *m_paint_device;


    GLuint m_displayTextures[2];
    int m_displayTexture;
};
#endif
class QSGPlainTexture;
class QQuickContext2DImageTexture : public QQuickContext2DTexture
{
    Q_OBJECT

public:
    QQuickContext2DImageTexture();
    ~QQuickContext2DImageTexture();

    QQuickCanvasItem::RenderTarget renderTarget() const override;

    QQuickContext2DTile* createTile() const override;
    QPaintDevice* beginPainting() override;
    void endPainting() override;
    void compositeTile(QQuickContext2DTile* tile) override;

    QSGTexture *textureForNextFrame(QSGTexture *lastFrame, QQuickWindow *window) override;

public Q_SLOTS:
    void grabImage(const QRectF& region = QRectF()) override;

private:
    QImage m_image;
    QImage m_displayImage;
    QPainter m_painter;
};

QT_END_NAMESPACE

#endif // QQUICKCONTEXT2DTEXTURE_P_H
