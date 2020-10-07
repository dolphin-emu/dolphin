/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
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

#ifndef QSGRHISHADEREFFECTNODE_P_H
#define QSGRHISHADEREFFECTNODE_P_H

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

#include <private/qsgadaptationlayer_p.h>
#include <qsgmaterial.h>

QT_BEGIN_NAMESPACE

class QSGDefaultRenderContext;
class QSGPlainTexture;
class QSGRhiShaderEffectNode;
class QSGRhiGuiThreadShaderEffectManager;
class QFileSelector;

class QSGRhiShaderLinker
{
public:
    void reset(const QShader &vs, const QShader &fs);

    void feedConstants(const QSGShaderEffectNode::ShaderData &shader, const QSet<int> *dirtyIndices = nullptr);
    void feedSamplers(const QSGShaderEffectNode::ShaderData &shader, const QSet<int> *dirtyIndices = nullptr);
    void linkTextureSubRects();

    void dump();

    struct Constant {
        uint size;
        QSGShaderEffectNode::VariableData::SpecialType specialType;
        QVariant value;
        bool operator==(const Constant &other) const {
            return size == other.size && specialType == other.specialType
                    && (specialType == QSGShaderEffectNode::VariableData::None ? value == other.value : true);
        }
    };

    bool m_error;
    QShader m_vs;
    QShader m_fs;
    uint m_constantBufferSize;
    QHash<uint, Constant> m_constants; // offset -> Constant
    QHash<int, QVariant> m_samplers; // binding -> value (source ref)
    QHash<QByteArray, int> m_samplerNameMap; // name -> binding
};

QDebug operator<<(QDebug debug, const QSGRhiShaderLinker::Constant &c);

class QSGRhiShaderEffectMaterial : public QSGMaterial
{
public:
    QSGRhiShaderEffectMaterial(QSGRhiShaderEffectNode *node);
    ~QSGRhiShaderEffectMaterial();

    int compare(const QSGMaterial *other) const override;
    QSGMaterialType *type() const override;
    QSGMaterialShader *createShader() const override;

    void updateTextureProviders(bool layoutChange);

    static const int MAX_BINDINGS = 32;

    QSGRhiShaderEffectNode *m_node;
    QSGMaterialType *m_materialType = nullptr;
    QSGRhiShaderLinker m_linker;
    QVector<QSGTextureProvider *> m_textureProviders; // [binding] = QSGTextureProvider
    bool m_geometryUsesTextureSubRect = false;
    QSGShaderEffectNode::CullMode m_cullMode = QSGShaderEffectNode::NoCulling;
    bool m_hasCustomVertexShader = false;
    bool m_hasCustomFragmentShader = false;
    QShader m_vertexShader;
    QShader m_fragmentShader;
    QSGPlainTexture *m_dummyTexture = nullptr;
};

class QSGRhiShaderEffectNode : public QObject, public QSGShaderEffectNode
{
    Q_OBJECT

public:
    QSGRhiShaderEffectNode(QSGDefaultRenderContext *rc, QSGRhiGuiThreadShaderEffectManager *mgr);

    QRectF updateNormalizedTextureSubRect(bool supportsAtlasTextures) override;
    void syncMaterial(SyncData *syncData) override;
    void preprocess() override;

    static void cleanupMaterialTypeCache();

private Q_SLOTS:
    void handleTextureChange();
    void handleTextureProviderDestroyed(QObject *object);

private:
    QSGDefaultRenderContext *m_rc;
    QSGRhiGuiThreadShaderEffectManager *m_mgr;
    QSGRhiShaderEffectMaterial m_material;
};

class QSGRhiGuiThreadShaderEffectManager : public QSGGuiThreadShaderEffectManager
{
public:
    bool hasSeparateSamplerAndTextureObjects() const override;
    QString log() const override;
    Status status() const override;
    void prepareShaderCode(ShaderInfo::Type typeHint, const QByteArray &src, ShaderInfo *result) override;

private:
    bool reflect(ShaderInfo *result);
    Status m_status = Uncompiled;
    QFileSelector *m_fileSelector = nullptr;
};

QT_END_NAMESPACE

#endif // QSGRHISHADEREFFECTNODE_P_H
