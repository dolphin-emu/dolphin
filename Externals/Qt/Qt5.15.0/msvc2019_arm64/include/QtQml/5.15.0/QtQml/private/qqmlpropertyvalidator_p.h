/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the tools applications of the Qt Toolkit.
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
#ifndef QQMLPROPERTYVALIDATOR_P_H
#define QQMLPROPERTYVALIDATOR_P_H

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

#include <private/qqmlengine_p.h>
#include <private/qqmlimport_p.h>
#include <private/qqmljsdiagnosticmessage_p.h>
#include <private/qqmlpropertycache_p.h>
#include <private/qv4compileddata_p.h>

#include <QtCore/qcoreapplication.h>

QT_BEGIN_NAMESPACE

class QQmlPropertyValidator
{
    Q_DECLARE_TR_FUNCTIONS(QQmlPropertyValidator)
public:
    QQmlPropertyValidator(QQmlEnginePrivate *enginePrivate, const QQmlImports &imports, const QQmlRefPointer<QV4::ExecutableCompilationUnit> &compilationUnit);

    QVector<QQmlError> validate();

private:
    QVector<QQmlError> validateObject(
            int objectIndex, const QV4::CompiledData::Binding *instantiatingBinding,
            bool populatingValueTypeGroupProperty = false) const;
    QQmlError validateLiteralBinding(
            QQmlPropertyCache *propertyCache, QQmlPropertyData *property,
            const QV4::CompiledData::Binding *binding) const;
    QQmlError validateObjectBinding(
            QQmlPropertyData *property, const QString &propertyName,
            const QV4::CompiledData::Binding *binding) const;

    bool canCoerce(int to, QQmlPropertyCache *fromMo) const;

    Q_REQUIRED_RESULT QVector<QQmlError> recordError(
            const QV4::CompiledData::Location &location, const QString &description) const;
    Q_REQUIRED_RESULT QVector<QQmlError> recordError(const QQmlError &error) const;
    QString stringAt(int index) const { return compilationUnit->stringAt(index); }
    QV4::ResolvedTypeReference *resolvedType(int id) const
    {
        return compilationUnit->resolvedType(id);
    }

    QQmlEnginePrivate *enginePrivate;
    QQmlRefPointer<QV4::ExecutableCompilationUnit> compilationUnit;
    const QQmlImports &imports;
    const QV4::CompiledData::Unit *qmlUnit;
    const QQmlPropertyCacheVector &propertyCaches;

    QVector<QV4::BindingPropertyData> * const bindingPropertyDataPerObject;
};

QT_END_NAMESPACE

#endif // QQMLPROPERTYVALIDATOR_P_H
