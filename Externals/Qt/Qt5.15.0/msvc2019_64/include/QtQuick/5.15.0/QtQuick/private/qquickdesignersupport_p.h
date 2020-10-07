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

#ifndef DESIGNERSUPPORT_H
#define DESIGNERSUPPORT_H

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

#include <QtQuick/qtquickglobal.h>
#include <QtCore/QtGlobal>
#include <QtCore/QHash>
#include <QtCore/QRectF>

QT_BEGIN_NAMESPACE

class QQuickItem;
class QSGLayer;
class QImage;
class QTransform;
class QQmlContext;
class QQuickView;
class QObject;
class QQuickWindow;

class Q_QUICK_EXPORT QQuickDesignerSupport
{
public:
    typedef QByteArray PropertyName;
    typedef QList<PropertyName> PropertyNameList;
    typedef QByteArray TypeName;

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

        TransformUpdateMask     = TransformOrigin | Transform | BasicTransform | Position | Size | Window,
        ComplexTransformUpdateMask     = Transform | Window,
        ContentUpdateMask       = Size | Content | Smooth | Window,
        ChildrenUpdateMask      = ChildrenChanged | ChildrenStackingChanged | EffectReference | Window,
        AllMask                 = TransformUpdateMask | ContentUpdateMask | ChildrenUpdateMask
    };


    QQuickDesignerSupport();
    ~QQuickDesignerSupport();

    void refFromEffectItem(QQuickItem *referencedItem, bool hide = true);
    void derefFromEffectItem(QQuickItem *referencedItem, bool unhide = true);

    QImage renderImageForItem(QQuickItem *referencedItem, const QRectF &boundingRect, const QSize &imageSize);

    static bool isDirty(QQuickItem *referencedItem, DirtyType dirtyType);
    static void addDirty(QQuickItem *referencedItem, DirtyType dirtyType);
    static void resetDirty(QQuickItem *referencedItem);

    static QTransform windowTransform(QQuickItem *referencedItem);
    static QTransform parentTransform(QQuickItem *referencedItem);

    static bool isAnchoredTo(QQuickItem *fromItem, QQuickItem *toItem);
    static bool areChildrenAnchoredTo(QQuickItem *fromItem, QQuickItem *toItem);
    static bool hasAnchor(QQuickItem *item, const QString &name);
    static QQuickItem *anchorFillTargetItem(QQuickItem *item);
    static QQuickItem *anchorCenterInTargetItem(QQuickItem *item);
    static QPair<QString, QObject*> anchorLineTarget(QQuickItem *item, const QString &name, QQmlContext *context);
    static void resetAnchor(QQuickItem *item, const QString &name);
    static void emitComponentCompleteSignalForAttachedProperty(QObject *item);

    static QList<QObject*> statesForItem(QQuickItem *item);

    static bool isComponentComplete(QQuickItem *item);

    static int borderWidth(QQuickItem *item);

    static void refreshExpressions(QQmlContext *context);

    static void setRootItem(QQuickView *view, QQuickItem *item);

    static bool isValidWidth(QQuickItem *item);
    static bool isValidHeight(QQuickItem *item);

    static void updateDirtyNode(QQuickItem *item);

    static void activateDesignerWindowManager();
    static void activateDesignerMode();

    static void disableComponentComplete();
    static void enableComponentComplete();

    static void createOpenGLContext(QQuickWindow *window);

    static void polishItems(QQuickWindow *window);

private:
    QHash<QQuickItem*, QSGLayer*> m_itemTextureHash;
};

class Q_QUICK_EXPORT ComponentCompleteDisabler
{
public:
    ComponentCompleteDisabler();

    ~ComponentCompleteDisabler();
};

typedef QQuickDesignerSupport DesignerSupport;

QT_END_NAMESPACE

#endif // DESIGNERSUPPORT_H
