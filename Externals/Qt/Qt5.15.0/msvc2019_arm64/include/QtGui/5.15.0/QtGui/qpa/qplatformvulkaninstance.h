/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

#ifndef QPLATFORMVULKANINSTANCE_H
#define QPLATFORMVULKANINSTANCE_H

//
//  W A R N I N G
//  -------------
//
// This file is part of the QPA API and is not meant to be used
// in applications. Usage of this API may make your code
// source and binary incompatible with future versions of Qt.
//

#include <QtGui/qtguiglobal.h>

#if QT_CONFIG(vulkan) || defined(Q_CLANG_QDOC)

#include <qvulkaninstance.h>

QT_BEGIN_NAMESPACE

class QPlatformVulkanInstancePrivate;

class Q_GUI_EXPORT QPlatformVulkanInstance
{
    Q_DECLARE_PRIVATE(QPlatformVulkanInstance)

public:
    QPlatformVulkanInstance();
    virtual ~QPlatformVulkanInstance();

    virtual QVulkanInfoVector<QVulkanLayer> supportedLayers() const = 0;
    virtual QVulkanInfoVector<QVulkanExtension> supportedExtensions() const = 0;
    virtual void createOrAdoptInstance() = 0;
    virtual bool isValid() const = 0;
    virtual VkResult errorCode() const = 0;
    virtual VkInstance vkInstance() const = 0;
    virtual QByteArrayList enabledLayers() const = 0;
    virtual QByteArrayList enabledExtensions() const = 0;
    virtual PFN_vkVoidFunction getInstanceProcAddr(const char *name) = 0;
    virtual bool supportsPresent(VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex, QWindow *window) = 0;
    virtual void presentAboutToBeQueued(QWindow *window);
    virtual void presentQueued(QWindow *window);
    virtual void setDebugFilters(const QVector<QVulkanInstance::DebugFilter> &filters);

private:
    QScopedPointer<QPlatformVulkanInstancePrivate> d_ptr;
    Q_DISABLE_COPY(QPlatformVulkanInstance)
};

QT_END_NAMESPACE

#endif // QT_CONFIG(vulkan)

#if defined(Q_CLANG_QDOC)
/*
  The following include file did not exist for clang-qdoc running
  in macOS, but the classes are documented in qvulkanfunctions.cpp.
  clang-qdoc must parse the class declarations in an include file,
  or else it can't find a place to put the documentation for the
  classes. Apparently these classes are created at build time if
  Vulkan is present.
 */
#ifndef QVULKANFUNCTIONS_H
#define QVULKANFUNCTIONS_H

#include <QtGui/qtguiglobal.h>

#if QT_CONFIG(vulkan) || defined(Q_CLANG_QDOC)

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>

#include <QtCore/qscopedpointer.h>

QT_BEGIN_NAMESPACE

class QVulkanInstance;
class QVulkanFunctionsPrivate;
class QVulkanDeviceFunctionsPrivate;

class Q_GUI_EXPORT QVulkanFunctions
{
public:
    ~QVulkanFunctions();

private:
    Q_DISABLE_COPY(QVulkanFunctions)
    QVulkanFunctions(QVulkanInstance *inst);

    QScopedPointer<QVulkanFunctionsPrivate> d_ptr;
    friend class QVulkanInstance;
};

class Q_GUI_EXPORT QVulkanDeviceFunctions
{
public:
    ~QVulkanDeviceFunctions();

private:
    Q_DISABLE_COPY(QVulkanDeviceFunctions)
    QVulkanDeviceFunctions(QVulkanInstance *inst, VkDevice device);

    QScopedPointer<QVulkanDeviceFunctionsPrivate> d_ptr;
    friend class QVulkanInstance;
};

QT_END_NAMESPACE

#endif // QT_CONFIG(vulkan) || defined(Q_CLANG_QDOC)
#endif // QVULKANFUNCTIONS_H;
#endif // Q_CLANG_QDOC

#endif // QPLATFORMVULKANINSTANCE_H
