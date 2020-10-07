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

#ifndef QSHADERFORMAT_P_H
#define QSHADERFORMAT_P_H

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

#include <QtCore/qstringlist.h>
#include <QtCore/qversionnumber.h>

QT_BEGIN_NAMESPACE

class QShaderFormat
{
public:
    enum Api : int {
        NoApi,
        OpenGLNoProfile,
        OpenGLCoreProfile,
        OpenGLCompatibilityProfile,
        OpenGLES,
        VulkanFlavoredGLSL
    };

    enum ShaderType : int {
        Vertex = 0,
        TessellationControl,
        TessellationEvaluation,
        Geometry,
        Fragment,
        Compute
    };

    Q_GUI_EXPORT QShaderFormat() noexcept;

    Q_GUI_EXPORT Api api() const noexcept;
    Q_GUI_EXPORT void setApi(Api api) noexcept;

    Q_GUI_EXPORT QVersionNumber version() const noexcept;
    Q_GUI_EXPORT void setVersion(const QVersionNumber &version) noexcept;

    Q_GUI_EXPORT QStringList extensions() const noexcept;
    Q_GUI_EXPORT void setExtensions(const QStringList &extensions) noexcept;

    Q_GUI_EXPORT QString vendor() const noexcept;
    Q_GUI_EXPORT void setVendor(const QString &vendor) noexcept;

    Q_GUI_EXPORT bool isValid() const noexcept;
    Q_GUI_EXPORT bool supports(const QShaderFormat &other) const noexcept;

    Q_GUI_EXPORT ShaderType shaderType() const Q_DECL_NOTHROW;
    Q_GUI_EXPORT void setShaderType(ShaderType shaderType) Q_DECL_NOTHROW;

private:
    Api m_api;
    QVersionNumber m_version;
    QStringList m_extensions;
    QString m_vendor;
    ShaderType m_shaderType;
};

Q_GUI_EXPORT bool operator==(const QShaderFormat &lhs, const QShaderFormat &rhs) noexcept;

inline bool operator!=(const QShaderFormat &lhs, const QShaderFormat &rhs) noexcept
{
    return !(lhs == rhs);
}

Q_DECLARE_TYPEINFO(QShaderFormat, Q_MOVABLE_TYPE);

QT_END_NAMESPACE

Q_DECLARE_METATYPE(QShaderFormat)

#endif // QSHADERFORMAT_P_H
