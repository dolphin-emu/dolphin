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

#ifndef QRHINULL_P_H
#define QRHINULL_P_H

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

#include "qrhinull_p.h"
#include "qrhi_p_p.h"

QT_BEGIN_NAMESPACE

struct QNullBuffer : public QRhiBuffer
{
    QNullBuffer(QRhiImplementation *rhi, Type type, UsageFlags usage, int size);
    ~QNullBuffer();
    void release() override;
    bool build() override;

    QByteArray data;
};

struct QNullRenderBuffer : public QRhiRenderBuffer
{
    QNullRenderBuffer(QRhiImplementation *rhi, Type type, const QSize &pixelSize,
                       int sampleCount, QRhiRenderBuffer::Flags flags);
    ~QNullRenderBuffer();
    void release() override;
    bool build() override;
    QRhiTexture::Format backingFormat() const override;
};

struct QNullTexture : public QRhiTexture
{
    QNullTexture(QRhiImplementation *rhi, Format format, const QSize &pixelSize,
                  int sampleCount, Flags flags);
    ~QNullTexture();
    void release() override;
    bool build() override;
    bool buildFrom(NativeTexture src) override;

    QImage image[QRhi::MAX_LAYERS][QRhi::MAX_LEVELS];
};

struct QNullSampler : public QRhiSampler
{
    QNullSampler(QRhiImplementation *rhi, Filter magFilter, Filter minFilter, Filter mipmapMode,
                 AddressMode u, AddressMode v, AddressMode w);
    ~QNullSampler();
    void release() override;
    bool build() override;
};

struct QNullRenderPassDescriptor : public QRhiRenderPassDescriptor
{
    QNullRenderPassDescriptor(QRhiImplementation *rhi);
    ~QNullRenderPassDescriptor();
    void release() override;
    bool isCompatible(const QRhiRenderPassDescriptor *other) const override;
};

struct QNullRenderTargetData
{
    QNullRenderTargetData(QRhiImplementation *) { }

    QNullRenderPassDescriptor *rp = nullptr;
    QSize pixelSize;
    float dpr = 1;
};

struct QNullReferenceRenderTarget : public QRhiRenderTarget
{
    QNullReferenceRenderTarget(QRhiImplementation *rhi);
    ~QNullReferenceRenderTarget();
    void release() override;

    QSize pixelSize() const override;
    float devicePixelRatio() const override;
    int sampleCount() const override;

    QNullRenderTargetData d;
};

struct QNullTextureRenderTarget : public QRhiTextureRenderTarget
{
    QNullTextureRenderTarget(QRhiImplementation *rhi, const QRhiTextureRenderTargetDescription &desc, Flags flags);
    ~QNullTextureRenderTarget();
    void release() override;

    QSize pixelSize() const override;
    float devicePixelRatio() const override;
    int sampleCount() const override;

    QRhiRenderPassDescriptor *newCompatibleRenderPassDescriptor() override;
    bool build() override;

    QNullRenderTargetData d;
};

struct QNullShaderResourceBindings : public QRhiShaderResourceBindings
{
    QNullShaderResourceBindings(QRhiImplementation *rhi);
    ~QNullShaderResourceBindings();
    void release() override;
    bool build() override;
};

struct QNullGraphicsPipeline : public QRhiGraphicsPipeline
{
    QNullGraphicsPipeline(QRhiImplementation *rhi);
    ~QNullGraphicsPipeline();
    void release() override;
    bool build() override;
};

struct QNullComputePipeline : public QRhiComputePipeline
{
    QNullComputePipeline(QRhiImplementation *rhi);
    ~QNullComputePipeline();
    void release() override;
    bool build() override;
};

struct QNullCommandBuffer : public QRhiCommandBuffer
{
    QNullCommandBuffer(QRhiImplementation *rhi);
    ~QNullCommandBuffer();
    void release() override;
};

struct QNullSwapChain : public QRhiSwapChain
{
    QNullSwapChain(QRhiImplementation *rhi);
    ~QNullSwapChain();
    void release() override;

    QRhiCommandBuffer *currentFrameCommandBuffer() override;
    QRhiRenderTarget *currentFrameRenderTarget() override;

    QSize surfacePixelSize() override;

    QRhiRenderPassDescriptor *newCompatibleRenderPassDescriptor() override;
    bool buildOrResize() override;

    QNullReferenceRenderTarget rt;
    QNullCommandBuffer cb;
    int frameCount = 0;
};

class QRhiNull : public QRhiImplementation
{
public:
    QRhiNull(QRhiNullInitParams *params);

    bool create(QRhi::Flags flags) override;
    void destroy() override;

    QRhiGraphicsPipeline *createGraphicsPipeline() override;
    QRhiComputePipeline *createComputePipeline() override;
    QRhiShaderResourceBindings *createShaderResourceBindings() override;
    QRhiBuffer *createBuffer(QRhiBuffer::Type type,
                             QRhiBuffer::UsageFlags usage,
                             int size) override;
    QRhiRenderBuffer *createRenderBuffer(QRhiRenderBuffer::Type type,
                                         const QSize &pixelSize,
                                         int sampleCount,
                                         QRhiRenderBuffer::Flags flags) override;
    QRhiTexture *createTexture(QRhiTexture::Format format,
                               const QSize &pixelSize,
                               int sampleCount,
                               QRhiTexture::Flags flags) override;
    QRhiSampler *createSampler(QRhiSampler::Filter magFilter,
                               QRhiSampler::Filter minFilter,
                               QRhiSampler::Filter mipmapMode,
                               QRhiSampler:: AddressMode u,
                               QRhiSampler::AddressMode v,
                               QRhiSampler::AddressMode w) override;

    QRhiTextureRenderTarget *createTextureRenderTarget(const QRhiTextureRenderTargetDescription &desc,
                                                       QRhiTextureRenderTarget::Flags flags) override;

    QRhiSwapChain *createSwapChain() override;
    QRhi::FrameOpResult beginFrame(QRhiSwapChain *swapChain, QRhi::BeginFrameFlags flags) override;
    QRhi::FrameOpResult endFrame(QRhiSwapChain *swapChain, QRhi::EndFrameFlags flags) override;
    QRhi::FrameOpResult beginOffscreenFrame(QRhiCommandBuffer **cb, QRhi::BeginFrameFlags flags) override;
    QRhi::FrameOpResult endOffscreenFrame(QRhi::EndFrameFlags flags) override;
    QRhi::FrameOpResult finish() override;

    void resourceUpdate(QRhiCommandBuffer *cb, QRhiResourceUpdateBatch *resourceUpdates) override;

    void beginPass(QRhiCommandBuffer *cb,
                   QRhiRenderTarget *rt,
                   const QColor &colorClearValue,
                   const QRhiDepthStencilClearValue &depthStencilClearValue,
                   QRhiResourceUpdateBatch *resourceUpdates) override;
    void endPass(QRhiCommandBuffer *cb, QRhiResourceUpdateBatch *resourceUpdates) override;

    void setGraphicsPipeline(QRhiCommandBuffer *cb,
                             QRhiGraphicsPipeline *ps) override;

    void setShaderResources(QRhiCommandBuffer *cb,
                            QRhiShaderResourceBindings *srb,
                            int dynamicOffsetCount,
                            const QRhiCommandBuffer::DynamicOffset *dynamicOffsets) override;

    void setVertexInput(QRhiCommandBuffer *cb,
                        int startBinding, int bindingCount, const QRhiCommandBuffer::VertexInput *bindings,
                        QRhiBuffer *indexBuf, quint32 indexOffset,
                        QRhiCommandBuffer::IndexFormat indexFormat) override;

    void setViewport(QRhiCommandBuffer *cb, const QRhiViewport &viewport) override;
    void setScissor(QRhiCommandBuffer *cb, const QRhiScissor &scissor) override;
    void setBlendConstants(QRhiCommandBuffer *cb, const QColor &c) override;
    void setStencilRef(QRhiCommandBuffer *cb, quint32 refValue) override;

    void draw(QRhiCommandBuffer *cb, quint32 vertexCount,
              quint32 instanceCount, quint32 firstVertex, quint32 firstInstance) override;

    void drawIndexed(QRhiCommandBuffer *cb, quint32 indexCount,
                     quint32 instanceCount, quint32 firstIndex,
                     qint32 vertexOffset, quint32 firstInstance) override;

    void debugMarkBegin(QRhiCommandBuffer *cb, const QByteArray &name) override;
    void debugMarkEnd(QRhiCommandBuffer *cb) override;
    void debugMarkMsg(QRhiCommandBuffer *cb, const QByteArray &msg) override;

    void beginComputePass(QRhiCommandBuffer *cb, QRhiResourceUpdateBatch *resourceUpdates) override;
    void endComputePass(QRhiCommandBuffer *cb, QRhiResourceUpdateBatch *resourceUpdates) override;
    void setComputePipeline(QRhiCommandBuffer *cb, QRhiComputePipeline *ps) override;
    void dispatch(QRhiCommandBuffer *cb, int x, int y, int z) override;

    const QRhiNativeHandles *nativeHandles(QRhiCommandBuffer *cb) override;
    void beginExternal(QRhiCommandBuffer *cb) override;
    void endExternal(QRhiCommandBuffer *cb) override;

    QVector<int> supportedSampleCounts() const override;
    int ubufAlignment() const override;
    bool isYUpInFramebuffer() const override;
    bool isYUpInNDC() const override;
    bool isClipDepthZeroToOne() const override;
    QMatrix4x4 clipSpaceCorrMatrix() const override;
    bool isTextureFormatSupported(QRhiTexture::Format format, QRhiTexture::Flags flags) const override;
    bool isFeatureSupported(QRhi::Feature feature) const override;
    int resourceLimit(QRhi::ResourceLimit limit) const override;
    const QRhiNativeHandles *nativeHandles() override;
    void sendVMemStatsToProfiler() override;
    bool makeThreadLocalNativeContextCurrent() override;
    void releaseCachedResources() override;
    bool isDeviceLost() const override;

    void simulateTextureUpload(const QRhiResourceUpdateBatchPrivate::TextureOp &u);
    void simulateTextureCopy(const QRhiResourceUpdateBatchPrivate::TextureOp &u);
    void simulateTextureGenMips(const QRhiResourceUpdateBatchPrivate::TextureOp &u);

    QRhiNullNativeHandles nativeHandlesStruct;
    QRhiSwapChain *currentSwapChain = nullptr;
    QNullCommandBuffer offscreenCommandBuffer;
};

QT_END_NAMESPACE

#endif
