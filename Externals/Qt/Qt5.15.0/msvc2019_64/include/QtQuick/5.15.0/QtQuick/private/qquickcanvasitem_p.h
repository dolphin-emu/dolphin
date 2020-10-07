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

#ifndef QQUICKCANVASITEM_P_H
#define QQUICKCANVASITEM_P_H

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

#include <QtQuick/qquickitem.h>
#include <private/qqmlrefcount_p.h>
#include <QtCore/QThread>
#include <QtCore/qmutex.h>
#include <QtGui/QImage>

QT_BEGIN_NAMESPACE

class QQuickCanvasContext;

class QQuickCanvasItemPrivate;
class QQuickPixmap;

class QQuickCanvasPixmap : public QQmlRefCount
{
public:
    QQuickCanvasPixmap(const QImage& image);
    QQuickCanvasPixmap(QQuickPixmap *pixmap);
    ~QQuickCanvasPixmap();

    QImage image();

    qreal width() const;
    qreal height() const;
    bool isValid() const;
    QQuickPixmap *pixmap() const { return m_pixmap;}

private:
    QQuickPixmap *m_pixmap;
    QImage m_image;
};

class QQuickCanvasItem : public QQuickItem
{
    Q_OBJECT

    Q_PROPERTY(bool available READ isAvailable NOTIFY availableChanged)
    Q_PROPERTY(QString contextType READ contextType WRITE setContextType NOTIFY contextTypeChanged)
    Q_PROPERTY(QJSValue context READ context NOTIFY contextChanged)
    Q_PROPERTY(QSizeF canvasSize READ canvasSize WRITE setCanvasSize NOTIFY canvasSizeChanged)
    Q_PROPERTY(QSize tileSize READ tileSize WRITE setTileSize NOTIFY tileSizeChanged)
    Q_PROPERTY(QRectF canvasWindow READ canvasWindow WRITE setCanvasWindow NOTIFY canvasWindowChanged)
    Q_PROPERTY(RenderTarget renderTarget READ renderTarget WRITE setRenderTarget NOTIFY renderTargetChanged)
    Q_PROPERTY(RenderStrategy renderStrategy READ renderStrategy WRITE setRenderStrategy NOTIFY renderStrategyChanged)
    QML_NAMED_ELEMENT(Canvas)

public:
    enum RenderTarget {
        Image,
        FramebufferObject
    };
    Q_ENUM(RenderTarget)

    enum RenderStrategy {
        Immediate,
        Threaded,
        Cooperative
    };
    Q_ENUM(RenderStrategy)

    QQuickCanvasItem(QQuickItem *parent = nullptr);
    ~QQuickCanvasItem();

    bool isAvailable() const;

    QString contextType() const;
    void setContextType(const QString &contextType);

    QJSValue context() const;

    QSizeF canvasSize() const;
    void setCanvasSize(const QSizeF &);

    QSize tileSize() const;
    void setTileSize(const QSize &);

    QRectF canvasWindow() const;
    void setCanvasWindow(const QRectF& rect);

    RenderTarget renderTarget() const;
    void setRenderTarget(RenderTarget target);

    RenderStrategy renderStrategy() const;
    void setRenderStrategy(RenderStrategy strategy);

    QQuickCanvasContext *rawContext() const;

    QImage toImage(const QRectF& rect = QRectF()) const;

    Q_INVOKABLE void getContext(QQmlV4Function *args);

    Q_INVOKABLE void requestAnimationFrame(QQmlV4Function *args);
    Q_INVOKABLE void cancelRequestAnimationFrame(QQmlV4Function *args);

    Q_INVOKABLE void requestPaint();
    Q_INVOKABLE void markDirty(const QRectF& dirtyRect = QRectF());

    Q_INVOKABLE bool save(const QString &filename) const;
    Q_INVOKABLE QString toDataURL(const QString& type = QLatin1String("image/png")) const;
    QQmlRefPointer<QQuickCanvasPixmap> loadedPixmap(const QUrl& url);

    bool isTextureProvider() const override;
    QSGTextureProvider *textureProvider() const override;

Q_SIGNALS:
    void paint(const QRect &region);
    void painted();
    void availableChanged();
    void contextTypeChanged();
    void contextChanged();
    void canvasSizeChanged();
    void tileSizeChanged();
    void canvasWindowChanged();
    void renderTargetChanged();
    void renderStrategyChanged();
    void imageLoaded();

public Q_SLOTS:
    void loadImage(const QUrl& url);
    void unloadImage(const QUrl& url);
    bool isImageLoaded(const QUrl& url) const;
    bool isImageLoading(const QUrl& url) const;
    bool isImageError(const QUrl& url) const;

private Q_SLOTS:
    void sceneGraphInitialized();
    void checkAnimationCallbacks();
    void invalidateSceneGraph();
    void schedulePolish();

protected:
    void componentComplete() override;
    void itemChange(QQuickItem::ItemChange, const QQuickItem::ItemChangeData &) override;
    void updatePolish() override;
    QSGNode *updatePaintNode(QSGNode *, UpdatePaintNodeData *) override;
    void geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry) override;
    void releaseResources() override;
    bool event(QEvent *event) override;
private:
    Q_DECLARE_PRIVATE(QQuickCanvasItem)
    Q_INVOKABLE void delayedCreate();
    bool createContext(const QString &contextType);
    void initializeContext(QQuickCanvasContext *context, const QVariantMap &args = QVariantMap());
    static QRect tiledRect(const QRectF &window, const QSize &tileSize);
    bool isPaintConnected();
};

class QQuickContext2DRenderThread : public QThread
{
    Q_OBJECT
public:
    QQuickContext2DRenderThread(QQmlEngine *eng);
    ~QQuickContext2DRenderThread();

    static QQuickContext2DRenderThread *instance(QQmlEngine *engine);

private:
    QQmlEngine *m_engine;
    QObject *m_eventLoopQuitHack;
    static QHash<QQmlEngine *,QQuickContext2DRenderThread*> renderThreads;
    static QMutex renderThreadsMutex;
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QQuickCanvasItem)

#endif //QQUICKCANVASITEM_P_H
