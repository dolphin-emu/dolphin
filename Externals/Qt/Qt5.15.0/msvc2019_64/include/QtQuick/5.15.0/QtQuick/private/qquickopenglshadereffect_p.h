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

#ifndef QQUICKOPENGLSHADEREFFECT_P_H
#define QQUICKOPENGLSHADEREFFECT_P_H

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

QT_REQUIRE_CONFIG(quick_shadereffect);

#include <QtQuick/qquickitem.h>

#include <QtQuick/qsgmaterial.h>
#include <private/qtquickglobal_p.h>
#include <private/qsgadaptationlayer_p.h>
#include <private/qquickopenglshadereffectnode_p.h>
#include "qquickshadereffect_p.h"
#include "qquickshadereffectmesh_p.h"

#include <QtCore/qpointer.h>
#include <functional>

QT_BEGIN_NAMESPACE

class QSGContext;
class QFileSelector;
class QQuickOpenGLCustomMaterialShader;

namespace QtPrivate {
class MappedSlotObject;
}

// Common class for QQuickOpenGLShaderEffect and QQuickCustomParticle.
struct Q_QUICK_PRIVATE_EXPORT QQuickOpenGLShaderEffectCommon
{
    typedef QQuickOpenGLShaderEffectMaterialKey Key;
    typedef QQuickOpenGLShaderEffectMaterial::UniformData UniformData;

    QQuickOpenGLShaderEffectCommon(QObject *host, std::function<void(int)> mappedPropertyChanged)
        : host(host), mappedPropertyChanged(mappedPropertyChanged), fileSelector(nullptr)
    { }

    ~QQuickOpenGLShaderEffectCommon();

    void disconnectPropertySignals(QQuickItem *item, Key::ShaderType shaderType);
    void connectPropertySignals(QQuickItem *item, const QMetaObject *itemMetaObject, Key::ShaderType shaderType);
    void updateParseLog(bool ignoreAttributes);
    void lookThroughShaderCode(QQuickItem *item, const QMetaObject *itemMetaObject, Key::ShaderType shaderType, const QByteArray &code);
    void updateShader(QQuickItem *item, const QMetaObject *itemMetaObject, Key::ShaderType shaderType);
    void updateMaterial(QQuickOpenGLShaderEffectNode *node, QQuickOpenGLShaderEffectMaterial *material,
                        bool updateUniforms, bool updateUniformValues, bool updateTextureProviders);
    void updateWindow(QQuickWindow *window);

    // Called by slots in QQuickOpenGLShaderEffect:
    void sourceDestroyed(QObject *object);
    void propertyChanged(QQuickItem *item, const QMetaObject *itemMetaObject, int mappedId, bool *textureProviderChanged);

    void clearSignalMappers(int shader);

    QObject *host;
    std::function<void(int)> mappedPropertyChanged;
    Key source;
    QVector<QByteArray> attributes;
    QVector<UniformData> uniformData[Key::ShaderTypeCount];
    QVector<QtPrivate::MappedSlotObject *> signalMappers[Key::ShaderTypeCount];
    QString parseLog;
    QFileSelector *fileSelector;
};


class Q_QUICK_PRIVATE_EXPORT QQuickOpenGLShaderEffect : public QObject
{
    Q_OBJECT

public:
    QQuickOpenGLShaderEffect(QQuickShaderEffect *item, QObject *parent = nullptr);
    ~QQuickOpenGLShaderEffect() override;

    QByteArray fragmentShader() const { return m_common.source.sourceCode[Key::FragmentShader]; }
    void setFragmentShader(const QByteArray &code);

    QByteArray vertexShader() const { return m_common.source.sourceCode[Key::VertexShader]; }
    void setVertexShader(const QByteArray &code);

    bool blending() const { return m_blending; }
    void setBlending(bool enable);

    QVariant mesh() const;
    void setMesh(const QVariant &mesh);

    QQuickShaderEffect::CullMode cullMode() const { return m_cullMode; }
    void setCullMode(QQuickShaderEffect::CullMode face);

    QString log() const { return m_log; }
    QQuickShaderEffect::Status status() const { return m_status; }

    bool supportsAtlasTextures() const { return m_supportsAtlasTextures; }
    void setSupportsAtlasTextures(bool supports);

    QString parseLog();

    void handleEvent(QEvent *);
    void handleGeometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry);
    QSGNode *handleUpdatePaintNode(QSGNode *, QQuickItem::UpdatePaintNodeData *);
    void handleItemChange(QQuickItem::ItemChange change, const QQuickItem::ItemChangeData &value);
    void maybeUpdateShaders(bool force = false);

private Q_SLOTS:
    void updateGeometry();
    void updateGeometryIfAtlased();
    void updateLogAndStatus(const QString &log, int status);
    void sourceDestroyed(QObject *object);

private:
    void propertyChanged(int mappedId);

    friend class QQuickCustomMaterialShader;
    friend class QQuickOpenGLShaderEffectNode;

    typedef QQuickOpenGLShaderEffectMaterialKey Key;
    typedef QQuickOpenGLShaderEffectMaterial::UniformData UniformData;

    QQuickShaderEffect *m_item;
    const QMetaObject *m_itemMetaObject;
    QSize m_meshResolution;
    QQuickShaderEffectMesh *m_mesh;
    QQuickGridMesh m_defaultMesh;
    QQuickShaderEffect::CullMode m_cullMode;
    QString m_log;
    QQuickShaderEffect::Status m_status;

    QQuickOpenGLShaderEffectCommon m_common;

    uint m_blending : 1;
    uint m_dirtyUniforms : 1;
    uint m_dirtyUniformValues : 1;
    uint m_dirtyTextureProviders : 1;
    uint m_dirtyProgram : 1;
    uint m_dirtyParseLog : 1;
    uint m_dirtyMesh : 1;
    uint m_dirtyGeometry : 1;
    uint m_customVertexShader : 1;
    uint m_supportsAtlasTextures : 1;
    uint m_vertNeedsUpdate : 1;
    uint m_fragNeedsUpdate : 1;
};

QT_END_NAMESPACE

#endif // QQUICKOPENGLSHADEREFFECT_P_H
