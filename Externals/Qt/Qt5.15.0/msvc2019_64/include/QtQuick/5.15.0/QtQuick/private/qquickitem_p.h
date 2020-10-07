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

#ifndef QQUICKITEM_P_H
#define QQUICKITEM_P_H

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

#include "qquickitem.h"

#include "qquickanchors_p.h"
#include "qquickanchors_p_p.h"
#include "qquickitemchangelistener_p.h"
#include "qquickevents_p_p.h"

#include "qquickwindow_p.h"

#include <QtQuick/qsgnode.h>
#include "qquickclipnode_p.h"

#include <QtQuick/private/qquickstate_p.h>
#include <private/qqmlnullablevalue_p.h>
#include <private/qqmlnotifier_p.h>
#include <private/qqmlglobal_p.h>
#include <private/qlazilyallocated_p.h>

#include <qqml.h>
#include <qqmlcontext.h>

#include <QtCore/qlist.h>
#include <QtCore/qdebug.h>
#include <QtCore/qelapsedtimer.h>
#include <QtCore/qpointer.h>

#if QT_CONFIG(quick_shadereffect)
#include <QtQuick/private/qquickshadereffectsource_p.h>
#endif

QT_BEGIN_NAMESPACE

class QNetworkReply;
class QQuickItemKeyFilter;
class QQuickLayoutMirroringAttached;
class QQuickEnterKeyAttached;
class QQuickScreenAttached;
class QQuickPointerHandler;

class QQuickContents : public QQuickItemChangeListener
{
public:
    QQuickContents(QQuickItem *item);
    ~QQuickContents() override;

    QRectF rectF() const { return m_contents; }

    inline void calcGeometry(QQuickItem *changed = nullptr);
    void complete();

protected:
    void itemGeometryChanged(QQuickItem *item, QQuickGeometryChange change, const QRectF &) override;
    void itemDestroyed(QQuickItem *item) override;
    void itemChildAdded(QQuickItem *, QQuickItem *) override;
    void itemChildRemoved(QQuickItem *, QQuickItem *) override;
    //void itemVisibilityChanged(QQuickItem *item)

private:
    bool calcHeight(QQuickItem *changed = nullptr);
    bool calcWidth(QQuickItem *changed = nullptr);
    void updateRect();

    QQuickItem *m_item;
    QRectF m_contents;
};

void QQuickContents::calcGeometry(QQuickItem *changed)
{
    bool wChanged = calcWidth(changed);
    bool hChanged = calcHeight(changed);
    if (wChanged || hChanged)
        updateRect();
}

class QQuickTransformPrivate : public QObjectPrivate
{
    Q_DECLARE_PUBLIC(QQuickTransform)
public:
    static QQuickTransformPrivate* get(QQuickTransform *transform) { return transform->d_func(); }

    QQuickTransformPrivate();

    QList<QQuickItem *> items;
};

#if QT_CONFIG(quick_shadereffect)

class QQuickItemLayer : public QObject, public QQuickItemChangeListener
{
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(QSize textureSize READ size WRITE setSize NOTIFY sizeChanged)
    Q_PROPERTY(QRectF sourceRect READ sourceRect WRITE setSourceRect NOTIFY sourceRectChanged)
    Q_PROPERTY(bool mipmap READ mipmap WRITE setMipmap NOTIFY mipmapChanged)
    Q_PROPERTY(bool smooth READ smooth WRITE setSmooth NOTIFY smoothChanged)
    Q_PROPERTY(QQuickShaderEffectSource::WrapMode wrapMode READ wrapMode WRITE setWrapMode NOTIFY wrapModeChanged)
    Q_PROPERTY(QQuickShaderEffectSource::Format format READ format WRITE setFormat NOTIFY formatChanged)
    Q_PROPERTY(QByteArray samplerName READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(QQmlComponent *effect READ effect WRITE setEffect NOTIFY effectChanged)
    Q_PROPERTY(QQuickShaderEffectSource::TextureMirroring textureMirroring READ textureMirroring WRITE setTextureMirroring NOTIFY textureMirroringChanged)
    Q_PROPERTY(int samples READ samples WRITE setSamples NOTIFY samplesChanged)
    QML_ANONYMOUS

public:
    QQuickItemLayer(QQuickItem *item);
    ~QQuickItemLayer() override;

    void classBegin();
    void componentComplete();

    bool enabled() const { return m_enabled; }
    void setEnabled(bool enabled);

    bool mipmap() const { return m_mipmap; }
    void setMipmap(bool mipmap);

    bool smooth() const { return m_smooth; }
    void setSmooth(bool s);

    QSize size() const { return m_size; }
    void setSize(const QSize &size);

    QQuickShaderEffectSource::Format format() const { return m_format; }
    void setFormat(QQuickShaderEffectSource::Format f);

    QRectF sourceRect() const { return m_sourceRect; }
    void setSourceRect(const QRectF &sourceRect);

    QQuickShaderEffectSource::WrapMode wrapMode() const { return m_wrapMode; }
    void setWrapMode(QQuickShaderEffectSource::WrapMode mode);

    QByteArray name() const { return m_name; }
    void setName(const QByteArray &name);

    QQmlComponent *effect() const { return m_effectComponent; }
    void setEffect(QQmlComponent *effect);

    QQuickShaderEffectSource::TextureMirroring textureMirroring() const { return m_textureMirroring; }
    void setTextureMirroring(QQuickShaderEffectSource::TextureMirroring mirroring);

    int samples() const { return m_samples; }
    void setSamples(int count);

    QQuickShaderEffectSource *effectSource() const { return m_effectSource; }

    void itemGeometryChanged(QQuickItem *, QQuickGeometryChange, const QRectF &) override;
    void itemOpacityChanged(QQuickItem *) override;
    void itemParentChanged(QQuickItem *, QQuickItem *) override;
    void itemSiblingOrderChanged(QQuickItem *) override;
    void itemVisibilityChanged(QQuickItem *) override;

    void updateMatrix();
    void updateGeometry();
    void updateOpacity();
    void updateZ();

Q_SIGNALS:
    void enabledChanged(bool enabled);
    void sizeChanged(const QSize &size);
    void mipmapChanged(bool mipmap);
    void wrapModeChanged(QQuickShaderEffectSource::WrapMode mode);
    void nameChanged(const QByteArray &name);
    void effectChanged(QQmlComponent *component);
    void smoothChanged(bool smooth);
    void formatChanged(QQuickShaderEffectSource::Format format);
    void sourceRectChanged(const QRectF &sourceRect);
    void textureMirroringChanged(QQuickShaderEffectSource::TextureMirroring mirroring);
    void samplesChanged(int count);

private:
    friend class QQuickTransformAnimatorJob;
    friend class QQuickOpacityAnimatorJob;

    void activate();
    void deactivate();
    void activateEffect();
    void deactivateEffect();

    QQuickItem *m_item;
    bool m_enabled;
    bool m_mipmap;
    bool m_smooth;
    bool m_componentComplete;
    QQuickShaderEffectSource::WrapMode m_wrapMode;
    QQuickShaderEffectSource::Format m_format;
    QSize m_size;
    QRectF m_sourceRect;
    QByteArray m_name;
    QQmlComponent *m_effectComponent;
    QQuickItem *m_effect;
    QQuickShaderEffectSource *m_effectSource;
    QQuickShaderEffectSource::TextureMirroring m_textureMirroring;
    int m_samples;
};

#endif

class Q_QUICK_PRIVATE_EXPORT QQuickItemPrivate : public QObjectPrivate
{
    Q_DECLARE_PUBLIC(QQuickItem)

public:
    static QQuickItemPrivate* get(QQuickItem *item) { return item->d_func(); }
    static const QQuickItemPrivate* get(const QQuickItem *item) { return item->d_func(); }

    QQuickItemPrivate();
    ~QQuickItemPrivate() override;
    void init(QQuickItem *parent);

    QQmlListProperty<QObject> data();
    QQmlListProperty<QObject> resources();
    QQmlListProperty<QQuickItem> children();
    QQmlListProperty<QQuickItem> visibleChildren();

    QQmlListProperty<QQuickState> states();
    QQmlListProperty<QQuickTransition> transitions();

    QString state() const;
    void setState(const QString &);

    QQuickAnchorLine left() const;
    QQuickAnchorLine right() const;
    QQuickAnchorLine horizontalCenter() const;
    QQuickAnchorLine top() const;
    QQuickAnchorLine bottom() const;
    QQuickAnchorLine verticalCenter() const;
    QQuickAnchorLine baseline() const;

    QQuickItemLayer *layer() const;

    bool hasPointerHandlers() const;
    bool hasHoverHandlers() const;
    virtual void addPointerHandler(QQuickPointerHandler *h);

    // data property
    static void data_append(QQmlListProperty<QObject> *, QObject *);
    static int data_count(QQmlListProperty<QObject> *);
    static QObject *data_at(QQmlListProperty<QObject> *, int);
    static void data_clear(QQmlListProperty<QObject> *);

    // resources property
    static QObject *resources_at(QQmlListProperty<QObject> *, int);
    static void resources_append(QQmlListProperty<QObject> *, QObject *);
    static int resources_count(QQmlListProperty<QObject> *);
    static void resources_clear(QQmlListProperty<QObject> *);

    // children property
    static void children_append(QQmlListProperty<QQuickItem> *, QQuickItem *);
    static int children_count(QQmlListProperty<QQuickItem> *);
    static QQuickItem *children_at(QQmlListProperty<QQuickItem> *, int);
    static void children_clear(QQmlListProperty<QQuickItem> *);

    // visibleChildren property
    static void visibleChildren_append(QQmlListProperty<QQuickItem> *prop, QQuickItem *o);
    static int visibleChildren_count(QQmlListProperty<QQuickItem> *prop);
    static QQuickItem *visibleChildren_at(QQmlListProperty<QQuickItem> *prop, int index);

    // transform property
    static int transform_count(QQmlListProperty<QQuickTransform> *list);
    static void transform_append(QQmlListProperty<QQuickTransform> *list, QQuickTransform *);
    static QQuickTransform *transform_at(QQmlListProperty<QQuickTransform> *list, int);
    static void transform_clear(QQmlListProperty<QQuickTransform> *list);

    void _q_resourceObjectDeleted(QObject *);
    quint64 _q_createJSWrapper(QV4::ExecutionEngine *engine);

    enum ChangeType {
        Geometry = 0x01,
        SiblingOrder = 0x02,
        Visibility = 0x04,
        Opacity = 0x08,
        Destroyed = 0x10,
        Parent = 0x20,
        Children = 0x40,
        Rotation = 0x80,
        ImplicitWidth = 0x100,
        ImplicitHeight = 0x200,
        Enabled = 0x400,
    };

    Q_DECLARE_FLAGS(ChangeTypes, ChangeType)

    struct ChangeListener {
        using ChangeTypes = QQuickItemPrivate::ChangeTypes;

        ChangeListener(QQuickItemChangeListener *l = nullptr, ChangeTypes t = { })
            : listener(l)
            , types(t)
            , gTypes(QQuickGeometryChange::All)
        {}

        ChangeListener(QQuickItemChangeListener *l, QQuickGeometryChange gt)
            : listener(l)
            , types(Geometry)
            , gTypes(gt)
        {}

        bool operator==(const ChangeListener &other) const
        { return listener == other.listener && types == other.types; }

        QQuickItemChangeListener *listener;
        ChangeTypes types;
        QQuickGeometryChange gTypes;  //NOTE: not used for ==
    };

    struct ExtraData {
        ExtraData();

        qreal z;
        qreal scale;
        qreal rotation;
        qreal opacity;

        QQuickContents *contents;
        QQuickScreenAttached *screenAttached;
        QQuickLayoutMirroringAttached* layoutDirectionAttached;
        QQuickEnterKeyAttached *enterKeyAttached;
        QQuickItemKeyFilter *keyHandler;
        QVector<QQuickPointerHandler *> pointerHandlers;
#if QT_CONFIG(quick_shadereffect)
        mutable QQuickItemLayer *layer;
#endif
#if QT_CONFIG(cursor)
        QCursor cursor;
#endif
        QPointF userTransformOriginPoint;

        // these do not include child items
        int effectRefCount;
        int hideRefCount;
        // updated recursively for child items as well
        int recursiveEffectRefCount;

        QSGOpacityNode *opacityNode;
        QQuickDefaultClipNode *clipNode;
        QSGRootNode *rootNode;

        // Mask contains() method
        QMetaMethod maskContains;

        QObjectList resourcesList;

        // Although acceptedMouseButtons is inside ExtraData, we actually store
        // the LeftButton flag in the extra.flag() bit.  This is because it is
        // extremely common to set acceptedMouseButtons to LeftButton, but very
        // rare to use any of the other buttons.
        Qt::MouseButtons acceptedMouseButtons;

        QQuickItem::TransformOrigin origin:5;
        uint transparentForPositioner : 1;

        // 26 bits padding
    };
    QLazilyAllocated<ExtraData> extra;
    // Contains mask
    QPointer<QObject> mask;
    // If the mask is an Item, inform it that it's being used as a mask (true) or is no longer being used (false)
    virtual void registerAsContainmentMask(QQuickItem * /* maskedItem */, bool /* set */) { }

    QQuickAnchors *anchors() const;
    mutable QQuickAnchors *_anchors;

    inline Qt::MouseButtons acceptedMouseButtons() const;

    QVector<QQuickItemPrivate::ChangeListener> changeListeners;

    void addItemChangeListener(QQuickItemChangeListener *listener, ChangeTypes types);
    void updateOrAddItemChangeListener(QQuickItemChangeListener *listener, ChangeTypes types);
    void removeItemChangeListener(QQuickItemChangeListener *, ChangeTypes types);
    void updateOrAddGeometryChangeListener(QQuickItemChangeListener *listener, QQuickGeometryChange types);
    void updateOrRemoveGeometryChangeListener(QQuickItemChangeListener *listener, QQuickGeometryChange types);

    QQuickStateGroup *_states();
    QQuickStateGroup *_stateGroup;

    inline QQuickItem::TransformOrigin origin() const;

    // Bit 0
    quint32 flags:5;
    bool widthValid:1;
    bool heightValid:1;
    bool componentComplete:1;
    bool keepMouse:1;
    bool keepTouch:1;
    bool hoverEnabled:1;
    bool smooth:1;
    bool antialiasing:1;
    bool focus:1;
    bool activeFocus:1;
    bool notifiedFocus:1;
    // Bit 16
    bool notifiedActiveFocus:1;
    bool filtersChildMouseEvents:1;
    bool explicitVisible:1;
    bool effectiveVisible:1;
    bool explicitEnable:1;
    bool effectiveEnable:1;
    bool polishScheduled:1;
    bool inheritedLayoutMirror:1;
    bool effectiveLayoutMirror:1;
    bool isMirrorImplicit:1;
    bool inheritMirrorFromParent:1;
    bool inheritMirrorFromItem:1;
    bool isAccessible:1;
    bool culled:1;
    bool hasCursor:1;
    bool subtreeCursorEnabled:1;
    // Bit 32
    bool subtreeHoverEnabled:1;
    bool activeFocusOnTab:1;
    bool implicitAntialiasing:1;
    bool antialiasingValid:1;
    // isTabFence: When true, the item acts as a fence within the tab focus chain.
    // This means that the item and its children will be skipped from the tab focus
    // chain when navigating from its parent or any of its siblings. Similarly,
    // when any of the item's descendants gets focus, the item constrains the tab
    // focus chain and prevents tabbing outside.
    bool isTabFence:1;
    bool replayingPressEvent:1;
    bool touchEnabled:1;
    bool hasCursorHandler:1;

    enum DirtyType {
        TransformOrigin         = 0x00000001,
        Transform               = 0x00000002,
        BasicTransform          = 0x00000004,
        Position                = 0x00000008,
        Size                    = 0x00000010,

        ZValue                  = 0x00000020,
        Content                 = 0x00000040,
        Smooth                  = 0x00000080,
        OpacityValue            = 0x00000100,
        ChildrenChanged         = 0x00000200,
        ChildrenStackingChanged = 0x00000400,
        ParentChanged           = 0x00000800,

        Clip                    = 0x00001000,
        Window                  = 0x00002000,

        EffectReference         = 0x00008000,
        Visible                 = 0x00010000,
        HideReference           = 0x00020000,
        Antialiasing             = 0x00040000,
        // When you add an attribute here, don't forget to update
        // dirtyToString()

        TransformUpdateMask     = TransformOrigin | Transform | BasicTransform | Position |
                                  Window,
        ComplexTransformUpdateMask     = Transform | Window,
        ContentUpdateMask       = Size | Content | Smooth | Window | Antialiasing,
        ChildrenUpdateMask      = ChildrenChanged | ChildrenStackingChanged | EffectReference | Window
    };

    quint32 dirtyAttributes;
    QString dirtyToString() const;
    void dirty(DirtyType);
    void addToDirtyList();
    void removeFromDirtyList();
    QQuickItem *nextDirtyItem;
    QQuickItem**prevDirtyItem;

    void setCulled(bool);

    QQuickWindow *window;
    int windowRefCount;
    inline QSGContext *sceneGraphContext() const;
    inline QSGRenderContext *sceneGraphRenderContext() const;

    QQuickItem *parentItem;

    QList<QQuickItem *> childItems;
    mutable QList<QQuickItem *> *sortedChildItems;
    QList<QQuickItem *> paintOrderChildItems() const;
    void addChild(QQuickItem *);
    void removeChild(QQuickItem *);
    void siblingOrderChanged();

    inline void markSortedChildrenDirty(QQuickItem *child);

    void refWindow(QQuickWindow *);
    void derefWindow();

    QPointer<QQuickItem> subFocusItem;
    void updateSubFocusItem(QQuickItem *scope, bool focus);

    QTransform windowToItemTransform() const;
    QTransform itemToWindowTransform() const;
    void itemToParentTransform(QTransform &) const;
    QTransform globalToWindowTransform() const;
    QTransform windowToGlobalTransform() const;

    static bool focusNextPrev(QQuickItem *item, bool forward);
    static QQuickItem *nextTabChildItem(const QQuickItem *item, int start);
    static QQuickItem *prevTabChildItem(const QQuickItem *item, int start);
    static QQuickItem *nextPrevItemInTabFocusChain(QQuickItem *item, bool forward);

    static bool canAcceptTabFocus(QQuickItem *item);

    qreal x;
    qreal y;
    qreal width;
    qreal height;
    qreal implicitWidth;
    qreal implicitHeight;

    qreal baselineOffset;

    QList<QQuickTransform *> transforms;

    inline qreal z() const { return extra.isAllocated()?extra->z:0; }
    inline qreal scale() const { return extra.isAllocated()?extra->scale:1; }
    inline qreal rotation() const { return extra.isAllocated()?extra->rotation:0; }
    inline qreal opacity() const { return extra.isAllocated()?extra->opacity:1; }

    void setAccessible();

    virtual qreal getImplicitWidth() const;
    virtual qreal getImplicitHeight() const;
    virtual void implicitWidthChanged();
    virtual void implicitHeightChanged();

#if QT_CONFIG(accessibility)
    virtual QAccessible::Role accessibleRole() const;
#endif

    void setImplicitAntialiasing(bool antialiasing);

    void resolveLayoutMirror();
    void setImplicitLayoutMirror(bool mirror, bool inherit);
    void setLayoutMirror(bool mirror);
    bool isMirrored() const {
        return effectiveLayoutMirror;
    }

    void emitChildrenRectChanged(const QRectF &rect) {
        Q_Q(QQuickItem);
        Q_EMIT q->childrenRectChanged(rect);
    }

    QPointF computeTransformOrigin() const;
    virtual void transformChanged();

    QPointF adjustedPosForTransform(const QPointF &centroid,
                                    const QPointF &startPos, const QVector2D &activeTranslatation,
                                    qreal startScale, qreal activeScale,
                                    qreal startRotation, qreal activeRotation);

    void deliverKeyEvent(QKeyEvent *);
    bool filterKeyEvent(QKeyEvent *, bool post);
#if QT_CONFIG(im)
    void deliverInputMethodEvent(QInputMethodEvent *);
#endif
    void deliverShortcutOverrideEvent(QKeyEvent *);

    bool anyPointerHandlerWants(QQuickEventPoint *point) const;
    virtual bool handlePointerEvent(QQuickPointerEvent *, bool avoidExclusiveGrabber = false);

    virtual void setVisible(bool visible);

    bool isTransparentForPositioner() const;
    void setTransparentForPositioner(bool trans);

    bool calcEffectiveVisible() const;
    bool setEffectiveVisibleRecur(bool);
    bool calcEffectiveEnable() const;
    void setEffectiveEnableRecur(QQuickItem *scope, bool);


    inline QSGTransformNode *itemNode();
    inline QSGNode *childContainerNode();

    /*
      QSGNode order is:
         - itemNode
         - (opacityNode)
         - (clipNode)
         - (rootNode) (shader effect source's root node)
     */

    QSGOpacityNode *opacityNode() const { return extra.isAllocated()?extra->opacityNode:nullptr; }
    QQuickDefaultClipNode *clipNode() const { return extra.isAllocated()?extra->clipNode:nullptr; }
    QSGRootNode *rootNode() const { return extra.isAllocated()?extra->rootNode:nullptr; }

    QSGTransformNode *itemNodeInstance;
    QSGNode *paintNode;

    virtual QSGTransformNode *createTransformNode();

    // A reference from an effect item means that this item is used by the effect, so
    // it should insert a root node.
    void refFromEffectItem(bool hide);
    void recursiveRefFromEffectItem(int refs);
    void derefFromEffectItem(bool unhide);

    void itemChange(QQuickItem::ItemChange, const QQuickItem::ItemChangeData &);

    virtual void mirrorChange() {}

    void setHasCursorInChild(bool hasCursor);
    void setHasHoverInChild(bool hasHover);
#if QT_CONFIG(cursor)
    QCursor effectiveCursor(const QQuickPointerHandler *handler) const;
    QQuickPointerHandler *effectiveCursorHandler() const;
#endif

    virtual void updatePolish() { }
};

/*
    Key filters can be installed on a QQuickItem, but not removed.  Currently they
    are only used by attached objects (which are only destroyed on Item
    destruction), so this isn't a problem.  If in future this becomes any form
    of public API, they will have to support removal too.
*/
class QQuickItemKeyFilter
{
public:
    QQuickItemKeyFilter(QQuickItem * = nullptr);
    virtual ~QQuickItemKeyFilter();

    virtual void keyPressed(QKeyEvent *event, bool post);
    virtual void keyReleased(QKeyEvent *event, bool post);
#if QT_CONFIG(im)
    virtual void inputMethodEvent(QInputMethodEvent *event, bool post);
    virtual QVariant inputMethodQuery(Qt::InputMethodQuery query) const;
#endif
    virtual void shortcutOverride(QKeyEvent *event);
    virtual void componentComplete();

    bool m_processPost;

private:
    QQuickItemKeyFilter *m_next;
};

class QQuickKeyNavigationAttachedPrivate : public QObjectPrivate
{
public:
    QQuickKeyNavigationAttachedPrivate()
        : leftSet(false), rightSet(false), upSet(false), downSet(false),
          tabSet(false), backtabSet(false) {}

    QPointer<QQuickItem> left;
    QPointer<QQuickItem> right;
    QPointer<QQuickItem> up;
    QPointer<QQuickItem> down;
    QPointer<QQuickItem> tab;
    QPointer<QQuickItem> backtab;
    bool leftSet : 1;
    bool rightSet : 1;
    bool upSet : 1;
    bool downSet : 1;
    bool tabSet : 1;
    bool backtabSet : 1;
};

class Q_QUICK_PRIVATE_EXPORT QQuickKeyNavigationAttached : public QObject, public QQuickItemKeyFilter
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QQuickKeyNavigationAttached)

    Q_PROPERTY(QQuickItem *left READ left WRITE setLeft NOTIFY leftChanged)
    Q_PROPERTY(QQuickItem *right READ right WRITE setRight NOTIFY rightChanged)
    Q_PROPERTY(QQuickItem *up READ up WRITE setUp NOTIFY upChanged)
    Q_PROPERTY(QQuickItem *down READ down WRITE setDown NOTIFY downChanged)
    Q_PROPERTY(QQuickItem *tab READ tab WRITE setTab NOTIFY tabChanged)
    Q_PROPERTY(QQuickItem *backtab READ backtab WRITE setBacktab NOTIFY backtabChanged)
    Q_PROPERTY(Priority priority READ priority WRITE setPriority NOTIFY priorityChanged)

    QML_NAMED_ELEMENT(KeyNavigation)
    QML_UNCREATABLE("KeyNavigation is only available via attached properties.")
    QML_ATTACHED(QQuickKeyNavigationAttached)

public:
    QQuickKeyNavigationAttached(QObject * = nullptr);

    QQuickItem *left() const;
    void setLeft(QQuickItem *);
    QQuickItem *right() const;
    void setRight(QQuickItem *);
    QQuickItem *up() const;
    void setUp(QQuickItem *);
    QQuickItem *down() const;
    void setDown(QQuickItem *);
    QQuickItem *tab() const;
    void setTab(QQuickItem *);
    QQuickItem *backtab() const;
    void setBacktab(QQuickItem *);

    enum Priority { BeforeItem, AfterItem };
    Q_ENUM(Priority)
    Priority priority() const;
    void setPriority(Priority);

    static QQuickKeyNavigationAttached *qmlAttachedProperties(QObject *);

Q_SIGNALS:
    void leftChanged();
    void rightChanged();
    void upChanged();
    void downChanged();
    void tabChanged();
    void backtabChanged();
    void priorityChanged();

private:
    void keyPressed(QKeyEvent *event, bool post) override;
    void keyReleased(QKeyEvent *event, bool post) override;
    void setFocusNavigation(QQuickItem *currentItem, const char *dir,
                            Qt::FocusReason reason = Qt::OtherFocusReason);
};

class QQuickLayoutMirroringAttached : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled RESET resetEnabled NOTIFY enabledChanged)
    Q_PROPERTY(bool childrenInherit READ childrenInherit WRITE setChildrenInherit NOTIFY childrenInheritChanged)

    QML_NAMED_ELEMENT(LayoutMirroring)
    QML_UNCREATABLE("LayoutMirroring is only available via attached properties.")
    QML_ATTACHED(QQuickLayoutMirroringAttached)

public:
    explicit QQuickLayoutMirroringAttached(QObject *parent = nullptr);

    bool enabled() const;
    void setEnabled(bool);
    void resetEnabled();

    bool childrenInherit() const;
    void setChildrenInherit(bool);

    static QQuickLayoutMirroringAttached *qmlAttachedProperties(QObject *);
Q_SIGNALS:
    void enabledChanged();
    void childrenInheritChanged();
private:
    friend class QQuickItemPrivate;
    QQuickItemPrivate *itemPrivate;
};

class QQuickEnterKeyAttached : public QObject
{
    Q_OBJECT
    Q_PROPERTY(Qt::EnterKeyType type READ type WRITE setType NOTIFY typeChanged)

    QML_NAMED_ELEMENT(EnterKey)
    QML_UNCREATABLE("EnterKey is only available via attached properties")
    QML_ADDED_IN_MINOR_VERSION(6)
    QML_ATTACHED(QQuickEnterKeyAttached)

public:
    explicit QQuickEnterKeyAttached(QObject *parent = nullptr);

    Qt::EnterKeyType type() const;
    void setType(Qt::EnterKeyType type);

    static QQuickEnterKeyAttached *qmlAttachedProperties(QObject *);
Q_SIGNALS:
    void typeChanged();
private:
    friend class QQuickItemPrivate;
    QQuickItemPrivate *itemPrivate;

    Qt::EnterKeyType keyType;
};

class QQuickKeysAttachedPrivate : public QObjectPrivate
{
public:
    QQuickKeysAttachedPrivate()
        : inPress(false), inRelease(false), inIM(false), enabled(true)
    {}

    //loop detection
    bool inPress:1;
    bool inRelease:1;
    bool inIM:1;

    bool enabled : 1;

    QQuickItem *imeItem = nullptr;
    QList<QQuickItem *> targets;
    QQuickItem *item = nullptr;
    QQuickKeyEvent theKeyEvent;
};

class QQuickKeysAttached : public QObject, public QQuickItemKeyFilter
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QQuickKeysAttached)

    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(QQmlListProperty<QQuickItem> forwardTo READ forwardTo)
    Q_PROPERTY(Priority priority READ priority WRITE setPriority NOTIFY priorityChanged)

    QML_NAMED_ELEMENT(Keys)
    QML_UNCREATABLE("Keys is only available via attached properties")
    QML_ATTACHED(QQuickKeysAttached)

public:
    QQuickKeysAttached(QObject *parent=nullptr);
    ~QQuickKeysAttached() override;

    bool enabled() const { Q_D(const QQuickKeysAttached); return d->enabled; }
    void setEnabled(bool enabled) {
        Q_D(QQuickKeysAttached);
        if (enabled != d->enabled) {
            d->enabled = enabled;
            Q_EMIT enabledChanged();
        }
    }

    enum Priority { BeforeItem, AfterItem};
    Q_ENUM(Priority)
    Priority priority() const;
    void setPriority(Priority);

    QQmlListProperty<QQuickItem> forwardTo() {
        Q_D(QQuickKeysAttached);
        return QQmlListProperty<QQuickItem>(this, &(d->targets));
    }

    void componentComplete() override;

    static QQuickKeysAttached *qmlAttachedProperties(QObject *);

Q_SIGNALS:
    void enabledChanged();
    void priorityChanged();
    void pressed(QQuickKeyEvent *event);
    void released(QQuickKeyEvent *event);
    void shortcutOverride(QQuickKeyEvent *event);
    void digit0Pressed(QQuickKeyEvent *event);
    void digit1Pressed(QQuickKeyEvent *event);
    void digit2Pressed(QQuickKeyEvent *event);
    void digit3Pressed(QQuickKeyEvent *event);
    void digit4Pressed(QQuickKeyEvent *event);
    void digit5Pressed(QQuickKeyEvent *event);
    void digit6Pressed(QQuickKeyEvent *event);
    void digit7Pressed(QQuickKeyEvent *event);
    void digit8Pressed(QQuickKeyEvent *event);
    void digit9Pressed(QQuickKeyEvent *event);

    void leftPressed(QQuickKeyEvent *event);
    void rightPressed(QQuickKeyEvent *event);
    void upPressed(QQuickKeyEvent *event);
    void downPressed(QQuickKeyEvent *event);
    void tabPressed(QQuickKeyEvent *event);
    void backtabPressed(QQuickKeyEvent *event);

    void asteriskPressed(QQuickKeyEvent *event);
    void numberSignPressed(QQuickKeyEvent *event);
    void escapePressed(QQuickKeyEvent *event);
    void returnPressed(QQuickKeyEvent *event);
    void enterPressed(QQuickKeyEvent *event);
    void deletePressed(QQuickKeyEvent *event);
    void spacePressed(QQuickKeyEvent *event);
    void backPressed(QQuickKeyEvent *event);
    void cancelPressed(QQuickKeyEvent *event);
    void selectPressed(QQuickKeyEvent *event);
    void yesPressed(QQuickKeyEvent *event);
    void noPressed(QQuickKeyEvent *event);
    void context1Pressed(QQuickKeyEvent *event);
    void context2Pressed(QQuickKeyEvent *event);
    void context3Pressed(QQuickKeyEvent *event);
    void context4Pressed(QQuickKeyEvent *event);
    void callPressed(QQuickKeyEvent *event);
    void hangupPressed(QQuickKeyEvent *event);
    void flipPressed(QQuickKeyEvent *event);
    void menuPressed(QQuickKeyEvent *event);
    void volumeUpPressed(QQuickKeyEvent *event);
    void volumeDownPressed(QQuickKeyEvent *event);

private:
    void keyPressed(QKeyEvent *event, bool post) override;
    void keyReleased(QKeyEvent *event, bool post) override;
#if QT_CONFIG(im)
    void inputMethodEvent(QInputMethodEvent *, bool post) override;
    QVariant inputMethodQuery(Qt::InputMethodQuery query) const override;
#endif
    void shortcutOverride(QKeyEvent *event) override;
    static QByteArray keyToSignal(int key);

    bool isConnected(const char *signalName) const;
};

Qt::MouseButtons QQuickItemPrivate::acceptedMouseButtons() const
{
    return ((extra.flag() ? Qt::LeftButton : Qt::MouseButton(0)) |
            (extra.isAllocated() ? extra->acceptedMouseButtons : Qt::MouseButtons{}));
}

QSGContext *QQuickItemPrivate::sceneGraphContext() const
{
    Q_ASSERT(window);
    return static_cast<QQuickWindowPrivate *>(QObjectPrivate::get(window))->context->sceneGraphContext();
}

QSGRenderContext *QQuickItemPrivate::sceneGraphRenderContext() const
{
    Q_ASSERT(window);
    return static_cast<QQuickWindowPrivate *>(QObjectPrivate::get(window))->context;
}

void QQuickItemPrivate::markSortedChildrenDirty(QQuickItem *child)
{
    // If sortedChildItems == &childItems then all in childItems have z == 0
    // and we don't need to invalidate if the changed item also has z == 0.
    if (child->z() != 0. || sortedChildItems != &childItems) {
        if (sortedChildItems != &childItems)
            delete sortedChildItems;
        sortedChildItems = nullptr;
    }
}

QQuickItem::TransformOrigin QQuickItemPrivate::origin() const
{
    return extra.isAllocated()?extra->origin:QQuickItem::Center;
}

QSGTransformNode *QQuickItemPrivate::itemNode()
{
    if (!itemNodeInstance) {
        itemNodeInstance = createTransformNode();
        itemNodeInstance->setFlag(QSGNode::OwnedByParent, false);
#ifdef QSG_RUNTIME_DESCRIPTION
        Q_Q(QQuickItem);
        qsgnode_set_description(itemNodeInstance, QString::fromLatin1("QQuickItem(%1:%2)").arg(QString::fromLatin1(q->metaObject()->className())).arg(q->objectName()));
#endif
    }
    return itemNodeInstance;
}

QSGNode *QQuickItemPrivate::childContainerNode()
{
    if (rootNode())
        return rootNode();
    else if (clipNode())
        return clipNode();
    else if (opacityNode())
        return opacityNode();
    else
        return itemNode();
}

Q_DECLARE_OPERATORS_FOR_FLAGS(QQuickItemPrivate::ChangeTypes)
Q_DECLARE_TYPEINFO(QQuickItemPrivate::ChangeListener, Q_PRIMITIVE_TYPE);

QT_END_NAMESPACE

#if QT_CONFIG(quick_shadereffect)
QML_DECLARE_TYPE(QQuickItemLayer)
#endif
QML_DECLARE_TYPE(QQuickKeysAttached)
QML_DECLARE_TYPE(QQuickKeyNavigationAttached)
QML_DECLARE_TYPE(QQuickLayoutMirroringAttached)
QML_DECLARE_TYPE(QQuickEnterKeyAttached)

#endif // QQUICKITEM_P_H
