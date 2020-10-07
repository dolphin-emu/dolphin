/****************************************************************************
**
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

#ifndef QWIDGET_P_H
#define QWIDGET_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of qapplication_*.cpp, qwidget*.cpp and qfiledialog.cpp.  This header
// file may change from version to version without notice, or even be removed.
//
// We mean it.
//

#include <QtWidgets/private/qtwidgetsglobal_p.h>
#include "QtWidgets/qwidget.h"
#include "private/qobject_p.h"
#include "QtCore/qrect.h"
#include "QtCore/qlocale.h"
#include "QtCore/qset.h"
#include "QtGui/qregion.h"
#include "QtGui/qinputmethod.h"
#include "QtGui/qopengl.h"
#include "QtGui/qsurfaceformat.h"
#include "QtWidgets/qsizepolicy.h"
#include "QtWidgets/qstyle.h"
#include "QtWidgets/qapplication.h"
#if QT_CONFIG(graphicseffect)
#include <private/qgraphicseffect_p.h>
#endif
#if QT_CONFIG(graphicsview)
#include "QtWidgets/qgraphicsproxywidget.h"
#include "QtWidgets/qgraphicsscene.h"
#include "QtWidgets/qgraphicsview.h"
#endif
#include <private/qgesture_p.h>
#include <qpa/qplatformbackingstore.h>

#include <vector>
#include <memory>

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(lcWidgetPainting);

// Extra QWidget data
//  - to minimize memory usage for members that are seldom used.
//  - top-level widgets have extra extra data to reduce cost further
class QWidgetWindow;
class QPaintEngine;
class QPixmap;
class QWidgetRepaintManager;
class QGraphicsProxyWidget;
class QWidgetItemV2;
class QOpenGLContext;

class QStyle;

class QUnifiedToolbarSurface;

// implemented in qshortcut.cpp
bool qWidgetShortcutContextMatcher(QObject *object, Qt::ShortcutContext context);

class QUpdateLaterEvent : public QEvent
{
public:
    explicit QUpdateLaterEvent(const QRegion& paintRegion)
        : QEvent(UpdateLater), m_region(paintRegion)
    {
    }

    ~QUpdateLaterEvent()
    {
    }

    inline const QRegion &region() const { return m_region; }

protected:
    QRegion m_region;
};

struct QTLWExtra {
    // *************************** Cross-platform variables *****************************

    // Regular pointers (keep them together to avoid gaps on 64 bits architectures).
    std::unique_ptr<QIcon> icon; // widget icon
    std::unique_ptr<QWidgetRepaintManager> repaintManager;
    QBackingStore *backingStore;
    QPainter *sharedPainter;
    QWidgetWindow *window;
#ifndef QT_NO_OPENGL
    mutable std::unique_ptr<QOpenGLContext> shareContext;
#endif

    // Implicit pointers (shared_null).
    QString caption; // widget caption
    QString iconText; // widget icon text
    QString role; // widget role
    QString filePath; // widget file path

    // Other variables.
    short incw, inch; // size increments
    short basew, baseh; // base sizes
     // frame strut, don't use these directly, use QWidgetPrivate::frameStrut() instead.
    QRect frameStrut;
    QRect normalGeometry; // used by showMin/maximized/FullScreen
    Qt::WindowFlags savedFlags; // Save widget flags while showing fullscreen
    // ### TODO replace initialScreenIndex with QScreen *, in case the screens change at runtime
    int initialScreenIndex; // Screen number when passing a QDesktop[Screen]Widget as parent.

#ifndef QT_NO_OPENGL
    std::vector<std::unique_ptr<QPlatformTextureList>> widgetTextures;
#endif

    // *************************** Cross-platform bit fields ****************************
    uint opacity : 8;
    uint posIncludesFrame : 1;
    uint sizeAdjusted : 1;
    uint embedded : 1;
};

struct QWExtra {
    // *************************** Cross-platform variables *****************************

    // Regular pointers (keep them together to avoid gaps on 64 bits architectures).
    void *glContext; // if the widget is hijacked by QGLWindowSurface
    std::unique_ptr<QTLWExtra> topextra; // only useful for TLWs
#if QT_CONFIG(graphicsview)
    QGraphicsProxyWidget *proxyWidget; // if the widget is embedded
#endif
#ifndef QT_NO_CURSOR
    std::unique_ptr<QCursor> curs;
#endif
    QPointer<QStyle> style;
    QPointer<QWidget> focus_proxy;

    // Implicit pointers (shared_empty/shared_null).
    QRegion mask; // widget mask
    QString styleSheet;

    // Other variables.
    qint32 minw;
    qint32 minh; // minimum size
    qint32 maxw;
    qint32 maxh; // maximum size
    quint16 customDpiX;
    quint16 customDpiY;
    QSize staticContentsSize;

    // *************************** Cross-platform bit fields ****************************
    uint explicitMinSize : 2;
    uint explicitMaxSize : 2;
    uint autoFillBackground : 1;
    uint nativeChildrenForced : 1;
    uint inRenderWithPainter : 1;
    uint hasMask : 1;
    uint hasWindowContainer : 1;
};

/*!
    \internal

    Returns \c true if \a p or any of its parents enable the
    Qt::BypassGraphicsProxyWidget window flag. Used in QWidget::show() and
    QWidget::setParent() to determine whether it's necessary to embed the
    widget into a QGraphicsProxyWidget or not.
*/
static inline bool bypassGraphicsProxyWidget(const QWidget *p)
{
    while (p) {
        if (p->windowFlags() & Qt::BypassGraphicsProxyWidget)
            return true;
        p = p->parentWidget();
    }
    return false;
}

class Q_WIDGETS_EXPORT QWidgetPrivate : public QObjectPrivate
{
    Q_DECLARE_PUBLIC(QWidget)
    Q_GADGET

public:
    // *************************** Cross-platform ***************************************
    enum DrawWidgetFlag {
        DrawAsRoot = 0x01,
        DrawPaintOnScreen = 0x02,
        DrawRecursive = 0x04,
        DrawInvisible = 0x08,
        DontSubtractOpaqueChildren = 0x10,
        DontDrawOpaqueChildren = 0x20,
        DontDrawNativeChildren = 0x40,
        DontSetCompositionMode = 0x80
    };
    Q_DECLARE_FLAGS(DrawWidgetFlags, DrawWidgetFlag)
    Q_FLAG(DrawWidgetFlags)

    enum CloseMode {
        CloseNoEvent,
        CloseWithEvent,
        CloseWithSpontaneousEvent
    };
    Q_ENUM(CloseMode)

    enum Direction {
        DirectionNorth = 0x01,
        DirectionEast = 0x10,
        DirectionSouth = 0x02,
        DirectionWest = 0x20
    };
    Q_ENUM(Direction)

    // Functions.
    explicit QWidgetPrivate(int version = QObjectPrivateVersion);
    ~QWidgetPrivate();

    static QWidgetPrivate *get(QWidget *w) { return w->d_func(); }
    static const QWidgetPrivate *get(const QWidget *w) { return w->d_func(); }

    QWExtra *extraData() const;
    QTLWExtra *topData() const;
    QTLWExtra *maybeTopData() const;
    QPainter *sharedPainter() const;
    void setSharedPainter(QPainter *painter);
    QWidgetRepaintManager *maybeRepaintManager() const;

    enum class WindowHandleMode {
        Direct,
        Closest,
        TopLevel
    };
    QWindow *windowHandle(WindowHandleMode mode = WindowHandleMode::Direct) const;

    QScreen *associatedScreen() const;

    template <typename T>
    void repaint(T t);

    template <typename T>
    void update(T t);

    void init(QWidget *desktopWidget, Qt::WindowFlags f);
    void create();
    void createRecursively();
    void createWinId();

    bool setScreenForPoint(const QPoint &pos);
    bool setScreen(QScreen *screen);

    void createTLExtra();
    void createExtra();
    void deleteExtra();
    void createSysExtra();
    void deleteSysExtra();
    void createTLSysExtra();
    void deleteTLSysExtra();
    void updateSystemBackground();
    void propagatePaletteChange();

    void setPalette_helper(const QPalette &);
    void resolvePalette();
    QPalette naturalWidgetPalette(uint inheritedMask) const;

    void setMask_sys(const QRegion &);

    void raise_sys();
    void lower_sys();
    void stackUnder_sys(QWidget *);

    QWidget *deepestFocusProxy() const;
    void setFocus_sys();
    void updateFocusChild();

    void updateFont(const QFont &);
    inline void setFont_helper(const QFont &font) {
        if (directFontResolveMask == font.resolve() && data.fnt == font)
            return;
        updateFont(font);
    }
    QFont localFont() const;
    void resolveFont();
    QFont naturalWidgetFont(uint inheritedMask) const;

    void setLayoutDirection_helper(Qt::LayoutDirection);
    void resolveLayoutDirection();

    void setLocale_helper(const QLocale &l, bool forceUpdate = false);
    void resolveLocale();

    void setStyle_helper(QStyle *newStyle, bool propagate);
    void inheritStyle();

    void setUpdatesEnabled_helper(bool );

    bool updateBrushOrigin(QPainter *, const QBrush &brush) const;
    void paintBackground(QPainter *, const QRegion &, DrawWidgetFlags flags = DrawAsRoot) const;
    bool isAboutToShow() const;
    QRegion prepareToRender(const QRegion &region, QWidget::RenderFlags renderFlags);
    void render_helper(QPainter *painter, const QPoint &targetOffset, const QRegion &sourceRegion,
                       QWidget::RenderFlags renderFlags);
    void render(QPaintDevice *target, const QPoint &targetOffset, const QRegion &sourceRegion,
                QWidget::RenderFlags renderFlags);
    void drawWidget(QPaintDevice *pdev, const QRegion &rgn, const QPoint &offset, DrawWidgetFlags flags,
                    QPainter *sharedPainter = nullptr, QWidgetRepaintManager *repaintManager = nullptr);
    void sendPaintEvent(const QRegion &toBePainted);


    void paintSiblingsRecursive(QPaintDevice *pdev, const QObjectList& children, int index,
                                const QRegion &rgn, const QPoint &offset, DrawWidgetFlags flags,
                                QPainter *sharedPainter, QWidgetRepaintManager *repaintManager);

#if QT_CONFIG(graphicsview)
    static QGraphicsProxyWidget * nearestGraphicsProxyWidget(const QWidget *origin);
#endif
    bool shouldPaintOnScreen() const;
    void paintOnScreen(const QRegion &rgn);

    QRect clipRect() const;
    QRegion clipRegion() const;
    void setSystemClip(QPaintEngine *paintEngine, qreal devicePixelRatio, const QRegion &region);
    void subtractOpaqueChildren(QRegion &rgn, const QRect &clipRect) const;
    void subtractOpaqueSiblings(QRegion &source, bool *hasDirtySiblingsAbove = nullptr,
                                bool alsoNonOpaque = false) const;
    void clipToEffectiveMask(QRegion &region) const;
    void updateIsOpaque();
    void setOpaque(bool opaque);
    void updateIsTranslucent();
#if QT_CONFIG(graphicseffect)
    void invalidateGraphicsEffectsRecursively();
#endif // QT_CONFIG(graphicseffect)

    const QRegion &getOpaqueChildren() const;
    void setDirtyOpaqueRegion();

    bool close_helper(CloseMode mode);

    void setWindowIcon_helper();
    void setWindowIcon_sys();
    void setWindowOpacity_sys(qreal opacity);
    void adjustQuitOnCloseAttribute();

    void scrollChildren(int dx, int dy);
    void moveRect(const QRect &, int dx, int dy);
    void scrollRect(const QRect &, int dx, int dy);
    void invalidateBackingStore_resizeHelper(const QPoint &oldPos, const QSize &oldSize);

    template <class T>
    void invalidateBackingStore(const T &);

    QRegion overlappedRegion(const QRect &rect, bool breakAfterFirst = false) const;
    void syncBackingStore();
    void syncBackingStore(const QRegion &region);

    bool shouldDiscardSyncRequest() const;

    // tells the input method about the widgets transform
    void updateWidgetTransform(QEvent *event);

    void reparentFocusWidgets(QWidget *oldtlw);

    static int pointToRect(const QPoint &p, const QRect &r);

    void setWinId(WId);
    void showChildren(bool spontaneous);
    void hideChildren(bool spontaneous);
    void setParent_sys(QWidget *parent, Qt::WindowFlags);
    void scroll_sys(int dx, int dy);
    void scroll_sys(int dx, int dy, const QRect &r);
    void deactivateWidgetCleanup();
    void setGeometry_sys(int, int, int, int, bool);
    void fixPosIncludesFrame();
    void sendPendingMoveAndResizeEvents(bool recursive = false, bool disableUpdates = false);
    void activateChildLayoutsRecursively();
    void show_recursive();
    void show_helper();
    void show_sys();
    void hide_sys();
    void hide_helper();
    void _q_showIfNotHidden();
    void setVisible(bool);

    void setEnabled_helper(bool);
    static void adjustFlags(Qt::WindowFlags &flags, QWidget *w = nullptr);

    void updateFrameStrut();
    QRect frameStrut() const;

#ifdef QT_KEYPAD_NAVIGATION
    static bool navigateToDirection(Direction direction);
    static QWidget *widgetInNavigationDirection(Direction direction);
    static bool canKeypadNavigate(Qt::Orientation orientation);
    static bool inTabWidget(QWidget *widget);
#endif

    void setWindowIconText_sys(const QString &cap);
    void setWindowIconText_helper(const QString &cap);
    void setWindowTitle_sys(const QString &cap);
    void setWindowFilePath_sys(const QString &filePath);

#ifndef QT_NO_CURSOR
    void setCursor_sys(const QCursor &cursor);
    void unsetCursor_sys();
#endif

    void setWindowTitle_helper(const QString &cap);
    void setWindowFilePath_helper(const QString &filePath);
    void setWindowModified_helper();
    virtual void setWindowFlags(Qt::WindowFlags windowFlags);

    bool setMinimumSize_helper(int &minw, int &minh);
    bool setMaximumSize_helper(int &maxw, int &maxh);
    void setConstraints_sys();
    bool pointInsideRectAndMask(const QPoint &) const;
    QWidget *childAt_helper(const QPoint &, bool) const;
    QWidget *childAtRecursiveHelper(const QPoint &p, bool) const;
    void updateGeometry_helper(bool forceUpdate);

    void getLayoutItemMargins(int *left, int *top, int *right, int *bottom) const;
    void setLayoutItemMargins(int left, int top, int right, int bottom);
    void setLayoutItemMargins(QStyle::SubElement element, const QStyleOption *opt = nullptr);

    void updateContentsRect();
    QMargins safeAreaMargins() const;

    // aboutToDestroy() is called just before the contents of
    // QWidget::destroy() is executed. It's used to signal QWidget
    // sub-classes that their internals are about to be released.
    virtual void aboutToDestroy() {}

    inline QWidget *effectiveFocusWidget() {
        QWidget *w = q_func();
        while (w->focusProxy())
            w = w->focusProxy();
        return w;
    }

    void setModal_sys();

    // This is an helper function that return the available geometry for
    // a widget and takes care is this one is in QGraphicsView.
    // If the widget is not embed in a scene then the geometry available is
    // null, we let QDesktopWidget decide for us.
    static QRect screenGeometry(const QWidget *widget)
    {
        QRect screen;
#if QT_CONFIG(graphicsview)
        QGraphicsProxyWidget *ancestorProxy = widget->d_func()->nearestGraphicsProxyWidget(widget);
        //It's embedded if it has an ancestor
        if (ancestorProxy) {
            if (!bypassGraphicsProxyWidget(widget) && ancestorProxy->scene() != nullptr) {
                // One view, let be smart and return the viewport rect then the popup is aligned
                if (ancestorProxy->scene()->views().size() == 1) {
                    QGraphicsView *view = ancestorProxy->scene()->views().at(0);
                    screen = view->mapToScene(view->viewport()->rect()).boundingRect().toRect();
                } else {
                    screen = ancestorProxy->scene()->sceneRect().toRect();
                }
            }
        }
#else
        Q_UNUSED(widget);
#endif
        return screen;
    }

    inline void setRedirected(QPaintDevice *replacement, const QPoint &offset)
    {
        Q_ASSERT(q_func()->testAttribute(Qt::WA_WState_InPaintEvent));
        redirectDev = replacement;
        redirectOffset = offset;
    }

    inline QPaintDevice *redirected(QPoint *offset) const
    {
        if (offset)
            *offset = redirectDev ? redirectOffset : QPoint();
        return redirectDev;
    }

    inline void restoreRedirected()
    { redirectDev = nullptr; }

    inline void enforceNativeChildren()
    {
        if (!extra)
            createExtra();

        if (extra->nativeChildrenForced)
            return;
        extra->nativeChildrenForced = 1;

        for (int i = 0; i < children.size(); ++i) {
            if (QWidget *child = qobject_cast<QWidget *>(children.at(i)))
                child->setAttribute(Qt::WA_NativeWindow);
        }
    }

    inline bool nativeChildrenForced() const
    {
        return extra ? extra->nativeChildrenForced : false;
    }

    inline QRect effectiveRectFor(const QRegion &region) const
    {
        return effectiveRectFor(region.boundingRect());
    }

    inline QRect effectiveRectFor(const QRect &rect) const
    {
#if QT_CONFIG(graphicseffect)
        if (graphicsEffect && graphicsEffect->isEnabled())
            return graphicsEffect->boundingRectFor(rect).toAlignedRect();
#endif // QT_CONFIG(graphicseffect)
        return rect;
    }

    QSize adjustedSize() const;

    inline void handleSoftwareInputPanel(Qt::MouseButton button, bool clickCausedFocus)
    {
        Q_Q(QWidget);
        if (button == Qt::LeftButton && qApp->autoSipEnabled()) {
            QStyle::RequestSoftwareInputPanel behavior = QStyle::RequestSoftwareInputPanel(
                    q->style()->styleHint(QStyle::SH_RequestSoftwareInputPanel));
            if (!clickCausedFocus || behavior == QStyle::RSIP_OnMouseClick) {
                QGuiApplication::inputMethod()->show();
            }
        }
    }

    void setWSGeometry();

    inline QPoint mapToWS(const QPoint &p) const
    { return p - data.wrect.topLeft(); }

    inline QPoint mapFromWS(const QPoint &p) const
    { return p + data.wrect.topLeft(); }

    inline QRect mapToWS(const QRect &r) const
    { return r.translated(-data.wrect.topLeft()); }

    inline QRect mapFromWS(const QRect &r) const
    { return r.translated(data.wrect.topLeft()); }

    QOpenGLContext *shareContext() const;

    virtual QObject *focusObject() { return nullptr; }

#ifndef QT_NO_OPENGL
    virtual GLuint textureId() const { return 0; }
    virtual QPlatformTextureList::Flags textureListFlags() {
        Q_Q(QWidget);
        return q->testAttribute(Qt::WA_AlwaysStackOnTop)
            ? QPlatformTextureList::StacksOnTop
            : QPlatformTextureList::Flags();
    }
    virtual QImage grabFramebuffer() { return QImage(); }
    virtual void beginBackingStorePainting() { }
    virtual void endBackingStorePainting() { }
    virtual void beginCompose() { }
    virtual void endCompose() { }
    void setRenderToTexture() { renderToTexture = true; setTextureChildSeen(); }
    void setTextureChildSeen()
    {
        Q_Q(QWidget);
        if (textureChildSeen)
            return;
        textureChildSeen = 1;

        if (!q->isWindow()) {
            QWidget *parent = q->parentWidget();
            if (parent)
                get(parent)->setTextureChildSeen();
        }
    }
    static void sendComposeStatus(QWidget *w, bool end);
    // Called on setViewport().
    virtual void initializeViewportFramebuffer() { }
    // When using a QOpenGLWidget as viewport with QAbstractScrollArea, resize events are
    // filtered away from the widget. This is fine for QGLWidget but bad for QOpenGLWidget
    // since the fbo must be resized. We need an alternative way to notify.
    virtual void resizeViewportFramebuffer() { }
    // Called after each paint event.
    virtual void resolveSamples() { }
#endif

    static void setWidgetParentHelper(QObject *widgetAsObject, QObject *newParent);

    // Variables.
    // Regular pointers (keep them together to avoid gaps on 64 bit architectures).
    std::unique_ptr<QWExtra> extra;
    QWidget *focus_next;
    QWidget *focus_prev;
    QWidget *focus_child;
    QLayout *layout;
    QRegion *needsFlush;
    QPaintDevice *redirectDev;
    QWidgetItemV2 *widgetItem;
    QPaintEngine *extraPaintEngine;
    mutable const QMetaObject *polished;
    QGraphicsEffect *graphicsEffect;
    // All widgets are added into the allWidgets set. Once
    // they receive a window id they are also added to the mapper.
    // This should just ensure that all widgets are deleted by QApplication
    static QWidgetMapper *mapper;
    static QWidgetSet *allWidgets;
#if !defined(QT_NO_IM)
    Qt::InputMethodHints imHints;
#endif
#ifdef QT_KEYPAD_NAVIGATION
    static QPointer<QWidget> editingWidget;
#endif

    // Implicit pointers (shared_null/shared_empty).
    QRegion opaqueChildren;
    QRegion dirty;
#ifndef QT_NO_TOOLTIP
    QString toolTip;
    int toolTipDuration;
#endif
#if QT_CONFIG(statustip)
    QString statusTip;
#endif
#if QT_CONFIG(whatsthis)
    QString whatsThis;
#endif
#ifndef QT_NO_ACCESSIBILITY
    QString accessibleName;
    QString accessibleDescription;
#endif

    // Other variables.
    uint directFontResolveMask;
    uint inheritedFontResolveMask;
    decltype(std::declval<QPalette>().resolve()) directPaletteResolveMask;
    uint inheritedPaletteResolveMask;
    short leftmargin;
    short topmargin;
    short rightmargin;
    short bottommargin;
    signed char leftLayoutItemMargin;
    signed char topLayoutItemMargin;
    signed char rightLayoutItemMargin;
    signed char bottomLayoutItemMargin;
    static int instanceCounter; // Current number of widget instances
    static int maxInstances; // Maximum number of widget instances
    Qt::HANDLE hd;
    QWidgetData data;
    QSizePolicy size_policy;
    QLocale locale;
    QPoint redirectOffset;
#ifndef QT_NO_ACTION
    QList<QAction*> actions;
#endif
#ifndef QT_NO_GESTURES
    QMap<Qt::GestureType, Qt::GestureFlags> gestureContext;
#endif

    // Bit fields.
    uint high_attributes[4]; // the low ones are in QWidget::widget_attributes
    QPalette::ColorRole fg_role : 8;
    QPalette::ColorRole bg_role : 8;
    uint dirtyOpaqueChildren : 1;
    uint isOpaque : 1;
    uint retainSizeWhenHiddenChanged : 1;
    uint inDirtyList : 1;
    uint isScrolled : 1;
    uint isMoved : 1;
    uint usesDoubleBufferedGLContext : 1;
    uint mustHaveWindowHandle : 1;
    uint renderToTexture : 1;
    uint textureChildSeen : 1;
#ifndef QT_NO_IM
    uint inheritsInputMethodHints : 1;
#endif
#ifndef QT_NO_OPENGL
    uint renderToTextureReallyDirty : 1;
    uint renderToTextureComposeActive : 1;
#endif
    uint childrenHiddenByWState : 1;
    uint childrenShownByExpose : 1;

    // *************************** Platform specific ************************************
#if defined(Q_OS_WIN)
    uint noPaintOnScreen : 1; // see qwidget.cpp ::paintEngine()
#elif defined(Q_OS_MAC)
    void macUpdateSizeAttribute();
#endif
    void setNetWmWindowTypes(bool skipIfMissing = false);

    bool stealKeyboardGrab(bool grab);
    bool stealMouseGrab(bool grab);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QWidgetPrivate::DrawWidgetFlags)

struct QWidgetPaintContext
{
    inline QWidgetPaintContext(QPaintDevice *d, const QRegion &r, const QPoint &o, QWidgetPrivate::DrawWidgetFlags f,
                               QPainter *p, QWidgetRepaintManager *rpm)
        : pdev(d), rgn(r), offset(o), flags(f), sharedPainter(p), repaintManager(rpm), painter(nullptr) {}

    QPaintDevice *pdev;
    QRegion rgn;
    QPoint offset;
    QWidgetPrivate::DrawWidgetFlags flags;
    QPainter *sharedPainter;
    QWidgetRepaintManager *repaintManager;
    QPainter *painter;
};

#if QT_CONFIG(graphicseffect)
class QWidgetEffectSourcePrivate : public QGraphicsEffectSourcePrivate
{
public:
    QWidgetEffectSourcePrivate(QWidget *widget)
        : QGraphicsEffectSourcePrivate(), m_widget(widget), context(nullptr), updateDueToGraphicsEffect(false)
    {}

    void detach() override
    { m_widget->d_func()->graphicsEffect = nullptr; }

    const QGraphicsItem *graphicsItem() const override
    { return nullptr; }

    const QWidget *widget() const override
    { return m_widget; }

    void update() override
    {
        updateDueToGraphicsEffect = true;
        m_widget->update();
        updateDueToGraphicsEffect = false;
    }

    bool isPixmap() const override
    { return false; }

    void effectBoundingRectChanged() override
    {
        // ### This function should take a rect parameter; then we can avoid
        // updating too much on the parent widget.
        if (QWidget *parent = m_widget->parentWidget())
            parent->update();
        else
            update();
    }

    const QStyleOption *styleOption() const override
    { return nullptr; }

    QRect deviceRect() const override
    { return m_widget->window()->rect(); }

    QRectF boundingRect(Qt::CoordinateSystem system) const override;
    void draw(QPainter *p) override;
    QPixmap pixmap(Qt::CoordinateSystem system, QPoint *offset,
                   QGraphicsEffect::PixmapPadMode mode) const override;

    QWidget *m_widget;
    QWidgetPaintContext *context;
    QTransform lastEffectTransform;
    bool updateDueToGraphicsEffect;
};
#endif // QT_CONFIG(graphicseffect)

inline QWExtra *QWidgetPrivate::extraData() const
{
    return extra.get();
}

inline QTLWExtra *QWidgetPrivate::topData() const
{
    const_cast<QWidgetPrivate *>(this)->createTLExtra();
    return extra->topextra.get();
}

inline QTLWExtra *QWidgetPrivate::maybeTopData() const
{
    return extra ? extra->topextra.get() : nullptr;
}

inline QPainter *QWidgetPrivate::sharedPainter() const
{
    Q_Q(const QWidget);
    QTLWExtra *x = q->window()->d_func()->maybeTopData();
    return x ? x->sharedPainter : nullptr;
}

inline void QWidgetPrivate::setSharedPainter(QPainter *painter)
{
    Q_Q(QWidget);
    QTLWExtra *x = q->window()->d_func()->topData();
    x->sharedPainter = painter;
}

inline bool QWidgetPrivate::pointInsideRectAndMask(const QPoint &p) const
{
    Q_Q(const QWidget);
    return q->rect().contains(p) && (!extra || !extra->hasMask || q->testAttribute(Qt::WA_MouseNoMask)
                                     || extra->mask.contains(p));
}

inline QWidgetRepaintManager *QWidgetPrivate::maybeRepaintManager() const
{
    Q_Q(const QWidget);
    QTLWExtra *x = q->window()->d_func()->maybeTopData();
    return x ? x->repaintManager.get() : nullptr;
}

QT_END_NAMESPACE

#endif // QWIDGET_P_H
