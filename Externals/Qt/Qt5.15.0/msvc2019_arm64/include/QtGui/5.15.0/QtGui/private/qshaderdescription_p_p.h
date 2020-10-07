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

#ifndef QSHADERDESCRIPTION_P_H
#define QSHADERDESCRIPTION_P_H

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

#include "qshaderdescription_p.h"
#include <QtCore/QVector>
#include <QtCore/QAtomicInt>
#include <QtCore/QJsonDocument>

QT_BEGIN_NAMESPACE

struct Q_GUI_EXPORT QShaderDescriptionPrivate
{
    QShaderDescriptionPrivate()
        : ref(1)
    {
        localSize[0] = localSize[1] = localSize[2] = 0;
    }

    QShaderDescriptionPrivate(const QShaderDescriptionPrivate *other)
        : ref(1),
          inVars(other->inVars),
          outVars(other->outVars),
          uniformBlocks(other->uniformBlocks),
          pushConstantBlocks(other->pushConstantBlocks),
          storageBlocks(other->storageBlocks),
          combinedImageSamplers(other->combinedImageSamplers),
          storageImages(other->storageImages),
          localSize(other->localSize)
    {
    }

    static QShaderDescriptionPrivate *get(QShaderDescription *desc) { return desc->d; }
    static const QShaderDescriptionPrivate *get(const QShaderDescription *desc) { return desc->d; }

    QJsonDocument makeDoc();
    void writeToStream(QDataStream *stream);
    void loadDoc(const QJsonDocument &doc);
    void loadFromStream(QDataStream *stream, int version);

    QAtomicInt ref;
    QVector<QShaderDescription::InOutVariable> inVars;
    QVector<QShaderDescription::InOutVariable> outVars;
    QVector<QShaderDescription::UniformBlock> uniformBlocks;
    QVector<QShaderDescription::PushConstantBlock> pushConstantBlocks;
    QVector<QShaderDescription::StorageBlock> storageBlocks;
    QVector<QShaderDescription::InOutVariable> combinedImageSamplers;
    QVector<QShaderDescription::InOutVariable> storageImages;
    std::array<uint, 3> localSize;
};

QT_END_NAMESPACE

#endif
