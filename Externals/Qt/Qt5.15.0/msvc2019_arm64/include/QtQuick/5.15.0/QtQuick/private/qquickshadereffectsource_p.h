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

#ifndef QQUICKSHADEREFFECTSOURCE_P_H
#define QQUICKSHADEREFFECTSOURCE_P_H

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

#include <QtQuick/private/qtquickglobal_p.h>

QT_REQUIRE_CONFIG(quick_shadereffect);

#include "qquickitem.h"
#include <QtQuick/qsgtextureprovider.h>
#include <private/qsgadaptationlayer_p.h>
#include <QtQuick/private/qsgcontext_p.h>
#include <private/qsgdefaultinternalimagenode_p.h>
#include <private/qquickitemchangelistener_p.h>

#include "qpointer.h"
#include "qsize.h"
#include "qrect.h"

QT_BEGIN_NAMESPACE

class QSGNode;
class UpdatePaintNodeData;
class QOpenGLFramebufferObject;
class QSGSimpleRectNode;

class QQuickShaderEffectSourceTextureProvider;

class Q_QUICK_PRIVATE_EXPORT QQuickShaderEffectSource : public QQuickItem, public QQuickItemChangeListener
{
    Q_OBJECT
    Q_PROPERTY(WrapMode wrapMode READ wrapMode WRITE setWrapMode NOTIFY wrapModeChanged)
    Q_PROPERTY(QQuickItem *sourceItem READ sourceItem WRITE setSourceItem NOTIFY sourceItemChanged)
    Q_PROPERTY(QRectF sourceRect READ sourceRect WRITE setSourceRect NOTIFY sourceRectChanged)
    Q_PROPERTY(QSize textureSize READ textureSize WRITE setTextureSize NOTIFY textureSizeChanged)
    Q_PROPERTY(Format format READ format WRITE setFormat NOTIFY formatChanged)
    Q_PROPERTY(bool live READ live WRITE setLive NOTIFY liveChanged)
    Q_PROPERTY(bool hideSource READ hideSource WRITE setHideSource NOTIFY hideSourceChanged)
    Q_PROPERTY(bool mipmap READ mipmap WRITE setMipmap NOTIFY mipmapChanged)
    Q_PROPERTY(bool recursive READ recursive WRITE setRecursive NOTIFY recursiveChanged)
    Q_PROPERTY(TextureMirroring textureMirroring READ textureMirroring WRITE setTextureMirroring NOTIFY textureMirroringChanged REVISION 6)
    Q_PROPERTY(int samples READ samples WRITE setSamples NOTIFY samplesChanged REVISION 9)
    QML_NAMED_ELEMENT(ShaderEffectSource)

public:
    enum WrapMode {
        ClampToEdge,
        RepeatHorizontally,
        RepeatVertically,
        Repeat
    };
    Q_ENUM(WrapMode)
    // Equivalents to GL_ALPHA and similar type constants.
    enum Format {
        Alpha = 0x1906,
        RGB = 0x1907,
        RGBA = 0x1908
    };
    Q_ENUM(Format)

    enum TextureMirroring {
        NoMirroring        = 0x00,
        MirrorHorizontally = 0x01,
        MirrorVertically   = 0x02
    };
    Q_ENUM(TextureMirroring)

    QQuickShaderEffectSource(QQuickItem *parent = nullptr);
    ~QQuickShaderEffectSource() override;

    WrapMode wrapMode() const;
    void setWrapMode(WrapMode mode);

    QQuickItem *sourceItem() const;
    void setSourceItem(QQuickItem *item);

    QRectF sourceRect() const;
    void setSourceRect(const QRectF &rect);

    QSize textureSize() const;
    void setTextureSize(const QSize &size);

    Format format() const;
    void setFormat(Format format);

    bool live() const;
    void setLive(bool live);

    bool hideSource() const;
    void setHideSource(bool hide);

    bool mipmap() const;
    void setMipmap(bool enabled);

    bool recursive() const;
    void setRecursive(bool enabled);

    TextureMirroring textureMirroring() const;
    void setTextureMirroring(TextureMirroring mirroring);

    bool isTextureProvider() const override { return true; }
    QSGTextureProvider *textureProvider() const override;

    Q_INVOKABLE void scheduleUpdate();

    int samples() const;
    void setSamples(int count);

Q_SIGNALS:
    void wrapModeChanged();
    void sourceItemChanged();
    void sourceRectChanged();
    void textureSizeChanged();
    void formatChanged();
    void liveChanged();
    void hideSourceChanged();
    void mipmapChanged();
    void recursiveChanged();
    void textureMirroringChanged();
    void samplesChanged();

    void scheduledUpdateCompleted();

private Q_SLOTS:
    void sourceItemDestroyed(QObject *item);
    void invalidateSceneGraph();

protected:
    void releaseResources() override;
    QSGNode *updatePaintNode(QSGNode *, UpdatePaintNodeData *) override;

    void itemGeometryChanged(QQuickItem *item, QQuickGeometryChange change, const QRectF &) override;
    void itemChange(ItemChange change, const ItemChangeData &value) override;

private:
    void ensureTexture();

    QQuickShaderEffectSourceTextureProvider *m_provider;
    QSGLayer *m_texture;
    WrapMode m_wrapMode;
    QQuickItem *m_sourceItem;
    QRectF m_sourceRect;
    QSize m_textureSize;
    Format m_format;
    int m_samples;
    uint m_live : 1;
    uint m_hideSource : 1;
    uint m_mipmap : 1;
    uint m_recursive : 1;
    uint m_grab : 1;
    uint m_textureMirroring : 2; // Stores TextureMirroring enum
};

QT_END_NAMESPACE

#endif // QQUICKSHADEREFFECTSOURCE_P_H
