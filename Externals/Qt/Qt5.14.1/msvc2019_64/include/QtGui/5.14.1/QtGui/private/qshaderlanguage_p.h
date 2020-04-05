/****************************************************************************
**
** Copyright (C) 2017 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtGui module of the Qt Toolkit.
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

#ifndef QSHADERLANGUAGE_P_H
#define QSHADERLANGUAGE_P_H

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

#include <QtGui/private/qtguiglobal_p.h>

#include <QtCore/qmetatype.h>

QT_BEGIN_NAMESPACE

namespace QShaderLanguage
{
    Q_NAMESPACE_EXPORT(Q_GUI_EXPORT)

    enum StorageQualifier : char {
        Const = 1,
        Input,
        BuiltIn,
        Output,
        Uniform
    };
    Q_ENUM_NS(StorageQualifier)

    enum VariableType : int {
        Bool = 1,
        Int,
        Uint,
        Float,
        Double,
        Vec2,
        Vec3,
        Vec4,
        DVec2,
        DVec3,
        DVec4,
        BVec2,
        BVec3,
        BVec4,
        IVec2,
        IVec3,
        IVec4,
        UVec2,
        UVec3,
        UVec4,
        Mat2,
        Mat3,
        Mat4,
        Mat2x2,
        Mat2x3,
        Mat2x4,
        Mat3x2,
        Mat3x3,
        Mat3x4,
        Mat4x2,
        Mat4x3,
        Mat4x4,
        DMat2,
        DMat3,
        DMat4,
        DMat2x2,
        DMat2x3,
        DMat2x4,
        DMat3x2,
        DMat3x3,
        DMat3x4,
        DMat4x2,
        DMat4x3,
        DMat4x4,
        Sampler1D,
        Sampler2D,
        Sampler3D,
        SamplerCube,
        Sampler2DRect,
        Sampler2DMs,
        SamplerBuffer,
        Sampler1DArray,
        Sampler2DArray,
        Sampler2DMsArray,
        SamplerCubeArray,
        Sampler1DShadow,
        Sampler2DShadow,
        Sampler2DRectShadow,
        Sampler1DArrayShadow,
        Sampler2DArrayShadow,
        SamplerCubeShadow,
        SamplerCubeArrayShadow,
        ISampler1D,
        ISampler2D,
        ISampler3D,
        ISamplerCube,
        ISampler2DRect,
        ISampler2DMs,
        ISamplerBuffer,
        ISampler1DArray,
        ISampler2DArray,
        ISampler2DMsArray,
        ISamplerCubeArray,
        USampler1D,
        USampler2D,
        USampler3D,
        USamplerCube,
        USampler2DRect,
        USampler2DMs,
        USamplerBuffer,
        USampler1DArray,
        USampler2DArray,
        USampler2DMsArray,
        USamplerCubeArray
    };
    Q_ENUM_NS(VariableType)
}

QT_END_NAMESPACE

#endif // QSHADERLANGUAGE_P_H
