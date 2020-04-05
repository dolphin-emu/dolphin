/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Gui module
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QSHADER_P_H
#define QSHADER_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of a number of Qt sources files.  This header file may change from
// version to version without notice, or even be removed.
//
// We mean it.
//

#include <QtGui/qtguiglobal.h>
#include <private/qshaderdescription_p.h>

QT_BEGIN_NAMESPACE

struct QShaderPrivate;
class QShaderKey;

class Q_GUI_EXPORT QShaderVersion
{
public:
    enum Flag {
        GlslEs = 0x01
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    QShaderVersion() = default;
    QShaderVersion(int v, Flags f = Flags());

    int version() const { return m_version; }
    void setVersion(int v) { m_version = v; }

    Flags flags() const { return m_flags; }
    void setFlags(Flags f) { m_flags = f; }

private:
    int m_version = 100;
    Flags m_flags;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QShaderVersion::Flags)
Q_DECLARE_TYPEINFO(QShaderVersion, Q_MOVABLE_TYPE);

class Q_GUI_EXPORT QShaderCode
{
public:
    QShaderCode() = default;
    QShaderCode(const QByteArray &code, const QByteArray &entry = QByteArray());

    QByteArray shader() const { return m_shader; }
    void setShader(const QByteArray &code) { m_shader = code; }

    QByteArray entryPoint() const { return m_entryPoint; }
    void setEntryPoint(const QByteArray &entry) { m_entryPoint = entry; }

private:
    QByteArray m_shader;
    QByteArray m_entryPoint;
};

Q_DECLARE_TYPEINFO(QShaderCode, Q_MOVABLE_TYPE);

class Q_GUI_EXPORT QShader
{
public:
    enum Stage {
        VertexStage = 0,
        TessellationControlStage,
        TessellationEvaluationStage,
        GeometryStage,
        FragmentStage,
        ComputeStage
    };

    enum Source {
        SpirvShader = 0,
        GlslShader,
        HlslShader,
        DxbcShader, // fxc
        MslShader,
        DxilShader, // dxc
        MetalLibShader // xcrun metal + xcrun metallib
    };

    enum Variant {
        StandardShader = 0,
        BatchableVertexShader
    };

    QShader();
    QShader(const QShader &other);
    QShader &operator=(const QShader &other);
    ~QShader();
    void detach();

    bool isValid() const;

    Stage stage() const;
    void setStage(Stage stage);

    QShaderDescription description() const;
    void setDescription(const QShaderDescription &desc);

    QVector<QShaderKey> availableShaders() const;
    QShaderCode shader(const QShaderKey &key) const;
    void setShader(const QShaderKey &key, const QShaderCode &shader);
    void removeShader(const QShaderKey &key);

    QByteArray serialized() const;
    static QShader fromSerialized(const QByteArray &data);

    using NativeResourceBindingMap = QHash<int, QPair<int, int> >; // binding -> native_binding[, native_binding]
    const NativeResourceBindingMap *nativeResourceBindingMap(const QShaderKey &key) const;
    void setResourceBindingMap(const QShaderKey &key, const NativeResourceBindingMap &map);
    void removeResourceBindingMap(const QShaderKey &key);

private:
    QShaderPrivate *d;
    friend struct QShaderPrivate;
    friend Q_GUI_EXPORT bool operator==(const QShader &, const QShader &) Q_DECL_NOTHROW;
    friend Q_GUI_EXPORT uint qHash(const QShader &, uint) Q_DECL_NOTHROW;
#ifndef QT_NO_DEBUG_STREAM
    friend Q_GUI_EXPORT QDebug operator<<(QDebug, const QShader &);
#endif
};

class Q_GUI_EXPORT QShaderKey
{
public:
    QShaderKey() = default;
    QShaderKey(QShader::Source s,
               const QShaderVersion &sver,
               QShader::Variant svar = QShader::StandardShader);

    QShader::Source source() const { return m_source; }
    void setSource(QShader::Source s) { m_source = s; }

    QShaderVersion sourceVersion() const { return m_sourceVersion; }
    void setSourceVersion(const QShaderVersion &sver) { m_sourceVersion = sver; }

    QShader::Variant sourceVariant() const { return m_sourceVariant; }
    void setSourceVariant(QShader::Variant svar) { m_sourceVariant = svar; }

private:
    QShader::Source m_source = QShader::SpirvShader;
    QShaderVersion m_sourceVersion;
    QShader::Variant m_sourceVariant = QShader::StandardShader;
};

Q_DECLARE_TYPEINFO(QShaderKey, Q_MOVABLE_TYPE);

Q_GUI_EXPORT bool operator==(const QShader &lhs, const QShader &rhs) Q_DECL_NOTHROW;
Q_GUI_EXPORT uint qHash(const QShader &s, uint seed = 0) Q_DECL_NOTHROW;

inline bool operator!=(const QShader &lhs, const QShader &rhs) Q_DECL_NOTHROW
{
    return !(lhs == rhs);
}

Q_GUI_EXPORT bool operator==(const QShaderVersion &lhs, const QShaderVersion &rhs) Q_DECL_NOTHROW;
Q_GUI_EXPORT bool operator==(const QShaderKey &lhs, const QShaderKey &rhs) Q_DECL_NOTHROW;
Q_GUI_EXPORT bool operator==(const QShaderCode &lhs, const QShaderCode &rhs) Q_DECL_NOTHROW;

inline bool operator!=(const QShaderVersion &lhs, const QShaderVersion &rhs) Q_DECL_NOTHROW
{
    return !(lhs == rhs);
}

inline bool operator!=(const QShaderKey &lhs, const QShaderKey &rhs) Q_DECL_NOTHROW
{
    return !(lhs == rhs);
}

inline bool operator!=(const QShaderCode &lhs, const QShaderCode &rhs) Q_DECL_NOTHROW
{
    return !(lhs == rhs);
}

Q_GUI_EXPORT uint qHash(const QShaderKey &k, uint seed = 0) Q_DECL_NOTHROW;

#ifndef QT_NO_DEBUG_STREAM
Q_GUI_EXPORT QDebug operator<<(QDebug, const QShader &);
Q_GUI_EXPORT QDebug operator<<(QDebug dbg, const QShaderKey &k);
Q_GUI_EXPORT QDebug operator<<(QDebug dbg, const QShaderVersion &v);
#endif

QT_END_NAMESPACE

#endif
