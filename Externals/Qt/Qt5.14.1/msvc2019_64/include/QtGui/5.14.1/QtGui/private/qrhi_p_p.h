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

#ifndef QRHI_P_H
#define QRHI_P_H

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

#include "qrhi_p.h"
#include "qrhiprofiler_p_p.h"
#include <QBitArray>
#include <QAtomicInt>
#include <QLoggingCategory>

QT_BEGIN_NAMESPACE

#define QRHI_RES(t, x) static_cast<t *>(x)
#define QRHI_RES_RHI(t) t *rhiD = static_cast<t *>(m_rhi)
#define QRHI_PROF QRhiProfilerPrivate *rhiP = m_rhi->profilerPrivateOrNull()
#define QRHI_PROF_F(f) for (bool qrhip_enabled = rhiP != nullptr; qrhip_enabled; qrhip_enabled = false) rhiP->f

Q_DECLARE_LOGGING_CATEGORY(QRHI_LOG_INFO)

class QRhiImplementation
{
public:
    virtual ~QRhiImplementation();

    virtual bool create(QRhi::Flags flags) = 0;
    virtual void destroy() = 0;

    virtual QRhiGraphicsPipeline *createGraphicsPipeline() = 0;
    virtual QRhiComputePipeline *createComputePipeline() = 0;
    virtual QRhiShaderResourceBindings *createShaderResourceBindings() = 0;
    virtual QRhiBuffer *createBuffer(QRhiBuffer::Type type,
                                     QRhiBuffer::UsageFlags usage,
                                     int size) = 0;
    virtual QRhiRenderBuffer *createRenderBuffer(QRhiRenderBuffer::Type type,
                                                 const QSize &pixelSize,
                                                 int sampleCount,
                                                 QRhiRenderBuffer::Flags flags) = 0;
    virtual QRhiTexture *createTexture(QRhiTexture::Format format,
                                       const QSize &pixelSize,
                                       int sampleCount,
                                       QRhiTexture::Flags flags) = 0;
    virtual QRhiSampler *createSampler(QRhiSampler::Filter magFilter, QRhiSampler::Filter minFilter,
                                       QRhiSampler::Filter mipmapMode,
                                       QRhiSampler:: AddressMode u, QRhiSampler::AddressMode v) = 0;

    virtual QRhiTextureRenderTarget *createTextureRenderTarget(const QRhiTextureRenderTargetDescription &desc,
                                                               QRhiTextureRenderTarget::Flags flags) = 0;

    virtual QRhiSwapChain *createSwapChain() = 0;
    virtual QRhi::FrameOpResult beginFrame(QRhiSwapChain *swapChain, QRhi::BeginFrameFlags flags) = 0;
    virtual QRhi::FrameOpResult endFrame(QRhiSwapChain *swapChain, QRhi::EndFrameFlags flags) = 0;
    virtual QRhi::FrameOpResult beginOffscreenFrame(QRhiCommandBuffer **cb, QRhi::BeginFrameFlags flags) = 0;
    virtual QRhi::FrameOpResult endOffscreenFrame(QRhi::EndFrameFlags flags) = 0;
    virtual QRhi::FrameOpResult finish() = 0;

    virtual void resourceUpdate(QRhiCommandBuffer *cb, QRhiResourceUpdateBatch *resourceUpdates) = 0;

    virtual void beginPass(QRhiCommandBuffer *cb,
                           QRhiRenderTarget *rt,
                           const QColor &colorClearValue,
                           const QRhiDepthStencilClearValue &depthStencilClearValue,
                           QRhiResourceUpdateBatch *resourceUpdates) = 0;
    virtual void endPass(QRhiCommandBuffer *cb, QRhiResourceUpdateBatch *resourceUpdates) = 0;

    virtual void setGraphicsPipeline(QRhiCommandBuffer *cb,
                                     QRhiGraphicsPipeline *ps) = 0;

    virtual void setShaderResources(QRhiCommandBuffer *cb,
                                    QRhiShaderResourceBindings *srb,
                                    int dynamicOffsetCount,
                                    const QRhiCommandBuffer::DynamicOffset *dynamicOffsets) = 0;

    virtual void setVertexInput(QRhiCommandBuffer *cb,
                                int startBinding, int bindingCount, const QRhiCommandBuffer::VertexInput *bindings,
                                QRhiBuffer *indexBuf, quint32 indexOffset,
                                QRhiCommandBuffer::IndexFormat indexFormat) = 0;

    virtual void setViewport(QRhiCommandBuffer *cb, const QRhiViewport &viewport) = 0;
    virtual void setScissor(QRhiCommandBuffer *cb, const QRhiScissor &scissor) = 0;
    virtual void setBlendConstants(QRhiCommandBuffer *cb, const QColor &c) = 0;
    virtual void setStencilRef(QRhiCommandBuffer *cb, quint32 refValue) = 0;

    virtual void draw(QRhiCommandBuffer *cb, quint32 vertexCount,
                      quint32 instanceCount, quint32 firstVertex, quint32 firstInstance) = 0;
    virtual void drawIndexed(QRhiCommandBuffer *cb, quint32 indexCount,
                             quint32 instanceCount, quint32 firstIndex,
                             qint32 vertexOffset, quint32 firstInstance) = 0;

    virtual void debugMarkBegin(QRhiCommandBuffer *cb, const QByteArray &name) = 0;
    virtual void debugMarkEnd(QRhiCommandBuffer *cb) = 0;
    virtual void debugMarkMsg(QRhiCommandBuffer *cb, const QByteArray &msg) = 0;

    virtual void beginComputePass(QRhiCommandBuffer *cb, QRhiResourceUpdateBatch *resourceUpdates) = 0;
    virtual void endComputePass(QRhiCommandBuffer *cb, QRhiResourceUpdateBatch *resourceUpdates) = 0;
    virtual void setComputePipeline(QRhiCommandBuffer *cb, QRhiComputePipeline *ps) = 0;
    virtual void dispatch(QRhiCommandBuffer *cb, int x, int y, int z) = 0;

    virtual const QRhiNativeHandles *nativeHandles(QRhiCommandBuffer *cb) = 0;
    virtual void beginExternal(QRhiCommandBuffer *cb) = 0;
    virtual void endExternal(QRhiCommandBuffer *cb) = 0;

    virtual QVector<int> supportedSampleCounts() const = 0;
    virtual int ubufAlignment() const = 0;
    virtual bool isYUpInFramebuffer() const = 0;
    virtual bool isYUpInNDC() const = 0;
    virtual bool isClipDepthZeroToOne() const = 0;
    virtual QMatrix4x4 clipSpaceCorrMatrix() const = 0;
    virtual bool isTextureFormatSupported(QRhiTexture::Format format, QRhiTexture::Flags flags) const = 0;
    virtual bool isFeatureSupported(QRhi::Feature feature) const = 0;
    virtual int resourceLimit(QRhi::ResourceLimit limit) const = 0;
    virtual const QRhiNativeHandles *nativeHandles() = 0;
    virtual void sendVMemStatsToProfiler() = 0;
    virtual bool makeThreadLocalNativeContextCurrent() = 0;
    virtual void releaseCachedResources() = 0;
    virtual bool isDeviceLost() const = 0;

    bool isCompressedFormat(QRhiTexture::Format format) const;
    void compressedFormatInfo(QRhiTexture::Format format, const QSize &size,
                              quint32 *bpl, quint32 *byteSize,
                              QSize *blockDim) const;
    void textureFormatInfo(QRhiTexture::Format format, const QSize &size,
                           quint32 *bpl, quint32 *byteSize) const;
    quint32 approxByteSizeForTexture(QRhiTexture::Format format, const QSize &baseSize,
                                     int mipCount, int layerCount);

    QRhiProfilerPrivate *profilerPrivateOrNull()
    {
        // return null when QRhi::EnableProfiling was not set
        QRhiProfilerPrivate *p = QRhiProfilerPrivate::get(&profiler);
        return p->rhiDWhenEnabled ? p : nullptr;
    }

    // only really care about resources that own native graphics resources underneath
    void registerResource(QRhiResource *res)
    {
        resources.insert(res);
    }

    void unregisterResource(QRhiResource *res)
    {
        resources.remove(res);
    }

    QSet<QRhiResource *> activeResources() const
    {
        return resources;
    }

    void addReleaseAndDestroyLater(QRhiResource *res)
    {
        if (inFrame)
            pendingReleaseAndDestroyResources.insert(res);
        else
            delete res;
    }

    void addCleanupCallback(const QRhi::CleanupCallback &callback)
    {
        cleanupCallbacks.append(callback);
    }

    bool sanityCheckGraphicsPipeline(QRhiGraphicsPipeline *ps);

    QRhi *q;

    static const int MAX_SHADER_CACHE_ENTRIES = 128;

protected:
    bool debugMarkers = false;
    int currentFrameSlot = 0; // for vk, mtl, and similar. unused by gl and d3d11.
    bool inFrame = false;

private:
    QRhi::Implementation implType;
    QThread *implThread;
    QRhiProfiler profiler;
    QVarLengthArray<QRhiResourceUpdateBatch *, 4> resUpdPool;
    QBitArray resUpdPoolMap;
    QSet<QRhiResource *> resources;
    QSet<QRhiResource *> pendingReleaseAndDestroyResources;
    QVector<QRhi::CleanupCallback> cleanupCallbacks;

    friend class QRhi;
    friend class QRhiResourceUpdateBatchPrivate;
};

template<typename T, size_t N>
bool qrhi_toTopLeftRenderTargetRect(const QSize &outputSize, const std::array<T, N> &r,
                                    T *x, T *y, T *w, T *h)
{
    // x,y are bottom-left in QRhiScissor and QRhiViewport but top-left in
    // Vulkan/Metal/D3D. Our input is an OpenGL-style scissor rect where both
    // negative x or y, and partly or completely out of bounds rects are
    // allowed. The only thing the input here cannot have is a negative width
    // or height. We must handle all other input gracefully, clamping to a zero
    // width or height rect in the worst case, and ensuring the resulting rect
    // is inside the rendertarget's bounds because some APIs' validation/debug
    // layers are allergic to out of bounds scissor or viewport rects.

    const T outputWidth = outputSize.width();
    const T outputHeight = outputSize.height();
    const T inputWidth = r[2];
    const T inputHeight = r[3];

    if (inputWidth < 0 || inputHeight < 0)
        return false;

    *x = r[0];
    *y = outputHeight - (r[1] + inputHeight);

    const T widthOffset = *x < 0 ? -*x : 0;
    const T heightOffset = *y < 0 ? -*y : 0;

    *x = qBound<T>(0, *x, outputWidth - 1);
    *y = qBound<T>(0, *y, outputHeight - 1);
    *w = qMax<T>(0, inputWidth - widthOffset);
    *h = qMax<T>(0, inputHeight - heightOffset);

    if (*x + *w > outputWidth)
        *w = qMax<T>(0, outputWidth - *x - 1);
    if (*y + *h > outputHeight)
        *h = qMax<T>(0, outputHeight - *y - 1);

    return true;
}

class QRhiResourceUpdateBatchPrivate
{
public:
    struct BufferOp {
        enum Type {
            DynamicUpdate,
            StaticUpload,
            Read
        };
        Type type;
        QRhiBuffer *buf;
        int offset;
        QByteArray data;
        int readSize;
        QRhiBufferReadbackResult *result;

        static BufferOp dynamicUpdate(QRhiBuffer *buf, int offset, int size, const void *data)
        {
            BufferOp op;
            op.type = DynamicUpdate;
            op.buf = buf;
            op.offset = offset;
            op.data = QByteArray(reinterpret_cast<const char *>(data), size ? size : buf->size());
            return op;
        }

        static BufferOp staticUpload(QRhiBuffer *buf, int offset, int size, const void *data)
        {
            BufferOp op;
            op.type = StaticUpload;
            op.buf = buf;
            op.offset = offset;
            op.data = QByteArray(reinterpret_cast<const char *>(data), size ? size : buf->size());
            return op;
        }

        static BufferOp read(QRhiBuffer *buf, int offset, int size, QRhiBufferReadbackResult *result)
        {
            BufferOp op;
            op.type = Read;
            op.buf = buf;
            op.offset = offset;
            op.readSize = size;
            op.result = result;
            return op;
        }
    };

    struct TextureOp {
        enum Type {
            Upload,
            Copy,
            Read,
            GenMips
        };
        Type type;
        QRhiTexture *dst;
        // Specifying multiple uploads for a subresource must be supported.
        // In the backend this can then end up, where applicable, as a
        // single, batched copy operation with only one set of barriers.
        // This helps when doing for example glyph cache fills.
        QVector<QRhiTextureSubresourceUploadDescription> subresDesc[QRhi::MAX_LAYERS][QRhi::MAX_LEVELS];
        QRhiTexture *src;
        QRhiTextureCopyDescription desc;
        QRhiReadbackDescription rb;
        QRhiReadbackResult *result;
        int layer;

        static TextureOp upload(QRhiTexture *tex, const QRhiTextureUploadDescription &desc)
        {
            TextureOp op;
            op.type = Upload;
            op.dst = tex;
            for (auto it = desc.cbeginEntries(), itEnd = desc.cendEntries(); it != itEnd; ++it)
                op.subresDesc[it->layer()][it->level()].append(it->description());
            return op;
        }

        static TextureOp copy(QRhiTexture *dst, QRhiTexture *src, const QRhiTextureCopyDescription &desc)
        {
            TextureOp op;
            op.type = Copy;
            op.dst = dst;
            op.src = src;
            op.desc = desc;
            return op;
        }

        static TextureOp read(const QRhiReadbackDescription &rb, QRhiReadbackResult *result)
        {
            TextureOp op;
            op.type = Read;
            op.rb = rb;
            op.result = result;
            return op;
        }

        static TextureOp genMips(QRhiTexture *tex, int layer)
        {
            TextureOp op;
            op.type = GenMips;
            op.dst = tex;
            op.layer = layer;
            return op;
        }
    };

    QVarLengthArray<BufferOp, 1024> bufferOps;
    QVarLengthArray<TextureOp, 256> textureOps;

    QRhiResourceUpdateBatch *q = nullptr;
    QRhiImplementation *rhi = nullptr;
    int poolIndex = -1;

    void free();
    void merge(QRhiResourceUpdateBatchPrivate *other);

    static QRhiResourceUpdateBatchPrivate *get(QRhiResourceUpdateBatch *b) { return b->d; }
};

Q_DECLARE_TYPEINFO(QRhiResourceUpdateBatchPrivate::BufferOp, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(QRhiResourceUpdateBatchPrivate::TextureOp, Q_MOVABLE_TYPE);

template<typename T>
struct QRhiBatchedBindings
{
    void feed(int binding, T resource) { // binding must be strictly increasing
        if (curBinding == -1 || binding > curBinding + 1) {
            finish();
            curBatch.startBinding = binding;
            curBatch.resources.clear();
            curBatch.resources.append(resource);
        } else {
            Q_ASSERT(binding == curBinding + 1);
            curBatch.resources.append(resource);
        }
        curBinding = binding;
    }

    void finish() {
        if (!curBatch.resources.isEmpty())
            batches.append(curBatch);
    }

    void clear() {
        batches.clear();
        curBatch.resources.clear();
        curBinding = -1;
    }

    struct Batch {
        uint startBinding;
        QVarLengthArray<T, 4> resources;

        bool operator==(const Batch &other) const
        {
            return startBinding == other.startBinding && resources == other.resources;
        }

        bool operator!=(const Batch &other) const
        {
            return !operator==(other);
        }
    };

    QVarLengthArray<Batch, 4> batches; // sorted by startBinding

    bool operator==(const QRhiBatchedBindings<T> &other) const
    {
        return batches == other.batches;
    }

    bool operator!=(const QRhiBatchedBindings<T> &other) const
    {
        return !operator==(other);
    }

private:
    Batch curBatch;
    int curBinding = -1;
};

class QRhiGlobalObjectIdGenerator
{
public:
#ifdef Q_ATOMIC_INT64_IS_SUPPORTED
    using Type = quint64;
#else
    using Type = quint32;
#endif
    static Type newId();
};

class QRhiPassResourceTracker
{
public:
    bool isEmpty() const;
    void reset();

    struct UsageState {
        int layout;
        int access;
        int stage;
    };

    enum BufferStage {
        BufVertexInputStage,
        BufVertexStage,
        BufFragmentStage,
        BufComputeStage
    };

    enum BufferAccess {
        BufVertexInput,
        BufIndexRead,
        BufUniformRead,
        BufStorageLoad,
        BufStorageStore,
        BufStorageLoadStore
    };

    void registerBuffer(QRhiBuffer *buf, int slot, BufferAccess *access, BufferStage *stage,
                        const UsageState &state);

    enum TextureStage {
        TexVertexStage,
        TexFragmentStage,
        TexColorOutputStage,
        TexDepthOutputStage,
        TexComputeStage
    };

    enum TextureAccess {
        TexSample,
        TexColorOutput,
        TexDepthOutput,
        TexStorageLoad,
        TexStorageStore,
        TexStorageLoadStore
    };

    void registerTexture(QRhiTexture *tex, TextureAccess *access, TextureStage *stage,
                         const UsageState &state);

    struct Buffer {
        int slot;
        BufferAccess access;
        BufferStage stage;
        UsageState stateAtPassBegin;
    };

    using BufferIterator = QHash<QRhiBuffer *, Buffer>::const_iterator;
    BufferIterator cbeginBuffers() const { return m_buffers.cbegin(); }
    BufferIterator cendBuffers() const { return m_buffers.cend(); }

    struct Texture {
        TextureAccess access;
        TextureStage stage;
        UsageState stateAtPassBegin;
    };

    using TextureIterator = QHash<QRhiTexture *, Texture>::const_iterator;
    TextureIterator cbeginTextures() const { return m_textures.cbegin(); }
    TextureIterator cendTextures() const { return m_textures.cend(); }

    static BufferStage toPassTrackerBufferStage(QRhiShaderResourceBinding::StageFlags stages);
    static TextureStage toPassTrackerTextureStage(QRhiShaderResourceBinding::StageFlags stages);

private:
    QHash<QRhiBuffer *, Buffer> m_buffers;
    QHash<QRhiTexture *, Texture> m_textures;
};

Q_DECLARE_TYPEINFO(QRhiPassResourceTracker::Buffer, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(QRhiPassResourceTracker::Texture, Q_MOVABLE_TYPE);

QT_END_NAMESPACE

#endif
