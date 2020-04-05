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

#ifndef QQUICKGENERICSHADEREFFECT_P_H
#define QQUICKGENERICSHADEREFFECT_P_H

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

#include <QtQuick/qquickitem.h>
#include <private/qtquickglobal_p.h>
#include <private/qsgadaptationlayer_p.h>
#include "qquickshadereffect_p.h"
#include "qquickshadereffectmesh_p.h"

QT_BEGIN_NAMESPACE

class Q_QUICK_PRIVATE_EXPORT QQuickGenericShaderEffect : public QObject
{
    Q_OBJECT

public:
    QQuickGenericShaderEffect(QQuickShaderEffect *item, QObject *parent = nullptr);
    ~QQuickGenericShaderEffect();

    QByteArray fragmentShader() const { return m_fragShader; }
    void setFragmentShader(const QByteArray &src);

    QByteArray vertexShader() const { return m_vertShader; }
    void setVertexShader(const QByteArray &src);

    bool blending() const { return m_blending; }
    void setBlending(bool enable);

    QVariant mesh() const;
    void setMesh(const QVariant &mesh);

    QQuickShaderEffect::CullMode cullMode() const { return m_cullMode; }
    void setCullMode(QQuickShaderEffect::CullMode face);

    QString log() const;
    QQuickShaderEffect::Status status() const;

    bool supportsAtlasTextures() const { return m_supportsAtlasTextures; }
    void setSupportsAtlasTextures(bool supports);

    QString parseLog();

    void handleEvent(QEvent *);
    void handleGeometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry);
    QSGNode *handleUpdatePaintNode(QSGNode *, QQuickItem::UpdatePaintNodeData *);
    void handleComponentComplete();
    void handleItemChange(QQuickItem::ItemChange change, const QQuickItem::ItemChangeData &value);
    void maybeUpdateShaders();

private slots:
    void propertyChanged(int mappedId);
    void sourceDestroyed(QObject *object);
    void markGeometryDirtyAndUpdate();
    void markGeometryDirtyAndUpdateIfSupportsAtlas();
    void shaderCodePrepared(bool ok, QSGGuiThreadShaderEffectManager::ShaderInfo::Type typeHint,
                            const QByteArray &src, QSGGuiThreadShaderEffectManager::ShaderInfo *result);

private:
    QSGGuiThreadShaderEffectManager *shaderEffectManager() const;

    enum Shader {
        Vertex,
        Fragment,

        NShader
    };
    bool updateShader(Shader shaderType, const QByteArray &src);
    void updateShaderVars(Shader shaderType);
    void disconnectSignals(Shader shaderType);
    bool sourceIsUnique(QQuickItem *source, Shader typeToSkip, int indexToSkip) const;

    QQuickShaderEffect *m_item;
    QSize m_meshResolution;
    QQuickShaderEffectMesh *m_mesh;
    QQuickGridMesh m_defaultMesh;
    QQuickShaderEffect::CullMode m_cullMode;
    bool m_blending;
    bool m_supportsAtlasTextures;
    mutable QSGGuiThreadShaderEffectManager *m_mgr;
    QByteArray m_fragShader;
    bool m_fragNeedsUpdate;
    QByteArray m_vertShader;
    bool m_vertNeedsUpdate;

    QSGShaderEffectNode::ShaderData m_shaders[NShader];
    QSGShaderEffectNode::DirtyShaderFlags m_dirty;
    QSet<int> m_dirtyConstants[NShader];
    QSet<int> m_dirtyTextures[NShader];
    QSGGuiThreadShaderEffectManager::ShaderInfo *m_inProgress[NShader];

    struct SignalMapper {
        SignalMapper() : mapper(nullptr), active(false) { }
        QObject *mapper;
        bool active;
    };
    QVector<SignalMapper> m_signalMappers[NShader];
};

QT_END_NAMESPACE

#endif // QQUICKGENERICSHADEREFFECT_P_H
