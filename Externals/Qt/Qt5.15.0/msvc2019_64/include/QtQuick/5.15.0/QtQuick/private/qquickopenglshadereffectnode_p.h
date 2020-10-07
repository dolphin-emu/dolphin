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

#ifndef QQUICKOPENGLSHADEREFFECTNODE_P_H
#define QQUICKOPENGLSHADEREFFECTNODE_P_H

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

#include <QtQuick/qsgnode.h>
#include <QtQuick/qsgmaterial.h>
#include <QtQuick/qsgtextureprovider.h>
#include <QtQuick/qquickitem.h>
#include <private/qtquickglobal_p.h>
#include <private/qquickshadereffect_p.h>

#include <QtCore/qsharedpointer.h>
#include <QtCore/qpointer.h>

QT_BEGIN_NAMESPACE

struct QQuickOpenGLShaderEffectMaterialKey {
    enum ShaderType
    {
        VertexShader,
        FragmentShader,
        ShaderTypeCount
    };

    QByteArray sourceCode[ShaderTypeCount];

    bool operator == (const QQuickOpenGLShaderEffectMaterialKey &other) const;
    bool operator != (const QQuickOpenGLShaderEffectMaterialKey &other) const;
};

uint qHash(const QQuickOpenGLShaderEffectMaterialKey &key);

class QQuickCustomMaterialShader;
class QQuickOpenGLShaderEffectNode;
class Q_QUICK_PRIVATE_EXPORT QQuickOpenGLShaderEffectMaterial : public QSGMaterial
{
public:
    struct UniformData
    {
        enum SpecialType { None, Sampler, SamplerExternal, SubRect, Opacity, Matrix };

        QByteArray name;
        QVariant value;
        int propertyIndex = -1;
        SpecialType specialType;

        bool operator == (const UniformData &other) const;

        void setValueFromProperty(QObject *item, const QMetaObject *itemMetaObject)
        {
            if (propertyIndex == -1) {
                value = item->property(name);
            } else {
                value = itemMetaObject->property(propertyIndex).read(item);
            }
        }
    };

    explicit QQuickOpenGLShaderEffectMaterial(QQuickOpenGLShaderEffectNode *node = nullptr);
    QSGMaterialType *type() const override;
    QSGMaterialShader *createShader() const override;
    int compare(const QSGMaterial *other) const override;

    QVector<QByteArray> attributes;
    QVector<UniformData> uniforms[QQuickOpenGLShaderEffectMaterialKey::ShaderTypeCount];
    QVector<QSGTextureProvider *> textureProviders;
    QQuickShaderEffect::CullMode cullMode;
    bool geometryUsesTextureSubRect;

    void setProgramSource(const QQuickOpenGLShaderEffectMaterialKey &source);
    void updateTextures() const;
    void invalidateTextureProvider(const QObject *provider);

    static void cleanupMaterialCache();

protected:
    friend class QQuickCustomMaterialShader;

    // Each material needs a unique type to ensure that the renderer has a one
    // and exactly one GL program for every unique set of shader sources.
    // setProgramSource() stores the sources in a cache along with the right
    // type. The type is cleaned up in cleanupMaterialCache() which is called
    // when the GL context is shut down.
    QSGMaterialType *m_type;
    QQuickOpenGLShaderEffectMaterialKey m_source;

    QQuickOpenGLShaderEffectNode *m_node;
    bool m_emittedLogChanged;
};


class QSGShaderEffectMesh;

class Q_QUICK_PRIVATE_EXPORT QQuickOpenGLShaderEffectNode : public QObject, public QSGGeometryNode
{
    Q_OBJECT
public:
    QQuickOpenGLShaderEffectNode();
    ~QQuickOpenGLShaderEffectNode() override;

    void preprocess() override;

Q_SIGNALS:
    void logAndStatusChanged(const QString &, int status);
    void dirtyTexture();

private Q_SLOTS:
    void markDirtyTexture();
    void textureProviderDestroyed(QObject *object);
};

QT_END_NAMESPACE

#endif // QQUICKOPENGLSHADEREFFECTNODE_P_H
