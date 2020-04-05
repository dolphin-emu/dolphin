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

#ifndef QSGMATERIALRHISHADER_P_H
#define QSGMATERIALRHISHADER_P_H

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
#include "qsgmaterialrhishader.h"
#include "qsgmaterial.h"
#include <QtGui/private/qrhi_p.h>
#include <QtGui/private/qshader_p.h>

QT_BEGIN_NAMESPACE

class QRhiSampler;

class Q_QUICK_PRIVATE_EXPORT QSGMaterialRhiShaderPrivate
{
public:
    Q_DECLARE_PUBLIC(QSGMaterialRhiShader)

    QSGMaterialRhiShaderPrivate(QSGMaterialRhiShader *q) : q_ptr(q) { }
    static QSGMaterialRhiShaderPrivate *get(QSGMaterialRhiShader *s) { return s->d_func(); }
    static const QSGMaterialRhiShaderPrivate *get(const QSGMaterialRhiShader *s) { return s->d_func(); }

    void clearCachedRendererData();
    void prepare(QShader::Variant vertexShaderVariant);

    QShader shader(QShader::Stage stage) const { return shaders[stage].shader; }

    static QShader loadShader(const QString &filename);

    QSGMaterialRhiShader *q_ptr;
    QHash<QShader::Stage, QString> shaderFileNames;
    QSGMaterialRhiShader::Flags flags;

    struct ShaderStageData {
        ShaderStageData() { } // so shader.isValid() == false
        ShaderStageData(const QShader &shader) : shader(shader) { }
        QShader shader;
        QShader::Variant shaderVariant = QShader::StandardShader;
        QVector<int> vertexInputLocations; // excluding rewriter-inserted ones
        int qt_order_attrib_location = -1; // rewriter-inserted
    };
    QHash<QShader::Stage, ShaderStageData> shaders;

    static const int MAX_SHADER_RESOURCE_BINDINGS = 32;

    int ubufBinding = -1;
    int ubufSize = 0;
    QRhiShaderResourceBinding::StageFlags ubufStages;
    QRhiShaderResourceBinding::StageFlags combinedImageSamplerBindings[MAX_SHADER_RESOURCE_BINDINGS];

    ShaderStageData *vertexShader = nullptr;
    ShaderStageData *fragmentShader = nullptr;

    QByteArray masterUniformData;

    QSGTexture *textureBindingTable[MAX_SHADER_RESOURCE_BINDINGS];
    QRhiSampler *samplerBindingTable[MAX_SHADER_RESOURCE_BINDINGS];
};

Q_DECLARE_TYPEINFO(QSGMaterialRhiShaderPrivate::ShaderStageData, Q_MOVABLE_TYPE);

QT_END_NAMESPACE

#endif // QSGMATERIALRHISHADER_P_H
