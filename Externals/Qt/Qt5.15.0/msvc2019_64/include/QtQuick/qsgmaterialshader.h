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

#ifndef QSGMATERIALSHADER_H
#define QSGMATERIALSHADER_H

#include <QtQuick/qtquickglobal.h>
#if QT_CONFIG(opengl)
# include <QtGui/qopenglshaderprogram.h>
#endif
#include <QtGui/QMatrix4x4>
#include <QtCore/QRect>
#include <QtQuick/qsgmaterialtype.h> // for source compat

QT_BEGIN_NAMESPACE

class QSGMaterial;
class QSGMaterialShaderPrivate;

namespace QSGBatchRenderer {
    class ShaderManager;
}

class Q_QUICK_EXPORT QSGMaterialShader
{
public:
    class Q_QUICK_EXPORT RenderState {
    public:
        enum DirtyState
        {
            DirtyMatrix             = 0x0001,
            DirtyOpacity            = 0x0002,
            DirtyCachedMaterialData = 0x0004,
            DirtyAll                = 0xFFFF
        };
        Q_DECLARE_FLAGS(DirtyStates, DirtyState)

        inline DirtyStates dirtyStates() const { return m_dirty; }

        inline bool isMatrixDirty() const { return m_dirty & DirtyMatrix; }
        inline bool isOpacityDirty() const { return m_dirty & DirtyOpacity; }
        bool isCachedMaterialDataDirty() const { return m_dirty & DirtyCachedMaterialData; }

        float opacity() const;
        QMatrix4x4 combinedMatrix() const;
        QMatrix4x4 modelViewMatrix() const;
        QMatrix4x4 projectionMatrix() const;
        QRect viewportRect() const;
        QRect deviceRect() const;
        float determinant() const;
        float devicePixelRatio() const;
#if QT_CONFIG(opengl)
        QOpenGLContext *context() const;
#endif
    private:
        friend class QSGRenderer;
        DirtyStates m_dirty;
        const void *m_data;
    };

    QSGMaterialShader();
    virtual ~QSGMaterialShader();

    virtual void activate();
    virtual void deactivate();
    // First time a material is used, oldMaterial is null.
    virtual void updateState(const RenderState &state, QSGMaterial *newMaterial, QSGMaterial *oldMaterial);
    virtual char const *const *attributeNames() const = 0; // Array must end with null.
#if QT_CONFIG(opengl)
    inline QOpenGLShaderProgram *program() { return &m_program; }
#endif
protected:
    Q_DECLARE_PRIVATE(QSGMaterialShader)
    QSGMaterialShader(QSGMaterialShaderPrivate &dd);

    friend class QSGDefaultRenderContext;
    friend class QSGBatchRenderer::ShaderManager;
#if QT_CONFIG(opengl)
    void setShaderSourceFile(QOpenGLShader::ShaderType type, const QString &sourceFile);
    void setShaderSourceFiles(QOpenGLShader::ShaderType type, const QStringList &sourceFiles);

    virtual void compile();
#endif
    virtual void initialize() { }
#if QT_CONFIG(opengl)
    virtual const char *vertexShader() const;
    virtual const char *fragmentShader() const;
#endif
private:
#if QT_CONFIG(opengl)
    QOpenGLShaderProgram m_program;
#endif
    QScopedPointer<QSGMaterialShaderPrivate> d_ptr;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QSGMaterialShader::RenderState::DirtyStates)

QT_END_NAMESPACE

#endif
