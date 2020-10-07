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

#ifndef QSGSIMPLEMATERIAL_H
#define QSGSIMPLEMATERIAL_H

#include <QtQuick/qsgmaterial.h>

QT_BEGIN_NAMESPACE

#if QT_DEPRECATED_SINCE(5, 15)

template <typename State>
class QSGSimpleMaterialShader : public QSGMaterialShader
{
public:
    void initialize() override {
        QSGMaterialShader::initialize();
#if QT_CONFIG(opengl)
        m_id_matrix = program()->uniformLocation(uniformMatrixName());
        if (m_id_matrix < 0) {
            qFatal("QSGSimpleMaterialShader does not implement 'uniform highp mat4 %s;' in its vertex shader",
                   uniformMatrixName());
        }

        const char *opacity = uniformOpacityName();
        if (opacity) {
            m_id_opacity = program()->uniformLocation(uniformOpacityName());
            if (m_id_opacity < 0) {
                qFatal("QSGSimpleMaterialShader does not implement 'uniform lowp float %s' in its fragment shader",
                       uniformOpacityName());
            }
        } else {
            m_id_opacity = -1;
        }
#endif
        resolveUniforms();
    }

    // ### Qt 6: make both virtual and fix docs
    const char *uniformMatrixName() const { return "qt_Matrix"; }
    const char *uniformOpacityName() const { return "qt_Opacity"; }

    void updateState(const RenderState &state, QSGMaterial *newMaterial, QSGMaterial *oldMaterial) override;

    QT_DEPRECATED_X("QSGSimpleMaterialShader is going to be removed in Qt 6.0. Use QSGMaterialShader instead.")
    virtual void updateState(const State *newState, const State *oldState) = 0;

    virtual void resolveUniforms() {}

    virtual QList<QByteArray> attributes() const = 0;

    char const *const *attributeNames() const override
    {
        if (m_attribute_pointers.size())
            return m_attribute_pointers.constData();

        QList<QByteArray> names = attributes();

        // Calculate the total number of bytes needed, so we don't get rellocs and
        // bad pointers while copying over the individual names.
        // Add an extra byte pr entry for the '\0' char.
        int total = 0;
        for (int i=0; i<names.size(); ++i)
            total += names.at(i).size() + 1;
        m_attribute_name_data.reserve(total);

        // Copy over the names
        for (int i=0; i<names.size(); ++i) {
            m_attribute_pointers << m_attribute_name_data.constData() + m_attribute_name_data.size();
            m_attribute_name_data.append(names.at(i));
            m_attribute_name_data.append('\0');
        }

        // Append the "null" terminator
        m_attribute_pointers << 0;

        return m_attribute_pointers.constData();
    }

private:
    int m_id_matrix;
    int m_id_opacity;

    mutable QByteArray m_attribute_name_data;
    mutable QVector<const char *> m_attribute_pointers;
};

#define QSG_DECLARE_SIMPLE_SHADER(Shader, State)                \
static QSGMaterialShader *createShader()                        \
{                                                               \
    return new Shader;                                          \
}                                                               \
public:                                                         \
static QSGSimpleMaterial<State> *createMaterial()               \
{                                                               \
    return new QSGSimpleMaterial<State>(createShader);          \
}


typedef QSGMaterialShader *(*PtrShaderCreateFunc)();


template <typename State>
class QSGSimpleMaterial : public QSGMaterial
{
public:
#ifndef Q_CLANG_QDOC
    QT_DEPRECATED_X("QSGSimpleMaterial is going to be removed in Qt 6.0. Use QSGMaterial instead.")
    QSGSimpleMaterial(const State &aState, PtrShaderCreateFunc func)
        : m_state(aState)
        , m_func(func)
    {
    }

    QT_DEPRECATED_X("QSGSimpleMaterial is going to be removed in Qt 6.0. Use QSGMaterial instead.")
    QSGSimpleMaterial(PtrShaderCreateFunc func)
        : m_func(func)
    {
    }

    QSGMaterialShader *createShader() const override { return m_func(); }
    QSGMaterialType *type() const override { return &m_type; }

    State *state() { return &m_state; }
    const State *state() const { return &m_state; }
#endif

private:
    static QSGMaterialType m_type;
    State m_state;
    PtrShaderCreateFunc m_func;
};

#define QSG_DECLARE_SIMPLE_COMPARABLE_SHADER(Shader, State)                     \
static QSGMaterialShader *createShader()                        \
{                                                               \
    return new Shader;                                          \
}                                                               \
public:                                                         \
static QSGSimpleMaterialComparableMaterial<State> *createMaterial()                            \
{                                                               \
    return new QSGSimpleMaterialComparableMaterial<State>(createShader);    \
}

template <typename State>
class QSGSimpleMaterialComparableMaterial : public QSGSimpleMaterial<State>
{

public:
    QSGSimpleMaterialComparableMaterial(const State &state, PtrShaderCreateFunc func)
        : QSGSimpleMaterial<State>(state, func) {}

    QSGSimpleMaterialComparableMaterial(PtrShaderCreateFunc func)
        : QSGSimpleMaterial<State>(func) {}

    int compare(const QSGMaterial *other) const override {
        return QSGSimpleMaterialComparableMaterial<State>::state()->compare(static_cast<const QSGSimpleMaterialComparableMaterial<State> *>(other)->state());
    }
};


template <typename State>
QSGMaterialType QSGSimpleMaterial<State>::m_type;


template <typename State>
Q_INLINE_TEMPLATE void QSGSimpleMaterialShader<State>::updateState(const RenderState &state, QSGMaterial *newMaterial, QSGMaterial *oldMaterial)
{
#if QT_CONFIG(opengl)
    if (state.isMatrixDirty())
        program()->setUniformValue(m_id_matrix, state.combinedMatrix());
    if (state.isOpacityDirty() && m_id_opacity >= 0)
        program()->setUniformValue(m_id_opacity, state.opacity());
#else
    Q_UNUSED(state)
#endif
    State *ns = static_cast<QSGSimpleMaterial<State> *>(newMaterial)->state();
    State *old = nullptr;
    if (oldMaterial)
        old = static_cast<QSGSimpleMaterial<State> *>(oldMaterial)->state();
    updateState(ns, old);
}

#endif

QT_END_NAMESPACE


#endif
