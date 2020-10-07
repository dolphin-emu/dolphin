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

#ifndef QVULKANWINDOW_P_H
#define QVULKANWINDOW_P_H

#include <QtGui/private/qtguiglobal_p.h>

#if QT_CONFIG(vulkan) || defined(Q_CLANG_QDOC)

#include "qvulkanwindow.h"
#include <QtCore/QHash>
#include <private/qwindow_p.h>

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

QT_BEGIN_NAMESPACE

class QVulkanWindowPrivate : public QWindowPrivate
{
    Q_DECLARE_PUBLIC(QVulkanWindow)

public:
    ~QVulkanWindowPrivate();

    void ensureStarted();
    void init();
    void reset();
    bool createDefaultRenderPass();
    void recreateSwapChain();
    uint32_t chooseTransientImageMemType(VkImage img, uint32_t startIndex);
    bool createTransientImage(VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspectMask,
                              VkImage *images, VkDeviceMemory *mem, VkImageView *views, int count);
    void releaseSwapChain();
    void beginFrame();
    void endFrame();
    bool checkDeviceLost(VkResult err);
    void addReadback();
    void finishBlockingReadback();

    enum Status {
        StatusUninitialized,
        StatusFail,
        StatusFailRetry,
        StatusDeviceReady,
        StatusReady
    };
    Status status = StatusUninitialized;
    QVulkanWindowRenderer *renderer = nullptr;
    QVulkanInstance *inst = nullptr;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    int physDevIndex = 0;
    QVector<VkPhysicalDevice> physDevs;
    QVector<VkPhysicalDeviceProperties> physDevProps;
    QVulkanWindow::Flags flags;
    QByteArrayList requestedDevExtensions;
    QHash<VkPhysicalDevice, QVulkanInfoVector<QVulkanExtension> > supportedDevExtensions;
    QVector<VkFormat> requestedColorFormats;
    VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT;
    QVulkanWindow::QueueCreateInfoModifier queueCreateInfoModifier;

    VkDevice dev = VK_NULL_HANDLE;
    QVulkanDeviceFunctions *devFuncs;
    uint32_t gfxQueueFamilyIdx;
    uint32_t presQueueFamilyIdx;
    VkQueue gfxQueue;
    VkQueue presQueue;
    VkCommandPool cmdPool = VK_NULL_HANDLE;
    VkCommandPool presCmdPool = VK_NULL_HANDLE;
    uint32_t hostVisibleMemIndex;
    uint32_t deviceLocalMemIndex;
    VkFormat colorFormat;
    VkColorSpaceKHR colorSpace;
    VkFormat dsFormat = VK_FORMAT_D24_UNORM_S8_UINT;

    PFN_vkCreateSwapchainKHR vkCreateSwapchainKHR = nullptr;
    PFN_vkDestroySwapchainKHR vkDestroySwapchainKHR;
    PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHR;
    PFN_vkAcquireNextImageKHR vkAcquireNextImageKHR;
    PFN_vkQueuePresentKHR vkQueuePresentKHR;
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR vkGetPhysicalDeviceSurfaceCapabilitiesKHR = nullptr;
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR vkGetPhysicalDeviceSurfaceFormatsKHR;

    static const int MAX_SWAPCHAIN_BUFFER_COUNT = 3;
    static const int MAX_FRAME_LAG = QVulkanWindow::MAX_CONCURRENT_FRAME_COUNT;
    // QVulkanWindow only supports the always available FIFO mode. The
    // rendering thread will get throttled to the presentation rate (vsync).
    // This is in effect Example 5 from the VK_KHR_swapchain spec.
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    int swapChainBufferCount = 2;
    int frameLag = 2;

    QSize swapChainImageSize;
    VkSwapchainKHR swapChain = VK_NULL_HANDLE;
    bool swapChainSupportsReadBack = false;

    struct ImageResources {
        VkImage image = VK_NULL_HANDLE;
        VkImageView imageView = VK_NULL_HANDLE;
        VkCommandBuffer cmdBuf = VK_NULL_HANDLE;
        VkFence cmdFence = VK_NULL_HANDLE;
        bool cmdFenceWaitable = false;
        VkFramebuffer fb = VK_NULL_HANDLE;
        VkCommandBuffer presTransCmdBuf = VK_NULL_HANDLE;
        VkImage msaaImage = VK_NULL_HANDLE;
        VkImageView msaaImageView = VK_NULL_HANDLE;
    } imageRes[MAX_SWAPCHAIN_BUFFER_COUNT];

    VkDeviceMemory msaaImageMem = VK_NULL_HANDLE;

    uint32_t currentImage;

    struct FrameResources {
        VkFence fence = VK_NULL_HANDLE;
        bool fenceWaitable = false;
        VkSemaphore imageSem = VK_NULL_HANDLE;
        VkSemaphore drawSem = VK_NULL_HANDLE;
        VkSemaphore presTransSem = VK_NULL_HANDLE;
        bool imageAcquired = false;
        bool imageSemWaitable = false;
    } frameRes[MAX_FRAME_LAG];

    uint32_t currentFrame;

    VkRenderPass defaultRenderPass = VK_NULL_HANDLE;

    VkDeviceMemory dsMem = VK_NULL_HANDLE;
    VkImage dsImage = VK_NULL_HANDLE;
    VkImageView dsView = VK_NULL_HANDLE;

    bool framePending = false;
    bool frameGrabbing = false;
    QImage frameGrabTargetImage;
    VkImage frameGrabImage = VK_NULL_HANDLE;
    VkDeviceMemory frameGrabImageMem = VK_NULL_HANDLE;

    QMatrix4x4 m_clipCorrect;
};

QT_END_NAMESPACE

#endif // QT_CONFIG(vulkan)

#endif
